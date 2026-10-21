"""Inspect every reordered menu route and real main-tile text; never submit in tools/games."""
import argparse
import re
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
root=Path(__file__).resolve().parents[1]

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--port',default='COM9');args=ap.parse_args()
    header=(root/'firmware/ui/wououi.h').read_text(encoding='utf-8')
    enum=re.search(r'enum PAGE_INDEX\s*\{(.*?)\};',header,re.S)[1]
    enum=re.sub(r'/\*.*?\*/|//[^\n]*','',enum,flags=re.S)
    pages={name.strip():i for i,name in enumerate(enum.split(',')) if name.strip()}
    layout=(root/'firmware/ui/menu_layout.h').read_text(encoding='utf-8')
    def entries(group):
        body=layout.split('#define '+group+'_MENU_ITEMS(X)',1)[1].split('#define',1)[0]
        return re.findall(r'X\("[^"\n]+",(M_\w+)\)',body)
    d=Device(args.port);beat=0
    out=root/'build/menu-layout-smoke';out.mkdir(parents=True,exist_ok=True)
    def pump(seconds):
        nonlocal beat
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
            d.read();time.sleep(.005)
    def click():
        d.send(2);pump(.15);d.send(3);pump(.6)
    def back():
        d.send(4);pump(.1);d.send(5);pump(.8)
    def rotate(n):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.2)
        pump(.3)
    def capture(name,page):
        assert d.telemetry.get('page')==pages[page],(name,d.telemetry,page)
        assert len(d.frame)==512 and any(d.frame)
        (out/(name+'.png')).write_bytes(png(d.frame));print('PASS',name,flush=True)
    try:
        pump(3);d.send(9,0);pump(.3)
        for _ in range(5):back()
        click();rotate(4);capture('fun_text_start','M_MAIN');pump(1.6);capture('fun_text_scroll','M_MAIN')
        click();capture('fun_first_pet','M_FUN_HUB')
        for i,page in enumerate(entries('FUN')):
            if i:rotate(1)
            capture(f'fun_row_{i}','M_FUN_HUB');click();pump(.5);capture(page,page)
            if page=='M_TETRIS':rotate(-1);click()
            else:back()
        back();rotate(3);capture('tools_text_start','M_MAIN');pump(1.6);capture('tools_text_scroll','M_MAIN')
        click();capture('tools_first_remote','M_TOOLS')
        for i,page in enumerate(entries('TOOLS')):
            if i:rotate(1)
            capture(f'tools_row_{i}','M_TOOLS');click();pump(5 if page=='M_WIFI_SCAN' else .8);capture(page,page);back()
        back();back();capture('home_after_menus','M_HOME')
        assert not any(m in d.log for m in (b'Guru Meditation',b'assert failed',b'Stack canary'))
        (out/'serial.log').write_bytes(d.log)
    finally:d.serial.close()
if __name__=='__main__':main()
