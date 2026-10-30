"""COM9 read-only network-message pages. Never confirms a send/queue/delete."""
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png

def main():
 d=Device('COM9');out=Path('build/network-messages-smoke');out.mkdir(parents=True,exist_ok=True);beat=0;uptime=0
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
  pump(.3);assert d.telemetry.get('page')==page,(name,d.telemetry)
  (out/(name+'.png')).write_bytes(png(d.frame));print('CAPTURE',name,flush=True)
 def home():
  assert d.telemetry.get('page')!=59,'Exit Tetris manually first'
  for _ in range(12):back()
  assert d.telemetry.get('page')==1,d.telemetry
 def hub(row):home();click();click();turn(row)
 try:
  pump(4);d.send(9);pump(.4)
  for row,name in [(7,'group'),(8,'announcement'),(9,'offline')]:
   hub(row);capture(name+'_entry',19);click();capture(name+'_actions',69)
   # Network settings and back are non-destructive. No Send/Queue selection.
   turn(2);click();capture(name+'_network',68);back();capture(name+'_return',69)
  hub(10);click();capture('outbox',72)
  for row in range(6):
   if row:turn(1)
   click();capture('outbox_slot_'+str(row+1),73);turn(3);click();capture('cancel_delete_'+str(row+1),74)
   click();capture('delete_canceled_'+str(row+1),73);back()
  home();capture('home',1)
  assert not any(x in d.log for x in (b'task_wdt:',b'Rebooting...',b'assert failed',b'Guru Meditation'))
  print('PASS new entries/actions/outbox/cancel confirmations; no transmission or stored message changed',flush=True)
 finally:(out/'private.log').write_bytes(d.log);d.serial.close()

if __name__=='__main__':main()
