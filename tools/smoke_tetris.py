"""Real OLED/page/control smoke test. Does not alter persistent settings."""
import argparse
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index
import binascii
import struct
import zlib

def portrait_png(frame):
    rows = []
    for y in range(128):
        pixels = bytes(255 if frame[(x // 8) * 128 + 127-y] & (1 << (x % 8)) else 0 for x in range(32))
        rows.extend([b'\0' + bytes(v for v in pixels for _ in range(4))] * 4)
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', binascii.crc32(kind + data) & 0xFFFFFFFF)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 128, 512, 8, 0, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(b''.join(rows))) + chunk(b'IEND', b''))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', default='COM9')
    args = ap.parse_args()
    d = Device(args.port)
    heartbeat = 0
    out = Path('build/tetris-smoke'); out.mkdir(parents=True, exist_ok=True)
    def pump(seconds):
        nonlocal heartbeat
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if time.monotonic() - heartbeat > .5:
                d.send(7); heartbeat = time.monotonic()
            d.read(); time.sleep(.005)
    def click(long=False):
        d.send(2); pump(1.7 if long else .15); d.send(3); pump(.65)
    def back():
        d.send(4); pump(.1); d.send(5); pump(.65)
    def rotate(n):
        for _ in range(abs(n)):
            d.send(1, 1 if n > 0 else -1); pump(.25)
        pump(.35)
    def capture(name, page):
        pump(.3)
        assert d.telemetry.get('page') == page, (name, d.telemetry)
        assert len(d.frame) == 512 and any(d.frame)
        (out / (name + '.png')).write_bytes(png(d.frame))
        if page == 59:
            (out / (name + '_upright.png')).write_bytes(portrait_png(d.frame))
        print('PASS', name, 'page', page, flush=True)
    try:
        pump(4); d.send(9, 0); pump(.3)
        for _ in range(5): back()
        capture('home', 1)
        click(); rotate(4); click(); rotate(menu_index('FUN','M_TETRIS')); click()
        capture('portrait_paused', 59)
        click(); rotate(2); click(); click(True)
        capture('portrait_play', 59)
        back(); capture('portrait_pause', 59)
        rotate(-1); click(); capture('fun_after_exit', 23)
        click(); capture('portrait_reentry', 59)
        rotate(-1); click(); back(); back(); capture('home_after_exit', 1)
        assert not any(marker in d.log for marker in (b'Guru Meditation', b'assert failed', b'Stack canary'))
        (out / 'serial.log').write_bytes(d.log)
        print('Real OLED/control smoke passed, not a full playability acceptance:', out.resolve())
    finally:
        d.serial.close()

if __name__ == '__main__':
    main()
