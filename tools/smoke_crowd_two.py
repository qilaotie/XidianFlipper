"""Native UI-only bilingual collaboration checks; no protocol/save injection.
Run --phase live after flash/fresh boot, then --phase persisted after reset/read-only NVS snapshots.
Private evidence only. Baseline empty work is required; existing saved slots are untouched.
"""
import argparse,json,time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index
ROOT=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--phase',choices=['live','persisted','finalrf'],required=True);args=ap.parse_args()
 out=ROOT/'build/crowd-two-private'/args.phase;out.mkdir(parents=True,exist_ok=False)
 baseline={p:json.loads((ROOT/'build/network-two-private/crowd-before'/p/'state-private.json').read_text()) for p in ('COM9','COM10')}
 assert all(not s['work'] and s['channel']==1 for s in baseline.values()),'Protect preexisting draft/channel'
 ds={p:Device(p) for p in baseline};beat=0;uptimes={p:0 for p in ds};records=[]
 def pump(seconds):
  nonlocal beat
  end=time.monotonic()+seconds
  while time.monotonic()<end:
   if time.monotonic()-beat>.5:
    for d in ds.values():d.send(7)
    beat=time.monotonic()
   for p,d in ds.items():
    d.read();n=d.telemetry.get('uptime_ms',uptimes[p]);assert n>=uptimes[p],(p,'unexpected reset');uptimes[p]=n
   time.sleep(.004)
 def click(d):d.send(2);pump(.18);d.send(3);pump(.65)
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
  home(d);click(d);turn(d,5);click(d);turn(d,menu_index('TOOLS','M_NETWORK_STATUS'));click(d);capture(d,'network_off',68);click(d);pump(3);capture(d,'network_on',68)
 def write_first(d,text):
  hub(d,4);click(d);pos=0
  for ch in text:
   index=ord(ch)-ord('A');step=(index-pos)%26
   if step>13:step-=26
   turn(d,-step);click(d);pos=index
  back(d);capture(d,'synthetic_draft',47);back(d)
 def create(d,row,name):
  hub(d,row);click(d);capture(d,name+'_create',90);turn(d,2);click(d);capture(d,name+'_confirm',91);turn(d,2);click(d);back(d);pump(3)
 def clear(d):
  hub(d,4);turn(d,1);click(d);capture(d,'clear_confirm',66);turn(d,1);click(d);back(d)
 try:
  pump(4)
  for d in ds.values():d.send(9);pump(.3);home(d)
  h,g=ds['COM9'],ds['COM10']
  if args.phase=='finalrf':
   current={p:json.loads((ROOT/'build/network-two-private/crowd-final-after'/p/'state-private.json').read_text()) for p in ds}
   assert not current['COM10']['work'] and all(e['state'] in (0,2) for s in current.values() for e in s['outbox']),'Protect user work/pending deliveries'
   for d in ds.values():network(d)
   h,g=ds['COM10'],ds['COM9'];write_first(h,'TEST');create(h,15,'final_buzzer');capture(h,'final_buzzer_open',89)
   hub(g,15);pump(3);turn(g,2);click(g);pump(4);capture(g,'final_guest_result',89)
   hub(h,15);turn(h,3);capture(h,'final_host_winner',89)
   for d in ds.values():hub(d,16);capture(d,'final_history',93)
   clear(h)
  elif args.phase=='live':
   for d in ds.values():network(d)
   # Inspect all previously untranslated menus without creating/sending messages.
   for row,name,page in [(7,'group_zh',69),(8,'announcement_zh',69),(9,'offline_zh',69),(10,'outbox_zh',72),(11,'management_zh',75),(12,'poll_zh',80),(13,'board_zh',83)]:
    hub(h,row);capture(h,name,page)
   write_first(h,'MEET')
   create(h,14,'checkin');capture(h,'checkin_host',88)
   hub(g,14);pump(3);capture(g,'checkin_discovered',88);turn(g,3);click(g);pump(3);capture(g,'on_way_confirmed',88)
   turn(g,-1);click(g);pump(3);capture(g,'arrived_confirmed',88)
   # Host has one received participant; row 6 is close. Host does not add itself.
   turn(h,6);capture(h,'checkin_host_list',88);click(h);capture(h,'checkin_close_confirm',91);turn(h,2);click(h);pump(3)
   hub(h,15);click(h);turn(h,2);click(h);turn(h,2);click(h);back(h);pump(3);capture(h,'buzzer_host_open',89)
   hub(g,15);pump(3);turn(g,2);click(g);pump(3);capture(g,'buzzer_guest_result',89)
   hub(h,15);turn(h,3);capture(h,'buzzer_host_winner',89)
   for d in ds.values():
    hub(d,17);turn(d,6);capture(d,'diagnostics_ack',92)
    hub(d,16);capture(d,'history_live',93);click(d);capture(d,'history_detail_live',94)
   clear(h)
  else:
   for d in ds.values():
    hub(d,16);capture(d,'history_after_reset',93);click(d);capture(d,'history_detail_after_reset',94)
  for d in ds.values():
   home(d);capture(d,'home',1);assert not any(x in d.log for x in (b'Guru Meditation',b'assert failed',b'task_wdt:',b'Rebooting...'))
  print('PASS UI phase '+args.phase+'; inspect screenshots/NVS for RF and persistence verdict',flush=True)
 finally:
  (out/'records.json').write_text(json.dumps(records,indent=2))
  for p,d in ds.items():(out/(p+'-private.log')).write_bytes(d.log);d.serial.close()
if __name__=='__main__':main()
