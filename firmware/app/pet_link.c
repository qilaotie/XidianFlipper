/* Ephemeral Connection-inspired play, not the Bandai infrared protocol. */
#include "pet_link.h"
#include "pet_games.h"
#include "pet_service.h"
#include "message_service.h"
#include "ble_radio.h"
#include "nvs_config.h"
#include "wououi.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include <string.h>
enum {HELLO=1,INVITE,ACCEPT,START,STATE,LEAVE};
enum {SEARCH,INVITING,OFFER,SYNC,RUN,DONE,LOST};
typedef struct {uint8_t mac[6],b[40];int8_t rssi;uint32_t at;} event_t;
typedef struct {uint8_t mac[6],stage,form;char name[8];uint32_t token,seen;int rssi;} peer_t;
static portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
static event_t queue[12];static unsigned head,count;static bool active;
static peer_t peers[6],peer;
static uint8_t local_mac[6],state;static bool duel,host,remote_done,settled,charged,confirmed,remote_ack;
static uint32_t token,seed,base,last_rx,last_tx;static uint16_t remote_score;
static uint8_t reward_gold;static int reward_gift;
static pet_game_t game;
static uint32_t ms(void){return (uint32_t)(esp_timer_get_time()/1000);}
static uint32_t read32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void write32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
void pet_link_receive(const mesh_packet_t *p,int8_t rssi){
    if(p->len!=40 || p->channel || p->flags || p->payload[0]!=2 ||
       p->payload[1]<HELLO || p->payload[1]>LEAVE || !read32(p->payload+2) ||
       p->payload[10]>1 || p->payload[11]>=PG_COUNT || p->payload[22]>1 ||
       p->payload[23]>3 || p->payload[24]>3 || p->payload[33]>1 ||
       ((p->payload[1]==HELLO)!=mesh_packet_is_broadcast(p->dst_mac)))return;
    portENTER_CRITICAL(&guard);
    if(active && count<12){event_t *e=&queue[(head+count++)%12];
        memcpy(e->mac,p->src_mac,6);memcpy(e->b,p->payload,40);e->rssi=rssi;e->at=ms();}
    portEXIT_CRITICAL(&guard);
}
static bool send_op(unsigned op){
    uint8_t b[40]={2,(uint8_t)op};write32(b+2,token);write32(b+6,op==HELLO?0:peer.token);
    b[10]=duel;b[11]=game.kind;write32(b+12,seed);write32(b+16,(state==RUN||state==DONE)?ms()-base:0);
    b[20]=(uint8_t)game.score;b[21]=(uint8_t)(game.score>>8);b[22]=game.done;
    b[33]=remote_done;
    const pet_t *p=pet_service_state();b[23]=(uint8_t)pet_stage(p);b[24]=p->form;
    const char *name=pet_service_name();if(!name[0])name=nvs_config_get_device_name();if(name)snprintf((char *)b+25,8,"%.7s",name);
    return message_service_send_control(MESH_TYPE_PET,op==HELLO?NULL:peer.mac,b,sizeof(b));
}
void pet_link_leave(void){
    if(active && peer.token)send_op(LEAVE);
    portENTER_CRITICAL(&guard);active=false;count=head=0;portEXIT_CRITICAL(&guard);
}
void pet_link_enter(bool battle){
    pet_link_leave();memset(peers,0,sizeof(peers));memset(&peer,0,sizeof(peer));memset(&game,0,sizeof(game));
    duel=battle;host=remote_done=settled=charged=confirmed=remote_ack=false;remote_score=0;
    reward_gold=0;reward_gift=-1;
    token=esp_random();if(!token)token=1;seed=esp_random();if(!seed)seed=1;
    static const uint8_t choices[]={PG_BUMP,PG_HEADING,PG_SPRINT};
    game.kind=choices[seed%3];state=SEARCH;base=last_rx=ms();last_tx=base-600;
    portENTER_CRITICAL(&guard);active=true;portEXIT_CRITICAL(&guard);
}
static peer_t decode(const event_t *e){
    peer_t p={0};memcpy(p.mac,e->mac,6);p.token=read32(e->b+2);p.seen=e->at;p.rssi=e->rssi;
    p.stage=e->b[23];p.form=e->b[24];memcpy(p.name,e->b+25,7);
    for(unsigned i=0;i<7 && p.name[i];i++)if((unsigned char)p.name[i]<32 || (unsigned char)p.name[i]>126)p.name[i]='?';
    return p;
}
static bool charge(void){if(!duel)return true;if(charged)return true;return charged=pet_service_action(PET_PLAY);}
static void events(uint32_t now){
    for(unsigned i=0;i<12;i++){
        event_t e;portENTER_CRITICAL(&guard);bool have=count!=0;
        if(have){e=queue[head];head=(head+1)%12;count--;}portEXIT_CRITICAL(&guard);if(!have)break;
        if(now-e.at>2000 || e.b[10]!=duel)continue;
        unsigned op=e.b[1];peer_t incoming=decode(&e);
        if(op==HELLO){
            if(state!=SEARCH)continue;
            unsigned slot=0;for(unsigned j=0;j<6;j++){
                if(!memcmp(peers[j].mac,e.mac,6)){slot=j;break;}
                if(peers[j].seen<peers[slot].seen)slot=j;
            }peers[slot]=incoming;continue;
        }
        if(read32(e.b+6)!=token)continue;
        if(op==INVITE && (state==SEARCH || (state==INVITING && memcmp(local_mac,e.mac,6)>0))){
            peer=incoming;state=OFFER;host=false;last_rx=now;last_tx=now-600;continue;
        }
        if(incoming.token!=peer.token || memcmp(peer.mac,e.mac,6))continue;
        last_rx=now;
        if(op==LEAVE){state=LOST;continue;}
        if(op==ACCEPT && state==INVITING){
            if(!charge()){send_op(LEAVE);state=LOST;continue;}
            host=true;base=now;state=RUN;pet_games_start(&game,game.kind,seed);last_tx=now-600;
        }else if(op==START && !host && (state==SYNC || state==RUN)){
            uint32_t age=read32(e.b+16);
            if(state==SYNC && age<=3000){
                seed=read32(e.b+12);pet_games_start(&game,e.b[11],seed);base=now-age;state=RUN;
            }
        }else if(op==STATE && (state==RUN || state==DONE)){
            if(e.b[11]!=game.kind || read32(e.b+12)!=seed)continue;
            unsigned score=(unsigned)e.b[20]|(unsigned)e.b[21]<<8;
            unsigned max=game.kind==PG_SPRINT?40:(game.kind==PG_BUMP?5:10);
            if(score>max)continue;
            confirmed=true;
            if(e.b[33])remote_ack=true;
            if(e.b[22] && (!remote_done || remote_score==score)){remote_done=true;remote_score=(uint16_t)score;}
        }
    }
}
static void draw_pet(int x,unsigned form,unsigned frame){
    int y=17+(int)(frame%2);u8g2_DrawRFrame(&u8g2,x,y,18,12,3);
    u8g2_DrawPixel(&u8g2,x+5,y+4);u8g2_DrawPixel(&u8g2,x+12,y+4);
    u8g2_DrawHLine(&u8g2,x+7,y+8,4);
    if(form==PET_SPORT){u8g2_DrawVLine(&u8g2,x+3,y-4,4);u8g2_DrawVLine(&u8g2,x+14,y-4,4);}
    else if(form!=PET_MELLOW){u8g2_DrawLine(&u8g2,x+2,y,x+4,y-3);u8g2_DrawLine(&u8g2,x+16,y,x+14,y-3);}
}
void pet_link_proc(unsigned input){
    uint32_t now=ms();bool ready=ble_radio_get_my_mac(local_mac)&&ble_radio_is_ready();events(now);
    if(input==PG_TAP){
        if(state==SEARCH){
            int best=-1;for(unsigned i=0;i<6;i++)if(peers[i].token && now-peers[i].seen<2200 &&
                (best<0 || peers[i].rssi>peers[best].rssi))best=(int)i;
            if(best>=0){peer=peers[best];state=INVITING;host=true;last_rx=now;last_tx=now-600;}
        }else if(state==OFFER){
            if(charge()){state=SYNC;last_tx=now-600;}else {send_op(LEAVE);state=LOST;}
        }else if(state==LOST || state==DONE){pet_link_enter(duel);input=PG_NONE;}
    }
    if(state==RUN){
        uint32_t age=now-base;
        if(age>=3000){
            if(duel)pet_games_step(&game,age-3000,input);
            else if(age>=11000)game.done=true;
        }
        if(game.done && remote_done && confirmed && remote_ack){
            state=DONE;
            if(!settled){
                settled=true;
                unsigned outcome=!duel?0:(game.score>remote_score?3:(game.score==remote_score?2:1));
                reward_gold=pet_service_social(outcome,seed,&reward_gift);pet_service_flush();
            }
        }
    }
    if(state!=SEARCH && state!=DONE && state!=LOST && now-last_rx>15000)state=LOST;
    if(ready && now-last_tx>=600){
        unsigned op=state==SEARCH?HELLO:(state==INVITING?INVITE:(state==SYNC?ACCEPT:
            ((state==RUN||state==DONE)?(host&&!confirmed?START:STATE):0)));
        if(op && send_op(op))last_tx=now;
    }
    char line[27];u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    snprintf(line,sizeof(line),"%s %s",duel?"DUEL":"VISIT",peer.name[0]?peer.name:"NEARBY");u8g2_DrawStr(&u8g2,1,7,line);
    const char *help="BACK: EXIT";
    if(!ready)help="RADIO NOT READY";
    else if(state==SEARCH)help="BOTH OPEN; SP: INVITE";
    else if(state==INVITING)help="WAIT PEER ACCEPT...";
    else if(state==OFFER)help="SP: ACCEPT BACK: NO";
    else if(state==SYNC)help="WAIT START...";
    else if(state==LOST)help="LEFT/NOT READY SP:RETRY";
    else if(state==DONE){
        if(duel)snprintf(line,sizeof(line),"%s %u:%u +%uG",game.score==remote_score?"DRAW":(game.score>remote_score?"WIN":"LOSE"),game.score,remote_score,reward_gold);
        else if(reward_gift>=0){static const char *const gifts[]={"APPLE","SNACK","SOAP"};snprintf(line,sizeof(line),"GIFT %s +%uG",gifts[reward_gift],reward_gold);}
        else if(reward_gold)snprintf(line,sizeof(line),"VISIT +%uG BAG FULL",reward_gold);
        else snprintf(line,sizeof(line),"VISIT: NO GIFT / LIMIT");
        u8g2_DrawStr(&u8g2,1,18,line);help="SP: AGAIN BACK: EXIT";
    }else if(now-base<3000){snprintf(line,sizeof(line),"START IN %lu",(unsigned long)((3999-(now-base))/1000));u8g2_DrawStr(&u8g2,1,18,line);}
    else if(!duel){draw_pet(24,pet_service_state()->form,now/300);draw_pet(84,peer.form,now/300+1);return;}
    else{
        snprintf(line,sizeof(line),"%s %u:%u",pet_games_name(game.kind),game.score,remote_score);u8g2_DrawStr(&u8g2,1,18,line);
        if(game.done)help="WAIT PEER RESULT...";
        else if(game.kind==PG_BUMP){snprintf(line,sizeof(line),"POWER %u SP:>=7",game.meter);help=line;}
        else if(game.kind==PG_HEADING){
            uint32_t elapsed=now-base-3000,remaining=game.deadline>elapsed?game.deadline-elapsed:0;
            snprintf(line,sizeof(line),"Y%u B%u %lums SP:<300",game.lane+1,game.target+1,(unsigned long)remaining);help=line;
        }
        else help="TAP FAST! BACK: EXIT";
    }
    u8g2_DrawStr(&u8g2,1,31,help);
}
