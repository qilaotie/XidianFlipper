"""Read-only COM9 help transition stress; capture logs even on a reset."""
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
import argparse

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--cycles',type=int,default=12);ap.add_argument('--tag',default='after');args=ap.parse_args()
    out=Path('build/help-transition')/args.tag;out.mkdir(parents=True,exist_ok=True)
    d=Device('COM9');beat=0;uptime=0
    def pump(seconds):
        nonlocal beat,uptime
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
            d.read();now=d.telemetry.get('uptime_ms',uptime)
            assert now>=uptime,'Device rebooted'
            uptime=now;time.sleep(.004)
    def click():d.send(2);pump(.16);d.send(3);pump(.3)
    def back():d.send(4);pump(.08);d.send(5);pump(.3)
    def turn(n):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.12)
        pump(.2)
    def snap(name):
        assert d.telemetry.get('page')==63,d.telemetry
        (out/(name+'.png')).write_bytes(png(d.frame))
    try:
        pump(3);d.send(9);pump(.3)
        assert d.telemetry.get('page')!=59,'Exit Tetris manually first'
        for _ in range(6):back()
        assert d.telemetry.get('page')==1,d.telemetry
        click();turn(4);click();turn(11);click();turn(10)
        for cycle in range(args.cycles):
            click();snap(f'{cycle:02d}-enter');pump(.5)
            turn(12);click();pump(.2);back();back();snap(f'{cycle:02d}-return');pump(.4)
            print('PASS transition',cycle+1,flush=True)
        back();back();back();pump(1)
        assert d.telemetry.get('page')==1,d.telemetry
        click();turn(5);pump(.6)
        assert d.telemetry.get('page')==2,d.telemetry
        (out/'tools-classic.png').write_bytes(png(d.frame))
        back();pump(.5)
        assert not any(x in d.log for x in (b'task_wdt:',b'Rebooting...',b'assert failed',b'Guru Meditation'))
        print('PASS stress; HOME',d.telemetry,flush=True)
    finally:
        (out/'private.log').write_bytes(d.log);d.serial.close()

if __name__=='__main__':main()
