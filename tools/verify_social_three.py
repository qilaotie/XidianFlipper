"""Read-only private RF evidence checks; prints only synthetic test verdicts."""
import argparse,ctypes,struct,json
from pathlib import Path
from inspect_message_nvs import parse_nvs,Packet,Inbox,Outbox
ROOT=Path(__file__).resolve().parents[1]/'build/network-two-private'
class RelayEntry(ctypes.LittleEndianStructure):_fields_=[('attempts',ctypes.c_uint16),('minutes',ctypes.c_uint16),('packet',Packet)]
class Mark(ctypes.LittleEndianStructure):_fields_=[('used',ctypes.c_uint8),('cancelled',ctypes.c_uint8),('src',ctypes.c_uint8*6),('dst',ctypes.c_uint8*6),('flags',ctypes.c_uint8),('session',ctypes.c_uint32),('id',ctypes.c_uint32),('ack_session',ctypes.c_uint32),('ack_id',ctypes.c_uint32)]
class Relay(ctypes.LittleEndianStructure):_fields_=[('version',ctypes.c_uint32),('entries',RelayEntry*4),('marks',Mark*8),('cursor',ctypes.c_uint8)]
class Stamp(ctypes.LittleEndianStructure):_fields_=[('mac',ctypes.c_uint8*6),('pet',ctypes.c_char*13),('visits',ctypes.c_uint32),('token',ctypes.c_uint32)]
class Social(ctypes.LittleEndianStructure):_fields_=[('version',ctypes.c_uint32),('room',ctypes.c_uint32),('room_name',ctypes.c_char*19),('nick',ctypes.c_char*13),('sign',ctypes.c_char*25),('pet',ctypes.c_char*13),('sharing',ctypes.c_uint8),('count',ctypes.c_uint8),('stamps',Stamp*12)]
def values(tag,port):return parse_nvs((ROOT/tag/port/'nvs-9000.bin').read_bytes())
def outbox(v):return [Outbox.from_buffer_copy(v[('msg_outbox','outbox_v1')],4+i*ctypes.sizeof(Outbox)) for i in range(6)]
def relay(v):return Relay.from_buffer_copy(v[('relay_mail','cache_v1')])
def inbox(v,text):return [Inbox.from_buffer_copy(b,4) for (ns,k),b in v.items() if ns=='messages_v3' and k.startswith('m') and isinstance(b,bytes) and len(b)==160 and bytes(Inbox.from_buffer_copy(b,4).text).split(b'\0',1)[0]==text]
def identity(p):return bytes(p.src),bytes(p.dst),p.session,p.id
def verify_relay():
 a=values('social-stashed','COM10');b=values('social-stashed','COM15');waiting=outbox(a)[0];assert waiting.state==1 and waiting.packet.flags&0x20
 cached=[e for e in relay(b).entries if e.minutes];assert len(cached)==1 and identity(cached[0].packet)==identity(waiting.packet)
 assert cached[0].packet.flags&12==4,'cached packet is the next link, not an unbounded hop reset'
 c=values('social-delivered','COM9');b=values('social-delivered','COM15');received=inbox(c,b'QM01');assert len(received)==1
 assert (bytes(received[0].src),received[0].session,received[0].id)==(bytes(waiting.packet.src),waiting.packet.session,waiting.packet.id)
 assert not any(e.minutes for e in relay(b).entries);assert any(m.used and not m.cancelled and m.session==waiting.packet.session and m.id==waiting.packet.id for m in relay(b).marks)
 a=values('social-receipt','COM10');delivered=outbox(a)[0];assert delivered.state==2 and identity(delivered.packet)==identity(waiting.packet)
 print('PASS RF custody: B durably cached while C stopped; B rebooted; source A stopped during B->C delivery; C stopped while stored real ACK reached returning A; same identity and exactly one inbox entry')
def verify_final(tag):
 for p in ('COM9','COM10','COM15'):
  before=values('social-before',p);after=values(tag,p)
  for key,value in before.items():
   if key[0]=='friends_v3' or (key[0]=='messages_v3' and key[1].startswith('m')) or key[0]=='msg_outbox':assert after.get(key)==value,(p,'original entry changed',key)
  a=json.loads((ROOT/'social-before'/p/'state-private.json').read_text());b=json.loads((ROOT/tag/p/'state-private.json').read_text());assert a['work']==b['work'] and a['friends']==b['friends'] and a['channel']==b['channel']
  default=bytes([1])+bytes(903);old=before.get(('msg_drafts','drafts_v1'),default);new=after.get(('msg_drafts','drafts_v1'),default);assert old[130:]==new[130:],(p,'saved draft slots')
  second=ROOT/tag/p/'nvs-3b0000.bin';assert parse_nvs(second.read_bytes())==parse_nvs((ROOT/'social-before'/p/'nvs-3b0000.bin').read_bytes()),(p,'secondary NVS')
  if ('social_user','social_v1') in after:
   s=Social.from_buffer_copy(after[('social_user','social_v1')]);assert not s.sharing and not s.nick and not s.sign
  print('PASS',p,'original messages/friends/work/saved slots/outbox/channel preserved')
 assert len(inbox(values(tag,'COM9'),b'QM01'))==1 and not inbox(values(tag,'COM9'),b'ROOM')
 assert len(inbox(values(tag,'COM15'),b'ROOM'))==1
 print('PASS room isolation and synthetic test cleanup; only intended new receiver messages remain')
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--final-tag');args=ap.parse_args();verify_relay()
 if args.final_tag:verify_final(args.final_tag)
