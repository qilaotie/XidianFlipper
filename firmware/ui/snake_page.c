#include "snake_page.h"
#include "snake.h"
#include "pet_service.h"
#include "wououi.h"
#include "esp_random.h"
#include "esp_timer.h"
#include <stdio.h>
static snake_t snake;
static bool paused;
static uint32_t last_step;
static unsigned earned;
static void new_game(void){snake_init(&snake,esp_random());pet_service_reward_begin(PET_REWARD_SNAKE);earned=0;}
void snake_page_enter(uint8_t page){
    if(page!=M_SNAKE)return;
    new_game();paused=true;last_step=(uint32_t)(esp_timer_get_time()/1000);
}
void snake_page_proc(void){
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    if(btn.pressed){
        int key=btn.id;btn.pressed=false;
        if(key==BTN_ID_BACK){pet_service_flush();if(ui.layer){ui.index=ui.parent_index[ui.layer];ui.state=S_LAYER_OUT;}return;}
        if(key==BTN_ID_LP || (key==BTN_ID_SP && snake.over)){
            new_game();paused=false;last_step=now;
        }else if(key==BTN_ID_SP){paused=!paused;last_step=now;}
        else if(!paused){
            if(key==BTN_ID_CW)snake_turn(&snake,1);
            if(key==BTN_ID_CC)snake_turn(&snake,-1);
        }
    }
    if(!paused && !snake.over && now-last_step>=snake_interval(&snake)){snake_step(&snake);last_step=now;}
    earned+=pet_service_reward(PET_REWARD_SNAKE,snake.length-3);
    u8g2_SetDrawColor(&u8g2,1);
    u8g2_DrawVLine(&u8g2,96,0,32);
    for(unsigned i=0;i<snake.length;i++){
        int x=(snake.body[i]%SNAKE_W)*4,y=(snake.body[i]/SNAKE_W)*4;
        if(!i)u8g2_DrawBox(&u8g2,x,y,4,4);else u8g2_DrawBox(&u8g2,x,y,3,3);
    }
    if(!snake.won){int x=(snake.food%SNAKE_W)*4,y=(snake.food/SNAKE_W)*4;u8g2_DrawFrame(&u8g2,x,y,3,3);}
    u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    char score[8];snprintf(score,sizeof(score),"%u",snake.length-3);
    u8g2_DrawStr(&u8g2,99,7,"SCORE");u8g2_DrawStr(&u8g2,99,16,score);
    u8g2_DrawStr(&u8g2,99,24,paused?"PAUSE":(snake.over?"OVER":"PLAY"));
    snprintf(score,sizeof(score),"G%u",earned);u8g2_DrawStr(&u8g2,99,32,score);
    if(paused || snake.over){
        u8g2_SetDrawColor(&u8g2,0);u8g2_DrawBox(&u8g2,3,3,90,26);u8g2_SetDrawColor(&u8g2,1);
        u8g2_DrawStr(&u8g2,5,10,snake.over?(snake.won?"YOU WIN!":"GAME OVER"):"SNAKE: SP START");
        u8g2_DrawStr(&u8g2,5,18,snake.over?"SP: NEW GAME":"TURN: LEFT/RIGHT");
        u8g2_DrawStr(&u8g2,5,26,"HOLD:NEW BACK:EXIT");
    }
}
