"""Normal button-only waiting-room/automatic board acceptance. No state injection."""
import argparse
from smoke_tabletop import Lab,ROOM,VIEW,LEAVE,NAMES
class FlowLab(Lab):
 def tap(self,d,hold=.10):
  before=self.page(d);d.send(2);self.pump(hold);d.send(3);self.pump(.8)
  after=self.page(d)
  if after!=before:
   nodes=self.nodes[d.port];parents=[i for i,n in enumerate(nodes[:-1]) if n[0]==after]
   if parents:del nodes[parents[-1]+1:]
   else:nodes.append([after,0])
 def end_game(self):
  host=self.devices[0]
  if self.pg(host)[0] and self.pg(host)[1] and self.pg(host)[7]!=255:
   self.ensure_room(host)
   if self.pg(host)[7]==0:
    # Reenter so default control is ready; four steps reaches End.
    self.home(host);self.hub(host);self.choose(host,1);self.turn(host,4,parameter=True);self.tap(host)
   else:self.choose(host,7)
   assert self.page(host)==LEAVE;self.choose(host,1);self.wait(lambda:self.pg(host)[7]==255)
 def run_game(self,kind,n,finish=True):
  self.end_game()
  for d in self.devices:self.enable(d)
  host=self.devices[0];self.choose(host,kind+2);self.choose(host,2);self.wait(lambda:self.page(host)==ROOM and self.pg(host)[7]==0)
  self.capture(host,NAMES[kind]+'-created-ready-room',ROOM)
  for d in self.devices[1:n]:
   self.pump(4);self.choose(d,kind+2);self.choose(d,3);self.nodes[d.port][-1][1]=0;self.choose(d,2);self.nodes[d.port][-1][1]=0;self.choose(d,4)
   self.wait(lambda:self.page(d)==ROOM and self.pg(host)[8]==self.devices.index(d)+1)
  self.sync(n)
  for d in self.devices[1:n]:
   self.capture(d,NAMES[kind]+'-joined-ready-room',ROOM);self.tap(d);self.wait(lambda:not self.pg(d)[4]);self.pump(2)
  self.capture(host,NAMES[kind]+'-everyone-ready',ROOM)
  self.turn(host,1,parameter=True);self.pump(1);self.capture(host,NAMES[kind]+'-start-selected',ROOM);self.tap(host);self.wait(lambda:self.pg(host)[7]!=0)
  self.sync(n)
  for d in self.devices[:n]:
   self.wait(lambda:self.page(d)==VIEW)
   if self.nodes[d.port][-1][0]!=VIEW:self.nodes[d.port].append([VIEW,0])
   self.capture(d,NAMES[kind]+'-automatic-board',VIEW)
  if kind in (0,3,4):
   self.tap(host,1.8);self.capture(host,NAMES[kind]+'-reveal-hand',VIEW)
  # No gameplay actions: only navigation and privacy reveal, then end test match.
  if finish:self.end_game()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--ports',nargs='+',required=True);ap.add_argument('--tag',required=True);ap.add_argument('--kinds',nargs='+',type=int,default=[4,0,2,3,1,5,9]);a=ap.parse_args();lab=FlowLab(a.ports,a.tag);original={}
 try:
  lab.pump(2)
  for d in lab.devices:d.send(9)
  lab.pump(.5)
  for d in lab.devices:original[d.port]=bool(lab.pg(d)[2]);assert not lab.pg(d)[0] or lab.pg(d)[7]==255,'Existing live game; stop without ending it'
  for kind in a.kinds:lab.run_game(kind,3 if kind in (1,3) else 2)
  for d in lab.devices:
   lab.hub(d)
   if bool(lab.pg(d)[2])!=original[d.port]:lab.choose(d,0)
   lab.home(d);lab.capture(d,'final-home',1)
  print('PASS normal room, ready, auto-board and private reveal; no full-match claim',flush=True)
 finally:lab.close()
if __name__=='__main__':main()
