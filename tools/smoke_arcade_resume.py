"""Real UI checkpoint/reboot helper; no board, score, NVS or reward injection."""
import argparse,time
from pathlib import Path
from collections import deque
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--port',default='COM9');ap.add_argument('--restore',action='store_true');a=ap.parse_args()
 d=Device(a.port);out=Path('build/arcade-resume-smoke')/('restore' if a.restore else 'play');out.mkdir(parents=True,exist_ok=True);beat=0
 def pump(s):
  nonlocal beat
  end=time.monotonic()+s
  while time.monotonic()<end:
   if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
   d.read();time.sleep(.003)
 def click(long=False):d.send(2);pump(1.65 if long else .09);d.send(3);pump(.12)
 def back():d.send(4);pump(.05);d.send(5);pump(.2)
 def turn(n):
  for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.14)
 def capture(name,page):
  pump(.05);assert d.telemetry.get('page')==page,(name,d.telemetry)
  (out/(name+'.png')).write_bytes(png(d.frame));(out/(name+'.raw')).write_bytes(d.frame);print('CAPTURE',name,flush=True)
 def enter(page):
  click();turn(4);click();turn(menu_index('FUN',page));click()
 def home():
  for _ in range(3):back()
 def board():
  def px(x,y):return bool(d.frame[y//8*128+x]&(1<<(y%8)))
  head=None;body=set();food=None
  for y in range(8):
   for x in range(24):
    cell=y*24+x
    if all(px(x*4+i,y*4+j) for i in range(4) for j in range(4)):head=cell
    if px(x*4+1,y*4+1):body.add(cell)
    elif all(px(x*4+i,y*4+j) for i,j in ((0,0),(1,0),(2,0),(0,1),(2,1),(0,2),(1,2),(2,2))):food=cell
  return head,body,food
 try:
  pump(5);d.send(9,0);pump(1)
  if len(d.frame)==512:(out/'startup.png').write_bytes(png(d.frame))
  assert d.telemetry.get('page')==1,('Start from boot HOME',d.telemetry)
  enter('M_PET');capture('pet_before',61);home()
  enter('M_TETRIS');capture('tetris_entry',59);click()
  if not a.restore:
   turn(2);click();click(True);pump(.3)
  capture('tetris_board',59);back();turn(2);click();home()
  enter('M_SNAKE');capture('snake_entry',60);click()
  if a.restore:click();capture('snake_resumed',60);click()
  else:
   previous=None;initial=None;deadline=time.monotonic()+18
   while time.monotonic()<deadline:
    pump(.01);head,body,food=board();assert head is not None and food is not None
    if initial is None:initial=len(body)
    if len(body)>initial:click();capture('snake_scored',60);break
    if previous==head:continue
    direction=0
    if previous is not None:
     dx=head%24-previous%24;dy=head//24-previous//24;direction=0 if dx>0 else (2 if dx<0 else (1 if dy>0 else 3))
    q=deque([head]);parent={head:None}
    while q:
     p=q.popleft()
     if p==food:break
     x,y=p%24,p//24
     for dx,dy in ((1,0),(0,1),(-1,0),(0,-1)):
      xx,yy=x+dx,y+dy;n=yy*24+xx
      if 0<=xx<24 and 0<=yy<8 and n not in body and n not in parent:parent[n]=p;q.append(n)
    assert food in parent
    n=food
    while parent[n]!=head:n=parent[n]
    dx=n%24-head%24;dy=n//24-head//24;wanted=0 if dx>0 else (2 if dx<0 else (1 if dy>0 else 3));delta=(wanted-direction)%4
    assert delta!=2
    if delta:d.send(1,1 if delta==1 else -1)
    previous=head
   else:raise AssertionError('No food before deadline')
  back();home();enter('M_PET');capture('pet_after',61);home();pump(2)
  assert not any(x in d.log for x in (b'Guru Meditation',b'assert failed',b'Stack canary'))
 finally:(out/'serial.log').write_bytes(d.log);d.serial.close()
if __name__=='__main__':main()
