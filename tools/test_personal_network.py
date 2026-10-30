"""Production social/custody stores and protocol paths with faulting fake NVS.
Host evidence only, not RF or hardware power-loss proof.
"""
from pathlib import Path
import re,subprocess,tempfile
from test_draft_store import shim
root=Path(__file__).resolve().parents[1]
relay=r'''
static void p32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
int main(void){
 uint8_t a[6]={2,0,0,0,0,1},b[6]={2,0,0,0,0,2},c[6]={2,0,0,0,0,3};mesh_packet_t p,out;
 assert(mesh_packet_pack(&p,a,c,MESH_TYPE_QUICK_MSG,(uint8_t *)"mail",4));p.flags=MESH_FLAG_ROUTED|MESH_FLAG_ACK_REQUEST|MESH_FLAG_CUSTODY;p.session_id=42;p.msg_id=1;p.crc16=mesh_packet_compute_crc(&p);
 relay_mail_enable(true,0);relay_mail_observe(&p,b,0);relay_mail_view_t v;relay_mail_view(&v);assert(v.count==1);unsigned old=writes;relay_mail_observe(&p,b,1);assert(writes==old);
 fail_commit=true;assert(!relay_mail_take(&out,3000));relay_mail_entry_t entry;relay_mail_get(3,&entry);assert(entry.attempts==0);fail_commit=false;assert(relay_mail_take(&out,33000));assert(out.src_mac[5]==1&&out.dst_mac[5]==3&&((out.flags&MESH_HOP_MASK)>>2)==1);
 /* Restart the custodian; sender is no longer needed for the cached retry. */
 loaded=false;disk=(relay_disk_t){.version=1};taken=false;relay_mail_enable(true,34000);relay_mail_view(&v);assert(v.count==1);assert(relay_mail_take(&out,38000));assert(out.session_id==42&&out.msg_id==1);
 uint8_t ack[8];p32(ack,42);p32(ack+4,1);mesh_packet_t receipt;assert(mesh_packet_pack(&receipt,c,a,MESH_TYPE_RECEIPT,ack,8));receipt.flags=MESH_FLAG_ROUTED;receipt.session_id=99;receipt.msg_id=1;receipt.crc16=mesh_packet_compute_crc(&receipt);
 relay_mail_observe(&receipt,b,39000);relay_mail_view(&v);assert(v.count==0);old=writes;relay_mail_observe(&p,b,40000);assert(writes==old);assert(relay_mail_take(&out,50000)&&out.type==MESH_TYPE_RECEIPT&&out.session_id==99&&out.msg_id==1&&!memcmp(out.payload,ack,8));
 p.msg_id=2;p.crc16=mesh_packet_compute_crc(&p);relay_mail_observe(&p,b,51000);uint8_t dst[6]=MESH_DST_BROADCAST_MAC,body[14];memcpy(body,c,6);p32(body+6,42);p32(body+10,2);mesh_packet_t cancel;assert(mesh_packet_pack(&cancel,a,dst,MESH_TYPE_CUSTODY_CANCEL,body,14));cancel.flags=MESH_FLAG_ROUTED;cancel.session_id=42;cancel.msg_id=3;cancel.crc16=mesh_packet_compute_crc(&cancel);relay_mail_observe(&cancel,b,52000);assert(relay_mail_cancelled(&p));relay_mail_observe(&p,b,53000);relay_mail_view(&v);assert(v.count==0);
 for(unsigned i=3;i<7;i++){p.msg_id=i;p.crc16=mesh_packet_compute_crc(&p);relay_mail_observe(&p,b,54000);}relay_mail_view(&v);assert(v.count==4);p.msg_id=7;p.crc16=mesh_packet_compute_crc(&p);relay_mail_observe(&p,b,54000);relay_mail_view(&v);assert(v.count==4);
 assert(!relay_mail_take(&out,1900000));relay_mail_view(&v);assert(!v.count);
 old=writes;nv_bytes[0]=99;loaded=false;blocked=false;disk=(relay_disk_t){.version=1};relay_mail_enable(true,2000000);relay_mail_observe(&p,b,2000000);assert(error&&writes==old);
 puts("PASS custody: persist-before-forward, reboot forwarding, exact endpoint receipt replay, cancellation, capacity/expiry, failed writes, future store preservation");
}
'''
social=r'''
static uint8_t frames[4][128],lengths[4];static unsigned sent;
static bool tx(void *ctx,const uint8_t *p,uint8_t n){(void)ctx;if(sent<4){memcpy(frames[sent],p,n);lengths[sent]=n;}sent++;return true;}
static mesh_packet_t frame(unsigned i,const uint8_t peer[6]){mesh_packet_t p;uint8_t dst[6]=MESH_DST_BROADCAST_MAC;assert(mesh_packet_pack(&p,peer,dst,MESH_TYPE_SOCIAL,frames[i],lengths[i]));p.flags=MESH_FLAG_ROUTED;p.channel=1;p.session_id=99;p.msg_id=i+1;p.crc16=mesh_packet_compute_crc(&p);return p;}
int main(void){
 uint8_t me[6]={2,0,0,0,0,1},peer[6]={2,0,0,0,0,2};social_init(tx,NULL);social_tick(me,1,true,1);assert(!social_scope());assert(social_edit(0,"Me"));assert(social_edit(1,"Hello"));assert(social_room(0x12345678,"Team"));assert(social_share(true));social_view_t v;social_view(&v);assert(v.active&&v.room==0x12345678&&!strcmp(v.signature,"Hello"));
 fail_commit=true;assert(!social_edit(1,"Lost"));fail_commit=false;social_view(&v);assert(!strcmp(v.signature,"Hello"));
 uint8_t control[128];for(unsigned i=0;i<128;i++)control[i]=(uint8_t)i;sent=0;assert(social_wrap(control,128)&&sent==2);mesh_packet_t p0=frame(0,peer),p1=frame(1,peer),out;assert(!social_receive(&p1,&out));assert(social_receive(&p0,&out)&&out.type==MESH_TYPE_COLLAB&&out.len==128&&!memcmp(out.payload,control,128));p0.payload[1]^=1;assert(!social_receive(&p0,&out));

 /* Concurrent tables: identical fragment IDs and boot from different hosts. */
 p0=frame(0,peer);p1=frame(1,peer);uint8_t peer2[6]={2,0,0,0,0,3};mesh_packet_t q0=frame(0,peer2),q1=frame(1,peer2);q0.payload[11]^=0x55;q1.payload[11]^=0x55;q0.crc16=mesh_packet_compute_crc(&q0);q1.crc16=mesh_packet_compute_crc(&q1);
 assert(!social_receive(&p1,&out));assert(!social_receive(&q0,&out));assert(social_receive(&p0,&out)&&out.len==128&&!memcmp(out.payload,control,128));assert(social_receive(&q1,&out)&&out.len==128&&out.payload[0]==(control[0]^0x55)&&out.payload[117]==(control[117]^0x55));
 unsigned old=writes;assert(social_stamp(peer,"Buddy",50));assert(writes==old+1);assert(social_stamp(peer,"Buddy",50)&&writes==old+1);assert(social_stamp(peer,"Buddy",51));social_view(&v);assert(v.stamps_count==1&&v.stamps[0].visits==2);
 loaded=false;active=false;share_armed=false;disk=(social_disk_t){.version=1};social_view(&v);assert(!v.active&&!v.sharing&&v.room==0x12345678&&v.stamps_count==1&&!strcmp(v.nickname,"Me"));assert(social_room(v.room,v.room_name));assert(social_share(true));
 social_tick(me,1,true,100);sent=0;social_tick(me,1,true,11000);assert(sent&&frames[0][0]==SOC_CARD);mesh_packet_t card=frame(0,peer);assert(!social_receive(&card,&out));social_view(&v);assert(v.peers[0].used || v.peers[7].used);
 unsigned note,role,beat;uint32_t seq=0;uint8_t bd[12]={SOC_BEAT};put32(bd+1,social_scope());put32(bd+5,500);bd[9]=0;bd[10]=1;bd[11]=120;uint8_t dst[6]=MESH_DST_BROADCAST_MAC;mesh_packet_t tune;mesh_packet_pack(&tune,peer,dst,MESH_TYPE_SOCIAL,bd,12);tune.channel=1;tune.flags=MESH_FLAG_ROUTED;tune.session_id=99;tune.msg_id=55;
 social_receive(&tune,&out);assert(!social_ensemble_event(&seq,&note,&role,&beat));social_ensemble_arm(true,3);social_receive(&tune,&out);assert(social_ensemble_event(&seq,&note,&role,&beat)&&role==3&&beat==0);social_receive(&tune,&out);assert(!social_ensemble_event(&seq,&note,&role,&beat));social_ensemble_arm(false,0);assert(!social_ensemble_start(0,120));
 assert(social_room(0,""));assert(!social_receive(&p1,&out));assert(!social_edit(0,"too-long-name-here"));
 puts("PASS social: persisted profile/room/passport, safe activation, failed save, 128-byte interleaved host room reassembly/isolation, profile visibility, opt-in ensemble and beat dedup");
}
'''
for name,ns,key,test in [('relay_mail','relay_mail','cache_v1',relay),('social','social_user','social_v1',social)]:
 header=name+'.h';setup=shim.replace('draft_store.h',header).replace('msg_drafts',ns).replace('drafts_v1',key).replace('disk','nv_bytes')
 source=re.sub(r'^#include[^\n]*\n','',(root/f'firmware/app/{name}.c').read_text(encoding='utf8'),flags=re.M)
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);(p/'test.c').write_text(setup+source+test,encoding='utf8')
  subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'firmware/app'),'-I'+str(root/'firmware/protocol'),str(p/'test.c'),str(root/'build/mesh-packet-host.o'),'-o',str(p/'test.exe')],check=True)
  subprocess.run([str(p/'test.exe')],check=True)
