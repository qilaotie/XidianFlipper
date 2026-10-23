"""Read-only, twice-verified local Flash backup before partition migration."""
import argparse
import hashlib
import json
import os
import struct
from datetime import datetime, timezone
from pathlib import Path
import esptool


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--expect-mac', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = root / 'build' / 'migration-private' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    out.mkdir(parents=True, exist_ok=False)
    esp = esptool.detect_chip(port=args.port)
    try:
        mac = ':'.join(f'{b:02x}' for b in esp.read_mac())
        if mac.lower() != args.expect_mac.lower() or esp.CHIP_NAME != 'ESP32-C6':
            raise RuntimeError('Device identity mismatch; no migration allowed')
        esp = esp.run_stub()
        esp.flash_spi_attach(0)
        flash_id = esp.flash_id()
        if (flash_id >> 16) & 255 != 0x16:
            raise RuntimeError(f'Expected 4 MiB Flash, JEDEC ID={flash_id:06x}')
        esp.change_baud(460800)
        copies = []
        for index in range(2):
            print(f'Reading full 4 MiB Flash, pass {index+1}/2 ...', flush=True)
            data = esp.read_flash(0, 0x400000)
            if len(data) != 0x400000:
                raise RuntimeError('Short Flash read')
            path = out / ('flash.bin' if index == 0 else 'flash-verify.bin')
            with path.open('xb') as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            if sha(path.read_bytes()) != sha(data):
                raise RuntimeError('Backup disk verification failed')
            copies.append(data)
        if copies[0] != copies[1]:
            raise RuntimeError('Two Flash reads differ; no migration allowed')
        data = copies[0]
        partitions = []
        for offset in range(0x8000, 0x8c00, 32):
            magic, kind, subtype, address, size, label, flags = struct.unpack_from('<HBBII16sI', data, offset)
            if magic != 0x50aa:
                break
            partitions.append(dict(name=label.split(b'\0')[0].decode(), type=kind, subtype=subtype,
                                   offset=address, size=size, sha256=sha(data[address:address+size])))
        if not partitions:
            raise RuntimeError('Cannot parse original partition table')
        manifest = dict(schema=1, port=args.port, mac=mac, bytes=len(data), sha256=sha(data),
                        twice_verified=True, partitions=partitions,
                        privacy='Contains private configuration. Never publish this directory.')
        (out / 'backup.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf8')
        print('PASS: full Flash backup read twice and disk hashes verified.', flush=True)
        print('Private backup: '+str(out), flush=True)
        print('Flash SHA-256: '+manifest['sha256'], flush=True)
        print('Partitions: '+', '.join(p['name']+'@'+hex(p['offset']) for p in partitions), flush=True)
    finally:
        try:
            esp.hard_reset()
        finally:
            esp._port.close()


if __name__ == '__main__':
    main()
