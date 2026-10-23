#include "wououi.h"
#include "help_text.h"
#include "ui_zh.h"
#include "ui_language.h"
#include <stdio.h>
static bool reading;
static bool pets;
static unsigned item,page;
void game_help_page_enter(unsigned page_index){
    if(page_index!=M_GAME_HELP)return;
    reading=false;
    pets=false;
    item=page=0;
    ui.num[M_GAME_HELP]=GAME_GUIDES;
}
/* This is an in-place category switch, not a new UI layer. Do not replay
 * the list unfold animation using the previous category's scroll offset. */
static void help_list_switch(bool pet_category,unsigned selection){
    pets=pet_category;
    ui.num[M_GAME_HELP]=pets?PET_GUIDES:GAME_GUIDES;
    ui.select[ui.layer]=selection<ui.num[M_GAME_HELP]?selection:0;
    list_update_scroll_targets();
    list.y=list.y_trg;
    list.box_y=list.box_y_trg[ui.layer];
    list.loop=false;
    ui.init=true;
}
void game_help_page_proc(void){
    if(!reading){
        unsigned count=pets?PET_GUIDES:GAME_GUIDES;
        if(ui.select[ui.layer]>=count)help_list_switch(pets,0);
        M_SELECT rows[PET_GUIDES];for(unsigned i=0;i<count;i++)rows[i].m_select=(char *)(pets?pet_guide_name(i):game_guide_name(i));
        ui.num[M_GAME_HELP]=count;list_show(rows,M_GAME_HELP);
        if(btn.pressed){int key=btn.id;btn.pressed=false;
            if(key==BTN_ID_CW || key==BTN_ID_CC)list_rotate_switch();
            if(key==BTN_ID_SP){item=ui.select[ui.layer];page=0;
                if(!pets && item==10)help_list_switch(true,0);
                else reading=true;}
            if(key==BTN_ID_BACK && ui.layer){
                if(pets)help_list_switch(false,10);
                else {ui.index=ui.parent_index[ui.layer];ui.state=S_LAYER_OUT;}}
        }
        return;
    }
    unsigned count=pets?pet_guide_pages(item):game_guide_pages(item);
    if(!count){reading=false;help_list_switch(pets,0);return;}
    page%=count;
    if(btn.pressed){int key=btn.id;btn.pressed=false;
        if(key==BTN_ID_CW)page=(page+1)%count;
        if(key==BTN_ID_CC)page=(page+count-1)%count;
        if(key==BTN_ID_SP || key==BTN_ID_BACK){reading=false;return;}
    }
    u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    for(unsigned row=0;row<2;row++)ui_zh_draw(&u8g2,1,ui_language_chinese()?10+12*row:9+12*row,pets?pet_guide_line(item,page,row):game_guide_line(item,page,row));
    char footer[27];snprintf(footer,sizeof(footer),ui_language_chinese()?"%u/%u < > [OK]":"%u/%u TURN:PAGE SP:BACK",page+1,pets?pet_guide_pages(item):game_guide_pages(item));
    u8g2_DrawStr(&u8g2,1,31,footer);
}
