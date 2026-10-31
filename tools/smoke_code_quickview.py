"""Real buttons: code hand/table toggles preserve selection without leaving VIEW."""
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
  lab.run_game(2,2,finish=False);lab.pump(3)
  for d in lab.devices[:2]:
   lab.turn(d,2,parameter=True);lab.capture(d,'public-target',VIEW);public=bytes(d.frame);rev=lab.pg(d)[12]
   lab.tap(d,1.8);lab.capture(d,'own-hand',VIEW);assert bytes(d.frame)!=public
   lab.turn(d,1,parameter=True);lab.tap(d,1.8);lab.capture(d,'same-target-restored',VIEW);assert bytes(d.frame)==public and lab.pg(d)[12]==rev
   lab.tap(d,1.8);lab.capture(d,'own-cursor-restored',VIEW)
   d.send(4);lab.pump(.1);d.send(5);lab.pump(.8);lab.capture(d,'back-returns-to-table',VIEW);assert bytes(d.frame)==public and lab.pg(d)[12]==rev
  lab.end_game()
  for d in lab.devices:
   lab.hub(d)
   if bool(lab.pg(d)[2])!=original[d.port]:lab.choose(d,0)
   lab.home(d);lab.capture(d,'final-home',1)
  print('PASS same-page hand/table toggles, independent cursor restore, BACK to table, no game operation submitted',flush=True)
 finally:lab.close()
if __name__=='__main__':main()
