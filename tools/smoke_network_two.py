"""Two-device native UI RF acceptance; no packet/save injection.
Run named phases between read-only NVS snapshots/reset. Baseline must show empty
working drafts and no pending outbox. Existing saved drafts/friends are untouched.
COM10 sends, COM9 receives; never confirm anything outside these two devices.
"""
import argparse,json,time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index
ROOT=Path(__file__).resolve().parents[1]

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--phase',choices=['group','cross','queue','offline_wait','recover','cleanup'],required=True);args=ap.parse_args()
 out=ROOT/'build/network-two-private'/('ui-'+args.phase);out.mkdir(parents=True,exist_ok=False)
 ports=['COM10'] if args.phase in ('queue','offline_wait') else ['COM9','COM10']
 devices={p:Device(p) for p in ports};beat=0;uptimes={p:0 for p in ports}
 baseline={p:json.loads((ROOT/'build/network-two-private/before'/p/'state-private.json').read_text()) for p in ('COM9','COM10')}
 assert all(not s['work'] and not any(e['state'] for e in s['outbox']) for s in baseline.values()),'Do not overwrite user work/outbox'
 def pump(seconds):
  nonlocal beat
  end=time.monotonic()+seconds
  while time.monotonic()<end:
   if time.monotonic()-beat>.5:
    for d in devices.values():d.send(7)
    beat=time.monotonic()
   for p,d in devices.items():
    d.read();now=d.telemetry.get('uptime_ms',uptimes[p]);assert now>=uptimes[p],f'{p} rebooted during phase';uptimes[p]=now
   time.sleep(.004)
 def click(d):d.send(2);pump(.18);d.send(3);pump(.65)
 def back(d):d.send(4);pump(.08);d.send(5);pump(.4)
 def turn(d,n):
  for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.13)
  pump(.2)
 def capture(d,name,page):
  pump(.3);assert d.telemetry.get('page')==page,(d.port,name,d.telemetry)
  (out/f'{d.port}-{name}.png').write_bytes(png(d.frame));print('CAPTURE',d.port,name,flush=True)
 def home(d):
  assert d.telemetry.get('page')!=59,'Exit Tetris manually'
  for _ in range(15):
   if d.telemetry.get('page')==1:return
   back(d)
  assert d.telemetry.get('page')==1,d.telemetry
 def hub(d,row):home(d);click(d);click(d);turn(d,row)
 def channel(d,current,target):
  if current==target:return
  hub(d,3);click(d);capture(d,'channel_before_'+str(target),26);turn(d,target-current);click(d);pump(1)
  capture(d,'channel_saved_'+str(target),19)
 def network_on(d):
  home(d);click(d);turn(d,5);click(d);turn(d,menu_index('TOOLS','M_NETWORK_STATUS'));click(d)
  capture(d,'network_before',68);click(d);pump(1);capture(d,'network_on',68)
 def write(d,text):
  # Each phase begins after hardware reset, so the editor selector starts ABC/A.
  hub(d,4);click(d);capture(d,'editor_original',47)
  if args.phase in ('cross','queue'):
   # Prior phase's test working text only; clear via NEW, never user text.
   turn(d,1);click(d);capture(d,'clear_test_confirm',66);turn(d,1);click(d)
  group=0;position=[0]*4;inside=False
  for ch in text:
   target=0 if 'A'<=ch<='Z' else (1 if 'a'<=ch<='z' else 2)
   if target!=group:
    if inside:back(d);inside=False
    step=(target-group)%11
    if step>5:step-=11
    turn(d,-step);group=target
   if not inside:click(d);inside=True
   index=ord(ch)-ord('A' if group==0 else ('a' if group==1 else '0'))
   count=26 if group<2 else 10;step=(index-position[group])%count
   if step>count//2:step-=count
   turn(d,-step);click(d);position[group]=index
  if inside:back(d)
  capture(d,'editor_'+text,47);back(d);capture(d,'editor_saved',19)
 def send(d,mode):
  hub(d,7+mode);click(d);turn(d,1);click(d)
  if mode==2:
   capture(d,'offline_friend',70);click(d) # first friend is the other scoped device, baseline checked externally
  capture(d,'send_confirm_'+str(mode),71);turn(d,2);click(d)
  capture(d,'sent_'+str(mode),73 if mode==2 else 30)
 def inbox(d,name):hub(d,1);click(d);capture(d,name+'_list',24);click(d);capture(d,name+'_body',25)
 try:
  pump(4)
  for d in devices.values():d.send(9);pump(.3);home(d)
  sender=devices['COM10'];receiver=devices.get('COM9')
  if args.phase=='group':
   for d in devices.values():network_on(d)
   pump(7)
   for d in devices.values():capture(d,'discovered_peer',68)
   write(sender,'QG01');send(sender,0);pump(5);inbox(receiver,'group_received')
  elif args.phase=='cross':
   channel(receiver,1,2)
   for d in devices.values():network_on(d)
   write(sender,'QG02');send(sender,0);pump(6);inbox(receiver,'different_channel_no_group')
   # Back through confirm/actions to editor. NEW is group 10 from reset AFTER this phase's typing: group 2.
   hub(sender,4);click(sender);turn(sender,3);click(sender);capture(sender,'clear_group2_confirm',66);turn(sender,1);click(sender)
   # Selector reset by NEW; use a clean character writer starting at current editor.
   click(sender);turn(sender,-16);click(sender);turn(sender,16);click(sender);back(sender) # QA
   turn(sender,-2);click(sender);click(sender);turn(sender,-1);click(sender);back(sender);back(sender) # 01
   send(sender,1);pump(5);inbox(receiver,'announcement_received')
  elif args.phase=='queue':
   write(sender,'QO01');send(sender,2);pump(2);capture(sender,'queued_mesh_off',73)
  elif args.phase=='offline_wait':
   network_on(sender);pump(38);hub(sender,10);click(sender);click(sender);capture(sender,'waiting_receiver_offline',73)
  elif args.phase=='recover':
   network_on(receiver);network_on(sender);pump(42)
   hub(sender,10);click(sender);click(sender);capture(sender,'outbox_after_peer_returns',73)
   inbox(receiver,'offline_received')
  else:
   # Restoring work to baseline empty and removing ONLY test slot 1.
   hub(sender,4);click(sender);turn(sender,1);click(sender);capture(sender,'restore_empty_confirm',66);turn(sender,1);click(sender);back(sender)
   hub(sender,10);click(sender);click(sender);capture(sender,'test_task_before_cleanup',73);turn(sender,3);click(sender);turn(sender,1);click(sender)
   channel(receiver,2,baseline['COM9']['channel'])
  for d in devices.values():home(d);capture(d,'home',1)
  for d in devices.values():
   assert not any(v in d.log for v in (b'task_wdt:',b'Rebooting...',b'assert failed',b'Guru Meditation'))
  print('PASS UI phase '+args.phase+'; inspect private NVS and screenshots for actual RF verdict',flush=True)
 finally:
  for p,d in devices.items():(out/(p+'-private.log')).write_bytes(d.log);d.serial.close()
if __name__=='__main__':main()
