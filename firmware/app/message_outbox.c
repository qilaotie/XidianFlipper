#include "message_outbox.h"
#include "nvs.h"
#include <string.h>
typedef struct {uint32_t version;outbox_entry_t entries[OUTBOX_SLOTS];} outbox_disk_t;
static outbox_disk_t store={.version=1};
static bool loaded,blocked,error;
static uint32_t due[OUTBOX_SLOTS],last_take;
static bool taken;
static unsigned cursor;
static bool valid_entry(const outbox_entry_t *e){
    if(e->state>OUTBOX_HELD || e->rounds>OUTBOX_ROUNDS_MAX)return false;
    if(!e->state)return true;
    const mesh_packet_t *p=&e->packet;
    if(!mesh_packet_verify_crc(p) || p->type!=MESH_TYPE_QUICK_MSG || !p->len ||
       p->channel || (p->flags&~MESH_FLAG_CUSTODY)!=(MESH_FLAG_ROUTED|MESH_FLAG_ACK_REQUEST) ||
       !p->session_id || !p->msg_id || !mesh_mac_is_valid_peer(p->src_mac) || !mesh_mac_is_valid_peer(p->dst_mac))return false;
    for(unsigned i=0;i<p->len;i++)if(p->payload[i]<32 || p->payload[i]>126)return false;
    return true;
}
static void load(void){
    if(loaded)return;
    loaded=true;blocked=error=false;store=(outbox_disk_t){.version=1};taken=false;cursor=0;
    for(unsigned i=0;i<OUTBOX_SLOTS;i++)due[i]=30000;
    nvs_handle_t h;esp_err_t rc=nvs_open("msg_outbox",NVS_READONLY,&h);
    if(rc==ESP_OK){
        outbox_disk_t saved={0};size_t n=sizeof(saved);rc=nvs_get_blob(h,"outbox_v1",&saved,&n);nvs_close(h);
        if(rc==ESP_OK){
            bool ok=n==sizeof(saved)&&saved.version==1;
            for(unsigned i=0;i<OUTBOX_SLOTS;i++)ok=ok&&valid_entry(&saved.entries[i]);
            if(ok)store=saved;else blocked=error=true;
        }else if(rc!=ESP_ERR_NVS_NOT_FOUND)blocked=error=true;
    }else if(rc!=ESP_ERR_NVS_NOT_FOUND)blocked=error=true;
}
static bool save(const outbox_disk_t *next){
    if(blocked){error=true;return false;}
    if(!memcmp(next,&store,sizeof(store))){error=false;return true;}
    nvs_handle_t h;esp_err_t rc=nvs_open("msg_outbox",NVS_READWRITE,&h);
    if(rc==ESP_OK){rc=nvs_set_blob(h,"outbox_v1",next,sizeof(*next));if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);}
    error=rc!=ESP_OK;if(error)return false;
    store=*next;return true;
}
bool message_outbox_get(unsigned slot,outbox_entry_t *out){
    load();if(slot>=OUTBOX_SLOTS || !out)return false;
    *out=store.entries[slot];return true;
}
bool message_outbox_error(void){load();return error;}
int message_outbox_add(const mesh_packet_t *p,uint32_t now){
    load();if(!p)return -1;
    outbox_entry_t e={.state=OUTBOX_WAITING,.packet=*p};if(!valid_entry(&e))return -1;
    unsigned slot=OUTBOX_SLOTS;
    for(unsigned i=0;i<OUTBOX_SLOTS;i++)if(!store.entries[i].state){slot=i;break;}
    if(slot==OUTBOX_SLOTS)return -1; /* Never evict a waiting/delivered user record. */
    outbox_disk_t next=store;next.entries[slot]=e;
    if(!save(&next))return -1;
    due[slot]=now;return (int)slot;
}
int message_outbox_take(mesh_packet_t *p,uint32_t now){
    load();if(!p || blocked || (taken && now-last_take<10000))return -1;
    for(unsigned step=0;step<OUTBOX_SLOTS;step++){
        unsigned i=(cursor+step)%OUTBOX_SLOTS;
        outbox_entry_t *e=&store.entries[i];
        if(e->state!=OUTBOX_WAITING || (int32_t)(now-due[i])<0)continue;
        outbox_disk_t next=store;
        if(e->rounds>=OUTBOX_ROUNDS_MAX){
            next.entries[i].state=OUTBOX_HELD;if(!save(&next))due[i]=now+30000;continue;
        }
        next.entries[i].rounds++;
        due[i]=now+30000;
        if(!save(&next))return -1; /* Persist attempt budget BEFORE any radio send. */
        *p=store.entries[i].packet;last_take=now;taken=true;cursor=(i+1)%OUTBOX_SLOTS;return (int)i;
    }
    return -1;
}
bool message_outbox_ack(const uint8_t peer[6],uint32_t session,uint32_t id){
    load();if(!peer)return false;
    for(unsigned i=0;i<OUTBOX_SLOTS;i++){
        const outbox_entry_t *e=&store.entries[i];
        if((e->state==OUTBOX_WAITING || e->state==OUTBOX_HELD) && e->packet.session_id==session && e->packet.msg_id==id && mesh_mac_equal(e->packet.dst_mac,peer)){
            outbox_disk_t next=store;next.entries[i].state=OUTBOX_DELIVERED;return save(&next);
        }
    }
    return false;
}
bool message_outbox_retry(unsigned slot,uint32_t now){
    load();if(slot>=OUTBOX_SLOTS || (store.entries[slot].state!=OUTBOX_WAITING && store.entries[slot].state!=OUTBOX_HELD))return false;
    outbox_disk_t next=store;next.entries[slot].state=OUTBOX_WAITING;next.entries[slot].rounds=0;
    if(!save(&next))return false;
    due[slot]=now;return true;
}
bool message_outbox_remove(unsigned slot){
    load();if(slot>=OUTBOX_SLOTS)return false;
    outbox_disk_t next=store;memset(&next.entries[slot],0,sizeof(next.entries[slot]));return save(&next);
}
bool message_outbox_hold(unsigned slot){
    load();if(slot>=OUTBOX_SLOTS || store.entries[slot].state!=OUTBOX_WAITING)return false;
    outbox_disk_t next=store;next.entries[slot].state=OUTBOX_HELD;return save(&next);
}
