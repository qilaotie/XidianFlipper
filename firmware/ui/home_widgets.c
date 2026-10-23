#include "home_widgets.h"
#include "ui_style.h"
#include "ui_zh.h"
#include "wououi.h"
#include "pet_service.h"
#include "arcade_store.h"
#include "esp_timer.h"
#include <stdio.h>
bool home_widget_draw(unsigned unread){
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);unsigned widget=ui_style_widget();
    if(widget==UI_WIDGET_AUTO)widget=(now/8000)%3;
    if(widget==UI_WIDGET_MESSAGES)return false;
    char line[24];u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    if(unread){snprintf(line,sizeof(line),"+%u",unread);u8g2_DrawStr(&u8g2,76,7,line);}
    if(widget==UI_WIDGET_PET){
        pet_service_tick();const pet_t *p=pet_service_state();
        u8g2_DrawRFrame(&u8g2,3,17,23,12,3);
        u8g2_DrawLine(&u8g2,5,18,7,14);u8g2_DrawLine(&u8g2,22,18,24,14);
        if(p->sleeping){u8g2_DrawHLine(&u8g2,8,22,4);u8g2_DrawHLine(&u8g2,18,22,4);}
        else {u8g2_DrawBox(&u8g2,9,21,2,2);u8g2_DrawBox(&u8g2,19,21,2,2);}
        u8g2_DrawHLine(&u8g2,13,26,4);
        const char *name=pet_service_name();
        if(name[0])u8g2_DrawStr(&u8g2,34,21,name);else ui_zh_draw(&u8g2,34,21,"Pet status");
        if((now/4000)%2)snprintf(line,sizeof(line),"M%u C%u G%u",p->mood,p->clean,p->coins);
        else snprintf(line,sizeof(line),"F%u E%u G%u",p->fullness,p->energy,p->coins);
        u8g2_DrawStr(&u8g2,34,31,line);
    }else{
        static uint32_t sampled,tetris,snake,length,unused;
        static bool ready,has_tetris,has_snake;
        if(!ready || now-sampled>=2000){
            ready=true;sampled=now;
            has_tetris=arcade_record_best(ARCADE_TETRIS,&tetris,&unused);
            has_snake=arcade_record_best(ARCADE_SNAKE,&snake,&length);
        }
        u8g2_DrawFrame(&u8g2,6,16,15,9);u8g2_DrawVLine(&u8g2,13,25,4);u8g2_DrawHLine(&u8g2,7,30,14);
        u8g2_DrawLine(&u8g2,3,17,3,22);u8g2_DrawLine(&u8g2,3,22,6,24);
        u8g2_DrawLine(&u8g2,24,17,24,22);u8g2_DrawLine(&u8g2,24,22,21,24);
        if(has_tetris)snprintf(line,sizeof(line),"T %lu",(unsigned long)tetris);else snprintf(line,sizeof(line),"T --");
        u8g2_DrawStr(&u8g2,34,21,line);
        if(has_snake)snprintf(line,sizeof(line),(now/4000)%2?"LEN %lu":"S %lu",(unsigned long)((now/4000)%2?length:snake));else snprintf(line,sizeof(line),"S --");
        u8g2_DrawStr(&u8g2,34,31,line);
    }
    return true;
}
