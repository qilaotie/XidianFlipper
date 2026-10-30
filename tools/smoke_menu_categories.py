"""COM9 UI checks; no transmissions, resets or persistent setting changes."""
from pathlib import Path
import json,time,argparse
from smoke_party import Device
from smoke_wireless_tools import png

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--port',default='COM9');a=ap.parse_args()
    out=Path('build/menu-categories-private')/a.port;out.mkdir(parents=True,exist_ok=True)
    d=Device(a.port);records=[];heartbeat=0;uptime=0
    def pump(seconds):
        nonlocal heartbeat,uptime
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-heartbeat>.5:d.send(7);heartbeat=time.monotonic()
            d.read();now=d.telemetry.get('uptime_ms',uptime);assert now>=uptime,'Unexpected reset';uptime=now
            time.sleep(.005)
    def click(hold=.18):d.send(2);pump(hold);d.send(3);pump(.65)
    def back():d.send(4);pump(.2);d.send(5);pump(.55)
    def turn(n):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.15)
        pump(.3)
    def capture(name,page):
        pump(.35);assert d.telemetry.get('page')==page,(name,d.telemetry)
        assert len(d.frame)==512 and any(d.frame)
        (out/(name+'.png')).write_bytes(png(d.frame));records.append({'name':name,'telemetry':d.telemetry.copy()});print('PASS',name,page,flush=True)
    def home():
        assert d.telemetry.get('page')!=59
        for _ in range(20):
            if d.telemetry.get('page')==1:return
            back()
        raise AssertionError(d.telemetry)
    try:
        pump(2);d.send(9);pump(1);home()
        click();turn(5);click();capture('tools',41)
        for i,page in enumerate((101,104,119,102,103)):
            if i:turn(1)
            click();capture('tool-category-'+str(i),page);back();capture('tools-return-'+str(i),41)
        home();click();turn(6);click();capture('settings',3)
        for i,page in enumerate((120,105,106,107,108)):
            if i:turn(1)
            click();capture('settings-category-'+str(i),page)
            if i==0:
                turn(1);click();capture('mesh-settings',68);back();capture('network-return',120)
            if i in (1,3):
                click();capture('numeric-window-'+str(i),0);back();capture('numeric-cancel-'+str(i),page)
            back();capture('settings-return-'+str(i),3)
        home();click();turn(4);click();capture('fun',23)
        for i,page in enumerate((116,117,118)):
            if i:turn(1)
            click();capture('fun-category-'+str(i),page);back();capture('fun-return-'+str(i),23)
        home();capture('finished-home',1)
    finally:
        (out/'result.json').write_text(json.dumps(records,indent=2),encoding='utf8');(out/'serial.log').write_bytes(d.log);d.serial.close()
if __name__=='__main__':main()
