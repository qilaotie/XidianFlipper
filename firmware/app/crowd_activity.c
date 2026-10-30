#include "crowd_activity.h"
#include <string.h>
/* Shares COLLAB v1 header with team_activity; disjoint operations 7..12. */
enum {CHECK_STATE=7,CHECK_REQ,CHECK_ACK,QUIZ_STATE,QUIZ_REQ,QUIZ_ACK};
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static bool eq(const uint8_t *a,const uint8_t *b){return !memcmp(a,b,6);}
static bool title_valid(const char *p){unsigned n=0;if(!p || !*p)return false;while(*p){if((uint8_t)*p<32 || (uint8_t)*p>126 || ++n>32)return false;p++;}return true;}
static bool same(const uint8_t *b,const crowd_room_t *r){return r->active && eq(b+12,r->host) && get32(b+18)==r->boot && get32(b+4)==r->id;}
static void header(crowd_activity_t *s,unsigned kind,uint8_t op,uint8_t b[128]){
    crowd_room_t *r=&s->rooms[kind];memset(b,0,128);b[0]=1;b[1]=op;b[2]=s->channel;put32(b+4,r->id);put32(b+8,r->rev);memcpy(b+12,r->host,6);put32(b+18,r->boot);
}
static void publish(crowd_activity_t *s,unsigned kind){
    crowd_room_t *r=&s->rooms[kind];uint8_t b[128];header(s,kind,kind==CROWD_CHECKIN?CHECK_STATE:QUIZ_STATE,b);
    b[22]=r->open;b[23]=(uint8_t)strlen(r->title);b[24]=r->count;memcpy(b+25,r->winner,6);memcpy(b+31,r->title,b[23]);
    unsigned at=31+b[23];for(unsigned i=0;i<r->count;i++){memcpy(b+at,r->people[i].mac,6);b[at+6]=r->people[i].status;at+=7;}
    s->send(s->ctx,b,(uint8_t)at);
}
void crowd_init(crowd_activity_t *s,const uint8_t self[6],uint32_t boot,team_send_fn send,void *ctx){memset(s,0,sizeof(*s));memcpy(s->self,self,6);s->boot=boot;s->send=send;s->ctx=ctx;}
void crowd_tick(crowd_activity_t *s,uint8_t channel,bool enabled,uint32_t now){
    if(s->channel && channel!=s->channel){memset(s->rooms,0,sizeof(s->rooms));s->pending.active=false;s->result=TEAM_IDLE;memset(s->ledger,0,sizeof(s->ledger));}
    s->channel=channel;s->enabled=enabled;s->now=now;
    if(!enabled){if(s->pending.active){s->pending.active=false;s->result=TEAM_TIMEOUT;s->timeouts++;}return;}
    if(s->pending.active){
        if(now-s->pending.start>=8000){s->pending.active=false;s->result=TEAM_TIMEOUT;s->timeouts++;}
        else if(s->pending.tries<6 && now-s->pending.last>=1000 && s->send(s->ctx,s->pending.bytes,28)){if(s->pending.tries)s->retries++;s->pending.tries++;s->pending.last=now;}
    }
    if(now-s->announce>=2000){s->announce=now;for(unsigned k=0;k<2;k++)if(s->rooms[k].active && s->rooms[k].local)publish(s,k);}
}
bool crowd_create(crowd_activity_t *s,unsigned kind,const char *title){
    if(kind>1 || !s->enabled || s->pending.active || !title_valid(title))return false;
    crowd_room_t *r=&s->rooms[kind];if(r->active && !r->local && s->now-r->seen<15000)return false;
    *r=(crowd_room_t){.active=true,.local=true,.open=true,.boot=s->boot,.id=++s->sequence,.rev=1,.seen=s->now};memcpy(r->host,s->self,6);strcpy(r->title,title);s->result=TEAM_IDLE;publish(s,kind);return true;
}
bool crowd_close(crowd_activity_t *s,unsigned kind){
    if(kind>1 || !s->enabled || !s->rooms[kind].active || !s->rooms[kind].local || !s->rooms[kind].open)return false;
    s->rooms[kind].open=false;s->rooms[kind].rev++;publish(s,kind);return true;
}
static uint8_t apply(crowd_activity_t *s,unsigned kind,const uint8_t mac[6],unsigned status){
    crowd_room_t *r=&s->rooms[kind];if(!r->open)return TEAM_REJECTED;
    if(kind==CROWD_QUIZ){if(status!=1)return TEAM_REJECTED;memcpy(r->winner,mac,6);r->open=false;r->rev++;publish(s,kind);return TEAM_OK;}
    if(status<1 || status>3)return TEAM_REJECTED;
    unsigned i;for(i=0;i<r->count;i++)if(eq(r->people[i].mac,mac))break;
    if(i==r->count){if(r->count==8)return TEAM_REJECTED;r->count++;memcpy(r->people[i].mac,mac,6);}
    r->people[i].status=status;r->rev++;publish(s,kind);return TEAM_OK;
}
bool crowd_request(crowd_activity_t *s,unsigned kind,unsigned status){
    if(kind>1 || !s->enabled || s->pending.active || !s->rooms[kind].active || !s->rooms[kind].open || status<1 || status>(kind==CROWD_CHECKIN?3u:1u))return false;
    if(s->rooms[kind].local){s->result=apply(s,kind,s->self,status);return s->result==TEAM_OK;}
    uint8_t b[128];header(s,kind,kind==CROWD_CHECKIN?CHECK_REQ:QUIZ_REQ,b);b[22]=status;put32(b+24,++s->sequence);
    memcpy(s->pending.bytes,b,28);s->pending.active=true;s->pending.tries=0;s->pending.start=s->now;s->pending.last=s->now-1000;s->result=TEAM_WAIT;return true;
}
void crowd_receive(crowd_activity_t *s,const mesh_packet_t *p){
    const uint8_t *b=p->payload;
    if(!s->enabled || p->type!=MESH_TYPE_COLLAB || p->len<22 || !(p->flags&MESH_FLAG_ROUTED) ||
       p->flags&(MESH_FLAG_ACK_REQUEST|MESH_FLAG_ANNOUNCEMENT) || !mesh_packet_is_broadcast(p->dst_mac) ||
       p->channel!=s->channel || b[0]!=1 || b[2]!=s->channel || b[3] || !get32(b+4) || !get32(b+18) || b[1]<7 || b[1]>12)return;
    unsigned kind=b[1]>=QUIZ_STATE?CROWD_QUIZ:CROWD_CHECKIN;crowd_room_t *r=&s->rooms[kind];
    if(b[1]==CHECK_STATE || b[1]==QUIZ_STATE){
        bool lineage=r->active && eq(b+12,r->host) && get32(b+18)==r->boot;
        if(!eq(b+12,p->src_mac) || get32(b+18)!=p->session_id || r->local ||
           (lineage && get32(b+4)<r->id) || (r->active && !same(b,r) && !(lineage && get32(b+4)>r->id) && s->now-r->seen<15000) || (same(b,r) && get32(b+8)<r->rev))return;
        if(p->len<32 || b[22]>1 || !b[23] || b[23]>32 || b[24]>8 || p->len!=31+b[23]+7*b[24] || (kind==CROWD_QUIZ && b[24]))return;
        crowd_room_t next={.active=true,.open=b[22],.count=b[24],.boot=get32(b+18),.id=get32(b+4),.rev=get32(b+8),.seen=s->now};
        memcpy(next.host,b+12,6);memcpy(next.winner,b+25,6);memcpy(next.title,b+31,b[23]);if(!title_valid(next.title) || strlen(next.title)!=b[23])return;
        if(kind==CROWD_QUIZ && next.open && memcmp(next.winner,(uint8_t[6]){0},6))return;
        if(kind==CROWD_QUIZ && !next.open && memcmp(next.winner,(uint8_t[6]){0},6) && !mesh_mac_is_valid_peer(next.winner))return;
        unsigned at=31+b[23];for(unsigned i=0;i<next.count;i++,at+=7){
            if(!mesh_mac_is_valid_peer(b+at) || b[at+6]<1 || b[at+6]>3)return;
            for(unsigned j=0;j<i;j++)if(eq(next.people[j].mac,b+at))return;
            memcpy(next.people[i].mac,b+at,6);next.people[i].status=b[at+6];
        }*r=next;return;
    }
    if(b[1]==CHECK_REQ || b[1]==QUIZ_REQ){
        if(p->len!=28 || !r->local || !same(b,r) || !get32(b+24) || b[23] || b[22]<1 || b[22]>(kind==CROWD_CHECKIN?3:1))return;
        bool found=false;uint8_t result=TEAM_REJECTED;
        for(unsigned i=0;i<16;i++)if(s->ledger[i].used && eq(s->ledger[i].mac,p->src_mac) && s->ledger[i].boot==p->session_id && s->ledger[i].nonce==get32(b+24)){
            found=true;if(s->ledger[i].id==r->id && s->ledger[i].op==b[1] && s->ledger[i].status==b[22])result=s->ledger[i].result;break;
        }
        if(!found){
            bool old=false;for(unsigned i=0;kind==CROWD_CHECKIN && i<r->count;i++)if(eq(r->people[i].mac,p->src_mac) && s->status_boot[i]==p->session_id && get32(b+24)<=s->status_nonce[i])old=true;
            if(!old)result=apply(s,kind,p->src_mac,b[22]);
            if(result==TEAM_OK && kind==CROWD_CHECKIN)for(unsigned i=0;i<r->count;i++)if(eq(r->people[i].mac,p->src_mac)){s->status_boot[i]=p->session_id;s->status_nonce[i]=get32(b+24);}
            unsigned i=s->cursor++%16;s->ledger[i].used=true;memcpy(s->ledger[i].mac,p->src_mac,6);s->ledger[i].boot=p->session_id;s->ledger[i].nonce=get32(b+24);s->ledger[i].id=r->id;s->ledger[i].op=b[1];s->ledger[i].status=b[22];s->ledger[i].result=result;
        }
        uint8_t ack[128];header(s,kind,kind==CROWD_CHECKIN?CHECK_ACK:QUIZ_ACK,ack);memcpy(ack+22,p->src_mac,6);put32(ack+28,p->session_id);put32(ack+32,get32(b+24));ack[36]=result;s->send(s->ctx,ack,37);return;
    }
    if(p->len==37 && s->pending.active && b[1]==s->pending.bytes[1]+1 && eq(b+22,s->self) && get32(b+28)==s->boot &&
       eq(p->src_mac,s->pending.bytes+12) && p->session_id==get32(s->pending.bytes+18) &&
       get32(b+4)==get32(s->pending.bytes+4) && eq(b+12,p->src_mac) && get32(b+18)==p->session_id && get32(b+32)==get32(s->pending.bytes+24) && (b[36]==TEAM_OK || b[36]==TEAM_REJECTED)){
        s->pending.active=false;s->result=b[36];s->last_ack_ms=s->now-s->pending.start;s->confirmed++;
    }
}
