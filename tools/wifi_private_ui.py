"""Enter a Wi-Fi password through normal masked device UI, stdin only, never saved."""
import argparse
import getpass
import threading
import time
import warnings
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--port',required=True);a=ap.parse_args()
    d=Device(a.port);beat=0;out=Path('build/cloud-smoke/private-wifi');out.mkdir(parents=True,exist_ok=True)
    def pump(s):
        nonlocal beat
        end=time.monotonic()+s
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
            d.read();time.sleep(.004)
    def click(long=False):d.send(2);pump(1.7 if long else .1);d.send(3);pump(.4)
    def back():d.send(4);pump(.08);d.send(5);pump(.4)
    def turn(n):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.12)
    def capture(name):
        pump(.2);(out/(name+'.png')).write_bytes(png(d.frame));print('CAPTURE '+name,flush=True)
    try:
        pump(5);d.send(9,0);pump(1);assert d.telemetry.get('page')==1
        click();turn(5);click();turn(menu_index('TOOLS','M_WIFI_CONNECT'));click();click();pump(9)
        assert d.telemetry.get('page')==55,d.telemetry
        capture('hotspots');answer=[];errors=[]
        def read_secret():
            try:
                index=int(input('AP index (0-based, inspect hotspots.png first): '))
                warnings.simplefilter('error',getpass.GetPassWarning)
                password=getpass.getpass('Wi-Fi password (hidden; not stored): ')
                if not 0<=index<24 or not 8<=len(password)<=63 or any(not 32<=ord(c)<=126 for c in password):
                    raise ValueError('Invalid input')
                answer.append((index,password))
            except BaseException as error:errors.append(type(error).__name__)
        thread=threading.Thread(target=read_secret,daemon=True);thread.start()
        while thread.is_alive():pump(.05)
        if errors:raise RuntimeError('Secret input failed: '+errors[0])
        index,password=answer.pop();turn(index);click();assert d.telemetry.get('page')==56,d.telemetry
        group=1;positions=[0]*8;selecting=True
        symbols='!"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~'
        for char in password:
            target=0 if 'A'<=char<='Z' else 1 if 'a'<=char<='z' else 2 if '0'<=char<='9' else 4 if char==' ' else 3
            if target!=group:
                if not selecting:back();selecting=True
                delta=(target-group)%7
                if delta>3:delta-=7
                turn(-delta);group=target
            if target==4:click();continue
            if selecting:click();selecting=False
            count=[26,26,10,32][target];position=ord(char)-ord('A' if target==0 else 'a' if target==1 else '0') if target<3 else symbols.index(char)
            delta=(position-positions[target])%count
            if delta>count//2:delta-=count
            turn(-delta);positions[target]=position;click()
        password=None;answer.clear();click(True);click();pump(12);capture('connected_status')
        assert d.telemetry.get('page')==57,d.telemetry
        print('Password entered through normal masked UI. Inspect connection status; no password file/log written.',flush=True)
    finally:d.serial.close()


if __name__=='__main__':main()
