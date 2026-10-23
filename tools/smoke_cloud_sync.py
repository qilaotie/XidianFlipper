"""COM9 normal-menu cloud acceptance; uploads only real device state/scores."""
import argparse
import json
import time
import urllib.request
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index
API='https://xidianflipper-api.pages.dev'


def public():
    request=urllib.request.Request(API+'/v1/pets',headers={'User-Agent':'XidianFlipper/1.0','Accept':'application/json'})
    with urllib.request.urlopen(request,timeout=15) as r:return json.load(r)


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--port',required=True)
    ap.add_argument('--already-enabled',action='store_true')
    ap.add_argument('--mode',choices=['enable','observe','remove','resync','inspect'],default='enable');args=ap.parse_args()
    out=Path('build/cloud-smoke')/args.mode;out.mkdir(parents=True,exist_ok=True)
    baseline=public();(out/'public-before.json').write_text(json.dumps(baseline),encoding='utf8')
    d=Device(args.port);beat=0;evidence=[]
    def pump(s):
        nonlocal beat
        end=time.monotonic()+s
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
            d.read();time.sleep(.004)
    def click(long=False):d.send(2);pump(1.7 if long else .1);d.send(3);pump(.5)
    def back():d.send(4);pump(.08);d.send(5);pump(.5)
    def turn(n):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.2)
    def capture(name,page=None):
        pump(.15)
        assert len(d.frame)==512
        (out/(name+'.png')).write_bytes(png(d.frame))
        evidence.append({'name':name,'telemetry':d.telemetry.copy()});print('CAPTURE '+name,flush=True)
        if page is not None:assert d.telemetry.get('page')==page,(name,d.telemetry)
    def enter_cloud():
        capture('home',1);click();turn(7);click();capture('tools',41)
        turn(menu_index('TOOLS','M_WIFI_CONNECT'));click();turn(2);click();capture('connect_saved_result');pump(10);capture('wifi_status',57)
        back();back();turn(1);click();capture('cloud_entry',62)
    try:
        pump(5);d.send(9,0);pump(1)
        if args.mode=='inspect':capture('current');return
        if args.mode=='enable':
            enter_cloud()
            if not args.already_enabled:capture('cloud_default_off',62);click(True)
            capture('cloud_share_on',62)
        elif args.mode=='remove':
            if d.telemetry.get('page')==1:enter_cloud()
            capture('cloud_before_remove',62);turn(2);click(True);capture('remove_requested',62)
        elif args.mode=='resync':
            if d.telemetry.get('page')==1:enter_cloud()
            capture('cloud_before_resync',62);click(True);capture('reshare_requested',62)
        else:
            if d.telemetry.get('page')==1:enter_cloud()
            capture('observe',62)
        for elapsed in range(0,100,10):
            pump(10);capture(f'progress_{elapsed+10}',62)
            try:
                state=public();(out/'public-after.json').write_text(json.dumps(state,indent=2),encoding='utf8')
                print('PUBLIC profiles='+str(len(state.get('items',[]))),flush=True)
                if args.mode in ('enable','resync','observe') and state.get('items'):
                    pump(3);capture('final',62)
                    print('OBSERVED public device profile; inspect captured ID and production records.',flush=True);break
                if args.mode=='remove' and not state.get('items'):
                    pump(3);capture('final',62)
                    print('OBSERVED empty public showcase after removal.',flush=True);break
            except Exception as error:print('Public read transient error: '+type(error).__name__,flush=True)
    finally:
        (out/'serial-private.log').write_bytes(d.log)
        (out/'telemetry.json').write_text(json.dumps(evidence,indent=2),encoding='utf8');d.serial.close()


if __name__=='__main__':main()
