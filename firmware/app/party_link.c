#include "party_link.h"
#include <stdio.h>
#include <string.h>
/* Public metadata/invites extend COLLAB v1 without exposing private state. */
enum {STATE=20,PRIVATE,REQUEST,ACK,ROSTER,INFO,INVITATION};
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=n>>(8*i);}
static bool eq(const uint8_t *a,const uint8_t *b){return !memcmp(a,b,6);}
static bool table_key(const party_table_t *t,const uint8_t *host,uint32_t boot,uint32_t id){return t->active && eq(t->host,host) && t->host_boot==boot && t->id==id;}
static bool selected(const party_link_t *s,const uint8_t *host,uint32_t boot,uint32_t id){return s->active && eq(s->host,host) && s->host_boot==boot && s->id==id;}
static bool can_switch(const party_link_t *s){
    if(s->pending.active)return false;
    if(!s->active)return true;
    if(s->watching)return true;
    if(s->local)return s->rules.phase==PG_FINISHED;
    return s->now-s->seen>=PARTY_TABLE_TTL || s->view.phase==PG_FINISHED || (!s->joined && s->view.me>=s->view.n);
}
static party_table_t *remember(party_link_t *s,const uint8_t *host,uint32_t boot,uint32_t id,uint32_t revision){
    party_table_t *slot=NULL;
    for(unsigned i=0;i<PARTY_TABLES;i++){
        party_table_t *t=&s->tables[i];
        if(table_key(t,host,boot,id))return revision<t->revision?NULL:t;
        if(t->active && eq(t->host,host) && t->host_boot==boot){if(id<t->id)return NULL;slot=t;}
    }
    if(!slot)for(unsigned i=0;i<PARTY_TABLES;i++)if(!s->tables[i].active){slot=&s->tables[i];break;}
    if(!slot)for(unsigned i=0;i<PARTY_TABLES;i++){
        party_table_t *t=&s->tables[i];if(selected(s,t->host,t->host_boot,t->id))continue;
        if(!slot || s->now-t->seen>s->now-slot->seen)slot=t;
    }
    if(!slot)return NULL;
    memset(slot,0,sizeof(*slot));slot->active=true;memcpy(slot->host,host,6);slot->host_boot=boot;slot->id=id;return slot;
}
static void name_copy(char out[7],const char *name){unsigned i=0;for(;i<6 && name && name[i];i++){unsigned c=(uint8_t)name[i];out[i]=c>=32&&c<=126?(char)c:'?';}out[i]=0;}
static void header(party_link_t *s,uint8_t *b,unsigned op){memset(b,0,128);b[0]=1;b[1]=op;b[2]=s->channel;put32(b+4,s->id);put32(b+8,s->revision);memcpy(b+12,s->host,6);put32(b+18,s->host_boot);}
static void refresh(party_link_t *s){s->revision++;if(!s->revision)s->revision++;s->stage_slot=s->publish_cycle=0;s->publish_at=s->now-400;s->seen=s->now;s->ready=true;}
static void public_encode(party_link_t *s,uint8_t *b){party_view_t v;party_rules_view(&s->rules,255,&v);header(s,b,STATE);b[22]=v.kind;b[23]=v.phase;b[24]=s->n;b[25]=v.turn;b[26]=v.winner;b[27]=v.round;b[28]=(uint8_t)v.direction;b[29]=v.last;b[30]=v.actor;b[31]=v.event;for(unsigned i=0;i<8;i++){b[32+i*2]=v.score[i];b[33+i*2]=v.score[i]>>8;}memcpy(b+48,v.flags,8);memcpy(b+56,v.data,8);memcpy(b+64,v.board,64);}
static void private_encode(party_link_t *s,uint8_t *b,unsigned seat){party_view_t v;party_rules_view(&s->rules,seat,&v);header(s,b,PRIVATE);memcpy(b+22,s->members[seat],6);b[28]=seat;b[29]=v.nh;b[30]=v.role;memcpy(b+31,v.hand,32);memcpy(b+63,v.extra,16);}
static void info_encode(party_link_t *s,uint8_t *b,unsigned op){header(s,b,op);b[22]=s->ready_mask;b[23]=s->rules.kind;b[24]=s->rules.phase;b[25]=s->n;memcpy(b+26,s->names[0],7);put32(b+33,s->invite_nonce);}
static bool publish(party_link_t *s){uint8_t b[128];unsigned total=s->n+3u,slot=(s->stage_slot+s->publish_cycle)%total,len;
    if(!slot){public_encode(s,b);len=128;}
    else if(slot<=s->n){private_encode(s,b,slot-1);len=79;}
    else if(slot==s->n+1u){header(s,b,ROSTER);b[22]=s->n;for(unsigned i=0;i<s->n;i++){memcpy(b+23+i*13,s->members[i],6);memcpy(b+29+i*13,s->names[i],7);}len=23+13*s->n;}
    else {info_encode(s,b,INFO);len=37;}
    if(!s->send(s->ctx,b,len))return false;
    s->stage_slot=(s->stage_slot+1)%total;if(!s->stage_slot)s->publish_cycle=(s->publish_cycle+1)%total;return true;
}
void party_link_init(party_link_t *s,const uint8_t self[6],uint32_t boot,team_send_fn send,void *ctx){memset(s,0,sizeof(*s));memcpy(s->self,self,6);s->boot=boot;s->send=send;s->ctx=ctx;s->view.me=255;}
void party_link_tick(party_link_t *s,uint8_t ch,uint32_t scope,bool enabled,uint32_t now){
    if((s->channel && s->channel!=ch)||s->scope!=scope){s->active=s->local=s->ready=s->pending.active=s->have_public=s->joined=s->watching=false;s->ready_mask=0;s->view.me=255;s->private_rev=0;s->revision=0;s->result=TEAM_IDLE;memset(s->tables,0,sizeof(s->tables));}
    s->channel=ch;s->scope=scope;s->enabled=enabled;s->now=now;
    for(unsigned i=0;i<PARTY_TABLES;i++){if(!enabled || now-s->tables[i].seen>=PARTY_TABLE_TTL)s->tables[i].active=false;if(now-s->tables[i].invite_seen>=60000)s->tables[i].invited=false;}
    if(!enabled){if(s->pending.active){s->pending.active=false;s->result=TEAM_TIMEOUT;}return;}
    if(s->active && s->local){party_table_t *t=remember(s,s->host,s->host_boot,s->id,s->revision);if(t){t->kind=s->rules.kind;t->phase=s->rules.phase;t->n=s->n;t->round=s->rules.round;t->revision=s->revision;t->seen=now;t->modern=true;t->ready_mask=s->ready_mask;memcpy(t->name,s->names[0],7);}}
    if(s->active && s->local && now-s->publish_at>=350){if(publish(s))s->publish_at=now;}
    if(s->pending.active){
        if(now-s->pending.start>=10000){s->pending.active=false;s->result=TEAM_TIMEOUT;}
        else if(s->pending.tries<8 && now-s->pending.last>=1000 && s->send(s->ctx,s->pending.bytes,40)){s->pending.tries++;s->pending.last=now;}
    }
}
bool party_link_create(party_link_t *s,unsigned kind,uint32_t seed,const char *name){
    if(!s->enabled || !party_game_available(kind) || !can_switch(s))return false;
    party_rules_lobby(&s->rules,kind,seed);s->rules.n=1;s->n=1;s->active=s->local=s->ready=true;s->have_public=s->watching=false;s->ready_mask=1;s->invite_nonce=0;s->invite_sent_once=false;s->id=++s->sequence;if(!s->id)s->id=++s->sequence;s->host_boot=s->boot;memcpy(s->host,s->self,6);memcpy(s->members[0],s->self,6);memset(s->member_nonce,0,sizeof(s->member_nonce));name_copy(s->names[0],name);refresh(s);return true;
}
bool party_link_join_table(party_link_t *s,const uint8_t host[6],uint32_t boot,uint32_t id,const char *name){
    if(!s->enabled || !host || s->pending.active)return false;
    if(selected(s,host,boot,id) && !s->watching)return party_link_request(s,PARTY_JOIN,0,0,0,0,name);
    if(!can_switch(s))return false;
    party_table_t target={0};
    for(unsigned i=0;i<PARTY_TABLES;i++)if(table_key(&s->tables[i],host,boot,id))target=s->tables[i];
    if(!target.active || s->now-target.seen>=PARTY_TABLE_TTL || target.phase==PG_FINISHED || !party_game_available(target.kind))return false;
    s->active=true;s->local=s->ready=s->have_public=s->joined=s->watching=false;s->ready_mask=target.ready_mask;s->private_rev=s->roster_rev=0;s->id=target.id;s->host_boot=target.host_boot;s->revision=target.revision;s->seen=target.seen;memcpy(s->host,target.host,6);
    memset(&s->view,0,sizeof(s->view));memset(s->private_bytes,0,sizeof(s->private_bytes));memset(s->names,0,sizeof(s->names));memcpy(s->names[0],target.name,7);
    s->view.me=255;s->view.kind=target.kind;s->view.phase=target.phase;s->view.n=target.n;s->view.round=target.round;
    return party_link_request(s,PARTY_JOIN,0,0,0,0,name);
}
bool party_link_watch_table(party_link_t *s,const uint8_t host[6],uint32_t boot,uint32_t id){
    if(!s->enabled || !host || !can_switch(s))return false;
    party_table_t target={0};for(unsigned i=0;i<PARTY_TABLES;i++)if(table_key(&s->tables[i],host,boot,id))target=s->tables[i];
    if(!target.active || s->now-target.seen>=PARTY_TABLE_TTL || eq(host,s->self))return false;
    s->active=s->watching=true;s->local=s->joined=s->ready=s->have_public=false;s->pending.active=false;s->private_rev=s->roster_rev=0;s->result=TEAM_IDLE;s->ready_mask=target.ready_mask;
    s->id=id;s->host_boot=boot;s->revision=target.revision;s->seen=target.seen;memcpy(s->host,host,6);memset(s->private_bytes,0,sizeof(s->private_bytes));memset(s->names,0,sizeof(s->names));memcpy(s->names[0],target.name,7);
    memset(&s->view,0,sizeof(s->view));s->view.me=255;s->view.kind=target.kind;s->view.phase=target.phase;s->view.n=target.n;return true;
}
bool party_link_stop_watching(party_link_t *s){if(!s->watching)return false;s->active=s->watching=s->ready=s->have_public=false;s->view.me=255;s->ready_mask=0;return true;}
static bool apply(party_link_t *s,unsigned seat,unsigned op,unsigned act,unsigned a,unsigned b,unsigned c){
    bool ok=false;
    if(op==PARTY_START && seat==0 && s->ready_mask==((1u<<s->n)-1))ok=party_rules_start(&s->rules,s->n);
    if(op==PARTY_READY && s->rules.phase==PG_LOBBY && seat<s->n && a<=1){if(a)s->ready_mask|=1u<<seat;else s->ready_mask&=(uint8_t)~(1u<<seat);ok=true;}
    if(op==PARTY_MOVE)ok=party_rules_move(&s->rules,seat,act,a,b,c);
    if((op==PARTY_LEAVE || (op==PARTY_CLOSE&&seat==0)) && seat<s->n){s->rules.phase=PG_FINISHED;s->rules.winner=0;s->rules.event=250;ok=true;}
    if(ok)refresh(s);
    return ok;
}
bool party_link_request(party_link_t *s,unsigned op,unsigned action,unsigned a,unsigned b,unsigned c,const char *name){
    if(!s->enabled || !s->active || s->watching || s->pending.active || op<PARTY_JOIN || op>PARTY_INVITE || a>255 || b>255 || c>255 || action>255)return false;
    if(op==PARTY_INVITE){if(!s->local || s->rules.phase!=PG_LOBBY || (s->invite_sent_once && s->now-s->invite_sent<5000))return false;uint8_t packet[128];s->invite_nonce++;if(!s->invite_nonce)s->invite_nonce++;info_encode(s,packet,INVITATION);if(!s->send(s->ctx,packet,37))return false;s->invite_sent=s->now;s->invite_sent_once=true;return true;}
    if(s->local){bool ok=op==PARTY_JOIN || apply(s,0,op,action,a,b,c);s->result=ok?TEAM_OK:TEAM_REJECTED;return ok;}
    if(s->now-s->seen>=15000 || (op!=PARTY_JOIN && (!s->ready || s->view.me>=s->view.n)))return false;
    uint8_t bytes[128];header(s,bytes,REQUEST);bytes[22]=op;bytes[23]=action;bytes[24]=a;bytes[25]=op==PARTY_READY?s->view.n:b;bytes[26]=c;bytes[27]=s->view.phase;bytes[39]=s->view.round;put32(bytes+28,++s->sequence);name_copy((char *)bytes+32,name);memcpy(s->pending.bytes,bytes,40);s->pending.active=true;s->pending.tries=0;s->pending.start=s->now;s->pending.last=s->now-1000;s->result=TEAM_WAIT;return true;
}
static bool lineage(party_link_t *s,const uint8_t *b){return s->active && eq(s->host,b+12) && s->host_boot==get32(b+18) && s->id==get32(b+4);}
static bool public_valid(const uint8_t *b){
    unsigned kind=b[22],phase=b[23],n=b[24];const uint8_t *data=b+56,*table=b+64;
    if(!party_game_available(kind) || (b[26]>>n) || (phase!=PG_LOBBY&&phase!=PG_FINISHED&&n<party_min(kind)))return false;
    if(kind==PG_CODE){unsigned total=0;for(unsigned p=0;p<n;p++){if(data[p]>24 || data[p]>64/n)return false;total+=data[p];}if(total>24)return false;for(unsigned i=0;i<64;i++)if(table[i]>23&&table[i]<253)return false;}
    if(kind==PG_UNO){if(data[0]>3 || data[1]>4 || data[2]>1)return false;for(unsigned p=0;p<n;p++)if(table[p]>PARTY_HAND)return false;}
    if(kind==PG_YACHT){if(data[0]>3 || data[1]>31)return false;for(unsigned i=0;i<5;i++)if(table[i]>6)return false;}
    if(kind==PG_LIAR)for(unsigned p=0;p<n;p++)if(data[p]>3 || table[p]>5)return false;
    if(kind==PG_AVALON && ((data[0]>>n)||data[1]>3||data[2]>3||data[3]>5||data[4]>5))return false;
    if(kind==PG_TIC || kind==PG_REVERSI)for(unsigned i=0;i<(kind==PG_TIC?9u:64u);i++)if(table[i]>2)return false;
    if(kind==PG_LOVE)for(unsigned i=0;i<16;i++)if(table[i]>8)return false;
    return true;
}
static void private_apply(party_link_t *s,const uint8_t *b){s->view.me=b[6];s->view.nh=b[7];s->view.role=b[8];memcpy(s->view.hand,b+9,32);memcpy(s->view.extra,b+41,16);s->ready=true;}
void party_link_receive(party_link_t *s,const mesh_packet_t *p){
    const uint8_t *b=p->payload;
    if(!s->enabled || p->type!=MESH_TYPE_COLLAB || p->channel!=s->channel || p->len<22 || !(p->flags&MESH_FLAG_ROUTED) || (p->flags&(MESH_FLAG_ACK_REQUEST|MESH_FLAG_ANNOUNCEMENT)) || !mesh_packet_is_broadcast(p->dst_mac) || b[0]!=1 || b[2]!=s->channel || b[3] || b[1]<STATE || b[1]>INVITATION || !get32(b+4) || !get32(b+18))return;
    uint32_t rev=get32(b+8);
    bool from_host=eq(p->src_mac,b+12)&&p->session_id==get32(b+18);
    if(b[1]==INFO || b[1]==INVITATION){
        if(!from_host || !mesh_mac_is_valid_peer(p->src_mac) || p->len!=37 || !rev || !party_game_available(b[23]) || !b[25] || b[25]>party_max(b[23]) || (b[22]>>b[25]) || b[32] || (b[24]>PG_HUNTER && b[24]!=PG_FINISHED) || (b[1]==INVITATION && (b[24]!=PG_LOBBY || !get32(b+33))))return;
        for(unsigned i=26;i<32;i++)if(b[i] && (b[i]<32 || b[i]>126))return;
        party_table_t *t=remember(s,b+12,get32(b+18),get32(b+4),rev);if(!t)return;
        t->kind=b[23];t->phase=b[24];t->n=b[25];t->revision=rev;t->seen=s->now;t->modern=true;t->ready_mask=b[22];name_copy(t->name,(const char *)b+26);
        if(b[1]==INVITATION && get32(b+33)>t->invite_nonce){t->invited=true;t->invite_nonce=get32(b+33);t->invite_seen=s->now;}
        if(lineage(s,b) && !s->local && rev>=s->revision){s->ready_mask=b[22];}
        return;
    }
    if(b[1]==STATE){
        if(!from_host || !mesh_mac_is_valid_peer(p->src_mac) || p->len!=128 || !rev || b[22]>=TABLETOP_GAMES || !b[24] || b[24]>party_max(b[22]) || b[25]>=b[24] || (b[28]!=1&&b[28]!=255) || (b[23]>PG_HUNTER&&b[23]!=PG_FINISHED) ||
           (lineage(s,b)&&(rev<s->revision || (s->have_public&&b[22]!=s->view.kind))) || !public_valid(b))return;
        party_table_t *t=remember(s,b+12,get32(b+18),get32(b+4),rev);if(!t)return;
        t->kind=b[22];t->phase=b[23];t->n=b[24];t->round=b[27];t->revision=rev;t->seen=s->now;if(b[23]!=PG_LOBBY)t->invited=false;
        if(s->local || !lineage(s,b))return;
        bool changed=!lineage(s,b)||rev!=s->revision;
        s->active=true;s->local=false;s->id=get32(b+4);s->host_boot=get32(b+18);memcpy(s->host,b+12,6);s->revision=rev;s->seen=s->now;
        if(changed || !s->have_public){party_view_t *v=&s->view;v->kind=b[22];v->phase=b[23];v->n=b[24];v->turn=b[25];v->winner=b[26];v->round=b[27];v->direction=(int8_t)b[28];v->last=b[29];v->actor=b[30];v->event=b[31];for(unsigned i=0;i<8;i++)v->score[i]=b[32+i*2]|(uint16_t)b[33+i*2]<<8;memcpy(v->flags,b+48,8);memcpy(v->data,b+56,8);memcpy(v->board,b+64,64);s->ready=false;s->have_public=true;}
        if(s->watching){s->ready=true;s->view.me=255;s->view.nh=0;memset(s->view.hand,0,sizeof(s->view.hand));memset(s->view.extra,0,sizeof(s->view.extra));s->view.role=0;}
        else if(s->private_rev==rev)private_apply(s,s->private_bytes);
        return;
    }
    if(b[1]==ROSTER){
        if(!from_host || p->len<36 || !b[22] || b[22]>8 || p->len!=23+13*b[22] || !eq(b+23,b+12))return;
        for(unsigned i=0;i<b[22];i++){if(!mesh_mac_is_valid_peer(b+23+i*13)||b[35+i*13])return;for(unsigned j=0;j<i;j++)if(eq(b+23+i*13,b+23+j*13))return;for(unsigned j=0;j<6;j++)if(b[29+i*13+j] && (b[29+i*13+j]<32 || b[29+i*13+j]>126))return;}
        for(unsigned i=0;i<PARTY_TABLES;i++)if(table_key(&s->tables[i],b+12,get32(b+18),get32(b+4)) && rev>=s->tables[i].revision)name_copy(s->tables[i].name,(const char *)b+29);
        if(!lineage(s,b) || s->local || rev<s->revision)return;
        for(unsigned i=0;i<b[22];i++){name_copy(s->names[i],(const char *)b+29+i*13);}
        s->roster_rev=rev;return;
    }
    if(!lineage(s,b))return;
    if(b[1]==PRIVATE){if(!from_host || s->local || s->watching || p->len!=79 || !eq(b+22,s->self) || b[28]>=8 || b[29]>32 || rev<s->revision || b[30]>8)return;memcpy(s->private_bytes,b+22,57);s->private_rev=rev;if(rev==s->revision && s->have_public && b[28]<s->view.n)private_apply(s,s->private_bytes);return;}
    if(b[1]==REQUEST){
        if(!s->local || p->len!=40 || b[22]<PARTY_JOIN || b[22]>PARTY_READY || b[38] || !get32(b+28) || !mesh_mac_is_valid_peer(p->src_mac))return;
        unsigned seat=8;for(unsigned i=0;i<s->n;i++)if(eq(s->members[i],p->src_mac))seat=i;
        uint8_t result=TEAM_REJECTED;
        if(seat<s->n && b[22]==PARTY_JOIN && s->rules.phase==PG_LOBBY && s->member_boot[seat] && s->member_boot[seat]!=p->session_id){s->ready_mask&=(uint8_t)~(1u<<seat);refresh(s);}
        if(seat==8 && b[22]==PARTY_JOIN && s->rules.phase==PG_LOBBY && s->n<party_max(s->rules.kind)){seat=s->n++;memcpy(s->members[seat],p->src_mac,6);name_copy(s->names[seat],(const char *)b+32);s->rules.n=s->n;s->member_nonce[seat]=0;s->member_boot[seat]=0;s->ready_mask&=1;refresh(s);}
        if(seat<s->n && !(b[22]==PARTY_READY && b[25]!=s->n)){
            bool duplicate=s->member_boot[seat]==p->session_id && s->member_nonce[seat]==get32(b+28);
            bool old=s->member_boot[seat]==p->session_id && s->member_nonce[seat]>get32(b+28);
            if(duplicate)result=s->member_result[seat];
            else if(!old){bool concurrent=(b[22]==PARTY_READY && b[27]==PG_LOBBY && s->rules.phase==PG_LOBBY) || (b[22]==PARTY_MOVE && b[27]==s->rules.phase && b[39]==s->rules.round && (b[23]==PG_VOTE || (s->rules.phase>=PG_NIGHT_WOLF&&s->rules.phase<=PG_NIGHT_WITCH)));bool ok=b[22]==PARTY_JOIN || ((rev==s->revision||concurrent)&&apply(s,seat,b[22],b[23],b[24],b[25],b[26]));result=ok?TEAM_OK:TEAM_REJECTED;s->member_boot[seat]=p->session_id;s->member_nonce[seat]=get32(b+28);s->member_result[seat]=result;}
        }
        uint8_t ack[128];header(s,ack,ACK);memcpy(ack+22,p->src_mac,6);put32(ack+28,p->session_id);put32(ack+32,get32(b+28));ack[36]=result;s->send(s->ctx,ack,37);return;
    }
    if(b[1]==ACK && from_host && !s->local && p->len==37 && s->pending.active && eq(b+22,s->self) && get32(b+28)==s->boot && get32(b+32)==get32(s->pending.bytes+28) && (b[36]==TEAM_OK||b[36]==TEAM_REJECTED)){if(s->pending.bytes[22]==PARTY_JOIN && b[36]==TEAM_OK)s->joined=true;s->pending.active=false;s->result=b[36];if(rev>s->revision)s->ready=false;}
}
void party_link_snapshot(party_link_t *s,party_snapshot_t *v){memset(v,0,sizeof(*v));v->active=s->active;v->local=s->local;v->watching=s->watching;v->ready_mask=s->ready_mask;v->enabled=s->enabled;v->ready=s->ready;v->pending=s->pending.active;v->result=s->result;v->id=s->id;v->host_boot=s->host_boot;v->revision=s->revision;v->seen=s->seen;v->stale=s->active&&!s->local&&s->now-s->seen>=PARTY_TABLE_TTL;if(s->local)party_rules_view(&s->rules,0,&v->game);else v->game=s->view;memcpy(v->names,s->names,sizeof(v->names));memcpy(v->host,s->host,6);memcpy(v->tables,s->tables,sizeof(v->tables));}
