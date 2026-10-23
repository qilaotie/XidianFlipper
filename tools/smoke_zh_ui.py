"""Real-device Chinese system UI observations. Never change settings, reset, pair or share."""
import argparse
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--port',required=True);ap.add_argument('--tools-only',action='store_true');a=ap.parse_args()
    d=Device(a.port);beat=0;out=Path('build/zh-ui-smoke')/a.port;out.mkdir(parents=True,exist_ok=True)
    def pump(seconds):
        nonlocal beat
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
            d.read();time.sleep(.004)
    def click():d.send(2);pump(.1);d.send(3);pump(.6)
    def back():d.send(4);pump(.08);d.send(5);pump(.6)
    def turn(n):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.18)
        pump(.3)
    def capture(name,page=None):
        pump(.2)
        if page is not None:assert d.telemetry.get('page')==page,(name,d.telemetry)
        assert len(d.frame)==512 and any(d.frame)
        (out/(name+'.png')).write_bytes(png(d.frame));print('CAPTURE '+name,flush=True)
    try:
        pump(4);d.send(9,0);pump(.5)
        for _ in range(5):back()
        click()
        if not a.tools_only:
            for i in range(8):
                if i:turn(1)
                capture('main_'+str(i),2)
            turn(-2);click();capture('settings_0')
            click();capture('brightness_dialog');back()
            for i in range(1,9):turn(1);capture('settings_'+str(i))
            click();capture('screen_power');back();back();turn(2)
        else:turn(7)
        click();capture('tools_0')
        for i in range(1,8):turn(1);capture('tools_'+str(i))
        # Read-only memory details, then observe both alternating rows.
        click();capture('memory_detail');pump(3.2);capture('memory_help');back()
        turn(-4);click();capture('wifi_0')
        for i in range(1,6):turn(1);capture('wifi_'+str(i))
        back();turn(1);click();capture('cloud_action');pump(3.2);capture('cloud_status');back()
        turn(-4)
        for i in range(3):
            if i:turn(1)
            click();pump(1);capture('remote_'+str(i));pump(3.2);capture('remote_help_'+str(i));back()
        for _ in range(4):back()
        capture('home_end',1)
        assert not any(m in d.log for m in (b'Guru Meditation',b'assert failed',b'Stack canary'))
        (out/'serial-private.log').write_bytes(d.log)
        print('PASS system Chinese UI navigation; no setting/reset/cloud actions submitted',flush=True)
    finally:d.serial.close()

if __name__=='__main__':main()
