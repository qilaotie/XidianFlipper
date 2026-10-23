"""Normal UI skin/widget changes and room previews; never spends pet coins."""
import argparse,time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--port',default='COM9');ap.add_argument('--restore',action='store_true');ap.add_argument('--record-state',action='store_true');ap.add_argument('--normalize',action='store_true',help='Only after visually confirming TERMINAL + PET');a=ap.parse_args()
 d=Device(a.port);beat=0;seen_uptime=0;out=Path('build/personalization-smoke')/a.port;out.mkdir(parents=True,exist_ok=True)
 def pump(seconds):
  nonlocal beat,seen_uptime
  end=time.monotonic()+seconds
  while time.monotonic()<end:
   if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
   d.read();uptime=d.telemetry.get('uptime_ms',seen_uptime)
   assert uptime>=seen_uptime,'Device rebooted during UI test'
   seen_uptime=uptime;time.sleep(.004)
 def click():d.send(2);pump(.18);d.send(3);pump(.8)
 def back():d.send(4);pump(.06);d.send(5);pump(.5)
 def turn(n):
  for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.25)
  pump(.3)
 def home():
  for _ in range(5):back()
  assert d.telemetry.get('page')==1,d.telemetry
 def capture(name,page):
  pump(1.5 if a.restore or a.record_state else .1);assert d.telemetry.get('page')==page,(name,d.telemetry)
  assert len(d.frame)==512 and any(d.frame)
  (out/(name+'.png')).write_bytes(png(d.frame));print('CAPTURE '+name,flush=True)
 def settings(row):home();click();turn(6);click();turn(row)
 try:
  pump(4);d.send(9,0);pump(.5);home()
  if a.normalize:
   settings(10);click();capture('normalized_skin',3);turn(1)
   for _ in range(3):click()
   capture('normalized_widget',3);home();return
  if a.restore or a.record_state:
   if a.record_state:
    for i in range(3):capture('carousel_'+str(i),1);pump(8.1)
   capture('persisted_home',1);settings(10);capture('persisted_skin',3)
   path=out/'expected_skin.raw'
   if a.record_state:path.write_bytes(d.frame[:256])
   else:assert path.read_bytes()==d.frame[:256],'Skin changed after reboot'
   turn(1);capture('persisted_widget',3);path=out/'expected_widget.raw'
   if a.record_state:path.write_bytes(d.frame[:256])
   else:assert path.read_bytes()==d.frame[:256],'Widget changed after reboot'
   home()
   if a.restore:
    click();turn(5);click();turn(4);click();capture('cloud_after_reboot',62);pump(12);capture('cloud_settled',62);home()
   print('PASS settings snapshot/reboot comparison',flush=True);return
  for skin in range(3):
   settings(10);capture('skin_'+str(skin)+'_setting',3)
   home();click();capture('skin_'+str(skin)+'_main',2);turn(5);capture('skin_'+str(skin)+'_tools_tile',2)
   settings(10);click()
  for widget in range(4):
   home();capture('widget_'+str(widget),1)
   settings(11);capture('widget_setting_'+str(widget),3);click();capture('widget_after_'+str(widget),3)
  # Default widget 0 -> AUTO 3; preserve classic skin after full cycle.
  settings(11)
  for _ in range(3):click()
  home();click();turn(4);click();click();capture('pet_home',61);turn(12);click()
  for room in range(4):
   if room:turn(1)
   capture('room_preview_'+str(room),61)
  back();home();capture('final_home',1)
  assert not any(x in d.log for x in (b'Guru Meditation',b'assert failed',b'Stack canary',b'task_wdt:',b'Rebooting...'))
  print('PASS skins/widgets/room previews, no purchases; classic skin + AUTO home',flush=True)
 finally:
  (out/'private-ui.log').write_bytes(d.log)
  d.serial.close()

if __name__=='__main__':main()
