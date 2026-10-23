"""Compile real widget renderer against bounded drawing and read-only record mocks."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=re.sub(r'^#include[^\n]*\n','',(root/'firmware/ui/home_widgets.c').read_text(encoding='utf8'),flags=re.M)
shim=r'''
#include "pet.h"
#include "ui_style.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {ARCADE_TETRIS,ARCADE_SNAKE};
static int u8g2,u8g2_font_5x7_tf;static unsigned widget,reads;static int64_t clock_us;
static pet_t pet={.fullness=100,.energy=100,.mood=100,.clean=100,.coins=9999};
unsigned ui_style_widget(void){return widget;}
static int64_t esp_timer_get_time(void){return clock_us;}
static void pet_service_tick(void){}
static const pet_t *pet_service_state(void){return &pet;}
static const char *pet_service_name(void){return "abcdefghijkl";}
static bool arcade_record_best(unsigned game,uint32_t *score,uint32_t *value){(void)game;reads++;*score=0xffffffffu;*value=0xffffffffu;return true;}
static void u8g2_SetFont(int *p,int f){(void)p;(void)f;}
static void u8g2_DrawStr(int *p,int x,int y,const char *s){(void)p;assert(x>=0&&y<32&&x+(int)strlen(s)*6<=128);}
static void ui_zh_draw(int *p,int x,int y,const char *s){u8g2_DrawStr(p,x,y,s);}
static void u8g2_DrawBox(int *p,int x,int y,int w,int h){(void)p;assert(x>=0&&y>=0&&x+w<=128&&y+h<=32);}
static void u8g2_DrawFrame(int *p,int x,int y,int w,int h){u8g2_DrawBox(p,x,y,w,h);}
static void u8g2_DrawRFrame(int *p,int x,int y,int w,int h,int r){(void)r;u8g2_DrawBox(p,x,y,w,h);}
static void u8g2_DrawHLine(int *p,int x,int y,int w){u8g2_DrawBox(p,x,y,w,1);}
static void u8g2_DrawVLine(int *p,int x,int y,int h){u8g2_DrawBox(p,x,y,1,h);}
static void u8g2_DrawLine(int *p,int x,int y,int a,int b){u8g2_DrawBox(p,x,y,1,1);u8g2_DrawBox(p,a,b,1,1);}
'''
tests=r'''
int main(void){
 widget=UI_WIDGET_MESSAGES;assert(!home_widget_draw(16)&&reads==0);
 widget=UI_WIDGET_PET;assert(home_widget_draw(16)&&reads==0);clock_us=4000000;assert(home_widget_draw(16));
 widget=UI_WIDGET_SCORES;assert(home_widget_draw(16)&&reads==2);assert(home_widget_draw(16)&&reads==2);
 clock_us+=2000000;assert(home_widget_draw(16)&&reads==4);
 widget=UI_WIDGET_AUTO;
 for(unsigned second=0;second<24;second++){clock_us=(int64_t)second*1000000;assert(home_widget_draw(16)==(second>=8));}
 puts("PASS widgets: 128x32 bounds/max counters, passive records, 2s read cache, 8s carousel");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-widget-') as t:
 p=Path(t);(p/'test.c').write_text(shim+source+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/app'),'-I',str(root/'firmware/ui'),str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
