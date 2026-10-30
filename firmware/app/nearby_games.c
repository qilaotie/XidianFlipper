/* Game state belongs to UI; radio callbacks only enqueue bounded copies. */
#include "nearby_games.h"
#include "reaction_party.h"
#ifdef NEARBY_HOST_TEST
#include "../../tools/nearby_host_stubs.h"
#else
#include "ui_zh.h"
#include "ui_language.h"
#include "message_service.h"
#include "ble_radio.h"
#include "wououi.h"
#include "ec06.h"
#include "haptic_feedback.h"
#include "nvs_config.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#endif
#include <string.h>
#include <stdio.h>
#include "../ui/game_pixels.h"

enum { DISCOVER=1, STATE=2, EXIT=3 };
#define NO_HIT 10002u
typedef struct { uint8_t mac[6],data[28]; int8_t rssi; uint32_t at; } event_t;
typedef struct { uint8_t mac[6]; char name[14]; uint32_t token,seen; int rssi; } peer_t;
static portMUX_TYPE s_lock=portMUX_INITIALIZER_UNLOCKED;
static event_t s_queue[16];
static unsigned s_head,s_count;
static uint8_t s_page;
static peer_t s_peers[8],s_peer;
static uint32_t s_token,s_tx,s_beep;
static bool s_treasure,s_locked,s_sound;
static uint32_t ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void write32(uint8_t *p,uint32_t value) {
    for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(8*i));
}
void nearby_games_receive(const mesh_packet_t *p,int8_t rssi) {
    if(p->type==MESH_TYPE_REACTION || p->type==MESH_TYPE_NUMBER_BOMB || p->type==MESH_TYPE_TIME_GUESS){reaction_party_receive(p);return;}
    if(p->type!=MESH_TYPE_TREASURE)return;
    if(p->len!=28 || p->channel || p->flags || p->payload[0]!=1 ||
       p->payload[1]<DISCOVER || p->payload[1]>EXIT || !read32(p->payload+2))return;
    bool treasure=p->type==MESH_TYPE_TREASURE;
    if((p->payload[1]==DISCOVER)!=mesh_packet_is_broadcast(p->dst_mac))return;
    if(treasure && p->payload[1]!=DISCOVER)return;
    if(p->payload[10]>3 || read32(p->payload+11)>NO_HIT)return;
    portENTER_CRITICAL(&s_lock);
    if(s_page==(treasure?M_TREASURE:M_REACTION) && s_count<16) {
        event_t *e=&s_queue[(s_head+s_count++)%16];
        memcpy(e->mac,p->src_mac,6); memcpy(e->data,p->payload,28);
        e->rssi=rssi;e->at=ms();
    }
    portEXIT_CRITICAL(&s_lock);
}
static bool send(uint8_t op) {
    uint8_t b[28]={1,op};
    write32(b+2,s_token);write32(b+6,op==DISCOVER?0:s_peer.token);
    const char *name=nvs_config_get_device_name();
    if(name)snprintf((char*)b+15,13,"%.12s",name);
    return message_service_send_control(s_page==M_TREASURE?MESH_TYPE_TREASURE:MESH_TYPE_REACTION,
        op==DISCOVER?NULL:s_peer.mac,b,sizeof(b));
}
static void reset(void) {
    memset(s_peers,0,sizeof(s_peers));memset(&s_peer,0,sizeof(s_peer));
    s_token=esp_random();if(!s_token)s_token=1;
    s_tx=ms()-700;s_locked=false;
    portENTER_CRITICAL(&s_lock);s_count=s_head=0;portEXIT_CRITICAL(&s_lock);
}
void nearby_games_enter(uint8_t page) {
    uint8_t game=page==M_NUMBER_BOMB?PARTY_GAME_BOMB:page==M_TIME_GUESS?PARTY_GAME_TIME:PARTY_GAME_REACTION;
    party_game_enter(game,page==M_REACTION || page==M_NUMBER_BOMB || page==M_TIME_GUESS);
    portENTER_CRITICAL(&s_lock);s_page=0;s_count=s_head=0;portEXIT_CRITICAL(&s_lock);
    if(page!=M_TREASURE && page!=M_REACTION && page!=M_NUMBER_BOMB && page!=M_TIME_GUESS)return;
    reset();s_treasure=s_sound=false;s_beep=ms();
    portENTER_CRITICAL(&s_lock);s_page=page;portEXIT_CRITICAL(&s_lock);
}
static void drain(uint32_t now) {
    for(unsigned n=0;n<16;n++) {
        event_t e;
        portENTER_CRITICAL(&s_lock);
        bool have=s_count!=0;
        if(have){e=s_queue[s_head];s_head=(s_head+1)%16;--s_count;}
        portEXIT_CRITICAL(&s_lock);
        if(!have)break;
        if(now-e.at>2000)continue;
        uint32_t token=read32(e.data+2);
        if(e.data[1]==DISCOVER) {
            unsigned slot=0;
            for(unsigned i=0;i<8;i++) {
                if(s_peers[i].token && !memcmp(s_peers[i].mac,e.mac,6)){slot=i;break;}
                if(s_peers[i].seen<s_peers[slot].seen)slot=i;
            }
            peer_t *p=&s_peers[slot];
            bool same=p->token==token && !memcmp(p->mac,e.mac,6);
            p->rssi=same?(3*p->rssi+e.rssi)/4:e.rssi;
            p->seen=e.at;p->token=token;memcpy(p->mac,e.mac,6);
            memcpy(p->name,e.data+15,12);p->name[12]=0;
            for(unsigned i=0;i<12 && p->name[i];i++)
                if((unsigned char)p->name[i]<32 || (unsigned char)p->name[i]>126)p->name[i]='?';
            continue;
        }
    }
}
static int best_peer(uint32_t now) {
    int best=-1;
    for(unsigned i=0;i<8;i++)
        if(s_peers[i].token && now-s_peers[i].seen<2500 &&
           (best<0 || s_peers[i].rssi>s_peers[best].rssi ||
            (s_peers[i].rssi==s_peers[best].rssi && memcmp(s_peers[i].mac,s_peers[best].mac,6)<0)))best=(int)i;
    return best;
}
static void leave(void) {
    nearby_games_enter(0);btn.pressed=false;haptic_play(HAPTIC_BACK);
    if(ui.layer){ui.index=ui.parent_index[ui.layer];ui.state=S_LAYER_OUT;}
}
/* Empirical RSSI rhythm, not a distance estimate. Keep ramping up after -40
 * instead of saturating together with the eight display bars. */
static uint32_t radar_interval(int rssi) {
    static const struct { int rssi; uint32_t interval; } points[]={
        {-95,2500},{-80,1800},{-65,1000},{-50,500},{-40,260},{-30,120},{-25,80}
    };
    if(rssi<=points[0].rssi)return points[0].interval;
    for(unsigned i=1;i<sizeof(points)/sizeof(points[0]);i++)
        if(rssi<points[i].rssi) {
            uint32_t reduction=(uint32_t)(rssi-points[i-1].rssi)*
                (points[i-1].interval-points[i].interval)/(uint32_t)(points[i].rssi-points[i-1].rssi);
            return points[i-1].interval-reduction;
        }
    return 80;
}
static void radar(uint32_t now) {
    if(btn.pressed) {
        uint8_t key=btn.id;btn.pressed=false;
        if(key==BTN_ID_BACK){leave();return;}
        if(key==BTN_ID_CW || key==BTN_ID_CC){s_treasure=!s_treasure;s_locked=false;}
        if(key==BTN_ID_SP && !s_treasure && s_peer.token && now-s_peer.seen<3000)s_locked=!s_locked;
        if(key==BTN_ID_LP)s_sound=!s_sound;
    }
    if(s_treasure) {
        if(now-s_tx>=700 && send(DISCOVER))s_tx=now;
        u8g2_DrawFrame(&u8g2,2,10,26,20);u8g2_DrawBox(&u8g2,3,16,24,2);u8g2_DrawBox(&u8g2,13,15,4,6);game_ring(15,6,4);
        ui_zh_draw_fit(&u8g2,36,11,"TREASURE: HIDING",92);ui_zh_draw_fit(&u8g2,36,23,"ONE HIDES, MANY HUNT",92);return;
    }
    int best=best_peer(now);
    if(!s_locked && best>=0)s_peer=s_peers[best];
    if(s_locked)for(unsigned i=0;i<8;i++)
        if(s_peer.token==s_peers[i].token && !memcmp(s_peer.mac,s_peers[i].mac,6))s_peer=s_peers[i];
    if(!s_peer.token || now-s_peer.seen>3000) {
        game_ring(15,16,14);game_ring(15,16,7);unsigned phase=(now/180)%4;u8g2_DrawBox(&u8g2,phase<2?15:2,phase%2?3:16,phase<2?1:14,phase<2?14:1);
        ui_zh_draw_fit(&u8g2,35,11,s_locked?"TARGET LOST":"RADAR: SEARCHING",93);ui_zh_draw_fit(&u8g2,35,26,"PEER: TURN TO HIDING",93);return;
    }
    int level=(s_peer.rssi+95)/7;if(level<0)level=0;if(level>8)level=8;
    char text[28];snprintf(text,sizeof(text),"%.8s %ddB%s",s_peer.name[0]?s_peer.name:"TREASURE",s_peer.rssi,s_locked?" *":"");
    game_ring(15,16,14);game_ring(15,16,9);game_ring(15,16,4);if(level)u8g2_DrawBox(&u8g2,14-level/2,15-level/2,2+level,2+level);
    u8g2_DrawStr(&u8g2,35,7,text);
    for(int i=0;i<8;i++)u8g2_DrawFrame(&u8g2,35+i*11,11,8,8);
    for(int i=0;i<level;i++)u8g2_DrawBox(&u8g2,36+i*11,12,6,6);
    ui_zh_draw_fit(&u8g2,35,31,s_sound?"SP:LOCK LP:MUTE":"SP:LOCK LP:SOUND",93);
    if(s_sound && now-s_beep>=radar_interval(s_peer.rssi)){haptic_play(HAPTIC_RADAR);s_beep=now;}
}
void nearby_games_proc(void) {
    uint32_t now=ms();drain(now);
    u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    if(s_page==M_TREASURE)radar(now);
    else if(s_page==M_REACTION || s_page==M_NUMBER_BOMB || s_page==M_TIME_GUESS)reaction_party_proc();
}
