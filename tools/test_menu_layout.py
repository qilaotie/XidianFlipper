"""Production menu label/destination coverage and tile marquee/clipping regression."""
from pathlib import Path
import re
import subprocess
import tempfile
from menu_layout import menu_index
root=Path(__file__).resolve().parents[1]
layout=(root/'firmware/ui/menu_layout.h').read_text(encoding='utf-8')
fun=['M_PET','M_TETRIS','M_SNAKE','M_ANSWER_BOOK','M_DICE','M_CRAPS','M_RPS','M_TREASURE','M_REACTION','M_NUMBER_BOMB','M_TIME_GUESS']
tools=['M_HID_SLIDES','M_HID_MEDIA','M_HID_CAMERA','M_WIFI_CONNECT','M_CLOUD_SYNC','M_WIFI_SCAN','M_BLE_SCAN','M_TOOL_MEMORY']
for group,pages in [('FUN',fun),('TOOLS',tools)]:
    for i,page in enumerate(pages):assert menu_index(group,page)==i
    body=layout.split('#define '+group+'_MENU_ITEMS(X)',1)[1].split('#define',1)[0]
    assert len(re.findall(r'X\(',body))==len(pages)
local=(root/'firmware/ui/local_pages.c').read_text(encoding='utf-8')
wireless=(root/'firmware/app/wireless_tools.c').read_text(encoding='utf-8')
assert 'FUN_MENU_ITEMS(MENU_LABEL)' in local and 'FUN_MENU_ITEMS(MENU_PAGE)' in local
assert 'TOOLS_MENU_ITEMS(MENU_LABEL)' in wireless and 'TOOLS_MENU_ITEMS(MENU_PAGE)' in wireless
source=(root/'firmware/ui/wououi.c').read_text(encoding='utf-8')
start=source.index('static void tile_text(')
end=source.index('/*===========================================================================',source.index('void tile_show(',start))
shim=r'''
#include "tile_marquee.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define DISP_W 128
#define DISP_H 32
#define TILE_ICON_S 1
#define TILE_ICON_W 32
#define TILE_ICON_H 28
#define TILE_ANI 0
#define DARK_MODE 1
static struct {bool init;uint8_t layer,index,num[100],select[20],param[16];}ui;
static struct {float icon_x,icon_x_trg;}tile;
static struct {bool long_text_scroll_enable;}g_config;
static int u8g2,u8g2_font_6x10_tf=6,u8g2_font_5x7_tf=5,font_width;
static uint64_t clock_us=1000000;
static bool clipping;
static int text_x;
static char last_text[80];
static int64_t esp_timer_get_time(void){return (int64_t)clock_us;}
static unsigned message_service_unread_count(void){return 0;}
static void animation(float *v,float *target,int key){(void)key;*v=*target;}
static void u8g2_SetDrawColor(int *p,int c){(void)p;(void)c;}
static void u8g2_SetFont(int *p,int f){(void)p;font_width=f;}
static unsigned u8g2_GetStrWidth(int *p,const char *s){(void)p;return (unsigned)strlen(s)*font_width;}
static void u8g2_SetClipWindow(int *p,int l,int t,int r,int b){(void)p;assert(l>=0&&r<=128&&l<r&&t>=0&&b<=32);clipping=true;}
static void u8g2_SetMaxClipWindow(int *p){(void)p;clipping=false;}
static void u8g2_DrawStr(int *p,int x,int y,const char *s){(void)p;(void)y;assert(clipping);text_x=x;snprintf(last_text,sizeof(last_text),"%s",s);}
static void u8g2_DrawXBMP(int *p,int x,int y,int w,int h,const uint8_t *data){(void)p;(void)x;(void)y;(void)w;(void)h;(void)data;assert(!clipping);}
static void u8g2_DrawBox(int *p,int x,int y,int w,int h){(void)p;(void)x;(void)y;(void)w;(void)h;assert(!clipping);}
static void u8g2_DrawHLine(int *p,int x,int y,int w){u8g2_DrawBox(p,x,y,w,1);}
'''
tests=r'''
int main(void){
 assert(tile_marquee_offset(90,90,10000)==0);
 assert(tile_marquee_offset(100,90,899)==0);
 assert(tile_marquee_offset(100,90,1075)==5);
 assert(tile_marquee_offset(100,90,1250)==10);
 assert(tile_marquee_offset(100,90,2150)==10);
 assert(tile_marquee_offset(100,90,2325)==5);
 assert(tile_marquee_offset(100,90,2500)==0);
 for(unsigned width=91;width<300;width++)for(unsigned t=0;t<20000;t+=17)
  assert(tile_marquee_offset(width,90,t)<=width-90);
 static const uint8_t icons[8][112]={{0}};
 ui.num[0]=8;ui.select[0]=4;ui.param[DARK_MODE]=1;g_config.long_text_scroll_enable=true;
 tile_show(icons);assert(text_x==37&&!strcmp(last_text,"Pet, arcade & party games"));
 clock_us+=1250000;tile_show(icons);assert(text_x==27&&!clipping);
 ui.select[0]=7;tile.icon_x_trg=-7;tile_show(icons);assert(text_x==37);
 clock_us+=1500000;tile_show(icons);assert(text_x<37);
 g_config.long_text_scroll_enable=false;tile_show(icons);assert(text_x==37&&!strcmp(last_text,"Remote / Wi-Fi"));
 ui.select[0]=4;ui.init=false;tile_show(icons);assert(!strcmp(last_text,"Pet & party games"));
 puts("PASS menus: complete unique routes, marquee timing/end pauses/reset, clipping, short fallback");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-menu-') as temp:
    path=Path(temp);(path/'test.c').write_text(shim+source[start:end]+tests,encoding='utf-8')
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/ui'),
                    str(path/'test.c'),'-o',str(path/'test.exe')],check=True)
    subprocess.run([str(path/'test.exe')],check=True)
