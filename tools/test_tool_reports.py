"""Validate the actual HID descriptor and legacy advertising envelope."""
import ast
from pathlib import Path
import re
import unittest

SOURCE = (Path(__file__).resolve().parents[1] / "firmware/radio/tool_hid.c").read_text(encoding="utf-8")


def array(name):
    body = re.search(r"\b" + name + r"\[\]\s*=\s*\{(.*?)\};", SOURCE, re.S)[1]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    values = []
    for part in body.split(","):
        part = part.strip()
        if part:
            value = ast.literal_eval(part)
            values.append(ord(value) if isinstance(value, str) else value)
    return bytes(values)


class ToolReports(unittest.TestCase):
    def test_character_editor_wheel_mapping(self):
        root = Path(__file__).resolve().parents[1]
        messages = (root / 'firmware/ui/local_pages.c').read_text(encoding='utf-8')
        wifi = (root / 'firmware/app/wireless_tools.c').read_text(encoding='utf-8')
        self.assertIn('message_draft_group_rotate(&s_draft_selector,key==BTN_ID_CW?-1:1);', messages)
        self.assertIn('int direction=key==BTN_ID_CW?-1:1;', wifi)

    def test_remote_wheel_mapping(self):
        ui = (Path(__file__).resolve().parents[1] / "firmware/app/wireless_tools.c").read_text(encoding="utf-8")
        mappings = dict((key, (int(media, 16), int(slides, 16))) for key, media, slides in
                        re.findall(r"if\(key==BTN_ID_(CW|CC|SP|LP)\) usage=media\?(0x[0-9A-Fa-f]+):(0x[0-9A-Fa-f]+);", ui))
        self.assertEqual(mappings, {"CW": (0xEA, 0x4B), "CC": (0xE9, 0x4E),
                                   "SP": (0xCD, 0x2C), "LP": (0xB5, 0x3E)})

    def test_hid_report_sizes(self):
        report = array("s_report_map")
        size = count = report_id = 0
        depth = 0
        bits = {}
        pos = 0
        while pos < len(report):
            prefix = report[pos]
            pos += 1
            self.assertNotEqual(prefix, 0xFE)
            length = (0, 1, 2, 4)[prefix & 3]
            self.assertLessEqual(pos + length, len(report))
            value = int.from_bytes(report[pos:pos+length], "little")
            pos += length
            kind, tag = (prefix >> 2) & 3, prefix >> 4
            if kind == 1:
                if tag == 7: size = value
                if tag == 8: report_id = value
                if tag == 9: count = value
            if kind == 0:
                if tag == 10: depth += 1
                if tag == 12: depth -= 1
                self.assertGreaterEqual(depth, 0)
                if tag in (8, 9, 11):
                    bits[report_id, tag] = bits.get((report_id, tag), 0) + size * count
        self.assertEqual(depth, 0)
        self.assertEqual(bits, {(1, 8): 64, (1, 9): 8, (2, 8): 16})

    def test_connectable_advertising_payload(self):
        payload = array("data")
        self.assertLessEqual(len(payload), 31)
        fields = {}
        pos = 0
        while pos < len(payload):
            length = payload[pos]
            self.assertGreaterEqual(length, 1)
            self.assertLessEqual(pos + 1 + length, len(payload))
            fields[payload[pos + 1]] = payload[pos + 2:pos + 1 + length]
            pos += length + 1
        self.assertEqual(fields[9], b"Halfstep Remote")
        self.assertEqual(fields[3], b"\x12\x18")
        self.assertEqual(fields[0x19], b"\xc1\x03")


if __name__ == "__main__":
    unittest.main()
