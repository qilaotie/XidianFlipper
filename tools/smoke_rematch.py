"""Two complete native tic matches with ready/start in the SAME room. No state injection."""
import argparse
from smoke_ready_flow import FlowLab
from smoke_tabletop import VIEW

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--ports',nargs='+',required=True);ap.add_argument('--tag',required=True);a=ap.parse_args();lab=FlowLab(a.ports,a.tag);original={}
 try:
  lab.pump(2)
  for d in lab.devices:d.send(9)
  lab.pump(.5)
  for d in lab.devices:original[d.port]=bool(lab.pg(d)[2]);assert not lab.pg(d)[0] or lab.pg(d)[7]==255
  lab.run_game(9,2,finish=False);host,peer=lab.devices[:2]
  for match in range(2):
   for d in lab.devices[:2]:lab.cursors[d.port]=0
   for cell in (0,3,1,4,2):lab.board_move(lab.pg(host)[9],cell,2)
   lab.sync(2);assert lab.pg(host)[7]==255 and lab.pg(host)[11]==1
   for d in lab.devices[:2]:lab.capture(d,f'match-{match+1}-finished-ready',VIEW)
   if match==0:
    revision=lab.pg(host)[12];lab.turn(host,1,parameter=True);lab.tap(host);lab.pump(.5);assert lab.pg(host)[7]==255 and lab.pg(host)[12]==revision
    lab.turn(host,-1,parameter=True)
    # Read the rendered ready rectangles, not injected/internal game state.
    def ready_mask():
     lab.pump(.4)
     return sum((1<<i) for i in range(2) if all(host.frame[x+(y//8)*128]&(1<<(y%8)) for x in range(i*16+9,i*16+12) for y in range(15,22)))
    for seat,d in enumerate((host,peer)):
     for attempt in range(4):
      if ready_mask()&(1<<seat):break
      lab.tap(d);lab.pump(2)
     assert ready_mask()&(1<<seat),('ready not applied',seat)
    lab.sync(2);lab.pump(3);assert ready_mask()==3
    for d in lab.devices[:2]:lab.capture(d,'same-room-ready-for-rematch',VIEW)
    lab.turn(host,1,parameter=True);lab.capture(host,'rematch-start-selected',VIEW)
    for attempt in range(3):
     lab.tap(host);lab.pump(.8)
     if lab.pg(host)[7]!=255:break
    lab.wait(lambda:lab.pg(host)[7]!=255);lab.sync(2)
    for d in lab.devices[:2]:
     assert lab.page(d)==VIEW and lab.pg(d)[8]==2
     assert bytes.fromhex(d.telemetry['ptable'])[:9]==bytes(9)
     lab.capture(d,'same-room-second-match-empty-board',VIEW)
  for d in lab.devices:
   lab.hub(d)
   if bool(lab.pg(d)[2])!=original[d.port]:lab.choose(d,0)
   lab.home(d);lab.capture(d,'final-home',1)
  print('PASS two complete matches; no rejoin; unready start blocked; fresh board and same seats',flush=True)
 finally:lab.close()
if __name__=='__main__':main()
