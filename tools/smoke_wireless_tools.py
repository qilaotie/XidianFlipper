"""Exercise real tool pages through existing USB control; save OLED evidence.

Run after flashing, with no other serial monitor owning the selected port.
This does not pair the PC as HID or claim HID input delivery was tested.
"""
import argparse
import binascii
import json
from pathlib import Path
import re
import struct
import time
import zlib
import serial
from menu_layout import menu_index


def png(frame):
    # SSD1306/U8g2 page-major bytes, one vertical group of eight pixels per byte.
    scale = 4
    rows = []
    for y in range(32):
        pixels = bytes(255 if frame[(y // 8) * 128 + x] & (1 << (y % 8)) else 0 for x in range(128))
        row = b"\0" + bytes(v for v in pixels for _ in range(scale))
        rows.extend([row] * scale)
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 128 * scale, 32 * scale, 8, 0, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b""))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM10")
    ap.add_argument("--out", default="build/tool-smoke")
    ap.add_argument("--compose", action="store_true", help="Check message editor without transmitting")
    ap.add_argument("--camera", action="store_true", help="Check camera key/pulse selection; never trigger shutter")
    ap.add_argument("--wifi-connect", action="store_true", help="Check station pages without credentials")
    ap.add_argument("--wifi-ap-index", type=int, help="Manually verified target AP row; leave its password page open")
    args = ap.parse_args()
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    header = (Path(__file__).resolve().parents[1] / "firmware/ui/wououi.h").read_text(encoding="utf-8")
    body = re.search(r"enum PAGE_INDEX\s*\{(.*?)\};", header, re.S)[1]
    body = re.sub(r"/\*.*?\*/|//[^\n]*", "", body, flags=re.S)
    pages = {name.strip(): i for i, name in enumerate(body.split(",")) if name.strip()}
    buffer = bytearray()
    frame = b""
    telemetry = {}
    log = bytearray()
    evidence = []
    sequence = 1000
    heartbeat_at = 0.0
    with serial.Serial(args.port, 115200, timeout=0.05) as device:
        def send(op, value=0):
            nonlocal sequence
            sequence += 1
            data = struct.pack("<BBIh", 2, op, sequence, value)
            data += struct.pack("<H", binascii.crc_hqx(data, 0xFFFF))
            device.write(b"\xaa\x55\x11\x0a\x00" + data)

        def pump(duration):
            nonlocal frame, telemetry, heartbeat_at
            until = time.monotonic() + duration
            while time.monotonic() < until:
                if time.monotonic() - heartbeat_at > 0.5:
                    send(7)
                    heartbeat_at = time.monotonic()
                data = device.read(max(1, device.in_waiting))
                buffer.extend(data)
                while len(buffer) >= 5:
                    start = buffer.find(b"\xaa\x55")
                    if start < 0:
                        log.extend(buffer[:-1]); del buffer[:-1]; break
                    if start:
                        log.extend(buffer[:start]); del buffer[:start]
                    if len(buffer) < 5: break
                    kind, size = buffer[2], int.from_bytes(buffer[3:5], "little")
                    if size > 1024: del buffer[0]; continue
                    if len(buffer) < 5 + size: break
                    payload = bytes(buffer[5:5+size]); del buffer[:5+size]
                    if kind == 0 and size == 512: frame = payload
                    elif kind == 1: log.extend(payload)
                    elif kind == 0x13 and size > 2 and binascii.crc_hqx(payload[:-2], 0xFFFF) == int.from_bytes(payload[-2:], "little"):
                        telemetry = json.loads(payload[:-2])

        def click(long=False):
            send(2); pump(1.7 if long else 0.15); send(3); pump(0.65)

        def back():
            send(4); pump(0.1); send(5); pump(0.65)

        def rotate(amount):
            for _ in range(abs(amount)):
                send(1, 1 if amount > 0 else -1); pump(0.12)
            pump(0.35)

        def capture(name, page):
            pump(0.3)
            assert telemetry.get("page") == pages[page], (name, telemetry, pages[page])
            assert len(frame) == 512 and any(frame), "No OLED image"
            (out / (name + ".png")).write_bytes(png(frame))
            evidence.append({"name": name, "telemetry": telemetry.copy()})
            print("PASS", name, "page", telemetry["page"], flush=True)

        pump(4)
        send(9, 0); pump(0.3)
        for _ in range(5): back()
        capture("home", "M_HOME")
        if args.wifi_connect:
            click(); rotate(5); click(); rotate(menu_index('TOOLS','M_WIFI_CONNECT')); click()
            capture("wifi_connect_menu", "M_WIFI_CONNECT")
            click(); pump(6); capture("wifi_networks", "M_WIFI_NETWORKS")
            if args.wifi_ap_index is not None:
                rotate(args.wifi_ap_index); capture("target_network", "M_WIFI_NETWORKS")
                click(); capture("wifi_password", "M_WIFI_PASSWORD")
            else:
                for index in range(1,6):
                    rotate(1); capture(f"wifi_networks_{index}", "M_WIFI_NETWORKS")
                back(); rotate(1); click(); capture("wifi_status", "M_WIFI_STATUS")
                back(); back(); back(); back(); capture("home_after_wifi_connect", "M_HOME")
            (out / "serial.log").write_bytes(log)
            (out / "evidence.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")
            text = log.decode("utf-8", errors="replace")
            assert "Guru Meditation" not in text and "assert failed" not in text and "Stack canary" not in text
            print("Wi-Fi UI/scanning checked; no password or connect command sent. Evidence:", out)
            return
        if args.camera:
            click(); rotate(5); click(); rotate(menu_index('TOOLS','M_HID_CAMERA')); click(); pump(2)
            capture("camera_default", "M_HID_CAMERA")
            rotate(1); capture("camera_volume_down", "M_HID_CAMERA")
            rotate(1); capture("camera_enter", "M_HID_CAMERA")
            rotate(1); capture("camera_space", "M_HID_CAMERA")
            rotate(1); click(True); capture("camera_pulse_200", "M_HID_CAMERA")
            click(True); capture("camera_pulse_80", "M_HID_CAMERA")
            click(True); capture("camera_pulse_120", "M_HID_CAMERA")
            back(); rotate(menu_index('TOOLS','M_HID_SLIDES')-menu_index('TOOLS','M_HID_CAMERA')); click(); pump(1); capture("slides_after_camera", "M_HID_SLIDES")
            back(); rotate(1); click(); pump(1); capture("media_after_camera", "M_HID_MEDIA")
            back(); back(); back(); capture("home_after_camera", "M_HOME")
            (out / "serial.log").write_bytes(log)
            (out / "evidence.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")
            text = log.decode("utf-8", errors="replace")
            assert "Guru Meditation" not in text and "assert failed" not in text
            assert "tool_hid: connectable advertising:" not in text
            print("Camera UI/advertising checked; no shutter key sent. Evidence:", out)
            return
        if args.compose:
            click(); click(); rotate(4); click()
            capture("compose_empty", "M_MESSAGE_COMPOSE")
            click(); click(); rotate(1); click()
            capture("compose_AB", "M_MESSAGE_COMPOSE")
            back(); rotate(1); click(); rotate(2); click()
            back(); rotate(1); click(); rotate(8); click()
            back(); rotate(1); click(); click()
            capture("compose_ABc8_symbol", "M_MESSAGE_COMPOSE")
            back(); rotate(-2); click(); capture("compose_lowercase_memory", "M_MESSAGE_COMPOSE")
            click(True); rotate(-1); click()
            capture("compose_delete_symbol", "M_MESSAGE_COMPOSE")
            rotate(-1); click(); capture("compose_space_direct", "M_MESSAGE_COMPOSE")
            rotate(1); click()
            click(True); capture("compose_send_selected", "M_MESSAGE_COMPOSE")
            click(); capture("compose_recipient", "M_RECIPIENT_PICK")
            back(); capture("compose_draft_retained", "M_MESSAGE_COMPOSE")
            rotate(1); click(); capture("emotes_list", "M_MESSAGE_EMOTES")
            click(); capture("emotes_recipient", "M_RECIPIENT_PICK")
            back(); back(); capture("draft_after_emote", "M_MESSAGE_COMPOSE")
            back(); back(); back(); capture("home_after_compose", "M_HOME")
            (out / "serial.log").write_bytes(log)
            (out / "evidence.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")
            text = log.decode("utf-8", errors="replace")
            assert "Guru Meditation" not in text and "assert failed" not in text
            print("Editor navigation checked; no message transmitted. Evidence:", out)
            return
        click(); rotate(5); click()
        capture("tools_menu", "M_TOOLS")
        rotate(menu_index('TOOLS','M_BLE_SCAN'))
        click(); pump(3)
        capture("ble_scan", "M_BLE_SCAN")
        click(); capture("ble_signal", "M_BLE_SCAN")
        back(); back()
        rotate(menu_index('TOOLS','M_HID_SLIDES')-menu_index('TOOLS','M_BLE_SCAN')); click(); pump(2)
        capture("slide_remote_pairing", "M_HID_SLIDES")
        back(); rotate(1); click(); pump(2)
        capture("media_remote_pairing", "M_HID_MEDIA")
        back(); rotate(menu_index('TOOLS','M_WIFI_SCAN')-menu_index('TOOLS','M_HID_MEDIA')); click(); pump(5)
        capture("wifi_scan", "M_WIFI_SCAN")
        click(); capture("wifi_details", "M_WIFI_SCAN")
        click(True); pump(5); capture("wifi_rescan", "M_WIFI_SCAN")
        back(); pump(1)
        rotate(menu_index('TOOLS','M_TOOL_MEMORY')-menu_index('TOOLS','M_WIFI_SCAN')); click(); capture("runtime_memory", "M_TOOL_MEMORY")
        back(); rotate(menu_index('TOOLS','M_BLE_SCAN')-menu_index('TOOLS','M_TOOL_MEMORY')); click(); pump(2)
        capture("ble_after_tools", "M_BLE_SCAN")
        back(); back(); back(); capture("home_after_tools", "M_HOME")
    (out / "serial.log").write_bytes(log)
    (out / "evidence.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    text = log.decode("utf-8", errors="replace")
    assert "Guru Meditation" not in text and "assert failed" not in text and "Stack canary" not in text, text[-3000:]
    assert "tool_hid: connectable advertising:" not in text, "HID advertising failed; inspect serial.log"
    print("Evidence:", out)


if __name__ == "__main__":
    main()
