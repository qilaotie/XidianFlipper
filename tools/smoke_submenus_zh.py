"""Normal read-only UI checks. No purchases, care, new eggs, game starts or messages."""
import argparse,time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from smoke_tetris import portrait_png
from menu_layout import menu_index

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--port',default='COM9');ap.add_argument('--finish',action='store_true');a=ap.parse_args()
 d=Device(a.port);beat=0;uptime=0;out=Path('build/submenus-zh-smoke')/a.port;out.mkdir(parents=True,exist_ok=True)
 def pump(seconds):
  nonlocal beat,uptime
  end=time.monotonic()+seconds
  while time.monotonic()<end:
   if time.monotonic()-beat>.5:d.send(7);beat=time.monotonic()
   d.read();now=d.telemetry.get('uptime_ms',uptime);assert now>=uptime,'Device rebooted';uptime=now;time.sleep(.004)
 def click():d.send(2);pump(.18);d.send(3);pump(.7)
 def back():d.send(4);pump(.08);d.send(5);pump(.5)
 def turn(n):
  for _ in range(abs(n)):d.send(1,1 if n>0 else -1);pump(.2)
  pump(.3)
 def capture(name,page):
  pump(.5);assert d.telemetry.get('page')==page,(name,d.telemetry)
  assert len(d.frame)==512 and any(d.frame)
  (out/(name+'.png')).write_bytes(portrait_png(d.frame) if name=='tetris' else png(d.frame));print('CAPTURE '+name,flush=True)
 def home():
  # BACK toggles pause in Tetris; exit via its paused menu instead.
  if d.telemetry.get('page')==59:turn(2);click()
  for _ in range(6):back()
  assert d.telemetry.get('page')==1,d.telemetry
 def tile(index):home();click();turn(index);click()
 def fun(page):tile(4);turn(menu_index('FUN',page));click()
 try:
  pump(4);d.send(9,0);pump(.5)
  if a.finish:
   for page,num,name in [('M_SNAKE',60,'snake'),('M_TETRIS',59,'tetris'),('M_RPS',40,'rps'),('M_REACTION',49,'reaction_lobby'),('M_NUMBER_BOMB',51,'bomb_lobby'),('M_TIME_GUESS',52,'time_lobby')]:
    fun(page);capture(name,num)
   home();capture('home',1);print('PASS final game menus and pause-menu exit; returned HOME',flush=True);return
  tile(0);capture('messages',19);turn(5);capture('messages_last',19)
  tile(1);capture('channel',20)
  tile(2);capture('learn',21);turn(3);capture('learn_last',21)
  tile(3);capture('friends',9)
  fun('M_GAME_HELP');turn(10);capture('help_pet_entry',63);click();capture('pet_topic_list',63)
  for item in range(13):
   if item:turn(1)
   click();capture(f'pet_help_{item}_first',63)
   turn(4 if item==12 else 3);capture(f'pet_help_{item}_last',63);back()
  back();capture('help_back_to_games',63)
  for action,name in [(4,'shop'),(5,'bag'),(6,'dex'),(7,'growth'),(9,'connect'),(10,'guides'),(12,'room')]:
   fun('M_PET');turn(action);capture('pet_action_'+name,61);click();capture('pet_'+name,61)
   if action==12:
    for room in range(1,4):turn(1);capture('room_'+str(room),61)
  for page,num,name in [('M_ANSWER_BOOK',16,'answer'),('M_DICE',17,'dice'),('M_CRAPS',18,'craps'),('M_SNAKE',60,'snake'),('M_TETRIS',59,'tetris'),('M_RPS',40,'rps')]:
   fun(page);capture(name,num)
  home();capture('home',1)
  assert not any(x in d.log for x in (b'task_wdt:',b'Rebooting...',b'assert failed',b'Guru Meditation'))
  print('PASS submenu Chinese/pet guides, normal navigation, no spending or gameplay; returned HOME',flush=True)
 finally:(out/'private-ui.log').write_bytes(d.log);d.serial.close()

if __name__=='__main__':main()
