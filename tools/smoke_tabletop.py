"""Real USB button/encoder acceptance; no injected RF packets, cards or NVS.
Screens and telemetry stay in ignored build/. High player counts need more devices.
"""
import argparse,json,time
from pathlib import Path
from smoke_party import Device
from smoke_wireless_tools import png
from menu_layout import menu_index
HUB,ROOM,VIEW,ACTION,HELP,CREATE,LEAVE=range(109,116)
NAMES=['uno','undercover','code','liar','love','yacht','avalon','one-night','werewolf','tic','reversi']

class Lab:
    def __init__(self,ports,tag):
        self.devices=[Device(p) for p in ports];self.out=Path('build/tabletop-private')/tag;self.out.mkdir(parents=True,exist_ok=False)
        self.beat=0;self.uptime={d.port:0 for d in self.devices};self.nodes={d.port:[[1,0]] for d in self.devices};self.records=[];self.sequence=0;self.cursors={d.port:0 for d in self.devices}
    def pump(self,seconds):
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            if time.monotonic()-self.beat>.45:
                for d in self.devices:d.send(7)
                self.beat=time.monotonic()
            for d in self.devices:
                d.read();now=d.telemetry.get('uptime_ms',self.uptime[d.port]);assert now>=self.uptime[d.port],(d.port,'unexpected reboot');self.uptime[d.port]=now
            time.sleep(.004)
    def page(self,d):return d.telemetry.get('page')
    def pg(self,d):return d.telemetry.get('party',[0]*14)
    def wait(self,predicate,timeout=15):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            self.pump(.15)
            if predicate():return
        raise AssertionError([(d.port,d.telemetry) for d in self.devices])
    def click(self,d,hold=.18):
        before=self.page(d);row=self.nodes[d.port][-1][1];expected=None
        if before==1:expected=2
        elif before==2 and row==4:expected=23
        elif before==23 and row==menu_index('FUN','M_PARTY_HUB'):expected=HUB
        elif before==HUB and row>=2:expected=CREATE
        elif before==HUB and row==1 and self.pg(d)[0]:expected=ROOM
        elif before==CREATE:expected=None  # Also hosts table browser; rows depend on view.
        elif before==ROOM:expected=({4:VIEW,5:VIEW,6:HELP,7:LEAVE} if self.pg(d)[6] in (2,3,4,9) else {3:VIEW,4:VIEW,5:ACTION,6:HELP,7:LEAVE}).get(row)
        elif before==ACTION:expected=None  # Visible rows depend on the selected action.
        elif before==LEAVE:expected=ROOM
        d.send(2);self.pump(hold);d.send(3);self.pump(.65)
        if expected is not None:self.wait(lambda:self.page(d)==expected,8)
        after=self.page(d)
        if before!=after:
            nodes=self.nodes[d.port]
            parents=[i for i,node in enumerate(nodes[:-1]) if node[0]==after]
            if parents:del nodes[parents[-1]+1:]
            else:nodes.append([after,0])
    def back(self,d):
        nodes=self.nodes[d.port];expected=nodes[-2][0] if len(nodes)>1 else None
        d.send(4);self.pump(.2);d.send(5);self.pump(.55)
        if expected is not None:self.wait(lambda:self.page(d)==expected,8)
        nodes=self.nodes[d.port]
        if len(nodes)>1:nodes.pop()
    def turn(self,d,n,parameter=False):
        for _ in range(abs(n)):d.send(1,1 if n>0 else -1);self.pump(.12)
        self.pump(.16)
        if not parameter:self.nodes[d.port][-1][1]+=n
    def choose(self,d,row):
        assert self.page(d)==self.nodes[d.port][-1][0],(self.page(d),self.nodes[d.port])
        self.turn(d,row-self.nodes[d.port][-1][1]);self.click(d)
    def capture(self,d,name,page=None):
        self.pump(.25)
        if page is not None:assert self.page(d)==page,(name,d.telemetry)
        assert len(d.frame)==512 and any(d.frame)
        self.sequence+=1;(self.out/f'{self.sequence:03}-{d.port}-{name}.png').write_bytes(png(d.frame))
        self.records.append({'port':d.port,'name':name,'telemetry':d.telemetry.copy()});self.save();print('PASS',d.port,name,self.pg(d),flush=True)
    def save(self):
        (self.out/'result.json').write_text(json.dumps(self.records,indent=2),encoding='utf8')
        for d in self.devices:(self.out/(d.port+'.log')).write_bytes(d.log)
    def home(self,d):
        assert self.page(d)!=59
        for _ in range(22):
            if self.page(d)==1:self.nodes[d.port]=[[1,0]];return
            self.back(d)
        raise AssertionError(d.telemetry)
    def hub(self,d):
        self.home(d);self.click(d);self.turn(d,4);self.click(d);assert self.page(d)==23
        self.turn(d,menu_index('FUN','M_PARTY_HUB'));self.click(d);assert self.page(d)==HUB
    def enable(self,d):
        self.hub(d)
        if not self.pg(d)[2]:self.choose(d,0)
        self.wait(lambda:self.pg(d)[2]==1)
    def ensure_room(self,d):
        while self.page(d)!=ROOM:
            if self.page(d) in (VIEW,ACTION,HELP,LEAVE):self.back(d)
            else:self.hub(d);self.choose(d,1)
        assert self.page(d)==ROOM
    def end_game(self):
        host=self.devices[0]
        if self.pg(host)[0] and self.pg(host)[1] and self.pg(host)[7]!=255:
            self.ensure_room(host);self.choose(host,7);self.choose(host,1);self.wait(lambda:self.pg(host)[7]==255)
    def create_game(self,kind,players):
        self.end_game()
        for d in self.devices:self.enable(d)
        host=self.devices[0];self.choose(host,kind+2);self.choose(host,2);self.wait(lambda:self.pg(host)[0] and self.pg(host)[1] and self.pg(host)[6]==kind and self.pg(host)[7]==0)
        for d in self.devices[1:players]:
            self.pump(5)
            self.choose(d,kind+2);self.choose(d,3)
            self.nodes[d.port][-1][1]=0  # Table list starts at Refresh.
            self.choose(d,2)  # Frozen table identity detail, no JOIN yet.
            self.nodes[d.port][-1][1]=0
            self.choose(d,4);self.wait(lambda:self.page(d)==ROOM)
            self.wait(lambda:self.pg(host)[8]>=self.devices.index(d)+1)
        self.sync(players)
        for d in self.devices[1:players]:
            self.ensure_room(d);self.choose(d,8);self.pump(6);self.sync(players)
        self.capture(host,NAMES[kind]+'-lobby-ready',ROOM)
        self.choose(host,2)
        if kind in (6,8):
            self.pump(1);assert self.pg(host)[7]==0 and self.pg(host)[5]==3
            self.capture(host,NAMES[kind]+'-minimum-enforced',ROOM);return False
        self.wait(lambda:self.pg(host)[7]!=0);self.sync(players)
        for d in self.devices[:players]:
            if kind in (0,5,9):
                self.wait(lambda:self.page(d)==VIEW)
                if self.nodes[d.port][-1][0]!=VIEW:self.nodes[d.port].append([VIEW,0])
                self.cursors[d.port]=0
            self.capture(d,NAMES[kind]+'-started',VIEW if kind in (0,5,9) else ROOM)
        return True
    def sync(self,n):
        host=self.devices[0]
        self.wait(lambda:all(self.pg(d)[0] and self.pg(d)[3] and not self.pg(d)[4] and self.pg(d)[12]==self.pg(host)[12] and self.pg(d)[8]==n for d in self.devices[:n]),20)
    def do(self,seat,which=0,a=0,b=0,c=None,players=3,allow_reject=False,liar_pick=False):
        if self.pg(self.devices[0])[6]==9:return self.board_move(seat,a,players)
        d=self.devices[seat];host=self.devices[0];self.sync(players);self.ensure_room(d);self.choose(d,5);assert self.page(d)==ACTION
        for _ in range(which):self.choose(d,0)
        kind=self.pg(d)[6];default_c=0 if which and kind==2 else 2
        for row,value,default in ((1,a,0),(2,b,0),(3,c,default_c)):
            if value is not None and value!=default:
                self.choose(d,row);self.turn(d,value-default,parameter=True);self.click(d)
        if liar_pick:
            self.choose(d,1);assert self.page(d)==VIEW;self.click(d,1.8);self.pump(.2);self.choose(d,0);self.back(d);assert self.page(d)==ACTION
        before=self.pg(host)[12];self.choose(d,4);self.pump(.6);self.wait(lambda:not self.pg(d)[4] and (self.pg(host)[12]>before or self.pg(d)[5] in (3,4)),12)
        self.sync(players);result=self.pg(d)[5]
        if not allow_reject:assert result==2 and self.pg(host)[12]>before,(NAMES[kind],seat,which,result,d.telemetry)
        self.back(d);assert self.page(d)==ROOM
        return result==2
    def board_move(self,seat,target,players=2):
        host=self.devices[0];self.sync(players);self.pump(.2)
        cells=bytes.fromhex(host.telemetry['ptable'])[:9]
        for peer in self.devices[:players]:
            current=self.cursors[peer.port]
            if cells[current] and self.pg(peer)[7]==1:
                current=next((current+i)%9 for i in range(1,10) if not cells[(current+i)%9])
                self.cursors[peer.port]=current
        d=self.devices[seat];assert self.page(d)==VIEW
        empty=[i for i,c in enumerate(cells) if not c];assert target in empty
        steps=(empty.index(target)-empty.index(self.cursors[d.port]))%len(empty)
        self.turn(d,steps,parameter=True);self.cursors[d.port]=target
        before=self.pg(host)[12];self.click(d);self.wait(lambda:self.pg(host)[12]>before);self.sync(players)
        updated=bytes.fromhex(host.telemetry['ptable'])[:9];assert updated[target]==seat+1
        assert sum(bool(c) for c in updated)==sum(bool(c) for c in cells)+1
        return True
    def board_games(self):
        for name,moves,winner in [('win',(0,3,1,4,2),1),('draw',(0,1,2,4,3,5,7,6,8),3)]:
            assert self.create_game(9,2);host,peer=self.devices[:2]
            if name=='win':
                before=self.pg(host)[12];self.click(peer);self.pump(.8);assert self.pg(host)[12]==before
                self.capture(peer,'waiting-no-move',VIEW)
            for i,pos in enumerate(moves):
                self.board_move(self.pg(host)[9],pos)
                if i==1:
                    for d in self.devices[:2]:self.capture(d,name+'-mid-board',VIEW)
            assert self.pg(host)[7]==255 and self.pg(host)[11]==winner
            for d in self.devices[:2]:
                self.capture(d,name+'-full-board',VIEW)
                before=self.pg(host)[12];self.click(d);self.pump(.4);assert self.pg(host)[12]==before
                self.back(d);self.capture(d,name+'-back-room',ROOM);self.choose(d,4);self.capture(d,name+'-reopen-board',VIEW)
    def browse(self):
        d=self.devices[0];self.enable(d);self.capture(d,'hub',HUB)
        for row,kind in enumerate(range(10)):
            self.choose(d,row+2);self.capture(d,NAMES[kind]+'-preview',CREATE);self.choose(d,1);self.capture(d,NAMES[kind]+'-help',HELP)
            self.turn(d,7,parameter=True);self.capture(d,NAMES[kind]+'-help-next',HELP);self.back(d);self.back(d)
        self.home(d);self.capture(d,'browse-home',1)
    def games(self,kinds):
        host=self.devices[0]
        for kind in kinds:
            n=2 if kind in (9,10) else 3
            if not self.create_game(kind,n):self.end_game();continue
            if kind==9:
                for pos in (0,3,1,4,2):self.do(self.pg(host)[9],a=pos,players=2)
                assert self.pg(host)[7]==255 and self.pg(host)[11]==1
            elif kind==10:
                for pos in (19,18,17,9):self.do(self.pg(host)[9],a=pos,players=2)
            elif kind==1:
                self.do(0,1);self.do(0,a=1);self.do(1,a=0);self.do(2,a=0);assert self.pg(host)[7]==255
            elif kind==7:
                for _ in range(18):
                    if self.pg(host)[7]==2:break
                    phase=self.pg(host)[7]
                    for seat in range(n):
                        if self.pg(host)[7]!=phase:break
                        self.do(seat,1,allow_reject=True)
                assert self.pg(host)[7]==2;self.do(0,0)
                for seat in range(n):self.do(seat,0,a=0)
                assert self.pg(host)[7]==255
            elif kind==3:
                for _ in range(9):
                    if self.pg(host)[7]==255:break
                    self.do(self.pg(host)[9],0,liar_pick=True);self.do(self.pg(host)[9],1)
                assert self.pg(host)[7]==255
            elif kind==5:
                for seat in range(n):
                    self.do(seat,0);self.do(seat,1,a=0);self.do(seat,0);self.do(seat,2,a=12)
                assert self.pg(host)[13]==2
            elif kind==0:
                self.do(0,1);self.do(0,2);self.do(1,1);self.do(1,2);assert self.pg(host)[9]==2
                played=False
                for _ in range(3):
                    seat=self.pg(host)[9];cards=bytes.fromhex(host.telemetry['ptable'])[seat]
                    for card in range(cards):
                        if self.do(seat,0,a=card,allow_reject=True):played=True;break
                    if played:break
                    self.do(seat,1);self.do(seat,2)
                assert played,'No playable card found in three turns; inspect recorded hands'
            elif kind==2:
                self.do(0,a=1,b=3,c=0);self.capture(host,'code-guess-result',ROOM)
            elif kind==4:
                accepted=False
                for card in range(2):
                    for target in range(n):
                        if self.do(0,a=card,b=target,c=2,allow_reject=True):accepted=True;break
                    if accepted:break
                assert accepted
            for d in self.devices[:n]:self.capture(d,NAMES[kind]+'-result',VIEW if kind==9 else ROOM)
            if kind in (9,10):
                self.ensure_room(host);self.choose(host,4);self.capture(host,NAMES[kind]+'-board',VIEW);self.back(host)
            elif kind==5:
                self.ensure_room(host);self.choose(host,2);self.capture(host,'yacht-scores',VIEW);self.back(host)
            if kind not in (1,3,7,9,10):
                self.ensure_room(host);self.choose(host,3);self.capture(host,NAMES[kind]+'-private-gate',VIEW);self.click(host,1.8);self.capture(host,NAMES[kind]+'-own-view',VIEW);self.back(host)
            self.end_game()
    def privacy(self,kinds):
        for kind in kinds:
            assert self.create_game(kind,3)
            for d in self.devices:
                self.ensure_room(d);self.choose(d,3);self.capture(d,NAMES[kind]+'-private-gate',VIEW);self.click(d,1.8);self.capture(d,NAMES[kind]+'-own-secret',VIEW);self.back(d)
            self.end_game()
    def cleanup(self):
        self.end_game()
        for d in self.devices:
            self.hub(d)
            if self.pg(d)[2]:self.choose(d,0)
            self.home(d);self.capture(d,'final-home-mesh-off',1)
    def close(self):
        self.save()
        for d in self.devices:d.serial.close()

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--phase',choices=['browse','games','board','privacy','cleanup'],required=True);ap.add_argument('--ports',nargs='+',default=['COM9','COM10','COM15']);ap.add_argument('--kinds',nargs='+',type=int,default=list(range(10)));ap.add_argument('--tag',required=True);args=ap.parse_args()
    if args.phase in ('games','privacy'):ap.error('Legacy text controls withdrawn; use smoke_graphical_games.py or --phase board')
    lab=Lab(args.ports,args.tag)
    try:
        lab.pump(2)
        for d in lab.devices:d.send(9)
        lab.pump(.5)
        if args.phase=='browse':lab.browse()
        elif args.phase=='games':assert len(lab.devices)==3;lab.games(args.kinds)
        elif args.phase=='privacy':assert len(lab.devices)==3;lab.privacy(args.kinds)
        elif args.phase=='board':assert len(lab.devices)>=2;lab.board_games()
        else:lab.cleanup()
    finally:lab.close()
if __name__=='__main__':main()
