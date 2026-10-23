"""Host tests for strict layouts and migration preservation masks."""
import hashlib
import struct
import unittest
from flash_layout import LEGACY, FACTORY_2M, capacity, parse_table, profile
from flash_migrate import preservation, write_payload, read_snapshot
from unittest.mock import patch


def table(entries):
    packed = b''.join(struct.pack('<HBBII16sI', 0x50aa, *value[:2], *value[2:], name.encode(), 0)
                      for name, value in entries.items())
    return (packed+b'\xeb\xeb'+b'\xff'*14+hashlib.md5(packed).digest()).ljust(0x1000, b'\xff')


class LayoutTests(unittest.TestCase):
    def test_legacy_and_new(self):
        self.assertEqual(profile(table(LEGACY))[0], 'legacy-three-slot')
        self.assertEqual(profile(table(FACTORY_2M))[0], 'factory-2m-usb-v1')
        self.assertEqual(capacity(table(FACTORY_2M)), 0x200000)
        for name in ('nvs','nvs_sec','phy_init','otadata'):
            self.assertEqual(LEGACY[name], FACTORY_2M[name])

    def test_reject_corruption_and_moved_storage(self):
        bad=bytearray(table(FACTORY_2M));bad[10]^=1
        with self.assertRaises(ValueError):parse_table(bad)
        bad=dict(FACTORY_2M);bad['nvs_sec']=(1,2,0x3c0000,0x6000)
        with self.assertRaises(ValueError):profile(table(bad))
        with self.assertRaises(ValueError):parse_table(b'\xff'*4096)

    def test_preservation_mask_rejects_every_persistent_region(self):
        before=b'\xff'*0x400000
        after=bytearray(before)
        for address in (0x8000,0xf000,0x10000,0x20000,0x12ffff):
            after[address]=0
        preservation(before,after,0x110000)
        for address in (0,0x9000,0x11000,0x130000,0x220000,0x3b0000,0x3fffff):
            bad=bytearray(after);bad[address]=0
            with self.assertRaises(ValueError):preservation(before,bad,0x110000)
        with self.assertRaises(ValueError):preservation(before,b'',0x110000)

    def test_writer_reuses_existing_stub_and_never_resets_between_writes(self):
        marker=object()
        with patch('flash_migrate.esptool.main') as run:
            write_payload(marker,0x20000,'app.bin')
        argv=run.call_args.args[0]
        self.assertIn('--no-stub',argv)
        self.assertEqual(argv[argv.index('--after')+1],'no_reset_stub')
        self.assertNotIn('--erase-all',argv)
        self.assertEqual(run.call_args.kwargs['esp'],marker)

    def test_explicit_bootloader_mask_preserves_nvs_and_padding(self):
        before=b'\xff'*0x400000
        after=bytearray(before);after[0]=0;after[0x5fff]=0
        preservation(before,after,0x110000,24224)
        with self.assertRaises(ValueError):preservation(before,after,0x110000)
        for address in (0x6000,0x7000,0x9000,0x3b0000):
            bad=bytearray(after);bad[address]=0
            with self.assertRaises(ValueError):preservation(before,bad,0x110000,24224)
        with self.assertRaises(ValueError):preservation(before,after,0x110000,0x8001)

    def test_snapshot_uses_bounded_complete_reads(self):
        class Reader:
            def __init__(self):self.calls=[]
            def read_flash(self,offset,length):
                self.calls.append((offset,length));return b'\xff'*length
        reader=Reader()
        self.assertEqual(len(read_snapshot(reader)),0x400000)
        self.assertEqual(reader.calls,[(i,0x10000) for i in range(0,0x400000,0x10000)])


if __name__ == '__main__':
    unittest.main()
