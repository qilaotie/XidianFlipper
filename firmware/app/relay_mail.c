#include "relay_mail.h"
#include "nvs.h"
#include <string.h>
typedef struct {uint8_t used,cancelled,src[6],dst[6],flags;uint32_t session,id,ack_session,ack_id;} receipt_mark_t;
typedef struct {uint32_t version;relay_mail_entry_t entries[4];receipt_mark_t marks[8];uint8_t cursor;} relay_disk_t;
static relay_disk_t disk={.version=1};static bool loaded,blocked,error,enabled;
static uint32_t due[4],last_take,minute_at,stored,delivered,expired;static bool taken,clocked;static unsigned cursor;
static mesh_packet_t replay;static bool replay_pending;
static uint32_t get32(const uint8_t *b){return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static void put32(uint8_t *b,uint32_t v){for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(v>>(8*i));}
static bool same(const receipt_mark_t *m,const mesh_packet_t *p){return m->used && m->session==p->session_id && m->id==p->msg_id && mesh_mac_equal(m->src,p->src_mac) && mesh_mac_equal(m->dst,p->dst_mac);}
static bool valid(const relay_disk_t *d){
    if(d->version!=1 || d->cursor>=8)return false;
    for(unsigned i=0;i<4;i++)if(d->entries[i].minutes){const relay_mail_entry_t *e=&d->entries[i];const mesh_packet_t *p=&e->packet;
        if(e->minutes>30 || e->attempts>120 || !mesh_packet_verify_crc(p) || p->type!=MESH_TYPE_QUICK_MSG || !(p->flags&MESH_FLAG_CUSTODY) || !p->len || !p->session_id || !p->msg_id || !mesh_mac_is_valid_peer(p->src_mac) || !mesh_mac_is_valid_peer(p->dst_mac))return false;
        for(unsigned j=0;j<p->len;j++)if(p->payload[j]<32 || p->payload[j]>126)return false;
    }
    for(unsigned i=0;i<8;i++)if(d->marks[i].used){const receipt_mark_t *m=&d->marks[i];if(m->used!=1 || m->cancelled>1 || !m->session || !m->id || !mesh_mac_is_valid_peer(m->src) || !mesh_mac_is_valid_peer(m->dst) || (!m->cancelled && (!m->ack_session || !m->ack_id)))return false;}
    return true;
}
static void load(void){
    if(loaded)return;
    loaded=true;nvs_handle_t h;esp_err_t rc=nvs_open("relay_mail",NVS_READONLY,&h);
    if(rc==ESP_ERR_NVS_NOT_FOUND)return;
    if(rc!=ESP_OK){blocked=error=true;return;}
    relay_disk_t d;size_t n=sizeof(d);rc=nvs_get_blob(h,"cache_v1",&d,&n);nvs_close(h);
    if(rc==ESP_ERR_NVS_NOT_FOUND)return;
    if(rc!=ESP_OK || n!=sizeof(d) || !valid(&d)){blocked=error=true;return;}disk=d;
}
static bool save(const relay_disk_t *d){
    if(blocked){error=true;return false;}if(!memcmp(d,&disk,sizeof(*d)))return true;
    nvs_handle_t h;esp_err_t rc=nvs_open("relay_mail",NVS_READWRITE,&h);
    if(rc==ESP_OK){rc=nvs_set_blob(h,"cache_v1",d,sizeof(*d));if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);}
    error=rc!=ESP_OK;if(error)return false;disk=*d;return true;
}
void relay_mail_enable(bool on,uint32_t now){load();enabled=on;minute_at=now;clocked=true;for(unsigned i=0;i<4;i++)due[i]=now+3000;}
void relay_mail_view(relay_mail_view_t *v){load();*v=(relay_mail_view_t){.enabled=enabled,.error=error,.stored=stored,.delivered=delivered,.expired=expired};for(unsigned i=0;i<4;i++)if(disk.entries[i].minutes)v->count++;}
bool relay_mail_get(unsigned i,relay_mail_entry_t *out){load();if(i>=4 || !out)return false;*out=disk.entries[i];return true;}
bool relay_mail_remove(unsigned i){load();if(i>=4)return false;relay_disk_t next=disk;memset(&next.entries[i],0,sizeof(next.entries[i]));return save(&next);}
bool relay_mail_cancelled(const mesh_packet_t *p){load();for(unsigned i=0;i<8;i++)if(same(&disk.marks[i],p) && disk.marks[i].cancelled)return true;return false;}
void relay_mail_observe(const mesh_packet_t *p,const uint8_t self[6],uint32_t now){
    load();if(!enabled || blocked || !mesh_packet_verify_crc(p))return;
    if(p->type==MESH_TYPE_CUSTODY_CANCEL && p->len==14 && !p->channel && mesh_packet_is_broadcast(p->dst_mac) && (p->flags&MESH_FLAG_ROUTED)){
        mesh_packet_t original={.session_id=get32(p->payload+6),.msg_id=get32(p->payload+10)};memcpy(original.src_mac,p->src_mac,6);memcpy(original.dst_mac,p->payload,6);
        if(!original.session_id || !original.msg_id || !mesh_mac_is_valid_peer(original.dst_mac))return;
        for(unsigned i=0;i<8;i++)if(same(&disk.marks[i],&original))return;
        relay_disk_t next=disk;unsigned at=next.cursor++%8;next.cursor%=8;receipt_mark_t *m=&next.marks[at];*m=(receipt_mark_t){.used=1,.cancelled=1,.session=original.session_id,.id=original.msg_id};memcpy(m->src,original.src_mac,6);memcpy(m->dst,original.dst_mac,6);
        for(unsigned i=0;i<4;i++){mesh_packet_t *q=&next.entries[i].packet;if(next.entries[i].minutes && q->session_id==m->session && q->msg_id==m->id && mesh_mac_equal(q->src_mac,m->src) && mesh_mac_equal(q->dst_mac,m->dst))memset(&next.entries[i],0,sizeof(next.entries[i]));}save(&next);return;
    }
    if(p->type==MESH_TYPE_RECEIPT && p->len==8 && !p->channel){
        for(unsigned i=0;i<4;i++){const relay_mail_entry_t *e=&disk.entries[i];const mesh_packet_t *q=&e->packet;
            if(!e->minutes || q->session_id!=get32(p->payload) || q->msg_id!=get32(p->payload+4) || !mesh_mac_equal(q->dst_mac,p->src_mac) || !mesh_mac_equal(q->src_mac,p->dst_mac))continue;
            relay_disk_t next=disk;unsigned at=next.cursor++%8;next.cursor%=8;receipt_mark_t *m=&next.marks[at];*m=(receipt_mark_t){.used=1,.session=q->session_id,.id=q->msg_id,.ack_session=p->session_id,.ack_id=p->msg_id,.flags=p->flags};memcpy(m->src,q->src_mac,6);memcpy(m->dst,q->dst_mac,6);memset(&next.entries[i],0,sizeof(next.entries[i]));if(save(&next))delivered++;return;
        }return;
    }
    if(p->type!=MESH_TYPE_QUICK_MSG || !(p->flags&MESH_FLAG_CUSTODY) || !p->len || !p->session_id || !p->msg_id || !mesh_mac_is_valid_peer(p->src_mac) || !mesh_mac_is_valid_peer(p->dst_mac) || mesh_mac_equal(p->src_mac,self) || mesh_mac_equal(p->dst_mac,self) || ((p->flags&MESH_HOP_MASK)>>MESH_HOP_SHIFT)>=2)return;
    for(unsigned j=0;j<p->len;j++)if(p->payload[j]<32 || p->payload[j]>126)return;
    for(unsigned i=0;i<8;i++)if(same(&disk.marks[i],p)){
        const receipt_mark_t *m=&disk.marks[i];unsigned hops=(m->flags&MESH_HOP_MASK)>>MESH_HOP_SHIFT;
        if(!m->cancelled && hops<2){uint8_t b[8];put32(b,m->session);put32(b+4,m->id);if(mesh_packet_pack(&replay,m->dst,m->src,MESH_TYPE_RECEIPT,b,8)){replay.flags=MESH_FLAG_ROUTED|((hops+1)<<MESH_HOP_SHIFT);replay.session_id=m->ack_session;replay.msg_id=m->ack_id;replay.crc16=mesh_packet_compute_crc(&replay);replay_pending=true;}}
        return;
    }
    unsigned slot=4;for(unsigned i=0;i<4;i++){const mesh_packet_t *q=&disk.entries[i].packet;if(disk.entries[i].minutes && q->session_id==p->session_id && q->msg_id==p->msg_id && mesh_mac_equal(q->src_mac,p->src_mac) && mesh_mac_equal(q->dst_mac,p->dst_mac))return;if(!disk.entries[i].minutes)slot=i;}
    if(slot==4)return;
    relay_disk_t next=disk;next.entries[slot]=(relay_mail_entry_t){.minutes=30,.packet=*p};mesh_packet_t *q=&next.entries[slot].packet;q->flags=(q->flags&~MESH_HOP_MASK)|((((q->flags&MESH_HOP_MASK)>>MESH_HOP_SHIFT)+1)<<MESH_HOP_SHIFT);q->crc16=mesh_packet_compute_crc(q);
    if(save(&next)){stored++;due[slot]=now+3000;}
}
bool relay_mail_take(mesh_packet_t *out,uint32_t now){
    load();if(!enabled || blocked || !out)return false;
    if(!clocked){minute_at=now;clocked=true;}
    if(now-minute_at>=60000){relay_disk_t next=disk;unsigned elapsed=(now-minute_at)/60000;for(unsigned i=0;i<4;i++)if(next.entries[i].minutes){if(next.entries[i].minutes<=elapsed){memset(&next.entries[i],0,sizeof(next.entries[i]));expired++;}else next.entries[i].minutes-=elapsed;}if(!save(&next)){minute_at=now;return false;}minute_at=now;}
    if(taken && now-last_take<10000)return false;
    if(replay_pending){*out=replay;replay_pending=false;taken=true;last_take=now;return true;}
    for(unsigned n=0;n<4;n++){unsigned i=(cursor+n)%4;relay_mail_entry_t *e=&disk.entries[i];if(!e->minutes || (int32_t)(now-due[i])<0)continue;
        relay_disk_t next=disk;if(e->attempts>=120){memset(&next.entries[i],0,sizeof(next.entries[i]));if(save(&next))expired++;continue;}
        next.entries[i].attempts++;due[i]=now+30000;if(!save(&next))return false;*out=disk.entries[i].packet;last_take=now;taken=true;cursor=(i+1)%4;return true;
    }return false;
}
