"""COM9 local editor checks; creates then removes test slot 6. Never transmits."""
import argparse,time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--phase',choices=['save','restore'],required=True);args=ap.parse_args()
 d=Device('COM9');out=Path('build/draft-smoke')/args.phase;out.mkdir(parents=True,exist_ok=True);beat=0;uptime=0
 def pump(s):
  nonlocal beat,uptime
  end=time.monotonic()+s
  while time.monotonic()<end:
   if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
   d.read();now=d.telemetry.get('uptime_ms',uptime);assert now>=uptime,'Reset during UI test';uptime=now;time.sleep(.004)
 def click():d.send(2);pump(.18);d.send(3);pump(.7)
 def back():d.send(4);pump(.08);d.send(5);pump(.5)
 def turn(n):
  for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.2)
  pump(.3)
 def capture(name,page):
  pump(.3);assert d.telemetry.get('page')==page,(name,d.telemetry)
  (out/(name+'.png')).write_bytes(png(d.frame));print('CAPTURE',name,flush=True)
 def home():
  assert d.telemetry.get('page')!=59,'Exit Tetris manually first'
  for _ in range(10):back()
  assert d.telemetry.get('page')==1,d.telemetry
 def messages():home();click();click()
 try:
  pump(4);d.send(9);pump(.4);messages();turn(4);click();capture('editor_before',47)
  if args.phase=='save':
   # Initial firmware install has no msg_drafts namespace. Do not run on user drafts.
   click();turn(-7);click();back() # H, upper group
   turn(-1);click();turn(-8);click();back() # i, lower group
   turn(-3);click() # SPACE, group 4
   turn(-3);click();capture('emote_categories',50);click();capture('emote_picker',67);click();capture('inserted_face',47)
   turn(-1);click();capture('save_slot_picker',64);turn(5);click()
   assert d.telemetry.get('page')==47,'Slot 6 not empty; cancel overwrite manually'
   capture('saved_work',47)
   # Save current work by leaving at group level; no recipient/send selection.
   back();home();capture('home',1)
  else:
   capture('working_after_reboot',47);back();turn(2);click();turn(5);capture('slot_after_reboot',64)
   click();capture('slot_actions',65);click();capture('loaded_copy',47)
   # Group 0 -> group 10 NEW. Confirm clear current work, leaving saved slot intact.
   turn(1);click();capture('clear_confirmation',66);turn(1);click();capture('empty_editor',47)
   back();capture('back_to_actions',65);turn(1);click();capture('delete_confirmation',66);turn(1);click();back();capture('removed_test_slot',64)
   home();capture('home',1)
  assert not any(x in d.log for x in (b'task_wdt:',b'Rebooting...',b'assert failed',b'Guru Meditation'))
  print('PASS local drafts '+args.phase+'; no message sent',flush=True)
 finally:(out/'private.log').write_bytes(d.log);d.serial.close()

if __name__=='__main__':main()
