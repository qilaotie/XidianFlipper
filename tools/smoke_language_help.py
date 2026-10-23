"""Normal UI language changes and read-only manuals. No game start, score/NVS injection."""
import argparse
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--port',required=True)
    ap.add_argument('--phase',choices=['english','chinese','restore'],required=True);a=ap.parse_args()
    d=Device(a.port);beat=0;out=Path('build/language-help-smoke')/a.port;out.mkdir(parents=True,exist_ok=True)
    def pump(seconds):
        nonlocal beat
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
            d.read();time.sleep(.004)
    def click():d.send(2);pump(.08);d.send(3);pump(.55)
    def back():d.send(4);pump(.06);d.send(5);pump(.5)
    def turn(n):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.1)
        pump(.25)
    def capture(name,page=None):
        pump(.1)
        if page is not None:assert d.telemetry.get('page')==page,(name,d.telemetry)
        assert len(d.frame)==512 and any(d.frame)
        (out/(name+'.png')).write_bytes(png(d.frame));(out/(name+'.raw')).write_bytes(d.frame)
        print('CAPTURE '+name,flush=True)
    def home():
        for _ in range(5):back()
        assert d.telemetry.get('page')==1,d.telemetry
    def settings():home();click();turn(6);click();turn(9);pump(1)
    def fun(page):home();click();turn(4);click();turn(menu_index('FUN',page));click()
    def compare(name):
        expected=(out/(name+'.raw')).read_bytes()
        assert d.frame==expected,'Language setting row changed across reboot'
        print('PASS language row survives reboot: '+name,flush=True)
    try:
        pump(4);d.send(9,0);pump(.5);settings();capture(a.phase+'_before')
        if a.phase=='english':click();pump(1);capture('saved_english')
        elif a.phase=='chinese':compare('saved_english');click();pump(1);capture('saved_chinese')
        else:
            compare('saved_chinese');home();capture('restore_home',1);return
        fun('M_GAME_HELP');capture(a.phase+'_guide_list',63)
        for item in range(10):
            if item:turn(1)
            click()
            for page in range(4):
                if page:turn(1)
                capture(f'{a.phase}_game_{item}_page_{page}',63)
            click()
        fun('M_PET');capture(a.phase+'_pet',61);turn(10);click()
        for item in range(9):
            if item:turn(1)
            click()
            pages=(5 if item==8 else 4) if a.phase=='chinese' else (3 if item==8 else 2)
            for page in range(pages):
                if page:turn(1)
                capture(f'{a.phase}_pet_{item}_page_{page}',61)
            back()
        home();capture(a.phase+'_home',1)
        assert not any(m in d.log for m in (b'Guru Meditation',b'assert failed',b'Stack canary'))
        (out/(a.phase+'-private.log')).write_bytes(d.log)
        print('PASS normal language change and all game/pet manuals; returned HOME',flush=True)
    finally:d.serial.close()

if __name__=='__main__':main()
