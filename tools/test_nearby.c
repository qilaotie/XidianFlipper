/* Radar production state test; multiplayer reaction uses test_party.c. */
#define NEARBY_HOST_TEST
#include "../firmware/app/nearby_games.c"
#include <assert.h>
static int64_t clock_us=100000;
static unsigned transmissions,radar_sounds,party_receives;
static bool party_active;
int64_t esp_timer_get_time(void){return clock_us;}
uint32_t esp_random(void){return 12345;}
const char *nvs_config_get_device_name(void){return "TEST";}
bool ble_radio_get_my_mac(uint8_t *mac){memset(mac,1,6);return true;}
bool ble_radio_is_ready(void){return true;}
bool message_service_send_control(uint8_t type,const uint8_t *dst,const void *data,uint8_t len){
    assert(type==MESH_TYPE_TREASURE && !dst && len==28 && ((const uint8_t*)data)[0]==1);
    ++transmissions;return true;
}
void reaction_party_enter(bool active){party_active=active;}
void party_game_enter(uint8_t game,bool active){(void)game;party_active=active;}
void reaction_party_receive(const mesh_packet_t *p){(void)p;++party_receives;}
void reaction_party_proc(void){}
void haptic_play(int effect){if(effect==HAPTIC_RADAR)++radar_sounds;}
void u8g2_SetDrawColor(void *d,int c){(void)d;(void)c;}
void u8g2_SetFont(void *d,const void *f){(void)d;(void)f;}
void u8g2_DrawStr(void *d,int x,int y,const char *s){(void)d;(void)x;(void)y;(void)s;}
void u8g2_DrawFrame(void *d,int x,int y,int w,int h){(void)d;(void)x;(void)y;(void)w;(void)h;}
void u8g2_DrawBox(void *d,int x,int y,int w,int h){(void)d;(void)x;(void)y;(void)w;(void)h;}
static mesh_packet_t beacon(uint32_t token,int tail){
    mesh_packet_t p={.type=MESH_TYPE_TREASURE,.len=28};
    memset(p.src_mac,2,6);p.src_mac[5]=(uint8_t)tail;memset(p.dst_mac,255,6);
    p.payload[0]=1;p.payload[1]=DISCOVER;write32(p.payload+2,token);
    memcpy(p.payload+15,"PEER",5);return p;
}
int main(void){
    assert(radar_interval(-120)==2500 && radar_interval(-95)==2500);
    assert(radar_interval(-50)==500 && radar_interval(-40)==260);
    assert(radar_interval(-30)==120 && radar_interval(-25)==80 && radar_interval(-10)==80);
    for(int r=-119;r<=0;r++)assert(radar_interval(r)<=radar_interval(r-1));
    nearby_games_enter(M_TREASURE);assert(!party_active);
    mesh_packet_t p=beacon(99,1);
    p.len=27;nearby_games_receive(&p,-30);assert(s_count==0);p.len=28;
    p.channel=1;nearby_games_receive(&p,-30);assert(s_count==0);p.channel=0;
    for(unsigned i=0;i<40;i++)nearby_games_receive(&p,-60);
    assert(s_count==16);nearby_games_proc();assert(s_peer.token==99 && s_peer.rssi==-60);
    p=beacon(100,2);nearby_games_receive(&p,-35);nearby_games_proc();assert(s_peer.token==100);
    btn.pressed=true;btn.id=BTN_ID_SP;nearby_games_proc();assert(s_locked);
    p=beacon(101,3);nearby_games_receive(&p,-20);nearby_games_proc();assert(s_peer.token==100);
    clock_us+=4000000;assert(best_peer(ms())==-1);
    s_sound=true;s_locked=false;p=beacon(200,4);
    nearby_games_receive(&p,-25);nearby_games_proc();s_beep=ms();unsigned sounds=radar_sounds;
    clock_us+=79000;nearby_games_proc();assert(radar_sounds==sounds);
    clock_us+=1000;nearby_games_proc();assert(radar_sounds==sounds+1);
    sounds=radar_sounds;s_sound=false;clock_us+=80000;nearby_games_proc();assert(radar_sounds==sounds);
    s_sound=true;clock_us+=4000000;nearby_games_proc();assert(radar_sounds==sounds);
    /* Independent passive hunters do not consume or exclusively claim a beacon. */
    for(unsigned hunter=0;hunter<3;hunter++){
        nearby_games_enter(M_TREASURE);p=beacon(400,9);
        nearby_games_receive(&p,(int8_t)(-40-(int)hunter*10));nearby_games_proc();
        assert(s_peer.token==400 && s_peer.rssi==-40-(int)hunter*10);
    }
    assert(!transmissions);
    btn.pressed=true;btn.id=BTN_ID_CW;nearby_games_proc();assert(transmissions==1);
    nearby_games_enter(0);nearby_games_receive(&p,-20);assert(!s_count);
    nearby_games_enter(M_REACTION);assert(party_active);
    p.type=MESH_TYPE_REACTION;nearby_games_receive(&p,-20);assert(party_receives==1);
    puts("PASS: one beacon / 3 passive hunters, RSSI selection/lock/staleness, 80ms sound/mute, queue bounds, party routing");
}
