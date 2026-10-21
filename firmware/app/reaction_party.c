/* Four-player, host-authoritative lobby/roster; local reaction timing.
 * Version 2 deliberately rejects the older two-player wire protocol. */
#include "reaction_party.h"
#ifdef PARTY_HOST_TEST
#include "../../tools/party_host_stubs.h"
#else
#include "message_service.h"
#include "ble_radio.h"
#include "wououi.h"
#include "ec06.h"
#include "haptic_feedback.h"
#include "nvs_config.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#endif
#include <string.h>
#include <stdio.h>

#define PARTY_MAX 4
#define PARTY_SNAP 128
#define PARTY_USER 40
#define PARTY_EARLY 10001u
#define PARTY_DNF 10002u
#define TIME_EARLY 65534u
#define TIME_DNF 65535u
enum { P_SNAPSHOT=1,P_JOIN,P_PLAYER,P_LEAVE,P_GUESS };
enum { P_LOBBY,P_LOCK,P_RUN,P_RESULTS,P_CLOSED };
enum { P_READY=1,P_NEXT=2,P_DROPPED=4 };
typedef struct {
    uint8_t mac[6];uint32_t token;char name[12];uint8_t flags;
    uint16_t score;uint32_t seen,sequence,ack;
} party_player_t;
typedef struct { mesh_packet_t packet;uint32_t at; } party_event_t;
typedef struct {
    bool host,joined,join_choice,quiet,pressed,go_shown,guess_pending;
    uint8_t game,target,bomb_low,bomb_high,bomb_turn,bomb_secret,guess;
    uint32_t guess_revision;
    uint8_t host_mac[6],mac[6],phase,count,view,quiet_remaining;
    uint32_t token,host_token,room,round,revision,sequence,host_sequence;
    uint32_t last_host,last_tx,next_tx,quiet_at,phase_at;
    uint8_t local_flags;uint16_t local_score;
    int64_t entered,deadline,go;
    party_player_t players[PARTY_MAX];
} party_t;
static party_t s_party;
static portMUX_TYPE s_party_lock=portMUX_INITIALIZER_UNLOCKED;
static party_event_t s_party_queue[16];
static unsigned s_party_head,s_party_count;
static bool s_party_active;
static uint8_t party_type(uint8_t game){
    return game==PARTY_GAME_BOMB?MESH_TYPE_NUMBER_BOMB:game==PARTY_GAME_TIME?MESH_TYPE_TIME_GUESS:MESH_TYPE_REACTION;
}
static bool party_score_valid(uint8_t type,uint16_t score){
    if(type==MESH_TYPE_TIME_GUESS)return score<=20001 || score>=TIME_EARLY;
    if(type==MESH_TYPE_NUMBER_BOMB)return score==0 || score==1 || score==PARTY_DNF;
    return score<=PARTY_DNF;
}
static uint16_t party_failure(void){return s_party.game==PARTY_GAME_TIME?TIME_DNF:PARTY_DNF;}
static bool party_failed(uint16_t score){return score>=(s_party.game==PARTY_GAME_TIME?TIME_EARLY:PARTY_EARLY);}
static uint32_t party_ms(void){return (uint32_t)(esp_timer_get_time()/1000);}
static uint32_t party_get(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void party_put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static uint16_t party_get16(const uint8_t *p){return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static void party_put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static bool party_newer(uint32_t a,uint32_t b){return (int32_t)(a-b)>0;}
static void party_name(char out[12],const char *name){
    unsigned i=0;
    for(;i<11 && name && name[i];i++)out[i]=(name[i]>=32 && (unsigned char)name[i]<=126)?name[i]:'?';
    out[i]=0;
}
static bool party_valid(const mesh_packet_t *p){
    if((p->type!=MESH_TYPE_REACTION && p->type!=MESH_TYPE_NUMBER_BOMB && p->type!=MESH_TYPE_TIME_GUESS) || p->channel || p->flags ||
       (p->len!=PARTY_SNAP && p->len!=PARTY_USER) || p->payload[0]!=2)return false;
    const uint8_t *b=p->payload;unsigned op=b[1];
    if(op<P_SNAPSHOT || op>P_GUESS || (op==P_GUESS && p->type!=MESH_TYPE_NUMBER_BOMB) || !party_get(b+2) || !party_get(b+6) ||
       !party_get(b+10) || !party_get(b+14) || !party_get(b+20) || b[18]>P_CLOSED)return false;
    if(op==P_SNAPSHOT){
        if(p->len!=PARTY_SNAP || !mesh_packet_is_broadcast(p->dst_mac) || !b[19] || b[19]>PARTY_MAX)return false;
        for(unsigned i=0;i<b[19];i++){
            const uint8_t *r=b+24+i*26;
            if(!mesh_mac_is_valid_peer(r) || !party_get(r+6) || (r[22]&~7) || !party_score_valid(p->type,party_get16(r+23)))return false;
            for(unsigned j=0;j<i;j++)if(!memcmp(r,b+24+j*26,6))return false;
        }
        if(p->type==MESH_TYPE_TIME_GUESS && b[18]>=P_LOCK && b[18]<=P_RESULTS && (b[49]<1 || b[49]>10))return false;
        if(p->type==MESH_TYPE_NUMBER_BOMB && (b[18]==P_RUN || b[18]==P_RESULTS) &&
           (!b[75] || b[75]>b[101] || b[101]>100 || b[127]>=b[19] || (b[18]==P_RESULTS && (!b[49] || b[49]>100))))return false;
        return !memcmp(b+24,p->src_mac,6) && party_get(b+30)==party_get(b+10);
    }
    return p->len==PARTY_USER && !mesh_packet_is_broadcast(p->dst_mac) &&
        !(b[24]&~(P_READY|P_NEXT)) && party_score_valid(p->type,party_get16(b+25)) &&
        (op!=P_GUESS || (b[39]>=1 && b[39]<=100));
}
void reaction_party_receive(const mesh_packet_t *p){
    if(!party_valid(p))return;
    portENTER_CRITICAL(&s_party_lock);
    if(s_party_active && p->type==party_type(s_party.game) && s_party_count<16){
        party_event_t *e=&s_party_queue[(s_party_head+s_party_count++)%16];
        e->packet=*p;e->at=party_ms();
    }
    portEXIT_CRITICAL(&s_party_lock);
}
void party_game_enter(uint8_t game,bool active){
    portENTER_CRITICAL(&s_party_lock);
    s_party_active=false;s_party_head=s_party_count=0;
    portEXIT_CRITICAL(&s_party_lock);
    memset(&s_party,0,sizeof(s_party));
    s_party.game=game<=PARTY_GAME_TIME?game:PARTY_GAME_REACTION;
    s_party.token=esp_random();if(!s_party.token)s_party.token=1;
    s_party.entered=esp_timer_get_time();s_party.next_tx=700;
    portENTER_CRITICAL(&s_party_lock);s_party_active=active;portEXIT_CRITICAL(&s_party_lock);
}
void reaction_party_enter(bool active){party_game_enter(PARTY_GAME_REACTION,active);}
static void party_header(uint8_t *b,unsigned op){
    b[0]=2;b[1]=(uint8_t)op;party_put(b+2,s_party.room);party_put(b+6,s_party.round);
    party_put(b+10,s_party.token);party_put(b+14,op==P_GUESS?s_party.guess_revision:s_party.revision);
    b[18]=s_party.phase;b[19]=s_party.count;
    ++s_party.sequence;if(!s_party.sequence)++s_party.sequence;party_put(b+20,s_party.sequence);
}
static bool party_send(unsigned op){
    if(!s_party.room || !ble_radio_is_ready())return false;
    uint8_t b[PARTY_SNAP]={0};party_header(b,op);
    if(op==P_SNAPSHOT){
        for(unsigned i=0;i<s_party.count;i++){
            const party_player_t *p=&s_party.players[i];uint8_t *r=b+24+i*26;
            memcpy(r,p->mac,6);party_put(r+6,p->token);memcpy(r+10,p->name,12);
            r[22]=p->flags;party_put16(r+23,p->score);
        }
        if(s_party.phase==P_LOBBY && s_party.quiet){
            uint32_t elapsed=party_ms()-s_party.quiet_at;
            b[49]=(uint8_t)(elapsed>=5000?0:(5000-elapsed+999)/1000);
        }
        if(s_party.game==PARTY_GAME_TIME && s_party.phase>=P_LOCK && s_party.phase<=P_RESULTS)b[49]=s_party.target;
        if(s_party.game==PARTY_GAME_BOMB && (s_party.phase==P_RUN || s_party.phase==P_RESULTS)){
            b[75]=s_party.bomb_low;b[101]=s_party.bomb_high;b[127]=s_party.bomb_turn;
            if(s_party.phase==P_RESULTS)b[49]=s_party.bomb_secret;
        }
        return message_service_send_control(party_type(s_party.game),NULL,b,PARTY_SNAP);
    }
    b[24]=s_party.local_flags;party_put16(b+25,s_party.local_score);
    party_name((char*)b+27,nvs_config_get_device_name());
    if(op==P_GUESS)b[39]=s_party.guess;
    return message_service_send_control(party_type(s_party.game),s_party.host_mac,b,PARTY_USER);
}
static void party_changed(void){s_party.quiet=false;++s_party.revision;s_party.last_tx=party_ms()-1000;}
static void party_bomb_finish(unsigned loser){
    s_party.bomb_turn=(uint8_t)loser;s_party.phase=P_RESULTS;
    for(unsigned i=0;i<s_party.count;i++)
        s_party.players[i].score=(i==loser || (s_party.players[i].flags&P_DROPPED))?PARTY_DNF:1;
    s_party.local_score=s_party.players[0].score;party_changed();
    ESP_LOGI("party","BOMB RESULT out=%u number=%u",loser,s_party.bomb_secret);
}
static void party_bomb_guess(unsigned player,unsigned guess,uint32_t revision){
    if(s_party.game!=PARTY_GAME_BOMB || s_party.phase!=P_RUN || player!=s_party.bomb_turn ||
       revision!=s_party.revision || guess<s_party.bomb_low || guess>s_party.bomb_high ||
       (s_party.players[player].flags&P_DROPPED))return;
    if(guess==s_party.bomb_secret){party_bomb_finish(player);return;}
    if(guess<s_party.bomb_secret)s_party.bomb_low=(uint8_t)(guess+1);
    else s_party.bomb_high=(uint8_t)(guess-1);
    for(unsigned n=1;n<=s_party.count;n++){
        unsigned next=(player+n)%s_party.count;
        if(!(s_party.players[next].flags&P_DROPPED)){s_party.bomb_turn=(uint8_t)next;break;}
    }
    s_party.guess=(uint8_t)((s_party.bomb_low+s_party.bomb_high)/2);
    s_party.phase_at=party_ms();party_changed();
    ESP_LOGI("party","BOMB GUESS player=%u value=%u range=%u..%u next=%u",player,guess,s_party.bomb_low,s_party.bomb_high,s_party.bomb_turn);
}
static void party_drop(unsigned player){
    party_player_t *p=&s_party.players[player];p->flags|=P_DROPPED;
    if(!p->score)p->score=party_failure();
    if(s_party.game==PARTY_GAME_BOMB && s_party.phase==P_RUN){
        unsigned alive=0;for(unsigned i=0;i<s_party.count;i++)if(!(s_party.players[i].flags&P_DROPPED))++alive;
        if(player==s_party.bomb_turn || alive<2)party_bomb_finish(player);
    }
}
static int party_self(void){
    for(unsigned i=0;i<s_party.count;i++)if(s_party.players[i].token==s_party.token && !memcmp(s_party.mac,s_party.players[i].mac,6))return (int)i;
    return -1;
}
static void party_create(void){
    s_party.host=s_party.joined=true;s_party.room=s_party.host_token=s_party.token;
    memcpy(s_party.host_mac,s_party.mac,6);s_party.round=s_party.revision=1;s_party.count=1;
    party_player_t *p=&s_party.players[0];memcpy(p->mac,s_party.mac,6);p->token=s_party.token;
    party_name(p->name,nvs_config_get_device_name());s_party.phase=P_LOBBY;
    s_party.last_tx=party_ms()-1000;
}
static void party_round_reset(void){
    unsigned count=1;
    for(unsigned i=1;i<s_party.count;i++)if(!(s_party.players[i].flags&P_DROPPED))s_party.players[count++]=s_party.players[i];
    s_party.count=(uint8_t)count;s_party.view=0;
    ++s_party.round;if(!s_party.round)++s_party.round;
    s_party.phase=P_LOBBY;s_party.local_flags=0;s_party.local_score=0;
    s_party.pressed=s_party.go_shown=false;s_party.deadline=s_party.go=0;
    s_party.target=s_party.bomb_low=s_party.bomb_high=s_party.bomb_turn=s_party.bomb_secret=0;
    s_party.guess_pending=false;s_party.guess=0;
    for(unsigned i=0;i<s_party.count;i++){s_party.players[i].flags=0;s_party.players[i].score=0;s_party.players[i].seen=party_ms();}
    party_changed();
}
static void party_remove(unsigned index){
    memmove(&s_party.players[index],&s_party.players[index+1],(s_party.count-index-1)*sizeof(party_player_t));
    --s_party.count;s_party.view=0;party_changed();
}
static void party_host_packet(const mesh_packet_t *p){
    const uint8_t *b=p->payload;uint32_t now=party_ms(),token=party_get(b+10),seq=party_get(b+20);
    if(!memcmp(p->src_mac,s_party.mac,6) || party_get(b+2)!=s_party.room || party_get(b+6)!=s_party.round ||
       party_get(b+14)>s_party.revision)return;
    int index=-1;
    for(unsigned i=1;i<s_party.count;i++)if(!memcmp(s_party.players[i].mac,p->src_mac,6)){index=(int)i;break;}
    if(index>=0 && s_party.players[index].token!=token){
        if(s_party.phase!=P_LOBBY)return;
        /* Rebooted device must explicitly rejoin with its new session token. */
        if(b[1]!=P_JOIN)return;
        party_remove((unsigned)index);index=-1;
    }
    if(index<0){
        if(b[1]!=P_JOIN || s_party.phase!=P_LOBBY || s_party.count>=PARTY_MAX || b[24] || party_get16(b+25))return;
        index=s_party.count++;
        party_player_t *player=&s_party.players[index];memset(player,0,sizeof(*player));
        memcpy(player->mac,p->src_mac,6);player->token=token;party_name(player->name,(const char*)b+27);
        party_changed();
    }
    party_player_t *player=&s_party.players[index];
    if(!party_newer(seq,player->sequence))return;
    player->sequence=seq;player->seen=now;
    if(b[1]==P_LEAVE){
        if(s_party.phase==P_LOBBY)party_remove((unsigned)index);
        else if(s_party.phase==P_LOCK){player->flags|=P_DROPPED;party_round_reset();}
        else party_drop((unsigned)index);
        return;
    }
    if(s_party.phase==P_LOBBY){
        if(b[18]!=P_LOBBY || party_get16(b+25))return;
        uint8_t ready=b[1]==P_JOIN?0:b[24]&P_READY;
        if((player->flags&P_READY)!=ready)s_party.quiet=false;
        player->flags=ready;
    }else if(s_party.phase==P_LOCK){
        if(b[1]==P_PLAYER && b[18]==P_LOCK && (b[24]&P_READY))player->ack=party_get(b+14);
    }else if(!(player->flags&P_DROPPED) && (s_party.phase==P_RUN || s_party.phase==P_RESULTS)){
        if(b[1]==P_GUESS){
            if(b[18]==P_RUN)party_bomb_guess((unsigned)index,b[39],party_get(b+14));
            return;
        }
        uint16_t score=party_get16(b+25);
        if(b[1]!=P_PLAYER || (b[18]!=P_RUN && b[18]!=P_RESULTS))return;
        if(s_party.game!=PARTY_GAME_BOMB && !player->score && score)player->score=score;
        if(s_party.phase==P_RESULTS && b[18]==P_RESULTS && (b[24]&P_NEXT))player->flags|=P_NEXT;
    }
}
static void party_client_packet(const mesh_packet_t *p){
    const uint8_t *b=p->payload;uint32_t now=party_ms();
    if(!s_party.room){
        if(b[18]!=P_LOBBY || b[19]>=PARTY_MAX)return;
        s_party.room=party_get(b+2);s_party.host_token=party_get(b+10);memcpy(s_party.host_mac,p->src_mac,6);
        s_party.round=party_get(b+6);s_party.last_tx=now-1000;
    }
    if(memcmp(s_party.host_mac,p->src_mac,6) || party_get(b+10)!=s_party.host_token ||
       party_get(b+2)!=s_party.room || !party_newer(party_get(b+20),s_party.host_sequence))return;
    uint32_t round=party_get(b+6);
    if(round!=s_party.round){
        if(!party_newer(round,s_party.round) || b[18]!=P_LOBBY)return;
        s_party.round=round;s_party.local_flags=0;s_party.local_score=0;
        s_party.phase=P_LOBBY;
        s_party.pressed=s_party.go_shown=false;s_party.deadline=s_party.go=0;s_party.view=0;
        s_party.target=s_party.bomb_low=s_party.bomb_high=s_party.bomb_turn=s_party.bomb_secret=0;
        s_party.guess_pending=false;s_party.guess=0;
    }
    if(round==s_party.round && b[18]<s_party.phase)return;
    uint32_t old_revision=s_party.revision;
    s_party.host_sequence=party_get(b+20);s_party.last_host=now;s_party.revision=party_get(b+14);
    s_party.count=b[19];s_party.quiet_remaining=b[18]==P_LOBBY?b[49]:0;
    if(s_party.game==PARTY_GAME_TIME && b[18]>=P_LOCK && b[18]<=P_RESULTS)s_party.target=b[49];
    if(s_party.game==PARTY_GAME_BOMB && (b[18]==P_RUN || b[18]==P_RESULTS)){
        s_party.bomb_low=b[75];s_party.bomb_high=b[101];s_party.bomb_turn=b[127];
        if(b[18]==P_RESULTS)s_party.bomb_secret=b[49];
        if(old_revision!=s_party.revision || s_party.phase!=P_RUN){
            s_party.guess_pending=false;s_party.guess=(uint8_t)((b[75]+b[101])/2);
        }
    }
    for(unsigned i=0;i<s_party.count;i++){
        party_player_t *player=&s_party.players[i];const uint8_t *r=b+24+i*26;
        memcpy(player->mac,r,6);player->token=party_get(r+6);party_name(player->name,(const char*)r+10);
        player->flags=r[22];player->score=party_get16(r+23);
    }
    int self=party_self();
    s_party.joined=self>=0;
    if(b[18]==P_CLOSED){s_party.phase=P_CLOSED;return;}
    if(self<0){
        if(s_party.phase!=P_LOBBY){s_party.phase=P_CLOSED;return;}
        s_party.local_flags=0;s_party.local_score=0;return;
    }
    if(s_party.players[self].flags&P_DROPPED){s_party.phase=P_CLOSED;return;}
    if(b[18]>=P_LOCK && !(s_party.local_flags&P_READY)){s_party.phase=P_CLOSED;return;}
    if(b[18]!=s_party.phase)s_party.last_tx=now-1000;
    if(s_party.game!=PARTY_GAME_BOMB && b[18]==P_RUN && s_party.phase!=P_RUN && s_party.phase!=P_RESULTS)
        s_party.deadline=esp_timer_get_time()+2000000+(esp_random()%3000000);
    s_party.phase=b[18];
}
static void party_drain(void){
    for(unsigned n=0;n<16;n++){
        party_event_t e;bool have;
        portENTER_CRITICAL(&s_party_lock);have=s_party_count!=0;
        if(have){e=s_party_queue[s_party_head];s_party_head=(s_party_head+1)%16;--s_party_count;}
        portEXIT_CRITICAL(&s_party_lock);
        if(!have)break;
        if(party_ms()-e.at>2000)continue;
        if(s_party.host && e.packet.payload[1]!=P_SNAPSHOT)party_host_packet(&e.packet);
        else if(!s_party.host && s_party.join_choice && e.packet.payload[1]==P_SNAPSHOT)party_client_packet(&e.packet);
    }
}
static void party_host_tick(void){
    uint32_t now=party_ms();
    party_player_t *self=&s_party.players[0];self->flags=s_party.local_flags;self->score=s_party.local_score;
    for(unsigned i=1;i<s_party.count;){
        party_player_t *p=&s_party.players[i];uint32_t age=now-p->seen;
        if(s_party.phase==P_LOBBY && age>=6000){party_remove(i);continue;}
        if(s_party.phase==P_LOCK && age>=6000){p->flags|=P_DROPPED;party_round_reset();return;}
        if(s_party.phase>=P_RUN && age>=10000 && !(p->flags&P_DROPPED))party_drop(i);
        ++i;
    }
    if(s_party.phase==P_LOBBY){
        bool all=s_party.count>=2;
        for(unsigned i=0;i<s_party.count;i++)all=all && (s_party.players[i].flags&P_READY);
        if(!all)s_party.quiet=false;
        else if(!s_party.quiet){s_party.quiet=true;s_party.quiet_at=now;}
        else if(now-s_party.quiet_at>=5000){
            if(s_party.game==PARTY_GAME_TIME)s_party.target=(uint8_t)(1+esp_random()%10);
            s_party.phase=P_LOCK;s_party.phase_at=now;party_changed();self->ack=s_party.revision;
        }
    }else if(s_party.phase==P_LOCK){
        bool all=true;
        for(unsigned i=1;i<s_party.count;i++)all=all && s_party.players[i].ack==s_party.revision;
        if(all){
            s_party.phase=P_RUN;s_party.phase_at=now;s_party.last_tx=now-1000;
            if(s_party.game==PARTY_GAME_BOMB){
                s_party.bomb_low=1;s_party.bomb_high=100;s_party.bomb_turn=0;
                s_party.bomb_secret=(uint8_t)(1+esp_random()%100);s_party.guess=50;
            }else s_party.deadline=esp_timer_get_time()+2000000+(esp_random()%3000000);
            ESP_LOGI("party","START game=%u round=%lu players=%u target=%u",s_party.game,(unsigned long)s_party.round,s_party.count,s_party.target);
        }
        else if(now-s_party.phase_at>=8000)party_round_reset();
    }else if(s_party.phase==P_RUN){
        if(s_party.game==PARTY_GAME_BOMB){
            if(now-s_party.phase_at>=20000)party_bomb_finish(s_party.bomb_turn);
            return;
        }
        bool all=true;
        for(unsigned i=0;i<s_party.count;i++){
            party_player_t *p=&s_party.players[i];
            if(!p->score && now-s_party.phase_at>=(s_party.game==PARTY_GAME_TIME?40000u:25000u))p->score=party_failure();
            if(!p->score)all=false;
        }
        if(all){s_party.phase=P_RESULTS;s_party.last_tx=now-1000;}
    }else if(s_party.phase==P_RESULTS){
        bool all=true;
        for(unsigned i=0;i<s_party.count;i++)if(!(s_party.players[i].flags&P_DROPPED) && !(s_party.players[i].flags&P_NEXT))all=false;
        if(all)party_round_reset();
    }
}
static void party_leave(void){
    if(s_party.host){s_party.phase=P_CLOSED;party_send(P_SNAPSHOT);}
    else if(s_party.room)party_send(P_LEAVE);
    reaction_party_enter(false);btn.pressed=false;haptic_play(HAPTIC_BACK);
    if(ui.layer){ui.index=ui.parent_index[ui.layer];ui.state=S_LAYER_OUT;}
}
static void party_input(void){
    ec06_event_t e;
    while((e=ec06_pop_event()).type!=EC06_EVENT_NONE){
        if(e.type==EC06_EVENT_KEY_DOWN){party_leave();return;}
        if((int64_t)e.timestamp_us<s_party.entered)continue;
        if(e.type==EC06_EVENT_CW || e.type==EC06_EVENT_CCW){
            if(!s_party.host && !s_party.join_choice)s_party.view^=1;
            else if(s_party.game==PARTY_GAME_BOMB && s_party.phase==P_RUN && party_self()==s_party.bomb_turn && !s_party.guess_pending){
                if(e.type==EC06_EVENT_CW)s_party.guess=s_party.guess>=s_party.bomb_high?s_party.bomb_low:(uint8_t)(s_party.guess+1);
                else s_party.guess=s_party.guess<=s_party.bomb_low?s_party.bomb_high:(uint8_t)(s_party.guess-1);
            }
            else if(s_party.count)s_party.view=(uint8_t)((s_party.view+s_party.count+(e.type==EC06_EVENT_CW?1:-1))%s_party.count);
        }
        if(e.type==EC06_EVENT_ENC_UP && s_party.pressed){
            s_party.pressed=false;
            if(s_party.joined && s_party.phase==P_LOBBY){s_party.local_flags|=P_READY;s_party.last_tx=party_ms()-1000;}
        }
        if(e.type!=EC06_EVENT_ENC_DOWN)continue;
        if(!s_party.host && !s_party.join_choice){
            if(s_party.view)s_party.join_choice=true;else party_create();
            s_party.view=0;continue;
        }
        if(s_party.phase==P_CLOSED){party_game_enter(s_party.game,true);return;}
        if(!s_party.joined)continue;
        if(s_party.phase==P_LOBBY){
            if(s_party.local_flags&P_READY){s_party.local_flags&=~P_READY;s_party.quiet=false;}
            else s_party.pressed=true;
            s_party.last_tx=party_ms()-1000;
        }else if(s_party.phase==P_RESULTS){s_party.local_flags|=P_NEXT;s_party.last_tx=party_ms()-1000;}
        else if(s_party.phase==P_RUN && s_party.game==PARTY_GAME_BOMB){
            if(party_self()==s_party.bomb_turn && !s_party.guess_pending){
                if(s_party.host)party_bomb_guess(0,s_party.guess,s_party.revision);
                else {s_party.guess_pending=true;s_party.guess_revision=s_party.revision;}
                s_party.last_tx=party_ms()-1000;
            }
        }
        else if(s_party.phase==P_RUN && !s_party.local_score){
            if(!s_party.go || e.timestamp_us<(uint64_t)s_party.go)s_party.local_score=s_party.game==PARTY_GAME_TIME?TIME_EARLY:PARTY_EARLY;
            else {
                uint64_t elapsed=(e.timestamp_us-s_party.go)/1000;
                s_party.local_score=elapsed>=(s_party.game==PARTY_GAME_TIME?20000u:10000u)?party_failure():(uint16_t)(elapsed+1);
            }
            s_party.last_tx=party_ms()-1000;
        }
    }
    btn.pressed=false;
}
static uint32_t party_metric(uint16_t score){
    if(!score || party_failed(score))return UINT32_MAX;
    if(s_party.game==PARTY_GAME_TIME){
        int error=(int)score-1-(int)s_party.target*1000;return (uint32_t)(error<0?-error:error);
    }
    return score;
}
static unsigned party_rank(unsigned index){
    uint16_t score=s_party.players[index].score;unsigned rank=1;
    if(!score || party_failed(score))return 0;
    for(unsigned i=0;i<s_party.count;i++)if(party_metric(s_party.players[i].score)<party_metric(score))++rank;
    return rank;
}
static void party_line(int y,const char *text){u8g2_DrawStr(&u8g2,2,y,text);}
static void party_bomb_draw(void){
    char text[30];
    if(s_party.phase==P_RESULTS){
        snprintf(text,sizeof(text),"BOMB NUMBER: %u",s_party.bomb_secret);party_line(8,text);
        snprintf(text,sizeof(text),"OUT: %s",s_party.players[s_party.bomb_turn].name);party_line(19,text);
        party_line(30,s_party.local_flags&P_NEXT?"NEXT READY / WAITING":"PRESS:NEXT BACK:EXIT");return;
    }
    snprintf(text,sizeof(text),"RANGE %u..%u",s_party.bomb_low,s_party.bomb_high);party_line(8,text);
    snprintf(text,sizeof(text),"TURN: %s",s_party.players[s_party.bomb_turn].name);party_line(19,text);
    if(party_self()==s_party.bomb_turn){
        if(s_party.guess_pending)party_line(30,"SUBMITTING...");
        else {snprintf(text,sizeof(text),"< %u > PRESS:GUESS",s_party.guess);party_line(30,text);}
    }else party_line(30,"WAIT FOR YOUR TURN");
}
static void party_draw(void){
    char top[30],middle[30];
    if(!s_party.host && !s_party.join_choice){
        party_line(8,s_party.game==PARTY_GAME_BOMB?"NUMBER BOMB 2-4":s_party.game==PARTY_GAME_TIME?"TIME GUESS 1-10s":"REACTION PARTY 2-4");
        party_line(19,s_party.view?"< JOIN ROOM >":"< CREATE ROOM >");party_line(30,"TURN:SELECT PRESS:OPEN");return;
    }
    if(s_party.phase==P_CLOSED){party_line(8,"ROOM CLOSED / LOST");party_line(20,"PRESS: RETURN TO MENU");return;}
    if(!s_party.room){party_line(8,"SEARCHING FOR ROOM");party_line(20,"HOST: CREATE ROOM");party_line(30,"BACK: EXIT");return;}
    if(!s_party.joined){party_line(8,s_party.count>=PARTY_MAX?"ROOM FULL":"JOINING ROOM...");party_line(20,"WAIT FOR HOST");return;}
    if(s_party.game==PARTY_GAME_BOMB && (s_party.phase==P_RUN || s_party.phase==P_RESULTS)){party_bomb_draw();return;}
    if(s_party.view>=s_party.count)s_party.view=0;
    party_player_t *p=&s_party.players[s_party.view];
    if(s_party.phase==P_RESULTS){
        unsigned rank=party_rank(s_party.view);
        snprintf(top,sizeof(top),"%u/%u %s%s",s_party.view+1,s_party.count,p->name,!memcmp(p->mac,s_party.mac,6)?" (YOU)":"");
        if(party_failed(p->score))snprintf(middle,sizeof(middle),"%s",p->score==(s_party.game==PARTY_GAME_TIME?TIME_EARLY:PARTY_EARLY)?"FALSE START":"DNF / DISCONNECTED");
        else if(s_party.game==PARTY_GAME_TIME){
            unsigned elapsed=p->score-1;
            snprintf(middle,sizeof(middle),"#%u %u.%03us E%lums",rank,elapsed/1000,elapsed%1000,(unsigned long)party_metric(p->score));
        }else snprintf(middle,sizeof(middle),"RANK %u: %u ms",rank,(unsigned)(p->score-1));
        party_line(8,top);party_line(19,middle);party_line(30,s_party.local_flags&P_NEXT?"NEXT READY / WAITING":"TURN:SCORES PRESS:NEXT");return;
    }
    if(s_party.phase==P_RUN){
        if(s_party.game==PARTY_GAME_TIME){snprintf(top,sizeof(top),"TARGET: %u SECONDS",s_party.target);party_line(8,top);}
        else party_line(8,"REACTION PARTY");
        if(s_party.local_score){
            if(party_failed(s_party.local_score))snprintf(middle,sizeof(middle),"%s",s_party.local_score==(s_party.game==PARTY_GAME_TIME?TIME_EARLY:PARTY_EARLY)?"FALSE START!":"NO RESPONSE");
            else if(s_party.game==PARTY_GAME_TIME){
                unsigned elapsed=s_party.local_score-1;
                snprintf(middle,sizeof(middle),"YOU: %u.%03u s",elapsed/1000,elapsed%1000);
            }else snprintf(middle,sizeof(middle),"YOU: %u ms",(unsigned)(s_party.local_score-1));
            party_line(20,middle);party_line(30,"WAIT FOR ALL RESULTS");
        }else if(s_party.game==PARTY_GAME_TIME){
            party_line(20,s_party.go_shown?"GO! COUNT SILENTLY":"WAIT FOR GO...");
            party_line(30,s_party.go_shown?"PRESS TO STOP":"DON'T PRESS YET");
        }else party_line(21,s_party.go_shown?"*** GO! PRESS NOW ***":"WAIT... DON'T PRESS");
        return;
    }
    unsigned ready=0;for(unsigned i=0;i<s_party.count;i++)if(s_party.players[i].flags&P_READY)++ready;
    snprintf(top,sizeof(top),"%s %u/4 READY %u",s_party.host?"HOST":"ROOM",s_party.count,ready);
    snprintf(middle,sizeof(middle),"%s %s",p->name,p->flags&P_READY?"READY":"WAIT");
    party_line(8,top);party_line(19,middle);
    if(s_party.phase==P_LOCK){
        if(s_party.game==PARTY_GAME_TIME){snprintf(middle,sizeof(middle),"TARGET %us / ARMING",s_party.target);party_line(30,middle);}
        else party_line(30,"LOCKING PLAYER LIST...");
    }
    else if(s_party.host && s_party.quiet){
        uint32_t elapsed=party_ms()-s_party.quiet_at;
        snprintf(middle,sizeof(middle),"ALL READY: START IN %lu",(unsigned long)(elapsed>=5000?0:(5000-elapsed+999)/1000));party_line(30,middle);
    }else if(!s_party.host && s_party.quiet_remaining && (s_party.local_flags&P_READY)){
        snprintf(middle,sizeof(middle),"ALL READY: START IN %u",s_party.quiet_remaining);party_line(30,middle);
    }else party_line(30,s_party.local_flags&P_READY?"READY / WAIT FOR HOST":"PRESS & RELEASE:READY");
}
void reaction_party_proc(void){
    if(!s_party_active)return;
    if(!ble_radio_get_my_mac(s_party.mac) || !ble_radio_is_ready()){
        u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);party_line(12,"RADIO STARTING...");
        /* BACK is still consumed while the radio starts. */
        ec06_event_t e;while((e=ec06_pop_event()).type!=EC06_EVENT_NONE)if(e.type==EC06_EVENT_KEY_DOWN){party_leave();break;}
        btn.pressed=false;return;
    }
    party_drain();party_input();if(!s_party_active)return;
    uint32_t now=party_ms();
    if(!s_party.host && s_party.room && now-s_party.last_host>=8000)s_party.phase=P_CLOSED;
    if(s_party.game!=PARTY_GAME_BOMB && s_party.phase==P_RUN && !s_party.go_shown && esp_timer_get_time()>=s_party.deadline){
        s_party.go_shown=true;s_party.go=esp_timer_get_time();
    }
    if(s_party.phase==P_RUN && s_party.go_shown && !s_party.local_score && esp_timer_get_time()-s_party.go>=(s_party.game==PARTY_GAME_TIME?20000000:10000000))s_party.local_score=party_failure();
    if(s_party.host)party_host_tick();
    if(s_party.room && s_party.phase!=P_CLOSED && now-s_party.last_tx>=s_party.next_tx){
        unsigned op=s_party.host?P_SNAPSHOT:!s_party.joined?P_JOIN:
            s_party.guess_pending && s_party.phase==P_RUN?P_GUESS:P_PLAYER;
        if(party_send(op)){s_party.last_tx=now;s_party.next_tx=650+esp_random()%201;}
    }
    u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);party_draw();
}
