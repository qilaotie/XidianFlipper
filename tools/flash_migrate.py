"""Backed-up legacy -> 2 MiB factory USB migration. No full-chip erase."""
import argparse
import hashlib
import json
import os
from datetime import datetime, timezone
from pathlib import Path
import esptool
from flash_layout import FLASH_SIZE, profile
from verify import verify
from build import ROOT


def sha(data):
    return hashlib.sha256(data).hexdigest()


def save_verified(path, data):
    with path.open('xb') as stream:
        stream.write(data); stream.flush(); os.fsync(stream.fileno())
    if path.read_bytes() != data:
        raise RuntimeError('Backup disk verification failed')


def preservation(before, after, app_size, bootloader_size=0):
    if len(before)!=FLASH_SIZE or len(after)!=FLASH_SIZE:
        raise ValueError('Incomplete Flash snapshot')
    ranges=((0x8000,0x9000),(0xf000,0x11000),(0x20000,0x20000+((app_size+4095)//4096)*4096))
    if bootloader_size:
        if not 0 < bootloader_size <= 0x8000:
            raise ValueError('Bootloader overlaps partition table')
        ranges=((0,((bootloader_size+4095)//4096)*4096),)+ranges
    position=0
    for start,end in sorted(ranges):
        if before[position:start]!=after[position:start]:
            raise ValueError(f'Unexpected Flash change at preserved range {position:#x}..{start:#x}')
        position=end
    if before[position:]!=after[position:]:
        raise ValueError('Unexpected Flash change after application')
    return ranges


def write_payload(esp, address, path):
    # esptool 4.x main() otherwise tries to upload the stub again even when
    # given an existing StubLoader. --no-stub skips that upload; the existing
    # instance still provides compressed writes and remains resident afterward.
    esptool.main(['--chip','esp32c6','--no-stub','--after','no_reset_stub','write_flash',
                  '--compress','--flash_mode','keep','--flash_size','4MB','--flash_freq','keep',
                  hex(address),str(path)],esp=esp)


def read_snapshot(esp):
    # Short independent transfers avoid a long USB/JTAG SLIP stream; every
    # transfer is checked by esptool and still compared across two full passes.
    chunks=[]
    for offset in range(0,FLASH_SIZE,0x10000):
        data=esp.read_flash(offset,0x10000)
        if len(data)!=0x10000:
            raise RuntimeError('Short Flash transfer')
        chunks.append(data)
    return b''.join(chunks)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port',required=True)
    ap.add_argument('--expect-mac',required=True)
    ap.add_argument('--release',type=Path,default=ROOT/'build/release')
    ap.add_argument('--apply',action='store_true',help='Actually write after all checks and fresh double backup')
    ap.add_argument('--replace-bootloader',action='store_true',help='Explicitly authorized replacement with verified release bootloader')
    args=ap.parse_args()
    release=args.release.resolve();verify(release)
    manifest=json.loads((release/'manifest.json').read_text(encoding='utf8'))
    if manifest['build'].get('layout')!='factory-2m-usb-v1':
        raise RuntimeError('Wrong target layout')
    images={int(i['offset'],0):(release/i['path']).read_bytes() for i in manifest['images']}
    if profile(images[0x8000])[0]!='factory-2m-usb-v1' or images[0xf000]!=b'\xff'*0x2000:
        raise RuntimeError('Unsafe partition or boot-selection payload')
    app_size=len(images[0x20000])
    if not 0 < len(images[0]) <= 0x8000:
        raise RuntimeError('Bootloader overlaps partition table')
    if not args.apply:
        print('Plan: freshly back up twice, then write app, clear OTA selection, write table LAST.')
        print(f'Writes: app 0x20000/{app_size}, OTA selection 0xf000/8192, table 0x8000/4096.')
        print('Bootloader replacement explicitly enabled.' if args.replace_bootloader else 'No bootloader write.')
        print('No NVS write; no full-chip erase. Add --apply to proceed.')
        return
    esp=esptool.detect_chip(port=args.port)
    writing=False;success=False
    try:
        mac=':'.join(f'{b:02x}' for b in esp.read_mac())
        if mac.lower()!=args.expect_mac.lower() or esp.CHIP_NAME!='ESP32-C6':
            raise RuntimeError('Device identity mismatch')
        if esp.get_secure_boot_enabled() or esp.get_flash_encryption_enabled():
            raise RuntimeError('Secure Boot/encrypted Flash requires a different migration procedure')
        esp=esp.run_stub();esp.flash_spi_attach(0);esp.change_baud(230400)
        if (esp.flash_id()>>16)&255!=0x16:
            raise RuntimeError('Expected 4 MiB Flash')
        out=ROOT/'build/migration-private'/('pre-migrate-'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
        out.mkdir(parents=True,exist_ok=False)
        print('Reading fresh full Flash backup, pass 1/2 ...',flush=True)
        before=read_snapshot(esp);save_verified(out/'flash.bin',before)
        print('Reading fresh full Flash backup, pass 2/2 ...',flush=True)
        repeat=read_snapshot(esp);save_verified(out/'flash-verify.bin',repeat)
        if before!=repeat or len(before)!=FLASH_SIZE:
            raise RuntimeError('Fresh backup differs or is short; refusing write')
        if profile(before[0x8000:0x9000])[0]!='legacy-three-slot':
            raise RuntimeError('Only legacy layout can use this migration. Already migrated? Use app-only update.')
        replace_bootloader=before[:len(images[0])]!=images[0] and args.replace_bootloader
        if before[:len(images[0])]!=images[0] and not replace_bootloader:
            raise RuntimeError('Bootloader differs from release; refusing implicit bootloader replacement')
        record=dict(schema=1,port=args.port,mac=mac,before_sha256=sha(before),twice_verified=True,
                    release_application_sha256=sha(images[0x20000]),status='backup_verified',
                    bootloader_replaced=replace_bootloader,
                    previous_bootloader_sha256=sha(before[:len(images[0])]),
                    release_bootloader_sha256=sha(images[0]),
                    privacy='Private Flash backup. Never upload this folder.')
        record_path=out/'migration.json'
        record_path.write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
        print('Fresh backup verified: '+str(out),flush=True)
        writing=True
        # Leave the stub running between operations: application code cannot mutate NVS.
        addresses=(0x20000,0xf000,0,0x8000) if replace_bootloader else (0x20000,0xf000,0x8000)
        for address in addresses:
            item=next(i for i in manifest['images'] if int(i['offset'],0)==address)
            write_payload(esp,address,release/item['path'])
        print('Read-back verification of entire Flash and all untouched ranges ...',flush=True)
        after=read_snapshot(esp);save_verified(out/'flash-after.bin',after)
        preservation(before,after,app_size,len(images[0]) if replace_bootloader else 0)
        for address in addresses:
            if after[address:address+len(images[address])]!=images[address]:
                raise RuntimeError(f'Payload read-back differs at {address:#x}')
        if profile(after[0x8000:0x9000])[0]!='factory-2m-usb-v1':
            raise RuntimeError('New partition table mismatch')
        record.update(status='passed_before_first_boot',after_sha256=sha(after),
                      all_untouched_bytes_identical=True,nvs_preserved=True,nvs_sec_preserved=True)
        record_path.write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
        success=True
        print('PASS: app/table verified; all bytes outside allowed writes unchanged, including both NVS partitions.',flush=True)
        print('Private migration record: '+str(record_path),flush=True)
    finally:
        if success or not writing:
            esp.hard_reset()
        else:
            print('WRITE/VERIFY FAILED: leaving device in download mode. Do not reset; use the private full Flash backup for recovery.',flush=True)
        esp._port.close()


if __name__=='__main__':
    main()
