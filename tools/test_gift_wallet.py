"""Actual pet_service + gift engine, atomic blob NVS mock, not flash power-cut proof."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=re.sub(r'^#include[^\n]*\n','',(root/'firmware/app/pet_service.c').read_text(encoding='utf8'),flags=re.M)
shim=r'''
#include "pet_service.h"
#include "mesh_packet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int nvs_handle_t;typedef int esp_err_t;
enum{ESP_OK,ESP_ERR_NVS_NOT_FOUND,NVS_READONLY,NVS_READWRITE};
static uint8_t disk[4][2048],stage[4][2048];static size_t sizes[4],stages[4];static unsigned writes,sets,last_sets;
static bool fail_set,fail_commit,ambiguous;static int64_t clock_us=1000000;
static int64_t esp_timer_get_time(void){return clock_us;}
static unsigned key(const char *s){const char *keys[]={"state_v2","extra_v1","name_v1","wallet_v1"};for(unsigned i=0;i<4;i++)if(!strcmp(s,keys[i]))return i;assert(!strcmp(s,"state"));return 4;}
static int nvs_open(const char *name,int mode,int *h){assert(!strcmp(name,"pet_user"));*h=1;if(mode==NVS_READWRITE){memset(stages,0,sizeof(stages));sets=0;return ESP_OK;}for(unsigned i=0;i<4;i++)if(sizes[i])return ESP_OK;return ESP_ERR_NVS_NOT_FOUND;}
static int nvs_get_blob(int h,const char *name,void *p,size_t *n){(void)h;unsigned i=key(name);if(i==4||!sizes[i])return ESP_ERR_NVS_NOT_FOUND;if(*n<sizes[i])return 9;memcpy(p,disk[i],sizes[i]);*n=sizes[i];return ESP_OK;}
static int nvs_set_blob(int h,const char *name,const void *p,size_t n){(void)h;unsigned i=key(name);assert(i<4&&n<=2048);if(fail_set)return 9;memcpy(stage[i],p,n);stages[i]=n;sets++;return ESP_OK;}
static int nvs_commit(int h){(void)h;if(fail_commit&&!ambiguous)return 9;for(unsigned i=0;i<4;i++)if(stages[i]){memcpy(disk[i],stage[i],stages[i]);sizes[i]=stages[i];}writes++;last_sets=sets;return fail_commit?9:ESP_OK;}
static void nvs_close(int h){(void)h;memset(stages,0,sizeof(stages));}
'''
tests=r'''
static const uint8_t me[6]={2,0,0,0,0,1},peer[6]={2,0,0,0,0,2};
static unsigned credits,replies;
static bool persist(void *ctx,const gift_book_t *b,unsigned asset,int delta){(void)ctx;credits++;return pet_service_gift_commit(b,asset,delta);}
static bool transport(void *ctx,const uint8_t *b,uint8_t n,uint8_t ch,uint32_t scope){(void)ctx;assert(n==33&&ch==1&&scope==77&&b[1]==GIFT_DONE);replies++;return true;}
static void boot(void){initialized=false;dirty=pending=error=false;last_action=-2000000;pet_service_tick();}
int main(void){
 boot();assert(pet_service_flush());uint8_t original[2][2048];memcpy(original,disk,sizeof(original));gift_book_t b;assert(pet_service_gift_book(&b));
 b.sequence=1;b.outgoing=(gift_tx_t){.peer={2,0,0,0,0,2},.asset=0,.amount=10,.channel=1,.scope=77,.id=1,.stage=GIFT_OFFERING};assert(pet_service_gift_commit(&b,0,-10));assert(state.coins==20&&last_sets==1&&wallet_active&&!dirty);assert(!memcmp(original,disk,sizeof(original)));
 boot();assert(state.coins==20&&wallet_active);assert(pet_service_gift_book(&b)&&b.outgoing.stage==GIFT_OFFERING);pet_service_help_seen(0);boot();assert(extra.views[0]==1&&state.coins==20);
 pet_service_reward_begin(PET_REWARD_SNAKE);assert(pet_service_reward(PET_REWARD_SNAKE,1)==2);assert(pet_service_flush());boot();assert(state.coins==22&&wallet.book.outgoing.stage==GIFT_OFFERING);
 b=wallet.book;b.outgoing.stage=GIFT_COMMITTING;fail_commit=true;unsigned old=writes;assert(!pet_service_gift_commit(&b,0,0)&&writes==old&&!writable);fail_commit=false;boot();assert(wallet.book.outgoing.stage==GIFT_OFFERING);assert(pet_service_gift_commit(&b,0,0));
 b.receipts[0]=(gift_tx_t){.peer={2,0,0,0,0,2},.asset=0,.amount=10,.channel=1,.scope=77,.id=50,.stage=GIFT_CREDITED};fail_commit=ambiguous=true;assert(!pet_service_gift_commit(&b,0,10)&&state.coins==22);fail_commit=ambiguous=false;boot();assert(state.coins==32&&wallet.book.receipts[0].stage==GIFT_CREDITED);
 pet_gifts_t engine;pet_gifts_init(&engine,&wallet.book,persist,transport,NULL);pet_gifts_tick(&engine,me,"Pet",1,77,true,false,1);uint8_t packet[33]={1,GIFT_COMMIT,1,0};packet[4]=77;packet[8]=50;memcpy(packet+12,me,6);packet[18]=10;pet_gifts_receive(&engine,peer,packet,33);assert(!credits&&replies==1&&state.coins==32);
 clock_us+=61000000;pet_service_tick();boot();assert(state.coins==32&&wallet.book.receipts[0].stage==GIFT_CREDITED&&!memcmp(original,disk,sizeof(original)));
 b=wallet.book;b.outgoing.stage=GIFT_SENT;state.coins=9999;old=writes;assert(!pet_service_gift_commit(&b,0,1)&&writes==old);state.items[0]=9;assert(!pet_service_gift_commit(&b,1,1));state.items[0]=8;assert(pet_service_gift_commit(&b,1,1));boot();assert(state.items[0]==9&&state.coins==9999&&wallet.book.outgoing.stage==GIFT_SENT);
 assert(pet_service_new_egg());assert(pet_service_flush());boot();assert(wallet.book.sequence==1&&wallet.book.receipts[0].stage==GIFT_CREDITED&&state.coins==9999);
 old=writes;disk[3][0]=99;boot();assert(error&&!writable&&!pet_service_gift_commit(&b,0,0)&&writes==old&&disk[3][0]==99);disk[3][0]=1;boot();assert(writable&&state.coins==9999);
 b=wallet.book;b.receipts[1]=b.receipts[0];assert(!gift_book_valid(&b));old=writes;assert(!pet_service_gift_commit(&b,0,0)&&writes==old);fail_set=true;assert(!pet_service_gift_commit(&wallet.book,0,-10)&&state.coins==9999);fail_set=false;
 puts("PASS authoritative gift wallet: single-key pet+ledger commit, legacy keys preserved, rewards/care saves retain ledger, failed/ambiguous commit reboot, duplicate credit guard, balance/inventory caps, future/corrupt records preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-gift-wallet-') as temp:
 p=Path(temp);(p/'test.c').write_text(shim+source+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'firmware/app'),'-I'+str(root/'firmware/protocol'),str(p/'test.c'),str(root/'firmware/app/pet.c'),str(root/'firmware/app/pet_gifts.c'),str(root/'build/mesh-packet-host.o'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True,timeout=30)
