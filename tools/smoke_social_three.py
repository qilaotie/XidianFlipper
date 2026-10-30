"""Native UI-only staged RF acceptance. A=COM10 B=COM15 C=COM9.
Parent must pause C before stash, pause A before deliver, and pause C before receipt.
No packet or NVS injection. Private evidence under ignored build/.
"""
import argparse,json,time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index
ROOT=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--phase',choices=['stash','deliver','receipt','cleanup','rooms','ensemble','view'],required=True);args=ap.parse_args()
 ports={'stash':['COM10','COM15'],'deliver':['COM15','COM9'],'receipt':['COM10','COM15'],'cleanup':['COM10'],'rooms':['COM9','COM10','COM15'],'ensemble':['COM9','COM10','COM15'],'view':['COM9']}[args.phase]
 out=ROOT/'build/social-three-private'/args.phase;out.mkdir(parents=True,exist_ok=False);ds={p:Device(p) for p in ports};beat=0;uptimes={p:0 for p in ports};records=[];groups={p:0 for p in ports}
 baseline={p:json.loads((ROOT/'build/network-two-private/social-before'/p/'state-private.json').read_text()) for p in ports}
 assert all(s['channel']==1 for s in baseline.values());assert not baseline.get('COM10',{}).get('work') and not baseline.get('COM15',{}).get('work'),'Protect user working drafts'
 def pump(seconds):
  nonlocal beat
  until=time.monotonic()+seconds
  while time.monotonic()<until:
   if time.monotonic()-beat>.5:
    for d in ds.values():d.send(7)
    beat=time.monotonic()
   for p,d in ds.items():
    d.read();now=d.telemetry.get('uptime_ms',uptimes[p]);assert now>=uptimes[p],(p,'unexpected reset');uptimes[p]=now
   time.sleep(.004)
 def click(d):d.send(2);pump(.18);d.send(3);pump(.65)
 def back(d):d.send(4);pump(.08);d.send(5);pump(.4)
 def turn(d,n):
  for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.13)
  pump(.2)
 def capture(d,name,page):
  pump(.3);assert d.telemetry.get('page')==page,(d.port,name,d.telemetry);(out/f'{d.port}-{name}.png').write_bytes(png(d.frame));records.append({'port':d.port,'name':name,'telemetry':d.telemetry.copy()});print('CAPTURE',d.port,name,flush=True)
 def home(d):
  assert d.telemetry.get('page')!=59
  for _ in range(18):
   if d.telemetry.get('page')==1:return
   back(d)
  raise AssertionError(d.telemetry)
 def hub(d,row):home(d);click(d);click(d);turn(d,row);click(d)
 def network(d):
  home(d);click(d);turn(d,5);click(d);turn(d,menu_index('TOOLS','M_NETWORK_STATUS'));click(d);click(d);pump(1);capture(d,'network_on',68)
 def section(d,n):hub(d,18);turn(d,n-1);click(d)
 def relay_on(d):section(d,2);capture(d,'relay_before',95);click(d);capture(d,'relay_on',95)
 def clear(d):
  hub(d,4);step=(10-groups[d.port])%11
  if step>5:step-=11
  turn(d,-step);click(d);capture(d,'clear_confirm',66);turn(d,1);click(d);groups[d.port]=0;back(d)
 def write(d,text,replace=False):
  if replace:clear(d)
  hub(d,4);group=groups[d.port];inside=False;positions=[0]*4
  for ch in text:
   target=0 if 'A'<=ch<='Z' else 2
   if target!=group:
    if inside:back(d);inside=False
    step=(target-group)%11
    if step>5:step-=11
    turn(d,-step);group=target
   if not inside:click(d);inside=True
   n=26 if group==0 else 10;index=ord(ch)-ord('A' if group==0 else '0');step=(index-positions[group])%n
   if step>n//2:step-=n
   turn(d,-step);click(d);positions[group]=index
  if inside:back(d)
  groups[d.port]=group;capture(d,'editor_'+text,47);back(d)
 def confirm_form(d):capture(d,'form_preview',96);turn(d,3);click(d)
 try:
  pump(4)
  for d in ds.values():d.send(9);pump(.3);home(d)
  a,b,c=ds.get('COM10'),ds.get('COM15'),ds.get('COM9')
  if args.phase=='stash':
   for d in ds.values():network(d)
   relay_on(b);write(a,'QM01');section(a,2);turn(a,1);click(a);turn(a,1);click(a);capture(a,'relay_recipient',70);click(a);capture(a,'relay_confirm',71);turn(a,2);click(a);capture(a,'outbox_wait',73);pump(8);section(b,2);capture(b,'cached_sender_mail',95)
  elif args.phase=='deliver':
   for d in ds.values():network(d)
   relay_on(b);pump(15);hub(c,1);click(c);capture(c,'custody_received',25);section(b,2);capture(b,'cache_after_receipt',95)
  elif args.phase=='receipt':
   for d in ds.values():network(d)
   relay_on(b);pump(40);hub(a,10);click(a);capture(a,'sender_delivery_after_return',73)
  elif args.phase=='cleanup':
   # Fresh reset, known test work QM01 and outbox slot 0 from this test only.
   current=json.loads((ROOT/'build/network-two-private/social-receipt/COM10/state-private.json').read_text());assert current['work']=='QM01' and current['outbox'][0]['state']==2
   groups[a.port]=0;clear(a);hub(a,10);click(a);turn(a,3);click(a);turn(a,1);click(a)
  elif args.phase=='rooms':
   for d in ds.values():network(d)
   for d in (a,b):
    write(d,'11223344');section(d,1);turn(d,3);click(d);confirm_form(d);turn(d,-3);capture(d,'joined_room',95)
   write(a,'ROOM',True);hub(a,12);click(a);turn(a,2);click(a);turn(a,2);click(a);back(a);pump(3)
   hub(b,12);capture(b,'same_room_poll',80);hub(c,12);capture(c,'public_excludes_room_poll',80)
   hub(a,7);turn(a,1);click(a);turn(a,2);click(a);pump(4);hub(b,1);click(b);capture(b,'room_message_received',25)
   section(a,3);turn(a,1);click(a);confirm_form(a);section(a,3);turn(a,2);click(a);confirm_form(a);section(a,3);turn(a,4);click(a);pump(12)
   section(b,3);turn(b,5);click(b);capture(b,'peer_profile',97)
   section(a,3);turn(a,4);click(a);clear(a)
   for field in (1,2):section(a,3);turn(a,field);click(a);confirm_form(a)
   clear(b)
   for d in (a,b):section(d,1);turn(d,4);click(d)
  elif args.phase=='view':
   section(c,1);capture(c,'full_room_code',95);section(c,3);click(c);capture(c,'own_profile_view',97)
   home(c);click(c);turn(c,4);click(c);turn(c,menu_index('FUN','M_PASSPORT'));click(c);capture(c,'passport_view',98)
  else:
   for d in ds.values():network(d)
   for d in ds.values():home(d);click(d);turn(d,4);click(d);turn(d,menu_index('FUN','M_ENSEMBLE'));click(d);capture(d,'ensemble_listening',99)
   turn(b,1);click(b);turn(b,-1)
   turn(c,1);click(c);click(c);click(c);turn(c,-1)
   turn(a,4);click(a);turn(a,-4);pump(10)
   for d in ds.values():capture(d,'ensemble_progress',99)
   for d in ds.values():home(d);click(d);turn(d,4);click(d);turn(d,menu_index('FUN','M_PASSPORT'));click(d);capture(d,'passport_entry',98)
  for d in ds.values():home(d);capture(d,'home',1);assert not any(x in d.log for x in (b'Guru Meditation',b'assert failed',b'task_wdt:',b'Rebooting...'))
  print('PASS native UI '+args.phase+'; inspect snapshots/screens for RF verdict',flush=True)
 finally:
  (out/'records.json').write_text(json.dumps(records,indent=2))
  for p,d in ds.items():(out/(p+'-private.log')).write_bytes(d.log);d.serial.close()
if __name__=='__main__':main()
