"""Production pet store/page with NVS, clock and drawing mocks; not hardware evidence."""
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
def source(path):
    return re.sub(r'^#include[^\n]*\n','',(root/path).read_text(encoding='utf-8'),flags=re.M)
shim=r'''
#include "pet_service.h"
#include "pet_games.h"
#include "pet_help.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int nvs_handle_t;
typedef int esp_err_t;
enum {ESP_OK=0,ESP_ERR_NVS_NOT_FOUND=1,NVS_READONLY=0,NVS_READWRITE=1};
static pet_t stored,staged;
static unsigned char stored_extra[20],staged_extra[20];
static bool exists_extra,stage_pet,stage_extra;
static pet_v1_t legacy;
static bool has_legacy;
static bool exists,commit_fail,open_fail;
static size_t blob_size=sizeof(pet_t);
static unsigned writes;
static int64_t clock_us=1000000;
static int64_t esp_timer_get_time(void){return clock_us;}
static int nvs_open(const char *name,int mode,nvs_handle_t *h){
 assert(!strcmp(name,"pet_user"));*h=1;
 if(open_fail)return 9;
 return mode==NVS_READONLY&&!exists&&!has_legacy?ESP_ERR_NVS_NOT_FOUND:ESP_OK;
}
static int nvs_get_blob(nvs_handle_t h,const char *key,void *data,size_t *size){
 (void)h;
 if(!strcmp(key,"extra_v1")){if(!exists_extra)return ESP_ERR_NVS_NOT_FOUND;assert(*size==20);memcpy(data,stored_extra,20);return ESP_OK;}
 if(!strcmp(key,"state")){if(!has_legacy)return ESP_ERR_NVS_NOT_FOUND;assert(*size>=sizeof(legacy));memcpy(data,&legacy,sizeof(legacy));*size=sizeof(legacy);return ESP_OK;}
 assert(!strcmp(key,"state_v2"));if(!exists)return ESP_ERR_NVS_NOT_FOUND;
 assert(*size>=blob_size);memcpy(data,&stored,blob_size);*size=blob_size;return ESP_OK;
}
static int nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size){
 (void)h;
 if(!strcmp(key,"extra_v1")){assert(size==20);memcpy(staged_extra,data,20);stage_extra=true;return ESP_OK;}
 assert(!strcmp(key,"state_v2")&&size==sizeof(staged));memcpy(&staged,data,size);stage_pet=true;return ESP_OK;
}
static int nvs_commit(nvs_handle_t h){(void)h;if(commit_fail)return 9;
 if(stage_pet){stored=staged;exists=true;stage_pet=false;}
 if(stage_extra){memcpy(stored_extra,staged_extra,20);exists_extra=true;stage_extra=false;}
 writes++;return ESP_OK;}
static void nvs_close(nvs_handle_t h){(void)h;}
enum {M_PET=61,BTN_ID_BACK=1,BTN_ID_CW,BTN_ID_CC,BTN_ID_SP,BTN_ID_LP,S_LAYER_OUT,U8G2_DRAW_ALL=15};
static struct {bool pressed;int id;}btn;
static struct {int index,layer,state,parent_index[20];}ui;
static int u8g2,u8g2_font_5x7_tf;
static uint32_t esp_random(void){return 123;}
static void pet_link_enter(bool duel){(void)duel;}
static void pet_link_leave(void){}
static void pet_link_proc(unsigned input){(void)input;}
static void u8g2_SetDrawColor(int *p,int c){(void)p;(void)c;}
static void u8g2_SetFont(int *p,int f){(void)p;(void)f;}
static void u8g2_DrawBox(int *p,int x,int y,int w,int h){(void)p;assert(x>=0&&y>=0&&x+w<=128&&y+h<=32);}
static void u8g2_DrawRFrame(int *p,int x,int y,int w,int h,int r){(void)r;u8g2_DrawBox(p,x,y,w,h);}
static void u8g2_DrawHLine(int *p,int x,int y,int w){u8g2_DrawBox(p,x,y,w,1);}
static void u8g2_DrawVLine(int *p,int x,int y,int h){u8g2_DrawBox(p,x,y,1,h);}
static void u8g2_DrawPixel(int *p,int x,int y){u8g2_DrawBox(p,x,y,1,1);}
static void u8g2_DrawLine(int *p,int x,int y,int a,int b){u8g2_DrawPixel(p,x,y);u8g2_DrawPixel(p,a,b);}
static void u8g2_DrawEllipse(int *p,int x,int y,int rx,int ry,int opts){(void)opts;u8g2_DrawBox(p,x-rx,y-ry,rx*2+1,ry*2+1);}
static void u8g2_DrawCircle(int *p,int x,int y,int r,int opts){u8g2_DrawEllipse(p,x,y,r,r,opts);}
static void u8g2_DrawDisc(int *p,int x,int y,int r,int opts){u8g2_DrawEllipse(p,x,y,r,r,opts);}
static void u8g2_DrawStr(int *p,int x,int y,const char *s){(void)p;assert(x>=0&&y>=7&&y<=32&&x+strlen(s)*5<=128);}
'''
tests=r'''
static void boot(void){initialized=false;dirty=pending=error=false;last_action=-2000000;pet_service_tick();}
static void key(int k){btn.id=k;btn.pressed=true;pet_page_proc();}
int main(void){
 boot();assert(!exists&&pending&&!error);clock_us+=5000000;pet_service_tick();assert(exists&&writes==1&&!pending);
 clock_us+=55000000;pet_service_tick();assert(state.age==60&&pet_stage(&state)==1);
 assert(pet_service_action(PET_FEED));assert(!pet_service_action(PET_FEED));assert(writes==2);
 clock_us+=2000000;assert(pet_service_action(PET_PLAY));assert(pending&&writes==2);
 clock_us+=3000000;pet_service_tick();assert(!pending&&writes==3);
 assert(pet_service_flush());uint32_t age=stored.age;clock_us+=9000000000LL;boot();assert(state.age==age);
 clock_us+=6000000;commit_fail=true;assert(pet_service_action(PET_SLEEP));assert(error&&pending);
 assert(!pet_service_flush());assert(!stored.sleeping);
 commit_fail=false;clock_us+=5000000;pet_service_tick();assert(!error&&!pending&&stored.sleeping);
 stored.version=99;pet_t bad=stored;unsigned before=writes;boot();assert(error&&!writable);
 assert(!pet_service_action(PET_FEED)&&pet_service_flush());assert(writes==before&&!memcmp(&bad,&stored,sizeof(bad)));
 pet_new(&stored);pet_advance(&stored,60);blob_size=sizeof(stored)-1;boot();assert(error&&!writable);
 blob_size=sizeof(stored);open_fail=true;boot();assert(error&&!writable);open_fail=false;
 exists=false;has_legacy=true;
 legacy=(pet_v1_t){.age=900,.version=1,.fullness=77,.mood=66,.energy=88,.clean=99,.sleeping=1};
 pet_v1_t backup=legacy;boot();assert(writable&&pending&&state.age==900&&state.sleeping&&state.version==2);
 assert(pet_service_flush());assert(exists&&!memcmp(&legacy,&backup,sizeof(backup)));
 boot();assert(state.age==900&&state.sleeping); /* v2 wins, legacy is not read again. */
 stored.version=99;boot();assert(!writable&&error); /* No silent rollback to valid v1. */
 exists=false;legacy.version=99;boot();assert(!writable&&error);
 has_legacy=false;exists=true;pet_new(&stored);pet_advance(&stored,60);boot();assert(!error);
 clock_us+=6000000;commit_fail=true;unsigned old_gold=stored.coins,old_items=stored.items[0];
 assert(pet_service_buy(PET_APPLE));assert(error&&pending&&state.coins==old_gold-10&&state.items[0]==old_items+1);
 assert(stored.coins==old_gold&&stored.items[0]==old_items);
 commit_fail=false;clock_us+=5000000;pet_service_tick();boot();
 assert(state.coins==old_gold-10&&state.items[0]==old_items+1&&!error);
 ui.layer=2;ui.parent_index[2]=23;ui.index=M_PET;
 pet_page_enter(M_PET);pet_page_proc();key(BTN_ID_LP);pet_page_proc();
 state.fullness=state.mood=state.energy=state.clean=100;pet_page_proc();
 selected=4;key(BTN_ID_SP);assert(mode==SHOP);state.coins=100;
 clock_us+=2500000;cursor=PET_PLANT;key(BTN_ID_SP);assert(state.coins==75&&(state.owned&2));
 key(BTN_ID_SP);assert(state.coins==75);key(BTN_ID_BACK);assert(mode==HOME);
 selected=5;key(BTN_ID_SP);assert(mode==BAG);cursor=PET_PLANT;clock_us+=2500000;key(BTN_ID_SP);assert(state.decor==1);
 key(BTN_ID_BACK);selected=6;key(BTN_ID_SP);assert(mode==DEX);cursor=6;key(BTN_ID_SP);assert(mode==NEW_EGG);
 key(BTN_ID_SP);assert(mode==DEX&&state.age>=60); /* Default choice keeps the pet. */
 key(BTN_ID_SP);cursor=1;clock_us+=2500000;key(BTN_ID_SP);assert(mode==HOME&&state.age==0&&state.coins==75&&state.decor==1);
 pet_advance(&state,60);state.energy=state.fullness=100;selected=1;
 clock_us+=2500000;key(BTN_ID_SP);assert(mode==GAMES);cursor=PG_COUNT;key(BTN_ID_SP);assert(mode==HELP);key(BTN_ID_SP);assert(mode==GAME);
 assert(go_at-clock_us>=2200000 && go_at-clock_us<=4000000);
 clock_us=go_at+600000;key(BTN_ID_SP);assert(mode==HOME&&state.coins==83&&state.games_won==1);
 clock_us+=2500000;key(BTN_ID_SP);cursor=PG_COUNT;key(BTN_ID_SP);key(BTN_ID_SP);assert(mode==GAME);unsigned gold=state.coins;
 key(BTN_ID_SP);assert(mode==HOME&&state.coins==gold); /* Early press never pays. */
 clock_us+=2500000;key(BTN_ID_SP);cursor=PG_COUNT;key(BTN_ID_SP);key(BTN_ID_SP);assert(mode==GAME);clock_us=go_at+3100000;pet_page_proc();assert(mode==HOME&&state.coins==gold);
 key(BTN_ID_BACK);assert(ui.index==23&&ui.state==S_LAYER_OUT);pet_page_enter(23);assert(!pending&&stored.coins==gold);
 pet_page_enter(M_PET);assert(state.coins==gold);
 selected=7;mode=HOME;key(BTN_ID_SP);assert(mode==GROWTH);pet_page_proc();key(BTN_ID_BACK);
 selected=8;clock_us+=2500000;key(BTN_ID_SP);assert(state.spare==1);clock_us+=2500000;key(BTN_ID_SP);assert(!state.spare);
 pet_service_reward_begin(PET_REWARD_TETRIS);unsigned balance=state.coins,training=state.games_won;
 assert(pet_service_reward(PET_REWARD_TETRIS,2)==10);assert(state.coins==balance+10);
 assert(!pet_service_reward(PET_REWARD_TETRIS,2));assert(!pet_service_reward(PET_REWARD_TETRIS,1));
 assert(pet_service_reward(PET_REWARD_TETRIS,4)==10&&state.games_won==training+1);
 assert(pet_service_reward(PET_REWARD_TETRIS,5)==5&&state.games_won==training+1);
 pet_service_reward_begin(PET_REWARD_SNAKE);assert(pet_service_reward(PET_REWARD_SNAKE,1)==2);
 assert(!pet_service_reward(PET_REWARD_SNAKE,1));assert(!pet_service_reward(99,5));
 state.coins=9998;assert(pet_service_reward(PET_REWARD_SNAKE,2)==1&&state.coins==9999);
 assert(pet_service_reward(PET_REWARD_SNAKE,3)==0);state.coins=9990;
 assert(!pet_service_reward(PET_REWARD_SNAKE,3));assert(pet_service_reward(PET_REWARD_SNAKE,4)==2);
 assert(pet_service_flush());balance=state.coins;boot();assert(state.coins==balance);
 for(unsigned kind=0;kind<PG_COUNT;kind++){
  state.energy=state.fullness=state.mood=state.clean=100;state.coins=100;
  mode=GAMES;cursor=kind;clock_us+=2500000;key(BTN_ID_SP);assert(mode==HELP && state.energy==100);key(BTN_ID_SP);assert(mode==MINIGAME && state.energy==92);
  for(unsigned frame=0;frame<1900;frame++){clock_us+=10000;pet_page_proc();}
  mini.done=mini.won=true;mini_settled=false;balance=state.coins;pet_page_proc();assert(state.coins==balance+8);
  for(unsigned frame=0;frame<20;frame++)pet_page_proc();assert(state.coins==balance+8);
  key(BTN_ID_BACK);assert(mode==GAMES);
 }
 for(unsigned item=0;item<=PET_HELP_DUEL;item++){
  mode=item<PET_HELP_VISIT?GAMES:CONNECT;cursor=item<PET_HELP_VISIT?item:item-PET_HELP_VISIT;
  unsigned origin=mode;balance=state.coins;unsigned rest=state.energy;
  key(BTN_ID_SP);assert(mode==HELP && help_page==0);
  unsigned pages=pet_help_pages(item);
  for(unsigned page=0;page<pages;page++){
   assert(help_page==page);pet_page_proc();
   for(unsigned row=0;row<3;row++)assert(strlen(pet_help_line(item,page,row))<=25);
   key(BTN_ID_CW);
  }
  assert(help_page==0);key(BTN_ID_CC);assert(help_page==pages-1);
  key(BTN_ID_BACK);assert(mode==origin && balance==state.coins && rest==state.energy);
 }
 state.energy=0;mode=GAMES;cursor=0;key(BTN_ID_SP);assert(mode==HELP);key(BTN_ID_SP);assert(mode==HELP && state.energy==0);key(BTN_ID_BACK);
 /* Count only full, >=2s automatic reads, once on leaving; independent and persistent. */
 memset(extra.views,0,sizeof(extra.views));state.energy=state.fullness=100;
 for(unsigned attempt=0;attempt<3;attempt++){
  mode=GAMES;cursor=0;key(BTN_ID_SP);assert(mode==HELP);
  clock_us+=2100000;key(BTN_ID_CW);key(BTN_ID_BACK);assert(extra.views[0]==attempt+1);
 }
 assert(!pet_service_help_needed(0) && pet_service_help_needed(1));assert(pet_service_flush());boot();
 assert(!pet_service_help_needed(0));mode=GAMES;cursor=0;clock_us+=2500000;key(BTN_ID_SP);assert(mode==MINIGAME);key(BTN_ID_BACK);
 mode=HOME;selected=10;key(BTN_ID_SP);assert(mode==GUIDES);cursor=0;unsigned energy=state.energy;
 key(BTN_ID_SP);assert(mode==HELP && help_manual);key(BTN_ID_SP);assert(mode==GUIDES && energy==state.energy);
 mode=GAMES;cursor=1;key(BTN_ID_SP);clock_us+=2100000;key(BTN_ID_BACK);assert(extra.views[1]==0);
 state.coins=100;memset(state.items,0,3);extra.next_visit=0;int gift=-1;
 assert(pet_service_social(0,2,&gift)==5 && gift==2 && state.items[2]==1);assert(pet_service_flush());boot();
 assert(!pet_service_social(0,2,&gift) && gift==-1 && state.coins==105 && state.items[2]==1);
 clock_us+=2500000;assert(pet_service_new_egg());assert(!pet_service_social(0,1,&gift));
 clock_us+=181000000;pet_service_tick();state.items[1]=9;assert(pet_service_social(0,1,&gift)==5 && gift==-1);
 unsigned wins=state.games_won;balance=state.coins;
 assert(pet_service_social(1,0,&gift)==6);assert(pet_service_social(2,0,&gift)==10);assert(pet_service_social(3,0,&gift)==20);
 assert(state.coins==balance+36 && state.games_won==wins+1);
 clock_us+=181000000;pet_service_tick();commit_fail=true;balance=state.coins;
 assert(!pet_service_social(0,0,&gift) && gift==-1 && state.coins==balance);commit_fail=false;
 state.coins=9998;assert(pet_service_social(3,0,&gift)==1 && state.coins==9999);
 assert(pet_service_flush());unsigned char saved_metadata[20];memcpy(saved_metadata,stored_extra,20);
 stored_extra[0]=99;before=writes;boot();assert(error && !writable);
 assert(!pet_service_social(3,0,&gift) && gift==-1 && writes==before);
 pet_service_help_seen(0);assert(writes==before && stored_extra[0]==99);
 memcpy(stored_extra,saved_metadata,20);boot();assert(writable && !error);
 for(unsigned decor=0;decor<4;decor++)for(unsigned form=1;form<=3;form++){
  pet_new(&state);pet_advance(&state,1800);state.form=(uint8_t)form;state.dex=63;state.owned=15;state.decor=(uint8_t)decor;
  for(unsigned frame=0;frame<64;frame++)creature(&state,frame);
  for(unsigned m=0;m<=CONNECT;m++){
   if(m==GAME || m==MINIGAME)continue;
   mode=m;unsigned count=m==HOME?11:(m==GAMES?7:(m==SHOP?PET_ITEMS:(m==BAG?PET_ITEMS+1:(m==DEX?7:(m==GROWTH?1:2)))));
   for(unsigned i=0;i<count;i++){cursor=i;selected=i;clock_us+=400000;pet_page_proc();}
  }
 }
 puts("PASS pet v2 store/UI: legacy migration preserved, v2 priority, atomic economy/reboot, menu bounds, game reward/timeout, new-egg confirmation");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-pet-') as temp:
    path=Path(temp);(path/'test.c').write_text(shim+source('firmware/app/pet_service.c')+source('firmware/ui/pet_page.c')+tests,encoding='utf-8')
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/app'),
                    str(path/'test.c'),str(root/'firmware/app/pet.c'),str(root/'firmware/app/pet_games.c'),str(root/'firmware/app/pet_help.c'),'-o',str(path/'test.exe')],check=True)
    subprocess.run([str(path/'test.exe')],check=True)
