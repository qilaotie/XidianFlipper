"""Restored tabletop native controls; no RF/card/NVS injection."""
import argparse
from smoke_tabletop import Lab,ROOM,VIEW,ACTION

class RestoredLab(Lab):
    def channel(self,d,delta,name):
        for _ in range(20):
            if self.page(d)==1:break
            d.send(4);self.pump(.2);d.send(5);self.pump(.55)
        assert self.page(d)==1
        self.nodes[d.port]=[[1,0]];self.click(d);self.click(d);self.turn(d,3);self.click(d);assert self.page(d)==26
        self.turn(d,delta,parameter=True);self.capture(d,name+'-selected',26);self.click(d);self.wait(lambda:self.page(d)==19);self.capture(d,name+'-saved',19)
    def submit(self,seat,which=0,a=0,b=0,c=None,players=3,allow_reject=False,liar_pick=False):
        d=self.devices[seat];host=self.devices[0];self.sync(players);self.ensure_room(d);self.choose(d,5)
        for _ in range(which):self.choose(d,0)
        kind=self.pg(d)[6];phase=self.pg(d)[7]
        if liar_pick:
            nh=bytes.fromhex(host.telemetry['ptable'])[seat]
            self.choose(d,1);self.click(d,1.8);self.click(d);self.turn(d,nh,parameter=True);self.click(d)
            assert self.page(d)==ACTION
        elif a:
            self.choose(d,1);self.turn(d,a,parameter=True);self.click(d)
        if b:self.choose(d,2);self.turn(d,b,parameter=True);self.click(d)
        if kind==2 and c:self.choose(d,3);self.turn(d,c,parameter=True);self.click(d)
        fields=3 if kind==2 and which==0 else 0 if kind==2 else 1 if kind in (1,3) and which==0 else 0 if kind in (1,3) else 1 if kind==7 and phase==3 else 0
        confirm=3 if kind==4 else fields+1
        before=self.pg(host)[12];self.choose(d,confirm);self.pump(1);self.wait(lambda:not self.pg(d)[4],15)
        if kind==4 and self.pg(host)[12]==before:
            # Guard's extra guess field occupies the first candidate confirm row.
            d.send(4);self.pump(.2);d.send(5);self.pump(.55)
            if self.page(d)==ACTION:self.choose(d,4);self.pump(.5);self.wait(lambda:not self.pg(d)[4],15)
            else:
                while self.nodes[d.port][-1][0]!=self.page(d):self.nodes[d.port].pop()
        self.sync(players);changed=self.pg(host)[12]>before
        if not allow_reject:assert changed and self.pg(d)[5]==2,(kind,seat,which,d.telemetry)
        self.ensure_room(d)
        return changed
    def private(self,d,name):
        self.ensure_room(d);self.choose(d,3);self.capture(d,name+'-gate',VIEW);self.click(d,1.8);self.capture(d,name+'-private',VIEW);self.back(d)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--ports',nargs='+',default=['COM9','COM10','COM15']);ap.add_argument('--tag',required=True);ap.add_argument('--resume-social',action='store_true');args=ap.parse_args();lab=RestoredLab(args.ports,args.tag)
    try:
        lab.pump(2)
        for d in lab.devices:d.send(9)
        lab.pump(.5);host=lab.devices[0];lab.channel(lab.devices[1],-1,'test-channel-1')
        if not args.resume_social:
            assert lab.create_game(2,3)
            lab.private(host,'code-own-cards');lab.ensure_room(host);lab.choose(host,4);lab.turn(host,3,parameter=True);lab.capture(host,'code-selected-opponent-tile',VIEW);lab.click(host)
            assert lab.page(host)==ACTION;lab.capture(host,'code-prefilled-target-and-tile',ACTION)
            before=lab.pg(host)[12];lab.choose(host,4);lab.wait(lambda:lab.pg(host)[12]>before);lab.sync(3);lab.capture(host,'code-guess-accepted',ACTION)
            assert lab.create_game(4,3)
            for d in lab.devices:lab.private(d,'love-hand')
            accepted=False
            for card in range(2):
                for target in range(3):
                    if lab.submit(0,a=card,b=target,allow_reject=True):accepted=True;break
                if accepted:break
            assert accepted
            lab.capture(host,'love-effect-accepted',ROOM)
        assert lab.create_game(3,3)
        for d in (lab.devices[:1] if args.resume_social else lab.devices):lab.private(d,'liar-hand')
        lab.submit(lab.pg(host)[9],liar_pick=True);lab.capture(host,'liar-selected-card-played',ROOM)
        lab.submit(lab.pg(host)[9],1);lab.capture(host,'liar-challenge-resolved',ROOM)
        assert lab.create_game(1,3)
        for d in lab.devices:lab.private(d,'undercover-word')
        lab.choose(host,4);lab.capture(host,'undercover-public-players',VIEW);lab.click(host);lab.wait(lambda:lab.pg(host)[7]==3);lab.sync(3)
        for seat,target in ((0,1),(1,0),(2,0)):
            d=lab.devices[seat]
            if lab.page(d)!=VIEW:lab.ensure_room(d);lab.choose(d,4)
            lab.turn(d,target,parameter=True);before=lab.pg(host)[12];lab.click(d);lab.wait(lambda:lab.pg(host)[12]>before);lab.sync(3)
        assert lab.pg(host)[7]==255
        lab.capture(host,'undercover-finished-players',VIEW);lab.click(host);lab.capture(host,'undercover-public-end-roles',VIEW)
        assert lab.create_game(7,3)
        for d in lab.devices:lab.private(d,'one-night-role')
        for _ in range(18):
            if lab.pg(host)[7]==2:break
            phase=lab.pg(host)[7]
            for seat in range(3):
                if lab.pg(host)[7]!=phase:break
                lab.submit(seat,1,allow_reject=True)
        assert lab.pg(host)[7]==2
        lab.submit(0,0)
        for seat in range(3):lab.submit(seat,0,a=0)
        assert lab.pg(host)[7]==255
        lab.capture(host,'one-night-complete',ROOM)
        for kind in (6,8):assert not lab.create_game(kind,3)
        lab.cleanup()
    finally:
        try:lab.channel(lab.devices[1],1,'restore-channel-2')
        finally:lab.close()

if __name__=='__main__':main()
