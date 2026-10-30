"""Read-only private NVS evidence verification. Never print user drafts/messages."""
import argparse,struct
from pathlib import Path
from inspect_message_nvs import parse_nvs
ROOT=Path(__file__).resolve().parents[1]
def history(data):
 version,sequence,count,nxt=struct.unpack_from('<IIBB',data)
 assert version==1 and count<=12 and nxt<12 and len(data)==1020
 out=[]
 for i in range(count):
  at=12+((nxt+11-i)%12)*84
  boot,id,seq,uptime=struct.unpack_from('<IIII',data,at)
  out.append(dict(boot=boot,id=id,sequence=seq,kind=data[at+16],channel=data[at+17],host=data[at+19:at+25],peer=data[at+25:at+31],values=list(data[at+31:at+35]),title=data[at+35:at+84].split(b'\0',1)[0]))
 return out
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--after',default='crowd-final-after');ap.add_argument('--new-round',action='store_true');args=ap.parse_args()
 base=ROOT/'build/network-two-private';results={}
 for p in ('COM9','COM10'):
  for name in ('nvs-9000.bin','nvs-3b0000.bin'):
   before=parse_nvs((base/'crowd-live-after'/p/name).read_bytes());after=parse_nvs((base/args.after/p/name).read_bytes())
   for key,value in before.items():
    if key[0] in ('msg_drafts','msg_outbox','messages_v3') or (key[0]=='activity_log' and not args.new_round) or name=='nvs-3b0000.bin':assert after.get(key)==value,(p,'unexpected save change',key)
   if name=='nvs-9000.bin':
    rows=history(after[('activity_log','history_v1')]);check=[r for r in rows if r['kind']==3 and r['title']==b'MEET'];quiz=[r for r in rows if r['kind']==4 and r['title']==b'MEET']
    assert check and check[-1]['values']==[1,0,0,0],(p,'check-in result')
    assert quiz and quiz[-1]['peer']==bytes.fromhex('ec5d9ffffee0'),(p,'buzzer winner')
    if args.new_round:
     old=history(before[('activity_log','history_v1')]);assert all(r in rows for r in old),(p,'old history retained')
     newer=[r for r in rows if r['kind']==4 and r['title']==b'TEST'];assert newer and newer[0]['peer']==bytes.fromhex('f6b19ffffee5'),(p,'final firmware round winner')
     print('PASS',p,'final firmware RF round winner=COM9; new record persisted; previous records retained')
    results[p]=(check[-1],quiz[-1]);print('PASS',p,'check-in arrived=1; quiz winner=COM10; results retained; drafts/messages/outbox preserved')
 for a,b in zip(results['COM9'],results['COM10']):assert (a['host'],a['boot'],a['id'],a['kind'])==(b['host'],b['boot'],b['id'],b['kind'])
 print('PASS both endpoints saved matching host/session/activity identities')
if __name__=='__main__':main()
