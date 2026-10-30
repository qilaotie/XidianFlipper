#include "social.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>
typedef struct {uint32_t version,room;char room_name[19],nickname[13],signature[25],pet[13];uint8_t sharing,count;passport_stamp_t stamps[12];} social_disk_t;
static social_disk_t disk={.version=1};static bool loaded,blocked,error,active,enabled,share_armed;static uint32_t now_ms,advertise,sequence;static uint8_t self_mac[6],channel;
static social_send_fn send_fn;static void *send_ctx;static social_card_t peers[8];
static struct {bool used;uint8_t mac[6],bytes[128],mask,total,hops;uint32_t boot,id,at;} fragments[4];
static struct {bool armed,host;unsigned role,tune,bpm,step;uint32_t armed_at,token,due,host_seen,boot,event;uint8_t host_mac[6];unsigned event_note,event_beat;} music;
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static bool ascii(const char *s,unsigned max,bool empty){if(!s || (!empty && !*s))return false;unsigned n=0;while(*s){if((uint8_t)*s<32 || (uint8_t)*s>126 || ++n>max)return false;s++;}return true;}
static void load(void){
    if(loaded)return;
    loaded=true;nvs_handle_t h;esp_err_t rc=nvs_open("social_user",NVS_READONLY,&h);
    if(rc==ESP_ERR_NVS_NOT_FOUND)return;
    if(rc!=ESP_OK){blocked=error=true;return;}
    social_disk_t d;size_t n=sizeof(d);rc=nvs_get_blob(h,"social_v1",&d,&n);nvs_close(h);if(rc==ESP_ERR_NVS_NOT_FOUND)return;
    bool ok=rc==ESP_OK && n==sizeof(d) && d.version==1 && d.count<=12 && d.sharing<=1;
    if(ok)ok=memchr(d.room_name,0,19) && memchr(d.nickname,0,13) && memchr(d.signature,0,25) && memchr(d.pet,0,13) && ascii(d.room_name,18,true) && ascii(d.nickname,12,true) && ascii(d.signature,24,true) && ascii(d.pet,12,true);
    for(unsigned i=0;ok && i<d.count;i++)ok=mesh_mac_is_valid_peer(d.stamps[i].mac) && memchr(d.stamps[i].pet,0,13) && ascii(d.stamps[i].pet,12,true) && d.stamps[i].visits;
    if(!ok){blocked=error=true;return;}disk=d;
}
static bool save(const social_disk_t *d){
    if(blocked){error=true;return false;}if(!memcmp(d,&disk,sizeof(*d)))return true;
    nvs_handle_t h;esp_err_t rc=nvs_open("social_user",NVS_READWRITE,&h);if(rc==ESP_OK){rc=nvs_set_blob(h,"social_v1",d,sizeof(*d));if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);}error=rc!=ESP_OK;if(error)return false;disk=*d;return true;
}
void social_init(social_send_fn fn,void *ctx){load();send_fn=fn;send_ctx=ctx;}
uint32_t social_scope(void){load();return active?disk.room:0;}
void social_view(social_view_t *v){load();memset(v,0,sizeof(*v));v->room=disk.room;v->active=active;v->sharing=disk.sharing && share_armed;v->error=error;strcpy(v->room_name,disk.room_name);strcpy(v->nickname,disk.nickname);strcpy(v->signature,disk.signature);strcpy(v->pet,disk.pet);v->stamps_count=disk.count;memcpy(v->stamps,disk.stamps,sizeof(v->stamps));memcpy(v->peers,peers,sizeof(peers));}
bool social_room(uint32_t room,const char *name){
    load();if(!room){active=false;share_armed=false;memset(peers,0,sizeof(peers));memset(fragments,0,sizeof(fragments));music.armed=music.host=false;return true;}
    if(!ascii(name,18,false))return false;
    social_disk_t next=disk;next.room=room;strcpy(next.room_name,name);if(!save(&next))return false;active=true;share_armed=false;memset(peers,0,sizeof(peers));memset(fragments,0,sizeof(fragments));music.armed=music.host=false;advertise=now_ms-10000;return true;
}
bool social_edit(unsigned field,const char *text){load();if(field>2 || !ascii(text,field==1?24:12,true))return false;social_disk_t next=disk;strcpy(field==0?next.nickname:field==1?next.signature:next.pet,text);if(!save(&next))return false;advertise=now_ms-10000;return true;}
bool social_share(bool on){load();social_disk_t next=disk;next.sharing=on;if(!save(&next))return false;share_armed=on;advertise=now_ms-10000;return true;}
bool social_stamp(const uint8_t mac[6],const char *name,uint32_t token){
    load();if(!mesh_mac_is_valid_peer(mac) || !token || !ascii(name,12,true))return false;unsigned i;for(i=0;i<disk.count;i++)if(mesh_mac_equal(mac,disk.stamps[i].mac))break;
    if(i==disk.count && i==12)return false;
    social_disk_t next=disk;if(i==disk.count){next.count++;memcpy(next.stamps[i].mac,mac,6);}
    passport_stamp_t *p=&next.stamps[i];if(p->last_token==token)return true;p->last_token=token;if(p->visits!=UINT32_MAX)p->visits++;strcpy(p->pet,name);return save(&next);
}
static bool send(const uint8_t *b,uint8_t n){return enabled && send_fn && send_fn(send_ctx,b,n);}
bool social_wrap(const uint8_t *bytes,uint8_t len){
    uint32_t room=social_scope();if(!room || !bytes || !len || len>128 || !enabled)return false;
    uint8_t b[128]={SOC_FRAGMENT};put32(b+1,room);uint32_t transfer=++sequence;if(!transfer)transfer=++sequence;put32(b+5,transfer);b[9]=len;
    for(unsigned part=0;part<(len>117?2u:1u);part++){unsigned n=len-part*117;if(n>117)n=117;b[10]=part;memcpy(b+11,bytes+117*part,n);if(!send(b,11+n))return false;}return true;
}
static void play(unsigned step,unsigned tune){static const unsigned melodies[3][8]={{0,2,4,7,4,2,0,7},{0,0,7,7,9,9,7,4},{0,4,7,4,2,5,9,5}};music.event_note=melodies[tune][step%8];music.event_beat=step;music.event++;}
void social_ensemble_arm(bool on,unsigned role){music.armed=on;music.host=false;music.role=role%4;music.armed_at=now_ms;music.host_seen=0;memset(music.host_mac,0,6);}
bool social_ensemble_start(unsigned tune,unsigned bpm){if(!music.armed || !enabled || tune>2 || (bpm!=90 && bpm!=120 && bpm!=150))return false;music.host=true;music.tune=tune;music.bpm=bpm;music.step=0;music.token=++sequence;if(!music.token)music.token=++sequence;music.due=now_ms+1500;return true;}
bool social_ensemble_event(uint32_t *seq,unsigned *note,unsigned *role,unsigned *beat){if(!music.armed || *seq==music.event)return false;*seq=music.event;*note=music.event_note;*role=music.role;*beat=music.event_beat;return true;}
void social_tick(const uint8_t self[6],uint8_t ch,bool on,uint32_t now){
    load();memcpy(self_mac,self,6);if(channel && ch!=channel){memset(peers,0,sizeof(peers));memset(fragments,0,sizeof(fragments));music.armed=music.host=false;}channel=ch;enabled=on;now_ms=now;
    if(!on){music.host=false;return;}if(music.armed && now-music.armed_at>60000)music.armed=music.host=false;
    bool sharing=disk.sharing && share_armed;
    if((active || sharing) && now-advertise>=10000){uint8_t b[128]={sharing?SOC_CARD:SOC_MEMBER};put32(b+1,social_scope());memcpy(b+5,disk.nickname,13);memcpy(b+18,disk.signature,25);memcpy(b+43,disk.pet,13);memcpy(b+56,disk.room_name,19);if(!sharing)memset(b+18,0,38);if(send(b,75))advertise=now;}
    if(music.armed && music.host && (int32_t)(now-music.due)>=0){uint8_t b[12]={SOC_BEAT};put32(b+1,social_scope());put32(b+5,music.token);b[9]=music.step;b[10]=music.tune;b[11]=music.bpm;if(send(b,12)){play(music.step,music.tune);music.step++;music.due=now+60000/music.bpm;if(music.step==16)music.host=false;}}
}
bool social_receive(const mesh_packet_t *p,mesh_packet_t *local){
    const uint8_t *b=p->payload;if(!enabled || p->type!=MESH_TYPE_SOCIAL || !(p->flags&MESH_FLAG_ROUTED) || p->flags&(MESH_FLAG_ACK_REQUEST|MESH_FLAG_ANNOUNCEMENT|MESH_FLAG_CUSTODY) || !mesh_packet_is_broadcast(p->dst_mac) || p->channel!=channel || p->len<5 || get32(b+1)!=social_scope())return false;
    if(b[0]==SOC_CHAT){if(p->len<6)return false;*local=*p;local->type=MESH_TYPE_QUICK_MSG;local->flags=0;local->len=p->len-5;memmove(local->payload,b+5,local->len);local->crc16=mesh_packet_compute_crc(local);return true;}
    if(b[0]==SOC_FRAGMENT){
        if(p->len<12 || !get32(b+5) || !b[9] || b[9]>128 || b[10]>1 || (b[10] && b[9]<=117))return false;
        unsigned n=b[9]-b[10]*117;if(n>117)n=117;if(p->len!=11+n)return false;unsigned slot=0;bool found=false;
        for(unsigned i=0;i<4;i++){if(fragments[i].used && fragments[i].boot==p->session_id && fragments[i].id==get32(b+5) && mesh_mac_equal(fragments[i].mac,p->src_mac)){slot=i;found=true;break;}if(!fragments[i].used || now_ms-fragments[i].at>now_ms-fragments[slot].at)slot=i;}
        if(found && now_ms-fragments[slot].at>3000)found=false;
        if(!found){memset(&fragments[slot],0,sizeof(fragments[slot]));fragments[slot].used=true;fragments[slot].boot=p->session_id;fragments[slot].id=get32(b+5);fragments[slot].total=b[9];fragments[slot].at=now_ms;memcpy(fragments[slot].mac,p->src_mac,6);}
        if(fragments[slot].total!=b[9])return false;
        if(fragments[slot].mask&(1<<b[10])){if(memcmp(fragments[slot].bytes+b[10]*117,b+11,n))return false;}
        memcpy(fragments[slot].bytes+b[10]*117,b+11,n);fragments[slot].mask|=1<<b[10];if((p->flags&MESH_HOP_MASK)>fragments[slot].hops)fragments[slot].hops=p->flags&MESH_HOP_MASK;
        if(fragments[slot].mask!=(b[9]>117?3:1))return false;
        *local=*p;local->type=MESH_TYPE_COLLAB;local->len=b[9];local->flags=MESH_FLAG_ROUTED|fragments[slot].hops;memcpy(local->payload,fragments[slot].bytes,local->len);local->crc16=mesh_packet_compute_crc(local);fragments[slot].used=false;return true;
    }
    if((b[0]==SOC_CARD || b[0]==SOC_MEMBER) && p->len==75){
        if(!memchr(b+5,0,13) || !memchr(b+18,0,25) || !memchr(b+43,0,13) || !memchr(b+56,0,19) || !ascii((const char *)b+5,12,true) || !ascii((const char *)b+18,24,true) || !ascii((const char *)b+43,12,true) || !ascii((const char *)b+56,18,true))return false;
        unsigned slot=0;for(unsigned i=0;i<8;i++){if(peers[i].used && mesh_mac_equal(peers[i].mac,p->src_mac)){slot=i;break;}if(!peers[i].used || now_ms-peers[i].seen>now_ms-peers[slot].seen)slot=i;}
        peers[slot]=(social_card_t){.used=true,.seen=now_ms};memcpy(peers[slot].mac,p->src_mac,6);strcpy(peers[slot].nickname,(const char *)b+5);if(b[0]==SOC_CARD){strcpy(peers[slot].signature,(const char *)b+18);strcpy(peers[slot].pet,(const char *)b+43);}return false;
    }
    if(b[0]==SOC_BEAT && p->len==12 && music.armed && !music.host && get32(b+5) && b[9]<16 && b[10]<3 && (b[11]==90 || b[11]==120 || b[11]==150)){
        bool same=mesh_mac_equal(music.host_mac,p->src_mac) && music.boot==p->session_id && music.token==get32(b+5);
        bool lineage=mesh_mac_equal(music.host_mac,p->src_mac) && music.boot==p->session_id;
        if(lineage && get32(b+5)<music.token)return false;
        if(music.host_seen && !same && !(lineage && get32(b+5)>music.token) && now_ms-music.host_seen<15000)return false;
        if(same && b[9]<music.step)return false;
        memcpy(music.host_mac,p->src_mac,6);music.boot=p->session_id;music.token=get32(b+5);music.step=b[9]+1;music.host_seen=now_ms;play(b[9],b[10]);
    }return false;
}
