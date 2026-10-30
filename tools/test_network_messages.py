"""Production receive policy plus real durable outbox, with fake radio/inbox/NVS."""
from pathlib import Path
import re,subprocess,tempfile
from test_draft_store import shim
root=Path(__file__).resolve().parents[1]
shim=shim.replace('draft_store.h','message_service.h').replace('msg_drafts','msg_outbox').replace('drafts_v1','outbox_v1')
outbox=re.sub(r'^#include[^\n]*\n','',(root/'firmware/app/message_outbox.c').read_text(encoding='utf8'),flags=re.M)
source=(root/'firmware/app/message_service.c').read_text(encoding='utf8')
def extract(signature):
 start=source.index(signature);brace=source.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]+'\n'
setup=r'''
static const uint8_t me[6]={2,0,0,0,0,1},peer[6]={2,0,0,0,0,2},other[6]={2,0,0,0,0,3};
static const uint8_t s_broadcast[6]=MESH_DST_BROADCAST_MAC;
static bool s_initialized=true,s_accepting=true,s_store_ok=true,inbox_fail;
static uint32_t s_session=123,s_next_message,s_next_inbox;
static uint8_t s_channel=2;
static mesh_route_t s_route;
static team_activity_t s_team;
static party_link_t s_party;
static network_lab_t s_lab;
void network_lab_receive(network_lab_t *s,const mesh_packet_t *p){(void)s;(void)p;}
void gift_runtime_receive(const mesh_packet_t *p){(void)p;}
void party_link_receive(party_link_t *s,const mesh_packet_t *p){(void)s;(void)p;}
static crowd_activity_t s_crowd;
void crowd_receive(crowd_activity_t *s,const mesh_packet_t *p){(void)s;(void)p;}
static void record_activities(void){}
uint32_t social_scope(void){return 0;}
bool social_receive(const mesh_packet_t *p,mesh_packet_t *out){(void)p;(void)out;return false;}
void relay_mail_observe(const mesh_packet_t *p,const uint8_t self[6],uint32_t now){(void)p;(void)self;(void)now;}
bool relay_mail_cancelled(const mesh_packet_t *p){(void)p;return false;}
static contact_seen_t s_contacts[32];
void contact_observe(contact_seen_t c[32],const mesh_packet_t *p,int8_t r,uint32_t now){(void)c;(void)p;(void)r;(void)now;}
void team_receive(team_activity_t *s,const mesh_packet_t *p){(void)s;(void)p;}
static message_service_snapshot_t s_stats;
static message_inbox_entry_t s_inbox[MESSAGE_INBOX_MAX];
typedef struct{mesh_packet_t packet;message_tx_status_t status;}tx_t;
static tx_t s_tx[8];static unsigned acks,inbox_writes;static uint8_t ack_flags;
static morse_live_t s_live;static int s_notifications;
typedef struct{mesh_packet_t packet;int8_t rssi;}rx_event_t;
#define LOCK() ((void)0)
#define UNLOCK() ((void)0)
#define pdTRUE 1
static uint32_t now_ms(void){return 1000;}
static uint32_t esp_random(void){return 50;}
static bool ble_radio_get_my_mac(uint8_t *m){memcpy(m,me,6);return true;}
static bool friend_mgr_is_friend(const uint8_t *m){return mesh_mac_equal(m,peer);}
static int find_tx(uint32_t id){(void)id;return -1;}
static void nearby_games_receive(const mesh_packet_t *p,int r){(void)p;(void)r;}
static void pet_link_receive(const mesh_packet_t *p,int r){(void)p;(void)r;}
static void rps_game_receive(const mesh_packet_t *p,int r){(void)p;(void)r;}
static void friend_mgr_on_peer_found(const mesh_packet_t *p,int r){(void)p;(void)r;}
bool morse_live_receive(morse_live_t *s,const uint8_t m[6],uint32_t id,const uint8_t *p,size_t n,uint32_t now){(void)s;(void)m;(void)id;(void)p;(void)n;(void)now;return true;}
static bool morse_codec_validate(const char *p,unsigned n){(void)p;(void)n;return true;}
static bool duplicate(const mesh_packet_t *p){for(unsigned i=0;i<MESSAGE_INBOX_MAX;i++)if(s_inbox[i].local_id&&s_inbox[i].msg_id==p->msg_id&&s_inbox[i].session_id==p->session_id&&mesh_mac_equal(s_inbox[i].src_mac,p->src_mac))return true;return false;}
static void remember(const mesh_packet_t *p){(void)p;}
static bool disk_save(unsigned slot,const message_inbox_entry_t *v){(void)slot;(void)v;if(inbox_fail)return false;inbox_writes++;return true;}
static bool xQueueSend(int q,const void *p,int ticks){(void)q;(void)p;(void)ticks;return true;}
static bool xQueueReceive(int q,void *p,int ticks){(void)q;(void)p;(void)ticks;return false;}
static bool send_control_flags(uint8_t type,const uint8_t *dst,const void *p,uint8_t n,uint8_t flags){(void)dst;(void)p;assert(type==MESH_TYPE_RECEIPT&&n==8);acks++;ack_flags=flags;return true;}
static bool send_text_flags(message_kind_t kind,const uint8_t *dst,uint8_t channel,const char *text,uint16_t len,bool ack,uint32_t *id,uint8_t flags){
 (void)id;assert(kind==MESSAGE_KIND_QUICK&&mesh_packet_is_broadcast(dst)&&!ack&&len<=128);
 if(!s_route.enabled)return false;
 if(flags&MESH_FLAG_ANNOUNCEMENT){assert(channel==0&&len>=6&&!memcmp(text,"[ALL] ",6));}else assert(channel==s_channel);
 return true;
}
uint8_t message_service_get_channel(void){return s_channel;}
'''
production=''.join(extract(s) for s in ['static void put32(','static uint32_t get32(','static bool text_valid(','static void receipt(','static void receive(','bool message_service_group_text(','int message_service_offline_text('])
tests=r'''
static mesh_packet_t make(const uint8_t *src,const uint8_t *dst,unsigned id,uint8_t flags,unsigned channel){mesh_packet_t p;assert(mesh_packet_pack(&p,src,dst,MESH_TYPE_QUICK_MSG,(const uint8_t *)"[ALL] Hello",11));p.flags=flags;p.channel=channel;p.session_id=42;p.msg_id=id;p.crc16=mesh_packet_compute_crc(&p);return p;}
static void rx(mesh_packet_t p){rx_event_t e={p,-60};receive(&e);}
int main(void){
 mesh_packet_t p=make(other,s_broadcast,1,MESH_FLAG_ROUTED|MESH_FLAG_ANNOUNCEMENT,0);rx(p);assert(!inbox_writes);
 mesh_route_enable(&s_route,true);rx(p);assert(inbox_writes==1&&s_inbox[0].kind==MESSAGE_KIND_QUICK&&s_inbox[0].channel==0);
 rx(p);assert(inbox_writes==1&&s_stats.rx_duplicates==1);
 p=make(other,s_broadcast,2,MESH_FLAG_ROUTED,1);rx(p);assert(inbox_writes==1);
 p=make(other,s_broadcast,3,MESH_FLAG_ROUTED,2);rx(p);assert(inbox_writes==2);
 p=make(other,me,4,MESH_FLAG_ROUTED|MESH_FLAG_ACK_REQUEST,0);rx(p);assert(inbox_writes==2&&!acks);
 p=make(peer,me,5,MESH_FLAG_ROUTED|MESH_FLAG_ACK_REQUEST,0);inbox_fail=true;rx(p);assert(inbox_writes==2&&!acks);
 inbox_fail=false;rx(p);assert(inbox_writes==3&&acks==1&&ack_flags==MESH_FLAG_ROUTED);rx(p);assert(inbox_writes==3&&acks==2);
 assert(message_service_group_text("hello",false,NULL)&&message_service_group_text("hello",true,NULL));
 char max[129];memset(max,'A',128);max[128]=0;assert(message_service_group_text(max,false,NULL)&&!message_service_group_text(max,true,NULL));
 mesh_route_enable(&s_route,false);assert(!message_service_group_text("hello",false,NULL));
 int slot=message_service_offline_text(peer,"offline");assert(slot==0);outbox_entry_t e;assert(message_outbox_get(0,&e)&&e.state==OUTBOX_WAITING);
 assert(message_service_offline_text(other,"no")<0);fail_commit=true;assert(message_service_offline_text(peer,"bad")<0);fail_commit=false;
 uint8_t ack[8];put32(ack,e.packet.session_id);put32(ack+4,e.packet.msg_id);
 assert(mesh_packet_pack(&p,other,me,MESH_TYPE_RECEIPT,ack,8));p.session_id=99;p.msg_id=1;rx(p);assert(message_outbox_get(0,&e)&&e.state==OUTBOX_WAITING);
 memcpy(p.src_mac,peer,6);fail_commit=true;rx(p);assert(message_outbox_get(0,&e)&&e.state==OUTBOX_WAITING);fail_commit=false;
 rx(p);assert(message_outbox_get(0,&e)&&e.state==OUTBOX_DELIVERED);loaded=false;assert(message_outbox_get(0,&e)&&e.state==OUTBOX_DELIVERED);
 puts("PASS production receive/actions: group isolation, opt-in global announcements, inbox-before-ACK/dedup, explicit send limits, durable offline queue and endpoint ACK commit");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-net-msg-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(shim+outbox+setup+production+tests,encoding='utf8')
 subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-fno-stack-protector','-I',str(root/'firmware/app'),'-I',str(root/'firmware/protocol'),str(p/'test.c'),str(root/'firmware/app/mesh_route.c'),str(root/'firmware/protocol/mesh_packet.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
