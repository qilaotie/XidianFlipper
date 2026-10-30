"""Single-device network UI/owner smoke. Cannot prove multi-hop RF reachability."""
import time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index

def main():
 d=Device('COM9');out=Path('build/network-smoke');out.mkdir(parents=True,exist_ok=True);beat=0;uptime=0
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
  pump(.4);assert d.telemetry.get('page')==page,(name,d.telemetry)
  (out/(name+'.png')).write_bytes(png(d.frame));print('CAPTURE',name,flush=True)
 try:
  pump(4);d.send(9);pump(.4);assert d.telemetry.get('page')!=59,'Exit Tetris first'
  for _ in range(10):back()
  assert d.telemetry.get('page')==1
  click();turn(5);click();turn(menu_index('TOOLS','M_NETWORK_STATUS'));capture('tools_network',41)
  click();capture('default_off',68);click();capture('enabled',68)
  pump(16);capture('enabled_idle',68);turn(1);capture('stats',68)
  back();click();capture('enabled_reenter',68);click();capture('disabled',68)
  click();pump(6);capture('enabled_final',68)
  back();back();back();capture('home',1)
  assert not any(x in d.log for x in (b'task_wdt:',b'Rebooting...',b'assert failed',b'Guru Meditation'))
  print('PASS COM9 network UI OFF/ON, idle discovery, stats, reentry; left ON until reboot; no multi-device RF claim',flush=True)
 finally:(out/'private.log').write_bytes(d.log);d.serial.close()

if __name__=='__main__':main()
