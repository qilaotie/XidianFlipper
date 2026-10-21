"""Two real devices via native UI controls. No packet, pet, score or save injection."""
import argparse
from pathlib import Path
import time
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--host',default='COM9');ap.add_argument('--join',default='COM10')
    ap.add_argument('--game',choices=['visit','duel','menus','solo','help','learn','skip','care'],default='visit')
    ap.add_argument('--direct',action='store_true',help='Auto help already learned for this project')
    ap.add_argument('--out',default='build/pet-connection-smoke');args=ap.parse_args()
    devices=[Device(args.host),Device(args.join)];out=Path(args.out)/args.game;out.mkdir(parents=True,exist_ok=True)
    beat=0
    def pump(seconds):
        nonlocal beat
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:
                for d in devices:d.send(7)
                beat=time.monotonic()
            for d in devices:d.read()
            time.sleep(.004)
    def click(d,long=False,wait=.35):
        d.send(2);pump(1.65 if long else .10);d.send(3);pump(wait)
    def back(d):d.send(4);pump(.08);d.send(5);pump(.4)
    def turn(d,n):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.15)
        pump(.15)
    def capture(name):
        pump(.15)
        for d in devices:
            assert d.telemetry.get('page')==61,(name,d.port,d.telemetry)
            (out/f'{d.port}-{name}.png').write_bytes(png(d.frame))
        print('CAPTURE',name,flush=True)
    def bag(prefix):
        for d in devices:turn(d,5);click(d)
        for i in range(3):
            capture(prefix+'_bag_'+str(i))
            if i<2:
                for d in devices:turn(d,1)
        for d in devices:back(d);turn(d,-5)
    try:
        pump(3)
        for d in devices:
            d.send(9,0)
            for _ in range(6):back(d)
            click(d);turn(d,4);click(d);turn(d,menu_index('FUN','M_PET'));click(d)
        capture('pet_before')
        for d in devices:click(d,True)
        capture('stats_before')
        for d in devices:click(d,True)
        if args.game=='care':
            # Only run after observing both pets awake and tired. Normal controls only.
            for repeat in range(3):
                for d in devices:click(d)
                pump(2.2)
            for d in devices:turn(d,2);click(d)
            pump(2.2)
            for d in devices:turn(d,1);click(d)
            capture('asleep')
            pump(40);capture('rest_40s');pump(40);capture('rest_80s');pump(45)
            for d in devices:click(d)
            pump(2.2);capture('awake')
        elif args.game=='help':
            for d in devices:turn(d,-1);click(d) # Dedicated HOW TO, no game launch.
            for i in range(7):
                for d in devices:click(d)
                capture('help_'+str(i)+'_1')
                for d in devices:turn(d,1)
                capture('help_'+str(i)+'_2')
                for d in devices:back(d);turn(d,1)
            for i in range(2):
                for d in devices:click(d)
                for page in range(3 if i else 2):
                    capture('link_help_'+str(i)+'_'+str(page))
                    for d in devices:turn(d,1)
                for d in devices:back(d);turn(d,1)
            for d in devices:back(d);turn(d,1)
            capture('pet_after')
        elif args.game in ('learn','skip'):
            for d in devices:turn(d,-2);click(d)
            if args.game=='learn':
                for attempt in range(3):
                    for d in devices:click(d)
                    capture('auto_read_'+str(attempt));pump(2.1)
                    for d in devices:turn(d,1);back(d)
            for d in devices:click(d)
            pump(1);capture('direct_search')
            for d in devices:back(d);back(d)
        elif args.game=='solo':
            # COM9 was observed awake but exhausted: normal free care + sleep, never save editing.
            a,b=devices
            click(a);pump(2.2);click(a);pump(2.2);turn(a,2);click(a);pump(2.2);turn(a,1);click(a)
            turn(b,1);click(b)
            for i in range(6):
                capture('menu_'+str(i));click(b);click(b);pump(.6);capture('playing_'+str(i))
                if i==5:
                    for _ in range(35):click(b,wait=.12)
                    pump(12)
                else:pump(21)
                capture('result_'+str(i));back(b);turn(b,1)
            click(a);pump(2.2);capture('recovered')
        elif args.game=='menus':
            for d in devices:turn(d,1);click(d)
            for i in range(7):
                capture('game_menu_'+str(i))
                for d in devices:turn(d,1)
        else:
            if args.game=='visit':bag('before')
            for d in devices:
                turn(d,-2);click(d)
                if args.game=='duel':turn(d,1)
                click(d)
                if not args.direct:click(d)
            pump(3);capture('search');click(devices[0]);pump(1);capture('offer')
            click(devices[1]);pump(.5);capture('countdown');pump(3.5);capture('playing')
            if args.game=='duel':
                # Human-like taps on one device, other device stays idle; actual engine determines results.
                for i in range(50):click(devices[0],wait=.12)
                pump(14)
            else:pump(9)
            capture('result');pump(3);capture('result_stable')
            for d in devices:back(d);back(d);turn(d,2) # CONNECT -> FEED in 11-action menu
            capture('pet_after')
            if args.game=='visit':bag('after')
        for d in devices:
            for _ in range(5):back(d)
        for d in devices:
            assert not any(x in d.log for x in (b'Guru Meditation',b'assert failed',b'Stack canary'))
    finally:
        for d in devices:(out/f'{d.port}-serial.log').write_bytes(d.log);d.serial.close()
    print('Inspect captured OLEDs for actual outcomes:',out,flush=True)
if __name__=='__main__':main()
