"""Read real OLED to steer Snake to food; no game/state injection or artificial rewards."""
import argparse
from collections import deque
from pathlib import Path
import time
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--port',default='COM9');args=ap.parse_args()
    d=Device(args.port);beat=0
    out=Path('build/pet-life-smoke');out.mkdir(parents=True,exist_ok=True)
    def pump(seconds):
        nonlocal beat
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
            d.read();time.sleep(.003)
    def click(long=False):
        d.send(2);pump(1.7 if long else .12);d.send(3);pump(.35)
    def back():
        d.send(4);pump(.1);d.send(5);pump(.55)
    def rotate(n):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.2)
        pump(.2)
    def capture(name,page):
        pump(.1);assert d.telemetry.get('page')==page,(name,d.telemetry)
        (out/(name+'.png')).write_bytes(png(d.frame));print('PASS',name,flush=True)
    def home():
        for _ in range(5):back()
    def pet():
        click();rotate(4);click();rotate(menu_index('FUN','M_PET'));click()
    def board():
        frame=d.frame
        def pixel(x,y):return bool(frame[(y//8)*128+x]&(1<<(y%8)))
        head=None;body=set();food=None
        for y in range(8):
            for x in range(24):
                cell=y*24+x
                if all(pixel(x*4+dx,y*4+dy) for dx in range(4) for dy in range(4)):head=cell
                if pixel(x*4+1,y*4+1):body.add(cell)
                elif all(pixel(x*4+dx,y*4+dy) for dx,dy in ((0,0),(1,0),(2,0),(0,1),(2,1),(0,2),(1,2),(2,2))):food=cell
        return head,body,food
    def next_cell(head,body,food):
        q=deque([head]);parent={head:None}
        while q:
            p=q.popleft()
            if p==food:break
            x,y=p%24,p//24
            for dx,dy in ((1,0),(0,1),(-1,0),(0,-1)):
                a,b=x+dx,y+dy;n=b*24+a
                if 0<=a<24 and 0<=b<8 and n not in body and n not in parent:
                    parent[n]=p;q.append(n)
        assert food in parent,'No conservative path to food'
        n=food
        while parent[n]!=head:n=parent[n]
        return n
    try:
        pump(3);d.send(9,0);pump(.3);home();pet();capture('pet_before',61)
        click(True);capture('stats_before',61);click(True)
        rotate(7);click();capture('growth',61);back();rotate(1);capture('calls_before',61)
        pump(2.1);click();capture('calls_toggled',61);pump(2.1);click();capture('calls_restored',61)
        back();back();back()
        click();rotate(4);click();rotate(menu_index('FUN','M_SNAKE'));click();click()
        direction=0;previous=None;target=None;initial_body=None;deadline=time.monotonic()+18
        while time.monotonic()<deadline:
            pump(.01);head,body,food=board()
            assert head is not None and food is not None,'Snake screen not in active play'
            if initial_body is None:initial_body=len(body)
            if len(body)>initial_body:
                click();capture('snake_earned',60);break
            if head==previous:continue
            if previous is not None:
                dx=head%24-previous%24;dy=head//24-previous//24
                direction=0 if dx>0 else (2 if dx<0 else (1 if dy>0 else 3))
            n=next_cell(head,body,food);dx=n%24-head%24;dy=n//24-head//24
            wanted=0 if dx>0 else (2 if dx<0 else (1 if dy>0 else 3))
            turn=(wanted-direction)%4;assert turn!=2,'Reverse is not allowed'
            if turn:d.send(1,1 if turn==1 else -1)
            previous=head;target=n
        else:raise AssertionError('No food eaten before deadline')
        back();back();back();pet();capture('pet_after',61)
        pump(.8);capture('idle_a',61);pump(1.6);capture('idle_b',61)
        back();back();back();pump(4);capture('home_after',1)
        assert not any(m in d.log for m in (b'Guru Meditation',b'assert failed',b'Stack canary'))
        (out/'serial.log').write_bytes(d.log)
    finally:d.serial.close()
if __name__=='__main__':main()
