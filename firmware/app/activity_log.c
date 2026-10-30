#include "activity_log.h"
#include "mesh_packet.h"
#include "nvs.h"
#include <string.h>
typedef struct {uint32_t version,sequence;uint8_t count,next,reserved[2];activity_record_t entries[ACTIVITY_LOG_MAX];} log_disk_t;
static log_disk_t s_log;
static bool s_ready,s_error,s_blocked;
static bool valid(const activity_record_t *r){
    if(r->kind<ACTIVITY_POLL || r->kind>ACTIVITY_QUIZ || !r->id || !r->boot || !r->sequence || r->channel<1 || r->channel>16 || !mesh_mac_is_valid_peer(r->host) || !memchr(r->title,0,sizeof(r->title)) || !r->title[0])return false;
    for(const char *p=r->title;*p;p++)if((uint8_t)*p<32 || (uint8_t)*p>126)return false;
    return true;
}
bool activity_log_init(void){
    if(s_ready)return !s_blocked;
    s_ready=true;nvs_handle_t h;esp_err_t rc=nvs_open("activity_log",NVS_READONLY,&h);
    if(rc==ESP_ERR_NVS_NOT_FOUND){s_log.version=1;return true;}
    if(rc!=ESP_OK){s_error=s_blocked=true;return false;}
    log_disk_t disk;size_t n=sizeof(disk);rc=nvs_get_blob(h,"history_v1",&disk,&n);nvs_close(h);
    if(rc==ESP_ERR_NVS_NOT_FOUND){s_log.version=1;return true;}
    if(rc!=ESP_OK || n!=sizeof(disk) || disk.version!=1 || disk.count>ACTIVITY_LOG_MAX || disk.next>=ACTIVITY_LOG_MAX){s_error=s_blocked=true;return false;}
    for(unsigned i=0;i<disk.count;i++){unsigned at=(disk.next+ACTIVITY_LOG_MAX-1-i)%ACTIVITY_LOG_MAX;if(!valid(&disk.entries[at])){s_error=s_blocked=true;return false;}}
    s_log=disk;return true;
}
bool activity_log_append(const activity_record_t *r){
    if(!r || !activity_log_init() || s_blocked)return false;
    for(unsigned i=0;i<s_log.count;i++){const activity_record_t *e=&s_log.entries[(s_log.next+ACTIVITY_LOG_MAX-1-i)%ACTIVITY_LOG_MAX];
        if(e->kind==r->kind && e->id==r->id && e->boot==r->boot && e->part==r->part && !memcmp(e->host,r->host,6))return true;
    }
    log_disk_t next=s_log;activity_record_t entry=*r;entry.sequence=++next.sequence;if(!entry.sequence)entry.sequence=++next.sequence;
    if(!valid(&entry))return false;
    next.entries[next.next]=entry;next.next=(next.next+1)%ACTIVITY_LOG_MAX;if(next.count<ACTIVITY_LOG_MAX)next.count++;
    nvs_handle_t h;esp_err_t rc=nvs_open("activity_log",NVS_READWRITE,&h);
    if(rc==ESP_OK){rc=nvs_set_blob(h,"history_v1",&next,sizeof(next));if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);}
    s_error=rc!=ESP_OK;if(s_error)return false;s_log=next;return true;
}
bool activity_log_get(unsigned i,activity_record_t *out){if(!out || !activity_log_init() || i>=s_log.count)return false;*out=s_log.entries[(s_log.next+ACTIVITY_LOG_MAX-1-i)%ACTIVITY_LOG_MAX];return true;}
unsigned activity_log_count(void){activity_log_init();return s_log.count;}
bool activity_log_error(void){activity_log_init();return s_error;}
