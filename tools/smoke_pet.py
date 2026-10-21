"""Real pet UI and care actions. Persists real care; never injects or rewrites a test save."""
import argparse
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--port',default='COM9')
    ap.add_argument('--restore',action='store_true',help='Inspect saved pet only, no care actions')
    args=ap.parse_args();d=Device(args.port);beat=0
    if not args.restore:
        d.serial.close()
        raise SystemExit('Pet v2 PLAY opens a minigame. Use smoke_pet_v2.py for care; --restore remains read-only.')
    out=Path('build/pet-smoke');out.mkdir(parents=True,exist_ok=True)
    def pump(seconds):
        nonlocal beat
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
            d.read();time.sleep(.005)
    def click(long=False):
        d.send(2);pump(1.7 if long else .15);d.send(3);pump(.6)
    def back():
        d.send(4);pump(.1);d.send(5);pump(.6)
    def rotate(n):
        for _ in range(n):d.send(1,1);pump(.2)
        pump(.3)
    def capture(name,page=61):
        pump(.3);assert d.telemetry.get('page')==page,(name,d.telemetry)
        assert len(d.frame)==512 and any(d.frame)
        (out/(name+'.png')).write_bytes(png(d.frame));print('PASS',name,flush=True)
    try:
        pump(3);d.send(9,0);pump(.3)
        for _ in range(5):back()
        click();rotate(4);click();rotate(menu_index('FUN','M_PET'));click()
        if args.restore:
            capture('restored_pet');click(True);capture('restored_stats')
        else:
            capture('initial_pet')
            for i in range(3):pump(22);print('Hatch wait',i+1,flush=True)
            capture('hatched_pet');click();pump(2);capture('feed')
            rotate(1);click();pump(2);capture('play')
            rotate(1);click();pump(2);capture('clean')
            rotate(1);click();pump(2);capture('sleep');click(True);capture('stats')
            back();capture('fun_exit',23);click();capture('reentry')
        back();back();back();capture('home_exit',1)
        assert not any(m in d.log for m in (b'Guru Meditation',b'assert failed',b'Stack canary'))
        (out/('restore.log' if args.restore else 'care.log')).write_bytes(d.log)
    finally:d.serial.close()
if __name__=='__main__':main()
