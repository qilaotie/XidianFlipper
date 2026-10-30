"""Native UI-only two-device collaboration smoke. Private screenshots/logs under build/.
Requires fresh boot, baseline empty work draft, channel 1 and MESH off on both.
Does not inject radio frames or saved data. Inspect screenshots for RF verdict.
"""
import json,time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index
ROOT=Path(__file__).resolve().parents[1]
def main():
 out=ROOT/'build/team-two-private';out.mkdir(exist_ok=False)
 baseline={p:json.loads((ROOT/'build/network-two-private/team-before'/p/'state-private.json').read_text()) for p in ('COM9','COM10')}
 assert all(not s['work'] and s['channel']==1 for s in baseline.values()),'Protect preexisting working draft/channel'
 ds={p:Device(p) for p in baseline};beat=0;uptimes={p:0 for p in ds};records=[]
 def pump(seconds):
  nonlocal beat
  until=time.monotonic()+seconds
  while time.monotonic()<until:
   if time.monotonic()-beat>.5:
    for d in ds.values():d.send(7)
    beat=time.monotonic()
   for p,d in ds.items():
    d.read();n=d.telemetry.get('uptime_ms',uptimes[p]);assert n>=uptimes[p],(p,'unexpected reset');uptimes[p]=n
   time.sleep(.004)
 def click(d):d.send(2);pump(.18);d.send(3);pump(.65)
 def hold(d):d.send(2);pump(1.3);d.send(3);pump(.7)
 def back(d):d.send(4);pump(.08);d.send(5);pump(.4)
 def turn(d,n):
  for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.13)
  pump(.2)
 def capture(d,name,page):
  pump(.3);assert d.telemetry.get('page')==page,(d.port,name,d.telemetry)
  (out/f'{d.port}-{name}.png').write_bytes(png(d.frame));records.append({'port':d.port,'name':name,'telemetry':d.telemetry.copy()});print('CAPTURE',d.port,name,flush=True)
 def home(d):
  assert d.telemetry.get('page')!=59
  for _ in range(18):
   if d.telemetry.get('page')==1:return
   back(d)
  raise AssertionError(d.telemetry)
 def hub(d,row):home(d);click(d);click(d);turn(d,row);click(d)
 def network(d):
  home(d);click(d);turn(d,5);click(d);turn(d,menu_index('TOOLS','M_NETWORK_STATUS'));click(d);click(d);pump(3);capture(d,'mesh_on',68)
 def write_first(d,text):
  hub(d,4);capture(d,'editor_start',47);click(d);pos=0
  for ch in text:
   index=ord(ch)-ord('A');step=(index-pos)%26
   if step>13:step-=26
   turn(d,-step);click(d);pos=index
  back(d);capture(d,'draft_'+text,47);back(d)
 def clear(d):
  hub(d,4);turn(d,1);click(d);capture(d,'clear_confirm',66);turn(d,1);click(d);back(d)
 try:
  pump(4)
  for d in ds.values():d.send(9);pump(.4);home(d);network(d)
  host=ds['COM9'];guest=ds['COM10'];pump(5)
  # Presence from direct discovery beacons.
  for d in ds.values():
   home(d);click(d);turn(d,3);click(d);capture(d,'friends_presence',9);turn(d,1);click(d);turn(d,3);click(d);capture(d,'contact_direct',78)
  write_first(host,'TEAM')
  hub(host,12);click(host);capture(host,'poll_create',81);turn(host,2);click(host);capture(host,'poll_confirmation',82);turn(host,2);click(host);back(host);pump(4)
  capture(host,'poll_host',80);hub(guest,12);pump(3);capture(guest,'poll_discovered',80)
  # Guest B, creator A. Scroll to counts to make them visible.
  turn(guest,2);click(guest);pump(4);capture(guest,'poll_vote_confirmed',80)
  turn(host,1);click(host);pump(3);capture(host,'poll_A_count',80);turn(host,1);capture(host,'poll_B_count',80)
  # Open same draft as first task; no overwrite of user text.
  hub(host,13);click(host);capture(host,'task_create',84);turn(host,2);click(host);turn(host,2);click(host);back(host);pump(4);capture(host,'board_host',83)
  hub(guest,13);pump(3);capture(guest,'board_discovered',83);turn(guest,1);click(guest);turn(guest,2);click(guest);pump(4);capture(guest,'claim_confirmed',85)
  hub(host,13);turn(host,1);click(host);capture(host,'task_owner',85)
  # Completion by actual assignee, propagated back to host.
  turn(guest,1);click(guest);pump(4);capture(guest,'complete_confirmed',85);back(host);capture(host,'task_done_host',83)
  # Message management and quote editor cancellation, using existing received test text.
  hub(host,11);capture(host,'message_filters',75);click(host);capture(host,'filtered_inbox',76);click(host);hold(host);capture(host,'message_actions',77)
  turn(host,2);click(host);capture(host,'quote_editor',47)
  # Do not send quote; clear only known synthetic work after baseline check.
  clear(host);home(host);home(guest)
  for d in ds.values():
   capture(d,'home',1);assert not any(x in d.log for x in (b'Guru Meditation',b'assert failed',b'task_wdt:',b'Rebooting...'))
  print('PASS UI actions/no unexpected reboot; inspect RF screenshots and NVS cleanup',flush=True)
 finally:
  (out/'records.json').write_text(json.dumps(records,indent=2))
  for p,d in ds.items():(out/(p+'-private.log')).write_bytes(d.log);d.serial.close()
if __name__=='__main__':main()
