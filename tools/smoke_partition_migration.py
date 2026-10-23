"""Real-device read-only UI observations before/after migration; no score injection."""
import argparse
import json
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port',required=True)
    ap.add_argument('--phase',choices=['before','after'],required=True)
    ap.add_argument('--out',help='Separate evidence directory for this device')
    args=ap.parse_args()
    out=Path(args.out) if args.out else Path('build/partition-smoke')/args.phase
    out.mkdir(parents=True,exist_ok=True)
    d=Device(args.port);beat=0;evidence=[]
    def pump(seconds):
        nonlocal beat
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
            d.read();time.sleep(.005)
    def click():d.send(2);pump(.12);d.send(3);pump(.5)
    def back():d.send(4);pump(.08);d.send(5);pump(.5)
    def turn(n):
        for _ in range(n):d.send(1,1);pump(.2)
    def capture(name,page):
        pump(.2);assert d.telemetry.get('page')==page,(name,d.telemetry)
        assert len(d.frame)==512 and any(d.frame)
        (out/(name+'.png')).write_bytes(png(d.frame));(out/(name+'.raw')).write_bytes(d.frame)
        evidence.append(dict(name=name,telemetry=d.telemetry.copy()))
        print('CAPTURE '+name,flush=True)
    def enter(page):click();turn(4);click();turn(menu_index('FUN',page));click()
    try:
        pump(5);d.send(9,0);pump(1)
        capture('home',1)
        enter('M_PET');capture('pet',61);back();back();back()
        enter('M_TETRIS');capture('tetris',59);turn(2);click();back();back()
        enter('M_SNAKE');capture('snake',60);back();back();back()
        capture('home_end',1)
        (out/'telemetry.json').write_text(json.dumps(evidence,indent=2),encoding='utf8')
        (out/'serial-private.log').write_bytes(d.log)
        print('PASS: pet page and both game resume/record menus accessible; returned HOME.',flush=True)
    finally:d.serial.close()


if __name__=='__main__':main()
