"""Strict, checksummed partition contracts for the two supported USB layouts."""
import hashlib
import struct

FLASH_SIZE = 0x400000
COMMON = {
    'nvs': (1, 2, 0x9000, 0x6000),
    'otadata': (1, 0, 0xf000, 0x2000),
    'phy_init': (1, 1, 0x11000, 0x1000),
    'nvs_sec': (1, 2, 0x3b0000, 0x6000),
}
LEGACY = dict(COMMON, factory=(0, 0, 0x20000, 0x130000),
              ota_0=(0, 16, 0x150000, 0x130000), ota_1=(0, 17, 0x280000, 0x130000))
FACTORY_2M = dict(COMMON, factory=(0, 0, 0x20000, 0x200000))
PROFILES = {'legacy-three-slot': LEGACY, 'factory-2m-usb-v1': FACTORY_2M}


def parse_table(data):
    entries = {}
    for pos in range(0, min(len(data), 0xc00), 32):
        entry = data[pos:pos+32]
        if len(entry) != 32:
            break
        magic = int.from_bytes(entry[:2], 'little')
        if magic == 0xebeb:
            if hashlib.md5(data[:pos]).digest() != entry[16:32]:
                raise ValueError('Partition MD5 mismatch')
            return entries
        if magic != 0x50aa:
            raise ValueError('Missing partition checksum or bad entry')
        _, kind, sub, offset, size, label, flags = struct.unpack('<HBBII16sI', entry)
        name = label.split(b'\0')[0].decode('ascii')
        if name in entries or flags or not size or offset+size > FLASH_SIZE:
            raise ValueError('Invalid or unsupported partition')
        if offset % (0x10000 if kind == 0 else 0x1000) or size % 0x1000:
            raise ValueError('Partition alignment')
        entries[name] = (kind, sub, offset, size)
    raise ValueError('Missing partition checksum')


def profile(data):
    entries = parse_table(data)
    for name, contract in PROFILES.items():
        if entries == contract:
            return name, entries
    raise ValueError('Unknown layout; refusing to move persistent storage')


def capacity(data):
    _, entries = profile(data)
    return entries['factory'][3]
