/* Local-only ASCII drafts. No transport, cloud or global storage ABI changes. */
#include "draft_store.h"
#include "nvs.h"
#include <stdint.h>
#include <string.h>
typedef struct {
    uint8_t version;
    char work[DRAFT_TEXT_BYTES];
    char slots[DRAFT_SLOTS][DRAFT_TEXT_BYTES];
} drafts_t;
static drafts_t data={.version=1};
static bool initialized,blocked;
static bool valid_text(const char *text){
    if(!text)return false;
    for(unsigned i=0;i<DRAFT_TEXT_BYTES;i++){
        unsigned char c=(unsigned char)text[i];
        if(!c)return true;
        if(c<32 || c>126)return false;
    }
    return false;
}
static void load(void){
    if(initialized)return;
    initialized=true;blocked=false;data=(drafts_t){.version=1};
    nvs_handle_t h;esp_err_t rc=nvs_open("msg_drafts",NVS_READONLY,&h);
    if(rc==ESP_OK){
        drafts_t saved={0};size_t n=sizeof(saved);
        rc=nvs_get_blob(h,"drafts_v1",&saved,&n);nvs_close(h);
        if(rc==ESP_OK){
            bool ok=n==sizeof(saved) && saved.version==1 && valid_text(saved.work);
            for(unsigned i=0;i<DRAFT_SLOTS;i++)ok=ok&&valid_text(saved.slots[i]);
            if(ok)data=saved;else blocked=true;
        }else if(rc!=ESP_ERR_NVS_NOT_FOUND)blocked=true;
    }else if(rc!=ESP_ERR_NVS_NOT_FOUND)blocked=true;
}
const char *draft_store_work(void){load();return data.work;}
const char *draft_store_slot(unsigned slot){load();return slot<DRAFT_SLOTS?data.slots[slot]:"";}
static bool save(const drafts_t *next){
    if(blocked)return false;
    if(!memcmp(&data,next,sizeof(data)))return true;
    nvs_handle_t h;esp_err_t rc=nvs_open("msg_drafts",NVS_READWRITE,&h);
    if(rc==ESP_OK){rc=nvs_set_blob(h,"drafts_v1",next,sizeof(*next));if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);}
    if(rc!=ESP_OK)return false;
    data=*next;return true;
}
bool draft_store_save_work(const char *text){
    load();if(!valid_text(text))return false;
    drafts_t next=data;memset(next.work,0,sizeof(next.work));strcpy(next.work,text);
    return save(&next);
}
bool draft_store_save_slot(unsigned slot,const char *text){
    load();if(slot>=DRAFT_SLOTS || !valid_text(text))return false;
    drafts_t next=data;memset(next.slots[slot],0,sizeof(next.slots[slot]));strcpy(next.slots[slot],text);
    return save(&next);
}
