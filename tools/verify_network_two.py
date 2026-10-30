"""Validate private, read-only NVS evidence from native two-device RF phases."""
import json
from pathlib import Path
from inspect_message_nvs import parse_nvs
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/network-two-private'
def state(tag,port):return json.loads((BASE/tag/port/'state-private.json').read_text())
def records(s):return {m['local_id']:(m['src'],m['session'],m['id'],m['channel'],m['sha256']) for m in s['inbox']}
def run():
 before={p:state('before',p) for p in ('COM9','COM10')}
 group=state('after_group','COM9');cross=state('after_cross','COM9');received=state('recovered','COM9')
 assert len(group['inbox'])==len(before['COM9']['inbox'])+1
 assert sum(m.get('test_text')=='QG01' and m['channel']==1 for m in group['inbox'])==1
 assert cross['channel']==2 and sum(m.get('test_text')=='[ALL] QA01' and m['channel']==0 for m in cross['inbox'])==1
 assert not any(m.get('test_text')=='QG02' for m in received['inbox'])
 queued=state('queued','COM10')['outbox'][0];waiting=state('receiver_offline','COM10')['outbox'][0];delivered=state('recovered','COM10')['outbox'][0]
 assert queued['state']==1 and queued['rounds']==0 and queued['body']=='QO01'
 assert waiting['state']==1 and waiting['rounds']>0 and delivered['state']==2 and delivered['rounds']>=waiting['rounds']
 assert (queued['session'],queued['id'])==(waiting['session'],waiting['id'])==(delivered['session'],delivered['id'])
 assert delivered['src']==before['COM9']['friends'][0] and delivered['dst']==before['COM10']['friends'][0]
 messages=[m for m in received['inbox'] if m.get('test_text')=='QO01'];assert len(messages)==1
 assert (messages[0]['src'],messages[0]['session'],messages[0]['id'])==(delivered['src'],delivered['session'],delivered['id'])
 for port in ('COM9','COM10'):
  after=state('after_cleanup',port);original=before[port]
  assert after['channel']==original['channel'] and after['work']==original['work']
  assert not any(e['state'] for e in after['outbox'])
  assert all(records(after).get(k)==v for k,v in records(original).items())
  a=parse_nvs((BASE/'before'/port/'nvs-9000.bin').read_bytes());b=parse_nvs((BASE/'after_cleanup'/port/'nvs-9000.bin').read_bytes())
  assert {k:v for k,v in a.items() if k[0]=='friends_v3'}=={k:v for k,v in b.items() if k[0]=='friends_v3'}
  def saved(v):
   blob=v.get(('msg_drafts','drafts_v1'));return blob[130:] if blob else bytes(6*129)
  assert saved(a)==saved(b),'Saved draft slots changed'
  for k,v in a.items():
   if k[0]=='messages_v3' and k[1].startswith('m') and len(k[1])==3:assert b[k]==v,'Original inbox record changed'
 assert len(state('after_cleanup','COM9')['inbox'])==len(before['COM9']['inbox'])+3
 assert len(state('after_cleanup','COM10')['inbox'])==len(before['COM10']['inbox'])
 print('PASS real RF evidence: same-channel group, channel isolation, cross-channel announcement, offline WAIT -> target receipt -> persisted DELIVERED')
 print('PASS cleanup: original channels/work/saved drafts/friends/inbox retained, test outbox removed; only 3 new inbox messages on COM9')
 print('BOUNDARY: 2 physical devices; this does NOT validate an A-B-C relay path or abrupt power-loss atomicity')
if __name__=='__main__':run()
