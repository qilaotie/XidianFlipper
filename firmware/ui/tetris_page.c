#include "tetris_page.h"
#include "tetris.h"
#include "pet_service.h"
#include "arcade_store.h"
#include "wououi.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "ui_zh.h"
#include "ui_language.h"
#include <stdio.h>
#include <string.h>
static tetris_t game;
static bool paused;
static unsigned selection;
static uint32_t last_tick;
static uint32_t last_frame, clear_elapsed;
static unsigned earned;
static bool active,restored,bad_save;
static void new_game(void){tetris_init(&game,esp_random());arcade_new(ARCADE_TETRIS,&game);earned=0;restored=bad_save=false;}
/* Portrait-local 32x128 coordinates; the shared u8g2 orientation never changes. */
static void box(int x,int y,int w,int h){u8g2_DrawBox(&u8g2,128-y-h,x,h,w);}
static const uint16_t letters[]={
0x5BEA,0x3AEB,0x624E,0x3B6B,0x72CF,0x12CF,0x6B4E,0x5BED,
0x7497,0x2B24,0x5AED,0x7249,0x5BFD,0x5FFD,0x2B6A,0x12EB,
0x6F6A,0x5AEB,0x388E,0x2497,0x7B6D,0x2B6D,0x5FED,0x5AAD,
0x24AD,0x72A7
};
static const uint16_t digits[]={0x7B6F,0x749A,0x73E7,0x79E7,0x49ED,0x79CF,0x7BCF,0x24A7,0x7BEF,0x79EF};
static void text(int x,int y,const char *s){
    for(;*s;s++,x+=4){
        uint16_t bits=0;
        if(*s>='A' && *s<='Z')bits=letters[*s-'A'];
        else if(*s>='0' && *s<='9')bits=digits[*s-'0'];
        for(int row=0;row<5;row++)for(int col=0;col<3;col++)if(bits&(1u<<(row*3+col)))box(x+col,y+row,1,1);
    }
}
static void menu_text(int x,int y,const char *s){
    if(!ui_language_chinese()){text(x,y,!strcmp(s,"TETRIS PLAY")?"PLAY":s);return;}
    u8g2_SetFontDirection(&u8g2,1);
    ui_zh_draw_fit(&u8g2,128-y-10,x,s,32-x);
    u8g2_SetFontDirection(&u8g2,0);
}
static void piece(int x,int y,unsigned p,unsigned r,bool ghost){
    uint16_t mask=tetris_shape(p,r);
    for(int row=0;row<4;row++)for(int col=0;col<4;col++)if(mask&(1u<<(row*4+col))){
        int px=1+(x+col)*3,py=42+(y+row)*4;
        if(ghost)box(px+1,py+2,1,1);else box(px,py,2,3);
    }
}
void tetris_page_enter(uint8_t page){
    if(page!=M_TETRIS){if(active)arcade_flush();active=false;return;}
    active=true;int found=arcade_load(ARCADE_TETRIS,&game);restored=found==1;bad_save=found<0;
    if(!found)new_game();else if(found<0)tetris_init(&game,esp_random());
    earned=0;paused=true;selection=bad_save?1:0;last_tick=(uint32_t)(esp_timer_get_time()/1000);
    last_frame=last_tick;clear_elapsed=0;
}
static void leave(void){
    arcade_flush();
    pet_service_flush();
    btn.pressed=false;
    if(ui.layer){ui.index=ui.parent_index[ui.layer];ui.state=S_LAYER_OUT;}
}
void tetris_page_proc(void){
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    if(game.clear_mask && !paused){
        clear_elapsed+=now-last_frame;
        if(clear_elapsed>=360){game.clear_mask=0;clear_elapsed=0;last_tick=now;}
    }
    last_frame=now;
    if(btn.pressed){
        int key=btn.id;btn.pressed=false;
        if(key==BTN_ID_BACK){paused=!paused;selection=0;last_tick=now;}
        else if(paused || game.over){
            if(key==BTN_ID_CW)selection=(selection+1)%3;
            if(key==BTN_ID_CC)selection=(selection+2)%3;
            if(key==BTN_ID_SP){
                if(selection==2){leave();return;}
                if(bad_save && selection==0)return;
                if(selection==1 || game.over){new_game();clear_elapsed=0;}
                paused=false;selection=0;last_tick=now;
            }
        }else if(!game.clear_mask){
            if(key==BTN_ID_CW)tetris_move(&game,1);
            if(key==BTN_ID_CC)tetris_move(&game,-1);
            if(key==BTN_ID_SP)tetris_rotate(&game);
            if(key==BTN_ID_LP){tetris_drop(&game);last_tick=now;}
        }
    }
    if(!paused && !game.over && !game.clear_mask && now-last_tick>=tetris_interval(&game)){tetris_step(&game);last_tick=now;}
    earned=arcade_tick(ARCADE_TETRIS,&game,now);
    u8g2_SetDrawColor(&u8g2,1);
    char value[9];
    text(2,1,paused?"BEST":"TETRIS");
    snprintf(value,sizeof(value),"%lu",(unsigned long)((paused?arcade_best(ARCADE_TETRIS,false):game.score)%1000000));text(2,9,value);
    snprintf(value,sizeof(value),"L%lu",(unsigned long)((paused?arcade_best(ARCADE_TETRIS,true):game.lines)%10000));text(2,17,value);
    text(2,25,"NEXT");
    snprintf(value,sizeof(value),"G%u",earned);text(2,33,value);
    uint16_t next=tetris_shape(game.next,0);
    for(int row=0;row<4;row++)for(int col=0;col<4;col++)if(next&(1u<<(row*4+col)))box(20+col*3,25+row*3,2,2);
    box(0,41,1,82);box(31,41,1,82);box(0,122,32,1);
    for(int row=0;row<TETRIS_H;row++){
        if(game.clear_mask&(1u<<row)){
            if((clear_elapsed/60)%2==0)box(1,42+row*4,30,3);
        }else for(int col=0;col<TETRIS_W;col++)
            if((game.clear_mask?game.clear_rows[row]:game.rows[row])&(1u<<col))box(1+col*3,42+row*4,2,3);
    }
    if(!game.over && !game.clear_mask){
        int y=game.y;while(tetris_fits(&game,game.x,y+1,game.rotation))y++;
        piece(game.x,y,game.piece,game.rotation,true);
        piece(game.x,game.y,game.piece,game.rotation,false);
    }
    if(paused || game.over){
        u8g2_SetDrawColor(&u8g2,0);box(1,49,30,77);u8g2_SetDrawColor(&u8g2,1);
        menu_text(2,51,arcade_error(ARCADE_TETRIS)?"SAVEERR":(game.over?"OVER":"PAUSED"));
        menu_text(5,63,game.over?"NEW":(restored?"CONT":"TETRIS PLAY"));menu_text(5,75,"NEW");menu_text(5,87,"EXIT");
        box(1,64+(int)selection*12,2,3);
        text(2,99,"TURN LR");text(2,107,"TAP ROT");text(2,115,"HOLD DN");
    }
}
