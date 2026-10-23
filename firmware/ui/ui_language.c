#include "ui_language.h"
#include "nvs.h"
#include <stdint.h>
typedef struct {uint8_t version,chinese,reserved[2];} language_t;
static bool initialized,chinese=true,blocked;
static void load(void){
    if(initialized)return;
    initialized=true;chinese=true;blocked=false;
    nvs_handle_t h;esp_err_t rc=nvs_open("ui_locale",NVS_READONLY,&h);
    if(rc==ESP_OK){
        language_t saved={0};size_t size=sizeof(saved);rc=nvs_get_blob(h,"language_v1",&saved,&size);nvs_close(h);
        if(rc==ESP_OK){
            if(size==sizeof(saved) && saved.version==1 && saved.chinese<=1 && !saved.reserved[0] && !saved.reserved[1])chinese=saved.chinese;
            else blocked=true;
        }else if(rc!=ESP_ERR_NVS_NOT_FOUND)blocked=true;
    }else if(rc!=ESP_ERR_NVS_NOT_FOUND)blocked=true;
}
bool ui_language_chinese(void){load();return chinese;}
bool ui_language_set_chinese(bool enabled){
    load();if(blocked)return false;
    if(chinese==enabled)return true;
    language_t next={.version=1,.chinese=enabled};nvs_handle_t h;
    esp_err_t rc=nvs_open("ui_locale",NVS_READWRITE,&h);
    if(rc==ESP_OK){rc=nvs_set_blob(h,"language_v1",&next,sizeof(next));if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);}
    if(rc!=ESP_OK)return false;
    chinese=enabled;return true;
}
