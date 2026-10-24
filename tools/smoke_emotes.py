"""Read-only category/list navigation on COM9. Never confirms a face or sends."""
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png

def main():
 d=Device('COM9');out=Path('build/emote-smoke');out.mkdir(parents=True,exist_ok=True);beat=0;uptime=0
 def pump(s):
  nonlocal beat,uptime
  end=time.monotonic()+s
  while time.monotonic()<end:
   if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
   d.read();now=d.telemetry.get('uptime_ms',uptime);assert now>=uptime,'Device reset';uptime=now;time.sleep(.004)
 def click():d.send(2);pump(.18);d.send(3);pump(.6)
 def back():d.send(4);pump(.08);d.send(5);pump(.4)
 def turn(n):
  for _ in range(n):d.send(1,1);pump(.2)
  pump(.2)
 def capture(name,page):
  pump(.2);assert d.telemetry.get('page')==page,(name,d.telemetry)
  (out/(name+'.png')).write_bytes(png(d.frame))
 try:
  pump(4);d.send(9);pump(.4);assert d.telemetry.get('page')!=59,'Exit Tetris manually first'
  for _ in range(10):back()
  assert d.telemetry.get('page')==1
  click();click();turn(5);click();capture('categories',50)
  for group in range(6):
   if group:turn(1)
   click();capture(f'group-{group}-first',67)
   for row in range(1,8):turn(1);capture(f'group-{group}-row-{row}',67)
   back();capture(f'group-{group}-back',50);print('PASS category',group+1,flush=True)
  back();back();back();capture('home',1)
  assert not any(x in d.log for x in (b'task_wdt:',b'Rebooting...',b'assert failed',b'Guru Meditation'))
  print('PASS 6 categories / 48 rows, no message sent or draft changed',flush=True)
 finally:(out/'private.log').write_bytes(d.log);d.serial.close()

if __name__=='__main__':main()
