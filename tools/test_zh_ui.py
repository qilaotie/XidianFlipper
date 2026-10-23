"""Actual U8g2 font decoder + production translation; software pixels, not OLED evidence."""
from pathlib import Path
import subprocess
import tempfile
from generate_zh_font import generate,ROOT

text,count,size=generate()
assert (ROOT/'firmware/ui/zh_font.c').read_text(encoding='utf8')==text
wireless=(ROOT/'firmware/app/wireless_tools.c').read_text(encoding='utf8')
assert 'raw_line(7,name)' in wireless and 'raw_line(18,name)' in wireless
assert 's_page==M_WIFI_PASSWORD' in wireless
core=(ROOT/'firmware/ui/wououi.c').read_text(encoding='utf8')
assert '(ui.index==M_WINDOW && win.index==M_SETTING)' in core
test=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
static bool test_chinese=true;
bool ui_language_chinese(void){return test_chinese;}
#include "ui_zh.c"
#include "help_text.c"
static unsigned pixels;
static int low,high;
void u8g2_SetDrawColor(u8g2_t *d,uint8_t c){d->draw_color=c;}
void u8g2_DrawBox(u8g2_t *d,u8g2_uint_t x,u8g2_uint_t y,u8g2_uint_t w,u8g2_uint_t h){(void)d;assert(x+w<=128 && y+h<=32);}
uint8_t u8x8_DrawTile(u8x8_t *d,uint8_t x,uint8_t y,uint8_t n,uint8_t *p){(void)d;(void)x;(void)y;(void)n;(void)p;assert(0);return 0;}
uint8_t u8g2_IsIntersection(u8g2_t *d,u8g2_uint_t x,u8g2_uint_t y,u8g2_uint_t a,u8g2_uint_t b){(void)d;(void)x;(void)y;(void)a;(void)b;return 1;}
void u8g2_DrawHVLine(u8g2_t *d,u8g2_uint_t x,u8g2_uint_t y,u8g2_uint_t n,uint8_t dir){
 (void)d;assert(dir<=3);
 if(dir==0){assert(x+n<=128);assert(y<32);}
 else if(dir==1){assert(x<128);assert(y+n<=32);}
 else if(dir==2){assert(x<128 && x+1>=n);assert(y<32);}
 else {assert(x<128);assert(y<32 && y+1>=n);}
 pixels+=n;if(y<low)low=y;if(y>high)high=y;
}
int main(void){
 u8g2_t d={0};u8g2_SetFontPosBaseline(&d);u8g2_SetFont(&d,halfstep_zh_font);d.font_decode.is_transparent=1;
 unsigned translated=0,too_wide=0;
 for(unsigned i=0;i<sizeof(translations)/sizeof(translations[0]);i++){
  const char *s=translations[i].value;
  for(unsigned n=0;s[n];){
   const unsigned char *p=(const unsigned char *)s+n;
   unsigned code=*p,bytes=1;
   if(*p>=0xe0){code=((*p&15)<<12)|((p[1]&63)<<6)|(p[2]&63);bytes=3;}
   assert(u8g2_GetGlyphWidth(&d,code)>0);n+=bytes;
  }
  assert(!strcmp(ui_zh_lookup(translations[i].key),s));
  unsigned width=ui_zh_width(&d,translations[i].key);
  ui_zh_draw_fit(&d,42,27,translations[i].key,86);
  /* Main tile descriptions intentionally scroll in a clipped viewport. */
  if(width<=124){low=100;high=-1;pixels=0;ui_zh_draw(&d,2,15,translations[i].key);assert(pixels);}
  translated++;
 }
 const char *user="my-ssid-A9";assert(ui_zh_lookup(user)==user);
 assert(ui_zh_width(&d,user)==u8g2_GetStrWidth(&d,user));
 low=100;high=-1;ui_zh_draw(&d,2,10,"MEDIA REMOTE");printf("Title pixels y=%d..%d\n",low,high);
 low=100;high=-1;ui_zh_draw(&d,2,27,"TURN:KEY CLICK:SHOOT");printf("Detail pixels y=%d..%d\n",low,high);
 low=100;high=-1;ui_zh_draw(&d,2,27,"HOLD:PULSE BACK:EXIT");printf("Alternate help pixels y=%d..%d\n",low,high);
 u8g2_SetFont(&d,u8g2_font_5x7_tf);
 for(unsigned lang=0;lang<2;lang++){
  test_chinese=lang;
  for(unsigned item=0;item<9;item++)for(unsigned p=0;p<ui_pet_help_pages(item);p++)for(unsigned r=0;r<(lang?2u:3u);r++){
   const char *s=ui_pet_help_line(item,p,r);assert(s[0]);unsigned w=ui_zh_width(&d,s);
   if(w>126){fprintf(stderr,"Pet help too wide lang%u %u/%u/%u: %u\n",lang,item,p,r,w);too_wide++;continue;}
   ui_zh_draw(&d,1,lang?10+12*r:7+8*r,s);
  }
  for(unsigned item=0;item<GAME_GUIDES;item++)for(unsigned p=0;p<game_guide_pages(item);p++)for(unsigned r=0;r<2;r++){
   const char *s=game_guide_line(item,p,r);assert(s[0]);unsigned w=ui_zh_width(&d,s);
   if(w>126){fprintf(stderr,"Game guide too wide lang%u %u/%u/%u: %u\n",lang,item,p,r,w);too_wide++;continue;}
   ui_zh_draw(&d,1,lang?10+12*r:9+12*r,s);
  }
  for(unsigned item=0;item<PET_GUIDES;item++)for(unsigned p=0;p<pet_guide_pages(item);p++)for(unsigned r=0;r<2;r++){
   const char *s=pet_guide_line(item,p,r);unsigned w=ui_zh_width(&d,s);
   if(w>126){fprintf(stderr,"Pet guide too wide lang%u %u/%u/%u: %u\n",lang,item,p,r,w);too_wide++;continue;}
   ui_zh_draw(&d,1,lang?10+12*r:9+12*r,s);
  }
 }
 assert(!too_wide);
 test_chinese=true;u8g2_SetFontDirection(&d,1);
 for(unsigned i=0;i<4;i++){static const char *const names[]={"PAUSED","CONT","NEW","EXIT"};ui_zh_draw_fit(&d,67-12*i,2,names[i],30);}
 u8g2_SetFontDirection(&d,0);
 test_chinese=false;assert(!strcmp(ui_zh_lookup("SETTINGS"),"SETTINGS"));assert(ui_zh_width(&d,"SETTINGS")==u8g2_GetStrWidth(&d,"SETTINGS"));
 printf("PASS %u translation keys, actual glyph coverage/width/draw, ASCII passthrough and English switch\n",translated);
}
'''
scope=core[core.index('static bool zh_scope(void)'):core.index('/* Restrict localization')]
scope_shim='enum {M_MAIN=1,M_SETTING,M_TOOLS,M_SLEEP_CONFIRM,M_WIFI_CONNECT,M_WIFI_FORGET,M_WINDOW,M_MESSAGE_VIEW,M_MESSAGE_EDIT,M_FUN_HUB,M_GAME_HELP,M_LEARN_HUB,M_MESSAGES_HUB,M_CHANNEL_HUB,M_FRIENDS,M_FRIEND_DETAIL,M_RECIPIENT_PICK,M_ABOUT,M_MESSAGE_COMPOSE,M_INBOX,M_QUICK_MSG,M_WIFI_NETWORKS,M_WIFI_PASSWORD};\nstatic struct {int index;} ui,win;\n'
test=test.replace('static unsigned pixels;',scope_shim+scope+'\nstatic unsigned pixels;')
test=test.replace(' u8g2_t d={0};',' ui.index=M_MESSAGE_VIEW;assert(!zh_scope());ui.index=M_MESSAGE_EDIT;assert(!zh_scope());ui.index=M_WINDOW;win.index=M_MESSAGE_VIEW;assert(!zh_scope());win.index=M_SETTING;assert(zh_scope());ui.index=M_MAIN;assert(zh_scope());\n u8g2_t d={0};')
with tempfile.TemporaryDirectory(prefix='halfstep-zh-') as temp:
    p=Path(temp);(p/'test.c').write_text(test,encoding='utf8')
    src=ROOT/'components/u8g2/csrc'
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
                    '-I',str(ROOT/'firmware/ui'),'-I',str(ROOT/'firmware/app'),'-I',str(src),str(p/'test.c'),str(ROOT/'firmware/app/pet_help.c'),
                    str(ROOT/'firmware/ui/zh_font.c'),str(src/'u8g2_font.c'),str(src/'u8g2_fonts.c'),str(src/'u8x8_8x8.c'),str(src/'u8g2_kerning.c'),
                    '-o',str(p/'test.exe')],check=True)
    subprocess.run([str(p/'test.exe')],check=True,timeout=20)
print(f'PASS reproducible font subset: {count} Han glyphs, {size} bytes')
