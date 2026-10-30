"""Production pet link against a scripted peer; not two-device radio evidence."""
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=re.sub(r'^#include[^\n]*\n','',(root/'firmware/app/pet_link.c').read_text(encoding='utf-8'),flags=re.M)
shim=r'''
#include "pet_link.h"
#include "pet_games.h"
#include "pet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
static uint32_t clock_ms=100;
static int64_t esp_timer_get_time(void){return (int64_t)clock_ms*1000;}
static uint32_t esp_random(void){return clock_ms+123;}
static pet_t pet;
static unsigned payments,rewards,flushes;
static bool can_pay=true,radio=true;
static const uint8_t mymac[]={2,1,2,3,4,5},other[]={2,1,2,3,4,6};
static bool ble_radio_get_my_mac(uint8_t *m){memcpy(m,mymac,6);return true;}
static bool ble_radio_is_ready(void){return radio;}
bool mesh_packet_is_broadcast(const uint8_t *mac){for(unsigned i=0;i<6;i++)if(mac[i]!=255)return false;return true;}
static const pet_t *pet_service_state(void){return &pet;}
static const char *pet_service_name(void){return "Mochi";}
static const char *nvs_config_get_device_name(void){return "TEST";}
static bool pet_service_action(unsigned a){assert(a==PET_PLAY);if(can_pay)payments++;return can_pay;}
static unsigned outcomes[4];
static unsigned pet_service_social(unsigned outcome,unsigned seed,int *gift){(void)seed;outcomes[outcome]++;rewards++;*gift=outcome==0?1:-1;return outcome==3?20:(outcome==2?10:(outcome==1?6:5));}
static bool pet_service_flush(void){flushes++;return true;}
static unsigned passport_calls;
static bool message_service_passport_visit(const uint8_t mac[6],const char *name,uint32_t token){assert(mac&&name&&token);passport_calls++;return true;}
static uint8_t sent[40];static unsigned sends;
static bool message_service_send_control(unsigned type,const uint8_t *dst,const void *b,unsigned n){
 assert(type==MESH_TYPE_PET && n==40);(void)dst;memcpy(sent,b,40);sends++;return radio;
}
static int u8g2,u8g2_font_5x7_tf;
static void u8g2_SetDrawColor(int *p,int c){(void)p;(void)c;}
static void u8g2_SetFont(int *p,int f){(void)p;(void)f;}
static void u8g2_DrawStr(int *p,int x,int y,const char *s){(void)p;assert(x>=0 && y>=7 && y<=32 && x+strlen(s)*5<=128);}
#define ui_zh_draw_fit(d,x,y,s,w) ((void)(w),u8g2_DrawStr(d,x,y,s))
static void u8g2_DrawRFrame(int *p,int x,int y,int w,int h,int r){(void)p;(void)r;assert(x>=0&&y>=0&&x+w<=128&&y+h<=32);}
static void u8g2_DrawPixel(int *p,int x,int y){u8g2_DrawRFrame(p,x,y,1,1,0);}
static void u8g2_DrawHLine(int *p,int x,int y,int w){u8g2_DrawRFrame(p,x,y,w,1,0);}
static void u8g2_DrawVLine(int *p,int x,int y,int h){u8g2_DrawRFrame(p,x,y,1,h,0);}
static void u8g2_DrawLine(int *p,int x,int y,int a,int b){u8g2_DrawPixel(p,x,y);u8g2_DrawPixel(p,a,b);}
'''
tests=r'''
static void packet(unsigned op,unsigned score,bool final,bool ack){
 mesh_packet_t p={0};p.len=40;p.type=MESH_TYPE_PET;memcpy(p.src_mac,other,6);
 memset(p.dst_mac,op==HELLO?255:0,6);p.payload[0]=2;p.payload[1]=(uint8_t)op;
 write32(p.payload+2,888);write32(p.payload+6,token);p.payload[10]=duel;p.payload[11]=game.kind;
 write32(p.payload+12,seed);p.payload[20]=(uint8_t)score;p.payload[22]=final;p.payload[33]=ack;
 p.payload[23]=3;p.payload[24]=2;memcpy(p.payload+25,"PEER",5);
 pet_link_receive(&p,-40);pet_link_proc(PG_NONE);
}
int main(void){
 pet_new(&pet);pet_advance(&pet,60);
 pet_link_enter(true);pet_link_proc(PG_NONE);assert(state==SEARCH && sends);
 assert(!strcmp((char *)sent+25,"Mochi"));
 packet(HELLO,0,false,false);pet_link_proc(PG_TAP);assert(state==INVITING && !payments);
 packet(ACCEPT,0,false,false);assert(state==RUN && payments==1 && host);
 packet(ACCEPT,0,false,false);assert(payments==1);
 packet(STATE,0,false,false);assert(confirmed);
 game.done=true;game.score=4;
 packet(STATE,2,true,false);assert(state==RUN && !rewards);
 packet(STATE,99,true,true);assert(state==RUN && !rewards);
 packet(STATE,2,true,true);assert(state==DONE && rewards==1 && flushes==1);
 for(unsigned i=0;i<50;i++)packet(STATE,2,true,true);assert(rewards==1);
 pet_link_proc(PG_TAP);assert(state==SEARCH);packet(STATE,2,true,true);assert(state==SEARCH && rewards==1);
 pet_link_leave();packet(INVITE,0,false,false);assert(!active);
 clock_ms+=5000;pet_link_enter(false);pet_link_proc(PG_NONE);
 packet(INVITE,0,false,false);assert(state==OFFER);pet_link_proc(PG_TAP);assert(state==SYNC);
 packet(START,0,false,false);assert(state==RUN && !host && payments==1);
 clock_ms+=11001;pet_link_proc(PG_NONE);assert(game.done);
 packet(STATE,0,true,true);assert(state==DONE && rewards==2 && outcomes[0]==1 && outcomes[3]==1 && passport_calls==1);
 for(unsigned i=0;i<20;i++)packet(STATE,0,true,true);assert(passport_calls==1);
 pet_link_enter(true);pet_link_proc(PG_NONE);packet(INVITE,0,false,false);assert(state==OFFER);
 can_pay=false;pet_link_proc(PG_TAP);assert(state==LOST && payments==1);
 can_pay=true;pet_link_enter(true);pet_link_proc(PG_NONE);packet(INVITE,0,false,false);pet_link_proc(PG_TAP);
 assert(state==SYNC && payments==2);clock_ms+=15001;pet_link_proc(PG_NONE);assert(state==LOST && rewards==2);
 pet_link_enter(false);pet_link_proc(PG_NONE);packet(INVITE,0,false,false);packet(LEAVE,0,false,false);assert(state==LOST);
 for(unsigned outcome=1;outcome<=2;outcome++){
  pet_link_enter(true);pet_link_proc(PG_NONE);packet(HELLO,0,false,false);pet_link_proc(PG_TAP);packet(ACCEPT,0,false,false);
  assert(state==RUN);game.done=true;game.score=outcome==1?0:2;
  packet(STATE,2,true,true);assert(state==DONE && outcomes[outcome]==1 && reward_gold==(outcome==1?6:10));
  unsigned claims=rewards;for(unsigned repeat=0;repeat<50;repeat++)packet(STATE,2,true,true);assert(rewards==claims);
 }
 radio=false;pet_link_enter(false);pet_link_proc(PG_NONE);assert(state==SEARCH);
 assert(passport_calls==1); /* Duels, failures and duplicate visit results never add stamps. */
 puts("PASS pet link: consent, host/guest, visit, timeout, rest guard, mutual result ACK, exactly-once reward, stale/invalid scores");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-pet-link-') as temp:
    path=Path(temp);(path/'test.c').write_text(shim+source+tests,encoding='utf-8')
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector',
                    '-I',str(root/'firmware/app'),'-I',str(root/'firmware/protocol'),
                    str(path/'test.c'),str(root/'firmware/app/pet.c'),str(root/'firmware/app/pet_games.c'),
                    '-o',str(path/'test.exe')],check=True)
    subprocess.run([str(path/'test.exe')],check=True,timeout=20)
