"""Compile production portrait page with drawing/input mocks and check every pixel bound."""
from pathlib import Path
import re
import subprocess
import tempfile
from smoke_wireless_tools import png
from smoke_tetris import portrait_png

root = Path(__file__).resolve().parents[1]
source = (root / 'firmware/ui/tetris_page.c').read_text(encoding='utf-8')
source = re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)
shim = r'''
#include "tetris.h"
#include "pet_service.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
enum { M_TETRIS=59, BTN_ID_BACK=1, BTN_ID_CW, BTN_ID_CC, BTN_ID_SP, BTN_ID_LP, S_LAYER_OUT };
static struct {bool pressed;int id;} btn;
static struct {int layer,index,state;int parent_index[20];} ui;
static unsigned char pixels[512];
static unsigned color=1;
static int u8g2;
static bool ui_language_chinese(void){return false;}
static void u8g2_SetFontDirection(int *p,int dir){(void)p;(void)dir;}
static void ui_zh_draw_fit(int *p,int x,int y,const char *s,unsigned w){(void)p;(void)x;(void)y;(void)s;(void)w;}
static uint64_t clock_us=1000000;
static int64_t esp_timer_get_time(void){return (int64_t)clock_us;}
static uint32_t esp_random(void){return 12345;}
static unsigned reward_mark,reward_gold;
void pet_service_reward_begin(unsigned source){assert(source==PET_REWARD_TETRIS);reward_mark=0;}
unsigned pet_service_reward(unsigned source,unsigned progress){assert(source==PET_REWARD_TETRIS);if(progress<=reward_mark)return 0;unsigned gold=(progress-reward_mark)*5;reward_mark=progress;reward_gold+=gold;return gold;}
bool pet_service_flush(void){return true;}
#include "arcade_ui_stubs.h"
static void u8g2_SetDrawColor(int *ctx,unsigned c){(void)ctx;color=c;}
static void u8g2_DrawBox(int *ctx,int x,int y,int w,int h){
    (void)ctx;assert(x>=0 && y>=0 && w>0 && h>0 && x+w<=128 && y+h<=32);
    for(int r=y;r<y+h;r++)for(int c=x;c<x+w;c++){
        unsigned char mask=(unsigned char)(1u<<(r%8));
        if(color)pixels[r/8*128+c]|=mask;else pixels[r/8*128+c]&=(unsigned char)~mask;
    }
}
'''
tests = r'''
static void render(void){memset(pixels,0,sizeof(pixels));tetris_page_proc();}
static void key(int k){btn.id=k;btn.pressed=true;render();}
int main(int argc,char **argv){
    assert(argc==2);ui.layer=2;ui.parent_index[2]=23;ui.index=M_TETRIS;
    tetris_page_enter(M_TETRIS);assert(paused && !game.over);render();
    clock_us+=20000000;render();assert(game.y==0);
    key(BTN_ID_SP);assert(!paused);
    int x=game.x;key(BTN_ID_CW);assert(game.x==x+1);key(BTN_ID_CC);assert(game.x==x);
    key(BTN_ID_SP);assert(game.rotation==1);
    key(BTN_ID_LP);assert(game.score>0);assert(game.y==0);
    key(BTN_ID_BACK);assert(paused);int y=game.y;clock_us+=20000000;render();assert(game.y==y);
    key(BTN_ID_SP);assert(!paused);clock_us+=900000;render();assert(game.y==y+1);
    FILE *out=fopen(argv[1],"wb");assert(out);assert(fwrite(pixels,1,512,out)==512);fclose(out);
    key(BTN_ID_BACK);key(BTN_ID_CW);key(BTN_ID_SP);assert(!paused && game.score==0 && game.y==0);
    game.over=true;key(BTN_ID_CW);key(BTN_ID_SP);assert(!game.over && !paused);
    key(BTN_ID_BACK);key(BTN_ID_CC);key(BTN_ID_SP);assert(ui.index==23 && ui.state==S_LAYER_OUT);
    tetris_page_enter(M_TETRIS);assert(paused && !game.over && game.score==0);
    key(BTN_ID_SP);game.piece=1;game.rotation=0;game.x=3;game.y=0;
    game.rows[18]=game.rows[19]=(uint16_t)(0x3FF&~(3u<<4));
    key(BTN_ID_LP);assert(game.lines==2 && game.clear_mask==((1u<<18)|(1u<<19)));
    assert(earned==10 && reward_gold==10);render();render();assert(earned==10&&reward_gold==10);
    assert(game.clear_rows[18]==0x3FF && game.rows[18]==0);
    unsigned char bright[512];memcpy(bright,pixels,sizeof(bright));
    clock_us+=60000;render();assert(memcmp(bright,pixels,sizeof(bright))!=0);
    x=game.x;unsigned rotation=game.rotation;uint32_t score=game.score;
    key(BTN_ID_CW);key(BTN_ID_SP);key(BTN_ID_LP);
    assert(game.x==x && game.rotation==rotation && game.score==score);
    key(BTN_ID_BACK);assert(paused);unsigned elapsed=clear_elapsed;
    clock_us+=2000000;render();assert(clear_elapsed==elapsed && game.clear_mask);
    key(BTN_ID_BACK);clock_us+=299000;render();assert(game.clear_mask);
    clock_us+=1000;render();assert(!game.clear_mask && game.y==0);
    clock_us+=819000;render();assert(game.y==0);
    clock_us+=1000;render();assert(game.y==1);
    for(unsigned i=0;i<20000;i++){
        if(game.over){tetris_page_enter(M_TETRIS);key(BTN_ID_SP);}
        clock_us+=25000;
        key(i%7==0?BTN_ID_LP:(i%3==0?BTN_ID_SP:(i%2?BTN_ID_CC:BTN_ID_CW)));
    }
    puts("PASS portrait UI: bounds, controls, restart/exit, clear flash/snapshot/360ms, input guard, paused animation, 20000 rendered inputs");
    return 0;
}
'''
raw = root / 'build/tetris-host.raw'
with tempfile.TemporaryDirectory(prefix='halfstep-tetris-') as temp:
    harness = Path(temp) / 'test.c'
    harness.write_text(shim + source + tests, encoding='utf-8')
    executable = Path(temp) / 'test.exe'
    subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fno-stack-protector',
                    '-I', str(root / 'firmware/app'), '-I', str(root / 'tools'), str(harness),
                    str(root / 'firmware/app/tetris.c'), '-o', str(executable)], check=True)
    subprocess.run([str(executable), str(raw)], check=True)
(root / 'build/tetris-host.png').write_bytes(png(raw.read_bytes()))
(root / 'build/tetris-host-portrait.png').write_bytes(portrait_png(raw.read_bytes()))
