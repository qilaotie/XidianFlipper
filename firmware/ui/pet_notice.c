#include "pet_notice.h"
#include "pet_attention.h"
#include "pet_service.h"
#include "pet_page.h"
#include "haptic_feedback.h"
#include "wououi.h"
#include "esp_timer.h"
static pet_attention_t attention;
static bool initialized;
void pet_notice_tick(void){
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    if(!initialized){pet_attention_init(&attention,now);initialized=true;}
    bool safe=ui.index==M_HOME || ui.index==M_MAIN || (ui.index==M_PET && pet_page_allows_attention());
    if(!pet_service_error() && pet_attention_poll(&attention,pet_service_state(),now,safe,haptic_is_busy()))
        haptic_play(HAPTIC_PET_CALL);
}
void pet_notice_draw(void){
    if(ui.index!=M_HOME || pet_service_error())return;
    const char *message=pet_attention_text(&attention,pet_service_state(),(uint32_t)(esp_timer_get_time()/1000));
    if(!message)return;
    u8g2_SetDrawColor(&u8g2,0);u8g2_DrawBox(&u8g2,33,24,95,8);
    u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);u8g2_DrawStr(&u8g2,34,31,message);
}
