"""Persistent offline-message production store with simulated NVS faults."""
from pathlib import Path
import re,subprocess,tempfile
from test_draft_store import shim
root=Path(__file__).resolve().parents[1]
shim=shim.replace('draft_store.h','message_outbox.h').replace('msg_drafts','msg_outbox').replace('drafts_v1','outbox_v1')
source=re.sub(r'^#include[^\n]*\n','',(root/'firmware/app/message_outbox.c').read_text(encoding='utf8'),flags=re.M)
tests=r'''
static const uint8_t me[6]={2,0,0,0,0,1},peer[6]={2,0,0,0,0,2},wrong[6]={2,0,0,0,0,3};
static mesh_packet_t make(unsigned id){mesh_packet_t p;assert(mesh_packet_pack(&p,me,peer,MESH_TYPE_QUICK_MSG,(const uint8_t *)"Hi (^_^)",8));p.session_id=42;p.msg_id=id;p.flags=MESH_FLAG_ACK_REQUEST|MESH_FLAG_ROUTED;p.crc16=mesh_packet_compute_crc(&p);return p;}
int main(void){
 outbox_entry_t e;mesh_packet_t p,q;
 assert(message_outbox_get(0,&e)&&!e.state&&!exists&&!writes);
 for(unsigned i=0;i<6;i++){p=make(i+1);assert(message_outbox_add(&p,0)==(int)i);}
 unsigned before=writes;p=make(100);assert(message_outbox_add(&p,0)<0&&writes==before);
 for(unsigned i=0;i<6;i++){assert(message_outbox_take(&q,i*10000)==(int)i);assert(q.msg_id==i+1);assert(message_outbox_take(&q,i*10000+1)<0);}
 loaded=false;assert(message_outbox_get(0,&e)&&e.rounds==1&&e.packet.session_id==42);
 assert(message_outbox_take(&q,29999)<0);assert(message_outbox_take(&q,30000)==0&&q.msg_id==1);
 assert(!message_outbox_ack(wrong,42,1)&&!message_outbox_ack(peer,43,1));
 fail_commit=true;assert(!message_outbox_ack(peer,42,1));assert(message_outbox_get(0,&e)&&e.state==OUTBOX_WAITING);
 fail_commit=false;assert(message_outbox_ack(peer,42,1));loaded=false;assert(message_outbox_get(0,&e)&&e.state==OUTBOX_DELIVERED);
 assert(!message_outbox_retry(0,0));assert(message_outbox_remove(0));assert(!message_outbox_ack(peer,42,1));
 for(unsigned i=1;i<6;i++)assert(message_outbox_remove(i));
 p=make(200);memset(p.payload,'A',128);p.len=128;p.crc16=mesh_packet_compute_crc(&p);assert(message_outbox_add(&p,0)==0);
 loaded=false;assert(message_outbox_get(0,&e)&&e.packet.len==128&&!memcmp(e.packet.payload,p.payload,128));
 fail_set=true;assert(message_outbox_take(&q,30000)<0);assert(message_outbox_get(0,&e)&&e.rounds==0);fail_set=false;
 for(unsigned i=0;i<120;i++)assert(message_outbox_take(&q,60000+i*30000)==0);
 assert(message_outbox_take(&q,60000+120*30000)<0);assert(message_outbox_get(0,&e)&&e.state==OUTBOX_HELD&&e.rounds==120);
 loaded=false;assert(message_outbox_get(0,&e)&&e.state==OUTBOX_HELD);assert(message_outbox_retry(0,0));assert(message_outbox_get(0,&e)&&!e.rounds&&e.packet.msg_id==200);
 assert(message_outbox_remove(0));p=make(201);p.payload[0]='\n';p.crc16=mesh_packet_compute_crc(&p);assert(message_outbox_add(&p,0)<0);
 before=writes;disk[0]=99;loaded=false;assert(message_outbox_get(0,&e)&&message_outbox_error());p=make(202);assert(message_outbox_add(&p,0)<0&&writes==before&&disk[0]==99);
 puts("PASS outbox: 6 slots, no eviction, fair retries/rate, reboot stable IDs, exact peer ACK, failed writes/ACK, hold120/manual retry, cancellation and corrupt-store preservation");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-outbox-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(shim+source+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-fno-stack-protector','-I',str(root/'firmware/app'),'-I',str(root/'firmware/protocol'),str(p/'test.c'),str(root/'firmware/protocol/mesh_packet.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
