#include "snake_page.h"
#include "snake.h"
#include "pet_service.h"
#include "arcade_store.h"
#include "wououi.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "ui_zh.h"
#include "ui_language.h"
#include <stdio.h>
static snake_t snake;
static bool paused;
static uint32_t last_step;
static unsigned earned;
static bool active,resume_menu,bad_save;
static unsigned choice,map_choice;
static bool map_menu;
static void new_game(void){snake_init_map(&snake,esp_random(),map_choice);arcade_new(ARCADE_SNAKE,&snake);earned=0;resume_menu=bad_save=map_menu=false;}
void snake_page_enter(uint8_t page){
    if(page!=M_SNAKE){if(active)arcade_flush();active=false;return;}
    active=true;snake_init(&snake,esp_random());int found=arcade_load(ARCADE_SNAKE,&snake);resume_menu=found==1;bad_save=found<0;choice=0;
    if(found<0)snake_init(&snake,esp_random());
    map_choice=snake.map;map_menu=found!=1;
    earned=0;paused=true;last_step=(uint32_t)(esp_timer_get_time()/1000);
}
void snake_page_proc(void){
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    if(btn.pressed){
        int key=btn.id;btn.pressed=false;
        if(key==BTN_ID_BACK){arcade_flush();pet_service_flush();if(ui.layer){ui.index=ui.parent_index[ui.layer];ui.state=S_LAYER_OUT;}return;}
        if(map_menu){
            if(key==BTN_ID_CW || key==BTN_ID_CC)map_choice=(map_choice+SNAKE_MAPS+(key==BTN_ID_CW?1:-1))%SNAKE_MAPS;
            if(key==BTN_ID_SP){new_game();paused=false;last_step=now;}
        }else if(resume_menu && (key==BTN_ID_CW || key==BTN_ID_CC))choice^=1;
        else if(resume_menu && key==BTN_ID_SP){if(choice)map_menu=true;resume_menu=false;paused=true;last_step=now;}
        else if(key==BTN_ID_LP || (key==BTN_ID_SP && snake.over)){
            map_menu=true;map_choice=snake.map;paused=true;
        }else if(key==BTN_ID_SP && !bad_save){paused=!paused;last_step=now;}
        else if(!paused){
            if(key==BTN_ID_CW)snake_turn(&snake,1);
            if(key==BTN_ID_CC)snake_turn(&snake,-1);
        }
    }
    if(!paused && !snake.over && now-last_step>=snake_interval(&snake)){snake_step(&snake);last_step=now;}
    if(!map_menu)earned=arcade_tick(ARCADE_SNAKE,&snake,now);
    u8g2_SetDrawColor(&u8g2,1);
    u8g2_DrawVLine(&u8g2,96,0,32);
    u8g2_DrawFrame(&u8g2,0,0,96,32);
    for(unsigned cell=0;cell<SNAKE_CELLS;cell++)if(snake_wall(snake.map,cell))
        u8g2_DrawBox(&u8g2,(cell%SNAKE_W)*4,(cell/SNAKE_W)*4,4,4);
    for(unsigned i=0;i<snake.length;i++){
        int x=(snake.body[i]%SNAKE_W)*4,y=(snake.body[i]/SNAKE_W)*4;
        if(!x)x++;
        if(!y)y++;
        if(!i)u8g2_DrawBox(&u8g2,x,y,3,3);else u8g2_DrawBox(&u8g2,x,y,2,2);
    }
    if(!snake.won){
        int x=(snake.food%SNAKE_W)*4,y=(snake.food/SNAKE_W)*4;
        if(!x)x++;
        if(!y)y++;
        u8g2_DrawFrame(&u8g2,x,y,3,3);
    }
    u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    char score[8];snprintf(score,sizeof(score),"%u",snake.length-3);
    u8g2_DrawStr(&u8g2,99,7,"SCORE");u8g2_DrawStr(&u8g2,99,16,score);
    u8g2_DrawStr(&u8g2,99,24,paused?"PAUSE":(snake.over?"OVER":"PLAY"));
    snprintf(score,sizeof(score),paused?"B%lu":"G%lu",(unsigned long)(paused?arcade_best(ARCADE_SNAKE,false):earned));u8g2_DrawStr(&u8g2,99,32,score);
    if(map_menu){
        u8g2_SetDrawColor(&u8g2,0);u8g2_DrawBox(&u8g2,3,3,90,26);u8g2_SetDrawColor(&u8g2,1);
        if(ui_language_chinese()){u8g2_SetDrawColor(&u8g2,0);u8g2_DrawBox(&u8g2,0,0,96,32);u8g2_SetDrawColor(&u8g2,1);}
        ui_zh_three_line(&u8g2,5,10,ui_language_chinese() && !map_choice?"SNAKE CLASSIC":snake_map_name(map_choice),86,now);
        ui_zh_three_line(&u8g2,5,18,"TURN:MAP SP:START",86,now);
        ui_zh_three_line(&u8g2,5,26,bad_save?"SAVE ERR NEW ONLY":"BACK:KEEP OLD SAVE",86,now);
    }else if(paused || snake.over){
        u8g2_SetDrawColor(&u8g2,0);u8g2_DrawBox(&u8g2,3,3,90,26);u8g2_SetDrawColor(&u8g2,1);
        if(ui_language_chinese()){u8g2_SetDrawColor(&u8g2,0);u8g2_DrawBox(&u8g2,0,0,96,32);u8g2_SetDrawColor(&u8g2,1);}
        ui_zh_three_line(&u8g2,5,10,resume_menu?(choice?" CONTINUE":">CONTINUE"):(snake.over?(snake.won?"YOU WIN!":"GAME OVER"):"SNAKE: SP START"),86,now);
        ui_zh_three_line(&u8g2,5,18,resume_menu?(choice?">NEW GAME":" NEW GAME"):(snake.over?"SP: NEW GAME":"TURN: LEFT/RIGHT"),86,now);
        ui_zh_three_line(&u8g2,5,26,arcade_error(ARCADE_SNAKE)?"SAVE ERR HOLD:NEW":"HOLD:NEW BACK:EXIT",86,now);
    }
}
