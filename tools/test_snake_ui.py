"""Production snake page with pixel bounds and clock/input mocks, not hardware evidence."""
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'firmware/ui/snake_page.c').read_text(encoding='utf-8')
source=re.sub(r'^#include[^\n]*\n','',source,flags=re.M)
shim=r'''
#include "snake.h"
#include "pet_service.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
enum {M_SNAKE=60,BTN_ID_BACK=1,BTN_ID_CW,BTN_ID_CC,BTN_ID_SP,BTN_ID_LP,S_LAYER_OUT};
static struct {bool pressed;int id;}btn;
static struct {int index,layer,state,parent_index[20];}ui;
static int u8g2,u8g2_font_5x7_tf;
static int64_t clock_us=1000000;
static int64_t esp_timer_get_time(void){return clock_us;}
static uint32_t esp_random(void){return 12345;}
static unsigned reward_mark,reward_gold;
void pet_service_reward_begin(unsigned source){assert(source==PET_REWARD_SNAKE);reward_mark=0;}
unsigned pet_service_reward(unsigned source,unsigned progress){assert(source==PET_REWARD_SNAKE);if(progress<=reward_mark)return 0;unsigned gold=(progress-reward_mark)*2;reward_mark=progress;reward_gold+=gold;return gold;}
bool pet_service_flush(void){return true;}
#include "arcade_ui_stubs.h"
static void u8g2_SetDrawColor(int *p,int c){(void)p;(void)c;}
static void u8g2_SetFont(int *p,int f){(void)p;(void)f;}
static void u8g2_DrawBox(int *p,int x,int y,int w,int h){(void)p;assert(x>=0&&y>=0&&x+w<=128&&y+h<=32);}
static void u8g2_DrawVLine(int *p,int x,int y,int h){u8g2_DrawBox(p,x,y,1,h);}
static void u8g2_DrawFrame(int *p,int x,int y,int w,int h){u8g2_DrawBox(p,x,y,w,h);}
static void u8g2_DrawStr(int *p,int x,int y,const char *s){(void)p;assert(x>=0&&y>=7&&y<=32&&x+strlen(s)*5<=128);}
static bool ui_language_chinese(void){return false;}
static void ui_zh_three_line(int *p,int x,int y,const char *s,unsigned w,unsigned now){(void)w;(void)now;u8g2_DrawStr(p,x,y,s);}
'''
tests=r'''
static void key(int k){btn.id=k;btn.pressed=true;snake_page_proc();}
int main(void){
 ui.layer=2;ui.parent_index[2]=23;ui.index=M_SNAKE;
 snake_page_enter(M_SNAKE);assert(paused);clock_us+=5000000;snake_page_proc();assert(snake.body[0]==102);
 key(BTN_ID_SP);assert(!paused);snake.food=103;
 clock_us+=240000;snake_page_proc();assert(snake.length==4);
 assert(earned==2&&reward_gold==2);snake_page_proc();assert(earned==2&&reward_gold==2);
 key(BTN_ID_SP);assert(paused);uint8_t head=snake.body[0];clock_us+=5000000;snake_page_proc();assert(snake.body[0]==head);
 key(BTN_ID_SP);key(BTN_ID_CW);clock_us+=230000;snake_page_proc();assert(snake.direction==1);
 key(BTN_ID_LP);assert(paused&&map_menu);key(BTN_ID_SP);assert(!paused&&snake.length==3);
 assert(earned==0&&reward_gold==2);
 snake.over=true;key(BTN_ID_SP);assert(map_menu);key(BTN_ID_SP);assert(!snake.over&&!paused);
 key(BTN_ID_BACK);assert(ui.index==23&&ui.state==S_LAYER_OUT);
 snake_page_enter(M_SNAKE);assert(paused&&snake.length==3);
 for(unsigned map=0;map<SNAKE_MAPS;map++){
  key(BTN_ID_LP);map_choice=map;key(BTN_ID_SP);assert(snake.map==map);
  for(unsigned i=0;i<5000;i++){
  clock_us+=250000;if(snake.over){key(BTN_ID_SP);key(BTN_ID_SP);}
  key(i%3?BTN_ID_CW:BTN_ID_CC);snake_page_proc();
  }
 }
 puts("PASS production snake UI: drawing bounds, start/pause clock, growth, turn, restart, exit/reentry, 5000 renders");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-snake-') as temp:
    path=Path(temp);(path/'test.c').write_text(shim+source+tests,encoding='utf-8')
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/app'),'-I',str(root/'tools'),
                    str(path/'test.c'),str(root/'firmware/app/snake.c'),'-o',str(path/'test.exe')],check=True)
    subprocess.run([str(path/'test.exe')],check=True)
