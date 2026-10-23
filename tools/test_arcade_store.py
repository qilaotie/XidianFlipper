"""Production arcade storage with NVS fault injection and a mocked pet sink, not hardware."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=re.sub(r'^#include[^\n]*\n','',(root/'firmware/app/arcade_store.c').read_text(encoding='utf-8'),flags=re.M)
shim=r'''
#include "arcade_store.h"
#include "snake.h"
#include "tetris.h"
#include "pet_service.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef int nvs_handle_t;
typedef int esp_err_t;
enum {ESP_OK,ESP_ERR_NVS_NOT_FOUND,NVS_READONLY,NVS_READWRITE};
static unsigned char disk[2][512],stage[512];static size_t sizes[2],stage_size;
static bool fail_commit,fail_pet;static unsigned writes,pet_mark[2],claims,training;
static pet_t pet,pet_disk;
static int nvs_open(const char *name,int mode,nvs_handle_t *h){assert(!strcmp(name,"arcade_user"));(void)mode;*h=0;return ESP_OK;}
static int nvs_get_blob(int h,const char *key,void *v,size_t *n){(void)h;unsigned k=!strcmp(key,"snake_v1");if(!sizes[k])return ESP_ERR_NVS_NOT_FOUND;assert(*n>=sizes[k]);memcpy(v,disk[k],sizes[k]);*n=sizes[k];return ESP_OK;}
static unsigned stage_kind;
static int nvs_set_blob(int h,const char *key,const void *v,size_t n){(void)h;stage_kind=!strcmp(key,"snake_v1");assert(n<=512);memcpy(stage,v,n);stage_size=n;return ESP_OK;}
static int nvs_commit(int h){(void)h;if(fail_commit)return 9;memcpy(disk[stage_kind],stage,stage_size);sizes[stage_kind]=stage_size;writes++;return ESP_OK;}
static void nvs_close(int h){(void)h;}
void pet_service_tick(void){}
bool pet_service_error(void){return false;}
const pet_t *pet_service_state(void){return &pet;}
void pet_service_reward_restore(unsigned k,unsigned p){pet_mark[k]=p;}
unsigned pet_service_reward(unsigned k,unsigned p){
 if(p<=pet_mark[k])return 0;
 unsigned threshold=k?5:4;if(pet_mark[k]<threshold && p>=threshold)training++;
 unsigned amount=(p-pet_mark[k])*(k?2:5),room=9999-pet.coins;if(amount>room)amount=room;
 pet_mark[k]=p;pet.coins+=amount;claims++;return amount;
}
bool pet_service_flush(void){if(fail_pet)return false;pet_disk=pet;return true;}
'''
tests=r'''
static void reboot(void){memset(loaded,0,sizeof(loaded));memset(dirty,0,sizeof(dirty));memset(blocked,0,sizeof(blocked));pet=pet_disk;}
int main(void){
 snake_t s,restored;snake_init(&s,123);assert(!arcade_load(1,&restored));assert(arcade_new(1,&s));
 unsigned initial=writes;arcade_tick(1,&s,1000);assert(writes==initial);
 s.food=103;snake_step(&s);assert(s.length==4);assert(arcade_tick(1,&s,1100)==2 && pet.coins==2);
 assert(arcade_best(1,false)==1);initial=claims;assert(arcade_flush());reboot();assert(arcade_load(1,&restored)==1);
 assert(!memcmp(&s,&restored,sizeof(s)));arcade_tick(1,&restored,1200);assert(claims==initial && pet.coins==2);
 restored.food=104;snake_step(&restored);fail_commit=true;arcade_tick(1,&restored,1300);assert(pet.coins==2 && arcade_error(1));
 fail_commit=false;arcade_tick(1,&restored,1400);assert(pet.coins==4 && !arcade_error(1));
 /* Claim is durable before wallet: simulate power loss when pet flush fails. */
 fail_pet=true;restored.food=105;snake_step(&restored);arcade_tick(1,&restored,1500);assert(pet.coins==6);
 fail_pet=false;reboot();assert(pet.coins==4 && arcade_load(1,&restored)==1);initial=claims;
 arcade_tick(1,&restored,1600);assert(claims==initial && pet.coins==4);
 unsigned best=arcade_best(1,false);snake_init(&s,124);assert(arcade_new(1,&s));assert(arcade_best(1,false)==best);
 s.food=103;snake_step(&s);arcade_tick(1,&s,1700);assert(pet.coins==6);
 /* Corruption/unknown versions preserved; explicit NEW is the only overwrite. */
 unsigned char backup[512];memcpy(backup,disk[1],512);disk[1][0]=99;initial=writes;
 assert(arcade_load(1,&restored)==-1);arcade_tick(1,&s,99999);arcade_flush();assert(writes==initial && disk[1][0]==99);
 memcpy(disk[1],backup,512);record_t bad;memcpy(&bad,disk[1],sizeof(bad));bad.game.snake.body[1]=bad.game.snake.body[0];bad.checksum=hash(&bad);memcpy(disk[1],&bad,sizeof(bad));
 assert(arcade_load(1,&restored)==-1);assert(arcade_new(1,&s));
 tetris_t t,out;tetris_init(&t,123);assert(!arcade_load(0,&out));assert(arcade_new(0,&t));
 t.lines=4;t.score=400;t.clear_mask=1;t.clear_rows[0]=1023;
 unsigned wins=training;arcade_tick(0,&t,2000);assert(training==wins+1);
 assert(arcade_load(0,&out)==1 && out.lines==4 && !out.clear_mask && !out.clear_rows[0]);
 initial=claims;arcade_tick(0,&out,2100);assert(claims==initial && training==wins+1);
 out.lines++;out.score=500;arcade_tick(0,&out,2200);assert(training==wins+1);
 out.over=true;arcade_tick(0,&out,2300);assert(arcade_load(0,&t)==0 && arcade_best(0,false)==500 && arcade_best(0,true)==5);
 /* Cloud summary reads must not load games or mutate reward/save ownership. */
 reboot();uint32_t cloud_score,cloud_progress;initial=writes;unsigned paid=claims;
 assert(arcade_record_best(0,&cloud_score,&cloud_progress) && cloud_score==500 && cloud_progress==5);
 assert(!loaded[0] && writes==initial && claims==paid);
 assert(arcade_record_best(1,&cloud_score,&cloud_progress) && cloud_progress==cloud_score+3);
 assert(!loaded[1] && writes==initial && claims==paid);
 memcpy(&bad,disk[0],sizeof(bad));bad.game.tetris.piece=9;bad.checksum=hash(&bad);memcpy(disk[0],&bad,sizeof(bad));assert(arcade_load(0,&t)==-1);
 /* New maps round-trip in the old zeroed padding without changing ABI. */
 assert(sizeof(snake_t)==208 && offsetof(snake_t,map)==206);
 for(unsigned map=0;map<SNAKE_MAPS;map++){
  snake_init_map(&s,123,map);assert(arcade_new(1,&s));reboot();assert(arcade_load(1,&restored)==1);
  assert(restored.map==map && !memcmp(&s,&restored,sizeof(s)));
 }
 memcpy(&bad,disk[1],sizeof(bad));bad.game.snake.map=99;bad.checksum=hash(&bad);memcpy(disk[1],&bad,sizeof(bad));assert(arcade_load(1,&s)==-1);
 puts("PASS arcade storage: resume, best records, new round, claim-before-credit, failed writes, corruption/shape checks, cleared animation, read-only cloud summary, no duplicate coins/training");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-arcade-') as temp:
 p=Path(temp);(p/'test.c').write_text(shim+source+tests,encoding='utf-8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/app'),str(p/'test.c'),str(root/'firmware/app/snake.c'),str(root/'firmware/app/tetris.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True,timeout=20)
