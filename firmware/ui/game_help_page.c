#include "wououi.h"
#include "help_text.h"
#include "ui_zh.h"
#include "ui_language.h"
#include <stdio.h>
static bool reading;
static unsigned item,page;
void game_help_page_proc(void){
    if(!reading){
        M_SELECT rows[GAME_GUIDES];for(unsigned i=0;i<GAME_GUIDES;i++)rows[i].m_select=game_guide_name(i);
        ui.num[M_GAME_HELP]=GAME_GUIDES;list_show(rows,M_GAME_HELP);
        if(btn.pressed){int key=btn.id;btn.pressed=false;
            if(key==BTN_ID_CW || key==BTN_ID_CC)list_rotate_switch();
            if(key==BTN_ID_SP){item=ui.select[ui.layer];page=0;reading=true;}
            if(key==BTN_ID_BACK && ui.layer){ui.index=ui.parent_index[ui.layer];ui.state=S_LAYER_OUT;}
        }
        return;
    }
    if(btn.pressed){int key=btn.id;btn.pressed=false;unsigned count=game_guide_pages(item);
        if(key==BTN_ID_CW)page=(page+1)%count;
        if(key==BTN_ID_CC)page=(page+count-1)%count;
        if(key==BTN_ID_SP || key==BTN_ID_BACK){reading=false;return;}
    }
    u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    for(unsigned row=0;row<2;row++)ui_zh_draw(&u8g2,1,ui_language_chinese()?10+12*row:9+12*row,game_guide_line(item,page,row));
    char footer[27];snprintf(footer,sizeof(footer),"%u/%u TURN:PAGE SP:BACK",page+1,game_guide_pages(item));
    u8g2_DrawStr(&u8g2,1,31,footer);
}
