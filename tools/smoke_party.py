"""Two-device real UI acceptance helper. Does not establish four-device RF behavior."""
import argparse
import binascii
import json
from pathlib import Path
import struct
import time
import serial
from smoke_wireless_tools import png
from menu_layout import menu_index

class Device:
    def __init__(self, port):
        self.port = port
        self.serial = serial.Serial(port, 115200, timeout=0)
        self.buffer = bytearray()
        self.log = bytearray()
        self.frame = b""
        self.telemetry = {}
        self.seq = time.monotonic_ns() & 0xffffffff

    def send(self, op, value=0):
        self.seq = (self.seq + 1) & 0xffffffff or 1
        payload = struct.pack("<BBIh", 2, op, self.seq, value)
        payload += struct.pack("<H", binascii.crc_hqx(payload, 0xffff))
        self.serial.write(b"\xaa\x55\x11\x0a\x00" + payload)

    def read(self):
        self.buffer.extend(self.serial.read(max(1, self.serial.in_waiting)))
        b = self.buffer
        while len(b) >= 5:
            start = b.find(b"\xaa\x55")
            if start < 0:
                self.log.extend(b[:-1]); del b[:-1]; break
            if start:
                self.log.extend(b[:start]); del b[:start]
            if len(b) < 5: break
            kind, size = b[2], int.from_bytes(b[3:5], "little")
            if size > 1024: del b[0]; continue
            if len(b) < size + 5: break
            payload = bytes(b[5:5+size]); del b[:5+size]
            if kind == 0 and size == 512: self.frame = payload
            elif kind == 1: self.log.extend(payload)
            elif kind == 0x13 and size > 2 and binascii.crc_hqx(payload[:-2], 0xffff) == int.from_bytes(payload[-2:], "little"):
                self.telemetry = json.loads(payload[:-2])

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="COM9")
    ap.add_argument("--join", default="COM10")
    ap.add_argument("--out", default="build/party-smoke")
    ap.add_argument("--game", choices=["reaction","bomb","time"], default="reaction")
    args = ap.parse_args()
    page = {"reaction":49,"bomb":51,"time":52}[args.game]
    tile = menu_index('FUN_NEARBY',{'reaction':'M_REACTION','bomb':'M_NUMBER_BOMB','time':'M_TIME_GUESS'}[args.game])
    out = Path(args.out).resolve(); out.mkdir(parents=True, exist_ok=True)
    devices = [Device(args.host), Device(args.join)]
    evidence = []
    heartbeat_at = 0
    def pump(seconds):
        nonlocal heartbeat_at
        until = time.monotonic() + seconds
        while time.monotonic() < until:
            if time.monotonic()-heartbeat_at > .5:
                for d in devices: d.send(7)
                heartbeat_at = time.monotonic()
            for d in devices: d.read()
            time.sleep(.005)
    def click(d):
        d.send(2); pump(.15); d.send(3); pump(.7)
    def back(d):
        d.send(4); pump(.1); d.send(5); pump(.7)
    def rotate(d, amount):
        for _ in range(abs(amount)):
            d.send(1, 1 if amount > 0 else -1); pump(.25)
        pump(.5)
    def capture(name, page):
        pump(.35)
        for d in devices:
            assert d.telemetry.get("page") == page, (name, d.port, d.telemetry)
            assert len(d.frame) == 512 and any(d.frame)
            (out / f"{d.port}-{name}.png").write_bytes(png(d.frame))
            evidence.append({"name": name, "port": d.port, "telemetry": d.telemetry.copy()})
        print("CAPTURE", name, flush=True)
    try:
        pump(3)
        for d in devices:
            d.send(9, 0)
            for _ in range(5): back(d)
            click(d); rotate(d, 4); click(d); rotate(d, menu_index('FUN','M_FUN_NEARBY'));click(d);rotate(d, tile); click(d)
        capture("role_menu", page)
        click(devices[0]); rotate(devices[1], 1); click(devices[1]); pump(4)
        capture("lobby", page)
        for d in devices: click(d)
        pump(1); capture("all_ready", page)
        pump(11); capture("go_or_wait", page)
        if args.game == "bomb":
            for turn in range(16):
                if b"party: BOMB RESULT" in devices[0].log: break
                click(devices[turn % 2]); pump(1.3)
                capture(f"bomb_step_{turn}", page)
            assert b"party: BOMB RESULT" in devices[0].log, "No bomb result in host log"
        else:
            for d in devices: click(d)
        pump(4); capture("results", page)
        if args.game != "bomb":
            for d in devices: rotate(d, 1)
            capture("other_result", page)
        for d in devices: click(d)
        pump(3); capture("next_lobby", page)
        for d in devices: back(d)
        capture("fun_after", 118)
    finally:
        for d in devices:
            (out / f"{d.port}-serial.log").write_bytes(d.log)
            d.serial.close()
        (out / "evidence.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    for d in devices:
        log = d.log.decode("utf-8", errors="replace")
        assert "Guru Meditation" not in log and "assert failed" not in log and "Stack canary" not in log
    print("Navigation/captures complete; inspect screenshots for game outcomes. Evidence:", out)

if __name__ == "__main__": main()
