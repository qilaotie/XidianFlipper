#include "team_activity.h"
#include <string.h>
enum {POLL_STATE=1,VOTE_REQ,VOTE_ACK,BOARD_STATE,TASK_REQ,TASK_ACK};
static uint32_t get32(const uint8_t *b){return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static void put32(uint8_t *b,uint32_t v){for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(v>>(i*8));}
static bool eq(const uint8_t *a,const uint8_t *b){return !memcmp(a,b,6);}
static bool text(const char *s,unsigned max){if(!s || !*s)return false;unsigned n=0;while(*s){if((uint8_t)*s<32 || (uint8_t)*s>126 || ++n>max)return false;s++;}return true;}
static void header(team_activity_t *s,uint8_t *b,uint8_t op,bool poll){
    memset(b,0,128);b[0]=1;b[1]=op;b[2]=s->channel;
    const uint8_t *host=poll?s->poll.host:s->board.host;
    put32(b+4,poll?s->poll.id:s->board.id);put32(b+8,poll?s->poll.rev:s->board.rev);
    memcpy(b+12,host,6);put32(b+18,poll?s->poll.boot:s->board.boot);
}
static void publish(team_activity_t *s,bool poll){
    uint8_t b[128];header(s,b,poll?POLL_STATE:BOARD_STATE,poll);
    if(poll){b[22]=s->poll.open;b[23]=s->poll.options;b[24]=(uint8_t)strlen(s->poll.question);memcpy(b+25,s->poll.counts,4);b[29]=s->poll.voters;memcpy(b+30,s->poll.question,b[24]);s->send(s->ctx,b,30+b[24]);}
    else {for(unsigned i=0;i<4;i++){uint8_t *p=b+22+i*26;team_task_t *t=&s->board.tasks[i];p[0]=t->state;p[1]=(uint8_t)strlen(t->title);memcpy(p+2,t->owner,6);memcpy(p+8,t->title,p[1]);}s->send(s->ctx,b,126);}
}
void team_init(team_activity_t *s,const uint8_t self[6],uint32_t boot,team_send_fn send,void *ctx){memset(s,0,sizeof(*s));memcpy(s->self,self,6);s->boot=boot;s->send=send;s->ctx=ctx;}
void team_tick(team_activity_t *s,uint8_t channel,bool enabled,uint32_t now){
    if(s->channel && s->channel!=channel){memset(&s->poll,0,sizeof(s->poll));memset(&s->board,0,sizeof(s->board));s->pending.active=false;s->result=TEAM_IDLE;memset(s->ledger,0,sizeof(s->ledger));}
    s->channel=channel;s->now=now;s->enabled=enabled;
    if(!enabled){if(s->pending.active){s->pending.active=false;s->result=TEAM_TIMEOUT;s->timeouts++;}return;}
    if(s->pending.active){if(now-s->pending.started>=8000){s->pending.active=false;s->result=TEAM_TIMEOUT;s->timeouts++;}
        else if(s->pending.tries<6 && now-s->pending.last>=1000 && s->send(s->ctx,s->pending.bytes,s->pending.len)){s->pending.last=now;if(s->pending.tries)s->retries++;s->pending.tries++;}}
    if(now-s->announce>=2000){s->announce=now;if(s->poll.active && s->poll.local)publish(s,true);if(s->board.active && s->board.local)publish(s,false);}
}
static bool available(team_activity_t *s,bool active,bool local,uint32_t seen){return s->enabled && !s->pending.active && (!active || local || s->now-seen>=15000);}
bool team_poll_create(team_activity_t *s,const char *q,unsigned options){
    if(!available(s,s->poll.active,s->poll.local,s->poll.seen) || !text(q,48) || options<2 || options>4)return false;
    s->poll=(team_poll_t){.active=true,.local=true,.open=true,.boot=s->boot,.id=++s->sequence,.rev=1,.options=options,.seen=s->now};
    memcpy(s->poll.host,s->self,6);strcpy(s->poll.question,q);memset(s->voter_mac,0,sizeof(s->voter_mac));s->result=TEAM_IDLE;publish(s,true);return true;
}
bool team_poll_close(team_activity_t *s){if(!s->enabled || !s->poll.active || !s->poll.local)return false;s->poll.open=false;s->poll.rev++;publish(s,true);return true;}
bool team_board_create(team_activity_t *s,const char *title){
    if(!available(s,s->board.active,s->board.local,s->board.seen) || !text(title,18))return false;
    s->board=(team_board_t){.active=true,.local=true,.boot=s->boot,.id=++s->sequence,.rev=1,.seen=s->now};memcpy(s->board.host,s->self,6);s->result=TEAM_IDLE;return team_task_add(s,title);
}
bool team_task_add(team_activity_t *s,const char *title){if(!s->enabled || !s->board.active || !s->board.local || !text(title,18))return false;for(unsigned i=0;i<4;i++)if(!s->board.tasks[i].state){s->board.tasks[i].state=1;strcpy(s->board.tasks[i].title,title);s->board.rev++;publish(s,false);return true;}return false;}
static uint8_t apply(team_activity_t *s,bool poll,const uint8_t mac[6],unsigned index,unsigned action,uint32_t rev){
    if(poll){
        for(unsigned i=0;i<s->poll.voters;i++)if(eq(mac,s->voter_mac[i]))return TEAM_REJECTED;
        if(!s->poll.open || index>=s->poll.options || s->poll.voters>=8)return TEAM_REJECTED;
        memcpy(s->voter_mac[s->poll.voters++],mac,6);s->poll.counts[index]++;s->poll.rev++;publish(s,true);return TEAM_OK;
    }
    if(index>=4 || rev!=s->board.rev)return TEAM_REJECTED;
    team_task_t *t=&s->board.tasks[index];
    if(action==TEAM_CLAIM && t->state==1){t->state=2;memcpy(t->owner,mac,6);}
    else if(action==TEAM_COMPLETE && t->state==2 && eq(t->owner,mac))t->state=3;
    else if(action==TEAM_RELEASE && t->state==2 && eq(t->owner,mac)){t->state=1;memset(t->owner,0,6);}
    else return TEAM_REJECTED;
    s->board.rev++;publish(s,false);return TEAM_OK;
}
static bool request(team_activity_t *s,bool poll,unsigned index,unsigned action){
    bool active=poll?s->poll.active:s->board.active,local=poll?s->poll.local:s->board.local;
    if(!s->enabled || !active || s->pending.active || (poll?index>=s->poll.options:index>=4))return false;
    if(local){s->result=apply(s,poll,s->self,index,action,poll?s->poll.rev:s->board.rev);return s->result==TEAM_OK;}
    uint8_t b[128];header(s,b,poll?VOTE_REQ:TASK_REQ,poll);b[22]=index;b[23]=action;put32(b+24,++s->sequence);
    memcpy(s->pending.bytes,b,28);s->pending.len=28;s->pending.active=true;s->pending.tries=0;s->pending.started=s->now;s->pending.last=s->now-1000;s->result=TEAM_WAIT;return true;
}
bool team_vote(team_activity_t *s,unsigned choice){return request(s,true,choice,0);}
bool team_task_request(team_activity_t *s,unsigned task,unsigned action){return request(s,false,task,action);}
static bool same(const uint8_t *b,const uint8_t host[6],uint32_t boot,uint32_t id){return eq(b+12,host) && get32(b+18)==boot && get32(b+4)==id;}
void team_receive(team_activity_t *s,const mesh_packet_t *p){
    const uint8_t *b=p->payload;
    if(!s->enabled || p->type!=MESH_TYPE_COLLAB || p->len<22 || !(p->flags&MESH_FLAG_ROUTED) ||
       p->flags&(MESH_FLAG_ACK_REQUEST|MESH_FLAG_ANNOUNCEMENT) || !mesh_packet_is_broadcast(p->dst_mac) ||
       p->channel!=s->channel || b[0]!=1 || b[2]!=s->channel || b[3] || !get32(b+4) || !get32(b+18))return;
    unsigned op=b[1];bool poll=op<=VOTE_ACK;
    if(op==POLL_STATE || op==BOARD_STATE){
        if(!eq(b+12,p->src_mac) || get32(b+18)!=p->session_id)return;
        bool active=poll?s->poll.active:s->board.active,local=poll?s->poll.local:s->board.local;
        uint32_t seen=poll?s->poll.seen:s->board.seen,rev=poll?s->poll.rev:s->board.rev;
        bool match=poll?same(b,s->poll.host,s->poll.boot,s->poll.id):same(b,s->board.host,s->board.boot,s->board.id);
        bool lineage=active && eq(b+12,poll?s->poll.host:s->board.host) && get32(b+18)==(poll?s->poll.boot:s->board.boot);
        uint32_t old_id=poll?s->poll.id:s->board.id;
        if(local || (lineage && get32(b+4)<old_id) || (active && !match && !(lineage && get32(b+4)>old_id) && s->now-seen<15000) || (active && match && get32(b+8)<rev))return;
        if(poll){
            if(p->len<31 || b[22]>1 || b[23]<2 || b[23]>4 || !b[24] || b[24]>48 || p->len!=30+b[24] || b[29]>8)return;
            unsigned sum=0;for(unsigned i=0;i<4;i++){if(i>=b[23] && b[25+i])return;sum+=b[25+i];}if(sum!=b[29])return;
            char q[49]={0};memcpy(q,b+30,b[24]);if(!text(q,48) || strlen(q)!=b[24])return;
            s->poll=(team_poll_t){.active=true,.open=b[22],.options=b[23],.voters=b[29],.boot=get32(b+18),.id=get32(b+4),.rev=get32(b+8),.seen=s->now};
            memcpy(s->poll.host,b+12,6);memcpy(s->poll.counts,b+25,4);strcpy(s->poll.question,q);
        }else{
            if(p->len!=126)return;
            team_task_t tasks[4]={0};for(unsigned i=0;i<4;i++){const uint8_t *t=b+22+i*26;if(t[0]>3 || t[1]>18 || (t[0] && !t[1]))return;tasks[i].state=t[0];memcpy(tasks[i].owner,t+2,6);memcpy(tasks[i].title,t+8,t[1]);if(t[0] && (!text(tasks[i].title,18) || strlen(tasks[i].title)!=t[1]))return;}
            s->board=(team_board_t){.active=true,.boot=get32(b+18),.id=get32(b+4),.rev=get32(b+8),.seen=s->now};memcpy(s->board.host,b+12,6);memcpy(s->board.tasks,tasks,sizeof(tasks));
        }return;
    }
    if(op==VOTE_REQ || op==TASK_REQ){
        if(b[22]>=4 || (poll?b[23]!=0:(b[23]<TEAM_CLAIM || b[23]>TEAM_RELEASE)))return;
        if(p->len!=28 || !get32(b+24) || !(poll?s->poll.active && s->poll.local:s->board.active && s->board.local) ||
           !(poll?same(b,s->poll.host,s->poll.boot,s->poll.id):same(b,s->board.host,s->board.boot,s->board.id)))return;
        uint8_t result=TEAM_REJECTED;bool found=false;
        for(unsigned i=0;i<16;i++)if(s->ledger[i].used && eq(s->ledger[i].mac,p->src_mac) && s->ledger[i].boot==p->session_id && s->ledger[i].nonce==get32(b+24)){
            found=true;if(s->ledger[i].op==op && s->ledger[i].id==get32(b+4) && s->ledger[i].action==(uint8_t)(b[22]*4+b[23]))result=s->ledger[i].result;break;
        }
        if(!found){result=apply(s,poll,p->src_mac,b[22],b[23],get32(b+8));unsigned i=s->ledger_next++%16;s->ledger[i].used=true;memcpy(s->ledger[i].mac,p->src_mac,6);s->ledger[i].boot=p->session_id;s->ledger[i].nonce=get32(b+24);s->ledger[i].id=get32(b+4);s->ledger[i].op=op;s->ledger[i].action=(uint8_t)(b[22]*4+b[23]);s->ledger[i].result=result;}
        uint8_t ack[128];header(s,ack,poll?VOTE_ACK:TASK_ACK,poll);memcpy(ack+22,p->src_mac,6);put32(ack+28,p->session_id);put32(ack+32,get32(b+24));ack[36]=result;s->send(s->ctx,ack,37);return;
    }
    if((op==VOTE_ACK || op==TASK_ACK) && p->len==37 && s->pending.active && eq(b+22,s->self) && get32(b+28)==s->boot &&
       eq(p->src_mac,b+12) && p->session_id==get32(b+18) && same(b,s->pending.bytes+12,get32(s->pending.bytes+18),get32(s->pending.bytes+4)) &&
       get32(b+32)==get32(s->pending.bytes+24) && op==(unsigned)s->pending.bytes[1]+1u && (b[36]==TEAM_OK || b[36]==TEAM_REJECTED)){
        s->pending.active=false;s->result=b[36];s->confirmed++;s->last_ack_ms=s->now-s->pending.started;
    }
}
