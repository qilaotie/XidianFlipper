"""Normal USB UI only: new-page boot checks, optional probes, gift preview CANCEL.
Never accepts a gift, confirms escrow, changes channel, or edits a user draft.
"""
import argparse
from smoke_tabletop import Lab
from menu_layout import menu_index

def tools_page(lab,d,target):
    lab.home(d);lab.click(d);lab.turn(d,5);lab.click(d);assert lab.page(d)==41
    lab.turn(d,menu_index('TOOLS','M_TOOLS_DIAGNOSTIC'));lab.click(d);assert lab.page(d)==103
    lab.turn(d,menu_index('TOOLS_DIAGNOSTIC',target));lab.click(d)

def gifts_page(lab,d):
    lab.home(d);lab.click(d);lab.turn(d,4);lab.click(d);assert lab.page(d)==23
    lab.turn(d,menu_index('FUN','M_FUN_PET'));lab.click(d);assert lab.page(d)==116
    lab.turn(d,menu_index('FUN_PET','M_PET_GIFTS'));lab.click(d);assert lab.page(d)==123

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--ports',nargs='+',required=True);ap.add_argument('--tag',required=True);ap.add_argument('--rf-pair',nargs=2,help='Only use when these boards were verified in the same channel/room');a=ap.parse_args()
    lab=Lab(a.ports,a.tag);original={}
    try:
        lab.pump(3)
        for d in lab.devices:d.send(9)
        lab.pump(.6)
        for d in lab.devices:
            original[d.port]=bool(lab.pg(d)[2]);lab.home(d)
            tools_page(lab,d,'M_NETWORK_LAB');lab.capture(d,'experiment-page',122)
            tools_page(lab,d,'M_NETWORK_TOPOLOGY');lab.capture(d,'topology-page',121)
            gifts_page(lab,d);lab.choose(d,3);lab.nodes[d.port][-1][1]=0;lab.capture(d,'gift-preview-default-cancel',123);lab.choose(d,0);lab.capture(d,'gift-preview-cancelled',123);lab.home(d)
        if a.rf_pair:
            pair=[next(d for d in lab.devices if d.port==p) for p in a.rf_pair]
            for d in pair:lab.enable(d)
            lab.pump(6);host,peer=pair
            tools_page(lab,host,'M_NETWORK_LAB');lab.choose(host,7);lab.capture(host,'probe-neighbors-check',122)
            lab.choose(host,8);lab.choose(host,2);lab.capture(host,'probe-target-check',122)
            lab.choose(host,6);lab.pump(20)
            for row,name in ((3,'probe-ack-result'),(4,'probe-attempt-result'),(5,'probe-rtt-result')):
                lab.turn(host,row-lab.nodes[host.port][-1][1]);lab.pump(2);lab.capture(host,name,122)
            for d in pair:gifts_page(lab,d)
            lab.pump(4)
            for d in pair:
                lab.choose(d,7);lab.turn(d,10-lab.nodes[d.port][-1][1]);lab.capture(d,'gift-peer-discovery',123);lab.click(d)
                lab.choose(d,3);lab.nodes[d.port][-1][1]=0;lab.capture(d,'gift-peer-preview-cancel-only',123);lab.choose(d,0);lab.home(d)
        for d in lab.devices:
            lab.hub(d)
            if bool(lab.pg(d)[2])!=original[d.port]:lab.choose(d,0)
            lab.home(d);lab.capture(d,'final-home',1)
            assert not any(x in d.log for x in (b'task_wdt:',b'Rebooting...',b'assert failed',b'Guru Meditation'))
        print('PASS UI/probe capture; visually inspect probe target and counters. No gift settled.',flush=True)
    finally:lab.close()

if __name__=='__main__':main()
