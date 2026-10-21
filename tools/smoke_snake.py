"""COM9 real snake controls and OLED smoke; does not alter persistent settings."""
import argparse
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--port',default='COM9');args=ap.parse_args()
    d=Device(args.port);beat=0
    out=Path('build/snake-smoke');out.mkdir(parents=True,exist_ok=True)
    def pump(seconds):
        nonlocal beat
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
            d.read();time.sleep(.005)
    def click(long=False):
        d.send(2);pump(1.7 if long else .15);d.send(3);pump(.3)
    def back():
        d.send(4);pump(.1);d.send(5);pump(.5)
    def rotate(n):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.15)
        pump(.3)
    def capture(name,page):
        pump(.15);assert d.telemetry.get('page')==page,(name,d.telemetry)
        assert len(d.frame)==512 and any(d.frame)
        (out/(name+'.png')).write_bytes(png(d.frame));print('PASS',name,flush=True)
    try:
        pump(3);d.send(9,0);pump(.3)
        for _ in range(5):back()
        capture('home',1);click();rotate(4);click();rotate(menu_index('FUN','M_SNAKE'));click();capture('start',60)
        click();rotate(1);capture('play_turn',60);click();capture('pause',60)
        click(True);capture('restart',60);back();capture('fun_exit',23)
        click();capture('reentry',60);back();back();back();capture('home_exit',1)
        assert not any(m in d.log for m in (b'Guru Meditation',b'assert failed',b'Stack canary'))
        (out/'serial.log').write_bytes(d.log)
    finally:d.serial.close()
if __name__=='__main__':main()
