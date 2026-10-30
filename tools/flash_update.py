"""App-only update that refuses a legacy/unknown device partition table."""
import argparse
import json
import time
from pathlib import Path
from datetime import datetime, timezone
import esptool
from build import ROOT
from flash_layout import profile
from flash_migrate import save_verified,sha,write_payload
from verify import verify

def receive_buffer(esp):
    # Loader.command restores the pre-command timeout. None would make a lost
    # SLIP delimiter wait forever during the following bulk data/digest read.
    esp._port.timeout=5
    # PySerial's Windows default is 4 KiB; a single stub data packet is 4 KiB
    # plus framing. The driver may ignore this recommendation, so exact checks stay.
    if hasattr(esp._port,'set_buffer_size'):
        esp._port.set_buffer_size(rx_size=1024*1024,tx_size=65536)

def read_range(esp,address,size):
    chunks=[]
    # Short bursts avoid USB/JTAG SLIP overruns; every byte is still read twice
    # for the backup and compared in full before any write is permitted.
    for offset in range(0,size,0x800):
        count=min(0x800,size-offset);chunk=esp.read_flash(address+offset,count)
        if len(chunk)!=count:raise RuntimeError('Short Flash read')
        chunks.append(chunk)
        time.sleep(.002)
    return b''.join(chunks)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port',required=True);ap.add_argument('--expect-mac',required=True)
    ap.add_argument('--release',type=Path,default=ROOT/'build/release')
    args=ap.parse_args();release=args.release.resolve();verify(release)
    manifest=json.loads((release/'manifest.json').read_text(encoding='utf8'))
    app_path=release/manifest['application']['path'];app=app_path.read_bytes()
    if manifest['build'].get('layout')!='factory-2m-usb-v1' or len(app)>0x200000:
        raise RuntimeError('Expected verified 2 MiB factory release')
    esp=esptool.detect_chip(port=args.port);writing=False;success=False
    try:
        mac=':'.join(f'{b:02x}' for b in esp.read_mac())
        if esp.CHIP_NAME!='ESP32-C6' or mac.lower()!=args.expect_mac.lower():raise RuntimeError('Wrong device')
        if esp.get_secure_boot_enabled() or esp.get_flash_encryption_enabled():raise RuntimeError('Unsupported security configuration')
        receive_buffer(esp)
        esp=esp.run_stub();esp.flash_spi_attach(0);esp.change_baud(230400)
        if (esp.flash_id()>>16)&255!=0x16:raise RuntimeError('Expected 4 MiB Flash')
        table=esp.read_flash(0x8000,0x1000)
        if profile(table)[0]!='factory-2m-usb-v1':raise RuntimeError('Legacy device: migrate before app-only update')
        out=ROOT/'build/migration-private'/('app-update-'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')+'-'+args.port.upper())
        out.mkdir(parents=True,exist_ok=False);save_verified(out/'partition-table.bin',table)
        persistent={address:esp.read_flash(address,0x6000) for address in (0x9000,0x3b0000)}
        for address,data in persistent.items():save_verified(out/f'nvs-{address:x}.bin',data)
        print('Backing up current 2 MiB application slot ...',flush=True)
        old=read_range(esp,0x20000,0x200000);save_verified(out/'previous-app-slot.bin',old)
        if old!=read_range(esp,0x20000,0x200000):raise RuntimeError('Application backup verification differs')
        record=dict(mac=mac,application_sha256=sha(app),previous_slot_sha256=sha(old),status='backup_verified')
        record_path=out/'update.json';record_path.write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
        writing=True;write_payload(esp,0x20000,app_path)
        if read_range(esp,0x20000,len(app))!=app:raise RuntimeError('Application read-back mismatch')
        if esp.read_flash(0x8000,0x1000)!=table:raise RuntimeError('Partition table changed unexpectedly')
        for address,data in persistent.items():
            if esp.read_flash(address,len(data))!=data:raise RuntimeError('NVS changed unexpectedly')
        record.update(status='passed_before_first_boot',both_nvs_unchanged=True,partition_table_unchanged=True)
        record_path.write_text(json.dumps(record,indent=2)+'\n',encoding='utf8');success=True
        print('PASS app-only update: image read-back, unchanged table and both NVS partitions.',flush=True)
        print('Private update record: '+str(record_path),flush=True)
    finally:
        if success or not writing:esp.hard_reset()
        else:print('Update verification failed: left in download mode; retain private backup for recovery.',flush=True)
        esp._port.close()


if __name__=='__main__':main()
