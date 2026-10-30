"""Production guide navigation is read-only; all pet topics and EN rows remain reachable."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=re.sub(r'^#include[^\n]*\n','',(root/'firmware/ui/game_help_page.c').read_text(encoding='utf8'),flags=re.M)
shim=r'''
#include "help_text.h"
#include "pet_help.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
static bool chinese=true;
bool ui_language_chinese(void){return chinese;}
enum {M_GAME_HELP=63,BTN_ID_CW=1,BTN_ID_CC,BTN_ID_SP,BTN_ID_BACK,S_LAYER_OUT};
static struct{bool pressed;int id;}btn;
static struct{unsigned layer,index,num[80],select[8],parent_index[8],state;bool init;}ui;
static struct{float y,y_trg,box_y,box_y_trg[8];bool loop;}list;
typedef struct{char *m_select;}M_SELECT;
static int u8g2,u8g2_font_5x7_tf;
static void list_show(M_SELECT *rows,unsigned page){assert(page==63);assert(ui.select[ui.layer]<ui.num[page]);for(unsigned i=0;i<ui.num[page];i++)assert(rows[i].m_select[0]);}
static void list_update_scroll_targets(void){list.y_trg=-16.0f*ui.select[ui.layer];list.box_y_trg[ui.layer]=0;}
static void list_rotate_switch(void){unsigned n=ui.num[M_GAME_HELP];ui.select[ui.layer]=(ui.select[ui.layer]+(btn.id==BTN_ID_CW?1:n-1))%n;}
static void u8g2_SetDrawColor(int *d,int c){(void)d;(void)c;}
static void u8g2_SetFont(int *d,int f){(void)d;(void)f;}
static void u8g2_DrawStr(int *d,int x,int y,const char *s){(void)d;(void)s;assert(x>=0&&y>=7&&y<=31);}
static void ui_zh_draw(int *d,int x,int y,const char *s){u8g2_DrawStr(d,x,y,s);}
'''
tests=r'''
static void key(int k){btn.id=k;btn.pressed=true;game_help_page_proc();}
int main(void){
 ui.layer=2;ui.index=63;ui.parent_index[2]=23;game_help_page_proc();assert(ui.num[63]==GAME_GUIDES);
 ui.select[2]=10;list.y=-160;list.y_trg=-160;list.loop=true;key(BTN_ID_SP);assert(pets&&!reading);
 assert(ui.init&&list.y==0&&list.y_trg==0&&!list.loop);game_help_page_proc();assert(ui.num[63]==PET_GUIDES);
 for(unsigned lang=0;lang<2;lang++){
  chinese=lang;
  for(unsigned i=0;i<PET_GUIDES;i++){
   ui.select[2]=i;key(BTN_ID_SP);assert(reading&&item==i&&page==0);
   for(unsigned p=0;p<pet_guide_pages(i);p++){game_help_page_proc();key(BTN_ID_CW);}
   assert(page==0);key(BTN_ID_CC);assert(page==pet_guide_pages(i)-1);key(BTN_ID_BACK);assert(!reading&&pets);
  }
 }
 key(BTN_ID_BACK);assert(!pets&&ui.select[2]==10);assert(ui.init&&list.y==-160&&list.y_trg==-160);game_help_page_proc();assert(ui.num[63]==GAME_GUIDES);
 ui.select[2]=255;game_help_page_proc();assert(ui.select[2]==0);
 reading=true;item=255;game_help_page_proc();assert(!reading);
 pets=true;reading=true;item=12;page=3;game_help_page_enter(1);assert(reading&&pets);
 game_help_page_enter(63);assert(!reading&&!pets&&item==0&&page==0&&ui.num[63]==GAME_GUIDES);
 key(BTN_ID_BACK);assert(ui.index==23&&ui.state==S_LAYER_OUT);
 chinese=false;
 for(unsigned i=0;i<9;i++)for(unsigned p=0;p<pet_help_pages(i);p++)for(unsigned r=0;r<3;r++){
  unsigned flat=p*3+r;assert(!strcmp(pet_help_line(i,p,r),pet_guide_line(i+4,flat/2,flat%2)));
 }
 puts("PASS guides: pet category, 13 topics, all pages/both languages/wrap/back, no omitted English rows; no pet service linked");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-guide-') as t:
 p=Path(t);(p/'test.c').write_text(shim+source+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/ui'),'-I',str(root/'firmware/app'),str(p/'test.c'),str(root/'firmware/ui/help_text.c'),str(root/'firmware/app/pet_help.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
