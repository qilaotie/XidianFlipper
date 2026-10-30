"""Production worker-to-UI gift queue; no NVS call from receive ownership."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=re.sub(r'^#include[^\n]*\n','',(root/'firmware/app/gift_runtime.c').read_text(encoding='utf8'),flags=re.M)
shim=r'''
#include "gift_runtime.h"
#include "pet_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
static bool ui_owner,enabled=true;static unsigned writes,coins=100,last_op;static uint32_t scope=77;static int64_t clock_us=2500000;static gift_book_t saved;
static const uint8_t self[6]={2,0,0,0,0,1},peer[6]={2,0,0,0,0,2};
static int64_t esp_timer_get_time(void){return clock_us;}
bool pet_service_gift_book(gift_book_t *b){assert(ui_owner);*b=saved;return true;}
bool pet_service_gift_commit(const gift_book_t *b,unsigned asset,int delta){assert(ui_owner&&!asset);assert((int)coins+delta>=0);saved=*b;coins+=delta;writes++;return true;}
const char *pet_service_name(void){return "Pet";}
static bool message_service_extras_context(uint8_t *mac,uint8_t *ch,uint32_t *room){memcpy(mac,self,6);*ch=1;*room=scope;return enabled;}
static bool message_service_extras_send(const uint8_t *b,uint8_t n,uint8_t ch,uint32_t room){assert(ui_owner&&n==33&&ch==1&&room==scope);last_op=b[1];return enabled;}
'''
tests=r'''
static void tick(bool armed){ui_owner=true;clock_us+=100000;gift_runtime_tick(armed);ui_owner=false;}
static mesh_packet_t packet(unsigned op,unsigned id){mesh_packet_t p={.type=MESH_TYPE_COLLAB,.len=33,.channel=1,.flags=MESH_FLAG_ROUTED};memset(p.dst_mac,255,6);memcpy(p.src_mac,peer,6);p.payload[0]=1;p.payload[1]=op;p.payload[2]=1;p.payload[4]=77;p.payload[8]=id;memcpy(p.payload+12,self,6);p.payload[18]=10;return p;}
int main(void){
 tick(true);assert(!writes);mesh_packet_t p=packet(GIFT_HELLO,0);p.payload[18]=0;memset(p.payload+12,255,6);p.channel=2;gift_runtime_receive(&p);assert(!count);p.channel=1;gift_runtime_receive(&p);assert(count==1&&!gifts.peers[0].used);tick(true);bool found=false;for(unsigned i=0;i<6;i++)found|=gifts.peers[i].used;assert(found&&!writes);
 p=packet(GIFT_OFFER,1);gift_runtime_receive(&p);assert(!gifts.offer&&!writes);tick(true);assert(gifts.offer&&!writes);ui_owner=true;assert(pet_gifts_decide(&gifts,true));ui_owner=false;assert(writes==1&&coins==100&&last_op==GIFT_ACCEPT);
 p=packet(GIFT_COMMIT,1);gift_runtime_receive(&p);assert(writes==1);tick(false);assert(writes==2&&coins==110&&last_op==GIFT_DONE);gift_runtime_receive(&p);tick(false);assert(writes==2&&coins==110);
 scope=99;gift_runtime_receive(&p);tick(false);assert(writes==2);scope=77;tick(false);
 p=packet(GIFT_OFFER,2);gift_runtime_receive(&p);tick(false);assert(!gifts.offer&&writes==2);p=packet(GIFT_CANCEL,2);gift_runtime_receive(&p);tick(false);assert(coins==110&&last_op==GIFT_REJECT);
 p=packet(GIFT_OFFER,3);for(unsigned i=0;i<20;i++)gift_runtime_receive(&p);assert(count==8);tick(false);assert(!count);
 puts("PASS gift runtime: bounded worker queue, UI-only wallet writes, outer/inner channel match, scope guard, explicit consent, background accepted settlement and duplicate receipt replay");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-gift-runtime-') as temp:
 p=Path(temp);(p/'test.c').write_text(shim+source+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'firmware/app'),'-I'+str(root/'firmware/protocol'),str(p/'test.c'),str(root/'firmware/app/pet_gifts.c'),str(root/'build/mesh-packet-host.o'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True,timeout=30)
