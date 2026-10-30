"""Real button/encoder graphics checks. No card/state/packet injection."""
import argparse
from smoke_tabletop import Lab,VIEW

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--ports',nargs='+',default=['COM9','COM10']);ap.add_argument('--tag',required=True);a=ap.parse_args()
    lab=Lab(a.ports,a.tag)
    try:
        lab.pump(2)
        for d in lab.devices:d.send(9)
        lab.pump(.5);host,peer=lab.devices[:2]
        assert lab.create_game(0,2)
        picks={d.port:0 for d in lab.devices}
        def nh(seat):return bytes.fromhex(host.telemetry['ptable'])[seat]
        def tap(seat,target,accept=True):
            d=lab.devices[seat];lab.sync(2);lab.turn(d,target-picks[d.port],parameter=True);picks[d.port]=target
            before=lab.pg(host)[12];lab.click(d);lab.pump(1)
            lab.wait(lambda:not lab.pg(d)[4],15);lab.sync(2)
            changed=lab.pg(host)[12]>before
            if accept:assert changed and lab.pg(d)[5]==2,d.telemetry
            picks[d.port]%=nh(seat)+4
            return changed
        # Own hand begins face down; revealing is a separate long press.
        lab.capture(host,'uno-private-backs',VIEW);lab.click(host,1.8);lab.capture(host,'uno-own-hand-private',VIEW)
        tap(0,nh(0));lab.capture(host,'uno-after-draw',VIEW);tap(0,nh(0)+1);assert lab.pg(host)[9]==1
        lab.click(peer,1.8);played=False
        for turn in range(3):
            seat=lab.pg(host)[9];d=lab.devices[seat]
            for card in range(nh(seat)):
                if tap(seat,card,False):played=True;break
                # A wild opens a local suit picker without transmitting yet.
                before=lab.pg(host)[12];lab.click(d);lab.pump(1);lab.wait(lambda:not lab.pg(d)[4]);lab.sync(2)
                if lab.pg(host)[12]>before:played=True;break
            if played:break
            tap(seat,nh(seat));tap(seat,nh(seat)+1)
        assert played,'No legal card in three turns; inspect actual hands'
        for d in lab.devices[:2]:lab.capture(d,'uno-card-played-private',VIEW)
        assert lab.create_game(5,2)
        picks={d.port:0 for d in lab.devices}
        def yacht(seat,target):
            d=lab.devices[seat];lab.sync(2);lab.turn(d,target-picks[d.port],parameter=True);picks[d.port]=target
            before=lab.pg(host)[12];lab.click(d);lab.wait(lambda:lab.pg(host)[12]>before);lab.sync(2)
        for seat in range(2):
            yacht(seat,5);lab.capture(lab.devices[seat],'yacht-five-dice',VIEW)
            yacht(seat,0);old=bytes.fromhex(host.telemetry['ptable'])[0];yacht(seat,5);assert bytes.fromhex(host.telemetry['ptable'])[0]==old
            lab.capture(lab.devices[seat],'yacht-kept-and-rerolled',VIEW)
            d=lab.devices[seat];lab.turn(d,6-picks[d.port],parameter=True);picks[d.port]=6;lab.click(d);lab.turn(d,12,parameter=True);lab.capture(d,'yacht-score-preview',VIEW)
            before=lab.pg(host)[12];lab.click(d);picks[d.port]=5;lab.wait(lambda:lab.pg(host)[12]>before);lab.sync(2)
        assert lab.pg(host)[9]==0 and lab.pg(host)[13]>=1
        for d in lab.devices[:2]:lab.capture(d,'yacht-round-complete',VIEW)
        lab.cleanup()
    finally:lab.close()

if __name__=='__main__':main()
