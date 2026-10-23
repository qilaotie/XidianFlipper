#include "ui_style.h"
#include "nvs.h"
#include <stdint.h>
typedef struct {uint8_t version,skin,widget,reserved;} style_t;
static style_t style={.version=1};
static bool initialized,blocked,error;
static void load(void){
    if(initialized)return;
    initialized=true;style=(style_t){.version=1};blocked=error=false;
    nvs_handle_t h;esp_err_t rc=nvs_open("ui_personal",NVS_READONLY,&h);
    if(rc==ESP_OK){style_t saved={0};size_t n=sizeof(saved);rc=nvs_get_blob(h,"style_v1",&saved,&n);nvs_close(h);
        if(rc==ESP_OK){if(n==sizeof(saved) && saved.version==1 && saved.skin<UI_SKINS && saved.widget<UI_WIDGETS && !saved.reserved)style=saved;else blocked=error=true;}
        else if(rc!=ESP_ERR_NVS_NOT_FOUND)blocked=error=true;
    }else if(rc!=ESP_ERR_NVS_NOT_FOUND)blocked=error=true;
}
unsigned ui_style_skin(void){load();return style.skin;}
unsigned ui_style_widget(void){load();return style.widget;}
bool ui_style_set(unsigned skin,unsigned widget){
    load();if(blocked || skin>=UI_SKINS || widget>=UI_WIDGETS)return false;
    if(style.skin==skin && style.widget==widget){error=false;return true;}
    style_t next={.version=1,.skin=skin,.widget=widget};nvs_handle_t h;
    esp_err_t rc=nvs_open("ui_personal",NVS_READWRITE,&h);
    if(rc==ESP_OK){rc=nvs_set_blob(h,"style_v1",&next,sizeof(next));if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);}
    error=rc!=ESP_OK;if(error)return false;style=next;return true;
}
const char *ui_style_skin_label(void){
    static const char *const names[]={"- Skin: PIXEL","- Skin: MINIMAL","- Skin: TERMINAL"};
    load();return error?"- Style SAVE ERROR":names[style.skin];
}
const char *ui_style_widget_label(void){
    static const char *const names[]={"- Home: MESSAGES","- Home: PET","- Home: SCORES","- Home: AUTO"};
    load();return error?"- Style SAVE ERROR":names[style.widget];
}
