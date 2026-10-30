"""Read-only, identity-checked NVS snapshots for RF acceptance. Never write the device.
Private raw dumps and working draft are kept under ignored build/, never published.
Uses bundled ESP-IDF's NVS parser, checks entry/chunk CRCs and reads each area twice.
"""
import argparse,ctypes,hashlib,json,sys
from pathlib import Path
import esptool
SDK=Path('C:/esp/v5.5.4/esp-idf/components/nvs_flash/nvs_partition_tool')
sys.path.insert(0,str(SDK))
from nvs_parser import NVS_Partition
ROOT=Path(__file__).resolve().parents[1]

class Packet(ctypes.LittleEndianStructure):
 _fields_=[('magic',ctypes.c_uint16),('version',ctypes.c_uint8),('src',ctypes.c_uint8*6),('dst',ctypes.c_uint8*6),('type',ctypes.c_uint8),('length',ctypes.c_uint8),('flags',ctypes.c_uint8),('channel',ctypes.c_uint8),('session',ctypes.c_uint32),('id',ctypes.c_uint32),('payload',ctypes.c_uint8*128),('crc',ctypes.c_uint16)]
class Inbox(ctypes.LittleEndianStructure):
 _fields_=[('local_id',ctypes.c_uint32),('src',ctypes.c_uint8*6),('session',ctypes.c_uint32),('id',ctypes.c_uint32),('channel',ctypes.c_uint8),('kind',ctypes.c_uint8),('length',ctypes.c_uint16),('rssi',ctypes.c_int8),('unread',ctypes.c_bool),('text',ctypes.c_uint8*129)]
class Outbox(ctypes.LittleEndianStructure):
 _fields_=[('state',ctypes.c_uint8),('rounds',ctypes.c_uint16),('packet',Packet)]
def mac(b):return ':'.join(f'{v:02x}' for v in b)
def parse_nvs(raw):
 part=NVS_Partition('private',bytearray(raw));entries=[]
 for page in sorted(part.pages,key=lambda p:p.header['page_index']):
  if page.is_empty:continue
  for e in page.entries:
   if e.state!='Written':continue
   assert e.metadata['crc']['original']==e.metadata['crc']['computed'],'NVS entry CRC mismatch'
   entries.append(e)
 names={e.data['value']:e.key for e in entries if e.metadata['namespace']==0 and e.metadata['type']=='uint8_t'}
 chunks={};values={}
 for e in entries:
  ns=names.get(e.metadata['namespace']);kind=e.metadata['type']
  if not ns:continue
  if kind in ('blob_data','blob','string'):
   assert e.data['crc']==e.metadata['crc']['data_computed'],'NVS chunk CRC mismatch'
   data=b''.join(bytes(child.raw) for child in e.children)[:e.data['size']]
   chunks[(ns,e.key,e.metadata['chunk_index'])]=data
   if kind!='blob_data':values[(ns,e.key)]=data
  elif kind!='blob_index':values[(ns,e.key)]=e.data['value']
 for e in entries:
  if e.metadata['type']!='blob_index':continue
  ns=names.get(e.metadata['namespace'])
  if not ns:continue
  data=b''.join(chunks[(ns,e.key,(e.data['chunk_start']+i)&255)] for i in range(e.data['chunk_count']))
  assert len(data)==e.data['size'];values[(ns,e.key)]=data
 return values
def summarize(raw,port,identity):
 values=parse_nvs(raw);friends=[];inbox=[];outbox=[]
 for (ns,key),v in sorted(values.items()):
  if ns=='friends_v3' and key.startswith('f') and isinstance(v,bytes):
   assert len(v)==32 and v[0]==1
   if v[1]==3:friends.append(mac(v[2:8]))
  if ns=='messages_v3' and key.startswith('m') and len(key)==3 and isinstance(v,bytes):
   assert len(v)==4+ctypes.sizeof(Inbox) and int.from_bytes(v[:4],'little')==1
   entry=Inbox.from_buffer_copy(v[4:]);body=bytes(entry.text[:entry.length])
   item=dict(local_id=entry.local_id,src=mac(entry.src),session=entry.session,id=entry.id,channel=entry.channel,sha256=hashlib.sha256(body).hexdigest())
   if body in (b'QG01',b'QG02',b'[ALL] QA01',b'QO01'):item['test_text']=body.decode()
   inbox.append(item)
 draft=values.get(('msg_drafts','drafts_v1'))
 work=draft[1:130].split(b'\0',1)[0].decode('ascii') if draft else ''
 saved=values.get(('msg_outbox','outbox_v1'))
 if saved:
  assert len(saved)==4+6*ctypes.sizeof(Outbox) and int.from_bytes(saved[:4],'little')==1
  for i in range(6):
   entry=Outbox.from_buffer_copy(saved,4+i*ctypes.sizeof(Outbox));p=entry.packet
   outbox.append(dict(slot=i,state=entry.state,rounds=entry.rounds,src=mac(p.src),dst=mac(p.dst),session=p.session,id=p.id,body=bytes(p.payload[:p.length]).decode('ascii') if entry.state else ''))
 return dict(port=port,mac=identity,channel=values.get(('messages_v3','channel'),1),friends=friends,work=work,inbox=inbox,outbox=outbox)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--port',required=True);ap.add_argument('--expect-mac',required=True);ap.add_argument('--tag',required=True);args=ap.parse_args()
 assert args.tag.replace('-','').replace('_','').isalnum()
 folder=ROOT/'build/network-two-private'/args.tag/args.port;folder.mkdir(parents=True,exist_ok=False)
 esp=esptool.detect_chip(port=args.port)
 try:
  identity=mac(esp.read_mac());assert identity.lower()==args.expect_mac.lower() and esp.CHIP_NAME=='ESP32-C6'
  esp=esp.run_stub();esp.flash_spi_attach(0);esp.change_baud(230400)
  parts={}
  for address in (0x9000,0x3b0000):
   raw=esp.read_flash(address,0x6000);repeat=esp.read_flash(address,0x6000);assert raw==repeat and len(raw)==0x6000
   (folder/f'nvs-{address:x}.bin').write_bytes(raw);parts[address]=raw
  summary=summarize(parts[0x9000],args.port,identity)
  (folder/'state-private.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf8')
  print(json.dumps(dict(port=args.port,channel=summary['channel'],friends=summary['friends'],work_length=len(summary['work']),inbox_count=len(summary['inbox']),outbox_used=sum(e['state']!=0 for e in summary['outbox']))),flush=True)
 finally:esp.hard_reset();esp._port.close()
if __name__=='__main__':main()
