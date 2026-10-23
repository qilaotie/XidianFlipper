"""COM9 normal UI: user-authorized name and new rounds; no NVS/score injection."""
import argparse
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--port',required=True)
    ap.add_argument('--restore',action='store_true')
    a=ap.parse_args();d=Device(a.port);beat=0
    out=Path('build/pet-name-maps')/a.port;out.mkdir(parents=True,exist_ok=True)
    def pump(seconds):
        nonlocal beat
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
            d.read();time.sleep(.004)
    def click(long=False):d.send(2);pump(1.7 if long else .08);d.send(3);pump(.12)
    def back():d.send(4);pump(.06);d.send(5);pump(.35)
    def turn(n):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.1)
    def capture(name,page):
        pump(.05);assert d.telemetry.get('page')==page,(name,d.telemetry)
        assert len(d.frame)==512 and any(d.frame)
        (out/(name+'.png')).write_bytes(png(d.frame))
        (out/(name+'.raw')).write_bytes(d.frame)
        print('CAPTURE '+name,flush=True)
    def home():
        for _ in range(5):back()
        assert d.telemetry.get('page')==1,d.telemetry
    def enter(page):click();turn(4);click();turn(menu_index('FUN',page));click()
    def pixel(x,y):return bool(d.frame[(y//8)*128+x] & (1<<(y%8)))
    def border():
        assert all(pixel(x,0) and pixel(x,31) for x in range(96))
        assert all(pixel(0,y) and pixel(95,y) for y in range(32))
    try:
        pump(4);d.send(9,0);pump(.5);home();enter('M_PET')
        if not a.restore:
            capture('name_prompt',61)
            # First-time prompt: ABC group -> abc, then p/e/t. User chose "pet".
            turn(-1);click();turn(11);click();turn(11);click();turn(11);click()
            capture('name_draft',61);click(True);capture('named_pet',61)
        else:
            capture('restored_pet',61)
            expected=(out/'named_pet.raw').read_bytes()
            assert bytes(b&127 for b in expected[42:128])==bytes(b&127 for b in d.frame[42:128]),'Pet name row differs after reboot'
            print('PASS pet name row survived hardware reset',flush=True)
        home();enter('M_SNAKE')
        if a.restore:
            capture('map_resume',60);click();click();capture('map_restored_play',60);border()
            assert all(pixel(x,y) for x in range(16,20) for y in range(8,12)),'GATES obstacle missing after reset'
            click();back()
        else:
            for map_id in range(3):
                click(True)
                if map_id:turn(1)
                capture('map_select_'+str(map_id),60);click();capture('map_play_'+str(map_id),60);border()
                if map_id==1:assert all(pixel(x,y) for x in range(40,44) for y in range(4,8))
                if map_id==2:assert all(pixel(x,y) for x in range(16,20) for y in range(8,12))
                click();capture('map_pause_'+str(map_id),60)
            back()
        home();capture('home_restored' if a.restore else 'home_end',1)
        assert not any(m in d.log for m in (b'Guru Meditation',b'assert failed',b'Stack canary'))
        (out/('restore-private.log' if a.restore else 'ui-private.log')).write_bytes(d.log)
    finally:d.serial.close()


if __name__=='__main__':main()
