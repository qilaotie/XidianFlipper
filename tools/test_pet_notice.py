"""Production notice routing + reminder policy; mocks the shared haptic sink, not audio hardware."""
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'firmware/ui/pet_notice.c').read_text(encoding='utf-8')
source=re.sub(r'^#include[^\n]*\n','',source,flags=re.M)
shim=r'''
#include "pet_attention.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {M_HOME=1,M_MAIN=2,M_PET=61,M_MORSE=6,M_REMOTE=43,M_TETRIS=59,HAPTIC_PET_CALL=25};
static struct{unsigned index;}ui;
static int u8g2,u8g2_font_5x7_tf;
static pet_t p;
static int64_t clock_us;
static unsigned sounds,banners;
static bool busy,error,game,muted;
static int64_t esp_timer_get_time(void){return clock_us;}
static bool pet_page_allows_attention(void){return !game;}
static const pet_t *pet_service_state(void){return &p;}
static bool pet_service_error(void){return error;}
static bool haptic_is_busy(void){return busy;}
static void haptic_play(unsigned pattern){assert(pattern==HAPTIC_PET_CALL);if(!muted)sounds++;}
static void u8g2_SetDrawColor(int *ctx,int color){(void)ctx;(void)color;}
static void u8g2_SetFont(int *ctx,int font){(void)ctx;(void)font;}
static void u8g2_DrawBox(int *ctx,int x,int y,int w,int h){(void)ctx;assert(x>=33&&y>=24&&x+w<=128&&y+h<=32);}
static void u8g2_DrawStr(int *ctx,int x,int y,const char *s){(void)ctx;assert(x>=33&&y==31&&x+strlen(s)*5<=128);banners++;}
'''
tests=r'''
int main(void){
 pet_new(&p);pet_advance(&p,60);p.fullness=0;ui.index=M_MORSE;pet_notice_tick();
 clock_us=11000000;pet_notice_tick();assert(!sounds);ui.index=M_REMOTE;pet_notice_tick();assert(!sounds);
 ui.index=M_TETRIS;pet_notice_tick();assert(!sounds);ui.index=M_PET;game=true;pet_notice_tick();assert(!sounds);
 game=false;busy=true;pet_notice_tick();assert(!sounds);busy=false;ui.index=M_HOME;
 pet_notice_tick();assert(sounds==1);pet_notice_draw();assert(banners==1);
 pet_notice_tick();assert(sounds==1);clock_us+=4000000;pet_notice_draw();assert(banners==1);
 clock_us+=176000000;muted=true;pet_notice_tick();assert(sounds==1);pet_notice_draw();assert(banners==2);
 p.spare=1;clock_us+=180000000;pet_notice_tick();pet_notice_draw();assert(sounds==1&&banners==2);
 p.spare=0;muted=false;p.sleeping=1;pet_notice_tick();assert(sounds==1);
 p.sleeping=0;error=true;pet_notice_tick();assert(sounds==1);error=false;pet_notice_tick();assert(sounds==2);
 puts("PASS pet notice: safe-page whitelist, gameplay defer, busy/mute, banner geometry, asleep/calls-off/storage-error");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-notice-') as temp:
    path=Path(temp);(path/'test.c').write_text(shim+source+tests,encoding='utf-8')
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/app'),str(path/'test.c'),
                    str(root/'firmware/app/pet.c'),str(root/'firmware/app/pet_attention.c'),'-o',str(path/'test.exe')],check=True)
    subprocess.run([str(path/'test.exe')],check=True)
