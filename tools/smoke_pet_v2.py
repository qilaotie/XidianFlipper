"""Real pet v2 care/shop/dex smoke. Spends game gold, never confirms a new egg."""
import argparse
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--port',default='COM9')
    ap.add_argument('--restore',action='store_true',help='Read only pet and stats after reboot')
    ap.add_argument('--wake',action='store_true',help='Use only after visually confirming the pet is sleeping')
    args=ap.parse_args();d=Device(args.port);beat=0
    out=Path('build/pet-v2-smoke');out.mkdir(parents=True,exist_ok=True)
    def pump(seconds):
        nonlocal beat
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
            d.read();time.sleep(.005)
    def click(long=False):
        d.send(2);pump(1.7 if long else .12);d.send(3);pump(.4)
    def back():
        d.send(4);pump(.1);d.send(5);pump(.6)
    def rotate(n):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.2)
        pump(.2)
    def capture(name,page=61):
        pump(.15);assert d.telemetry.get('page')==page,(name,d.telemetry)
        assert len(d.frame)==512 and any(d.frame)
        (out/(name+'.png')).write_bytes(png(d.frame));print('PASS',name,flush=True)
    try:
        pump(3);d.send(9,0);pump(.3)
        for _ in range(5):back()
        click();rotate(4);click();rotate(menu_index('FUN','M_PET'));click()
        if args.restore:
            capture('restored_home');click(True);capture('restored_stats')
            rotate(5);click()
            for i in range(8):
                if i:rotate(1)
                capture('restored_bag_'+str(i))
            back();rotate(1);click()
            for i in range(6):
                if i:rotate(1)
                capture('restored_dex_'+str(i))
            back()
        else:
            capture('migrated_home');click(True);capture('migrated_stats');click(True)
            if args.wake:
                rotate(3);click();pump(2.1);rotate(-3)
            click();pump(2.1);rotate(1);click();capture('game_wait')
            wait_row=d.frame[42:127];deadline=time.monotonic()+4
            while time.monotonic()<deadline and d.frame[42:127]==wait_row:pump(.025)
            assert d.frame[42:127]!=wait_row,'No GO transition: check wake/food/energy'
            click();capture('game_result');pump(2.1)
            rotate(3);click();rotate(4);capture('plant_before_buy');click();capture('plant_bought');pump(2.1)
            back();rotate(1);click();rotate(4);capture('plant_in_bag');click();capture('plant_equipped');pump(2.1)
            back();rotate(1);click()
            for i in range(6):
                if i:rotate(1)
                capture('dex_'+str(i))
            rotate(1);click();capture('new_egg_keep_default');back();capture('home_kept')
        back();back();back();capture('home_exit',1)
        assert not any(m in d.log for m in (b'Guru Meditation',b'assert failed',b'Stack canary'))
        (out/('restore.log' if args.restore else 'care.log')).write_bytes(d.log)
    finally:d.serial.close()
if __name__=='__main__':main()
