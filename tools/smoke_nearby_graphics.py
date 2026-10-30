"""Real RPS hand reveal and radar screens, using only native controls."""
import argparse
from smoke_tabletop import Lab
from menu_layout import menu_index

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--ports',nargs='+',default=['COM9','COM10']);ap.add_argument('--tag',required=True);a=ap.parse_args();lab=Lab(a.ports,a.tag)
    def enter(d,row,page):
        lab.home(d);lab.click(d);lab.turn(d,4);lab.click(d);lab.turn(d,menu_index('FUN','M_FUN_NEARBY'));lab.click(d);lab.choose(d,row);lab.wait(lambda:lab.page(d)==page)
    try:
        lab.pump(2)
        for d in lab.devices:d.send(9)
        lab.pump(.5);host,peer=lab.devices[:2]
        for d in (host,peer):enter(d,0,40)
        lab.pump(8)
        for d in (host,peer):lab.capture(d,'rps-hand-picker',40)
        lab.click(host);lab.turn(peer,1,parameter=True);lab.click(peer);lab.pump(3)
        for d in (host,peer):lab.capture(d,'rps-revealed-result',40)
        for d in (host,peer):enter(d,1,48)
        lab.turn(host,1,parameter=True);lab.pump(3)
        lab.capture(host,'radar-hiding-chest',48);lab.capture(peer,'radar-live-signal',48)
        lab.click(peer);lab.capture(peer,'radar-target-locked',48);lab.back(host);lab.pump(4);lab.capture(peer,'radar-target-lost',48)
        for d in (host,peer):lab.home(d);lab.capture(d,'final-home',1)
    finally:lab.close()

if __name__=='__main__':main()
