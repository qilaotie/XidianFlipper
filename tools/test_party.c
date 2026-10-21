/* Run five isolated copies of the production state (one host, 3 players,
 * one late spectator) on a lossy/delayed simulated radio. Not an RF test. */
#define PARTY_HOST_TEST
#include "../firmware/app/reaction_party.c"
#include <assert.h>
typedef struct {
    party_t state;party_event_t queue[16];unsigned head,count;bool active;
    ec06_event_t input;
} node_t;
static node_t nodes[5];
static unsigned current,random_counter=100,drop_every,send_count;
static uint8_t test_game=PARTY_GAME_REACTION;
static int64_t clock_us=1000000;
typedef struct { mesh_packet_t p; } transmission_t;
static transmission_t bus[128];static unsigned bus_count;
int64_t esp_timer_get_time(void){return clock_us;}
uint32_t esp_random(void){return ++random_counter*12347u;}
const char *nvs_config_get_device_name(void){static const char *names[]={"HOST","ALICE","BOB","CAROL","LATE"};return names[current];}
bool ble_radio_get_my_mac(uint8_t *mac){memset(mac,1,6);mac[5]=(uint8_t)(current+1);return true;}
bool ble_radio_is_ready(void){return true;}
bool message_service_send_control(uint8_t type,const uint8_t *dst,const void *data,uint8_t len){
    assert(type==party_type(s_party.game) && bus_count<128);
    mesh_packet_t *p=&bus[bus_count++].p;memset(p,0,sizeof(*p));p->type=type;p->len=len;
    ble_radio_get_my_mac(p->src_mac);if(dst)memcpy(p->dst_mac,dst,6);else memset(p->dst_mac,255,6);
    memcpy(p->payload,data,len);assert(party_valid(p));++send_count;
    if(drop_every && send_count%drop_every==0)--bus_count;
    return true;
}
void haptic_play(int effect){(void)effect;}
void u8g2_SetDrawColor(void *d,int c){(void)d;(void)c;}
void u8g2_SetFont(void *d,const void *f){(void)d;(void)f;}
void u8g2_DrawStr(void *d,int x,int y,const char *s){(void)d;(void)x;(void)y;(void)s;}
ec06_event_t ec06_pop_event(void){ec06_event_t e=nodes[current].input;nodes[current].input.type=EC06_EVENT_NONE;return e;}
static void load(unsigned i){
    current=i;s_party=nodes[i].state;s_party_active=nodes[i].active;
    memcpy(s_party_queue,nodes[i].queue,sizeof(s_party_queue));s_party_head=nodes[i].head;s_party_count=nodes[i].count;
}
static void save(void){
    nodes[current].state=s_party;nodes[current].active=s_party_active;
    memcpy(nodes[current].queue,s_party_queue,sizeof(s_party_queue));nodes[current].head=s_party_head;nodes[current].count=s_party_count;
}
static void deliver(void){
    for(unsigned n=0;n<bus_count;n++)for(unsigned i=0;i<5;i++){
        const mesh_packet_t *p=&bus[n].p;
        if(!nodes[i].active || p->src_mac[5]==i+1 ||
           (!mesh_packet_is_broadcast(p->dst_mac) && p->dst_mac[5]!=i+1))continue;
        load(i);reaction_party_receive(p);save();
    }
    bus_count=0;
}
static void tick(unsigned milliseconds){
    for(unsigned t=0;t<milliseconds;t+=20){
        clock_us+=20000;
        for(unsigned i=0;i<5;i++)if(nodes[i].active){load(i);reaction_party_proc();save();}
        deliver();
    }
}
static void open_node(unsigned i){
    load(i);party_game_enter(test_game,true);save();
}
static void event(unsigned i,int type,uint64_t at){nodes[i].input=(ec06_event_t){.type=type,.timestamp_us=at};}
static void click(unsigned i){event(i,EC06_EVENT_ENC_DOWN,clock_us);tick(20);event(i,EC06_EVENT_ENC_UP,clock_us);tick(20);}
static void join(unsigned i){open_node(i);event(i,EC06_EVENT_CW,clock_us);tick(20);click(i);}
static bool all_phase(unsigned phase){for(unsigned i=0;i<4;i++)if(nodes[i].state.phase!=phase)return false;return true;}
static void wait_phase(unsigned phase,unsigned limit){
    for(unsigned n=0;n<limit && !all_phase(phase);n+=20)tick(20);
    if(!all_phase(phase)){
        for(unsigned i=0;i<4;i++)fprintf(stderr,"node%u phase=%u joined=%d count=%u ready=%u round=%lu\n",i,nodes[i].state.phase,nodes[i].state.joined,nodes[i].state.count,nodes[i].state.local_flags,(unsigned long)nodes[i].state.round);
    }
    assert(all_phase(phase));
}
static void test_reaction(void){
    open_node(0);click(0);assert(nodes[0].state.host);
    join(1);join(2);tick(2500);
    assert(nodes[0].state.count==3 && nodes[1].state.joined && nodes[2].state.joined);
    click(0);click(1);click(2);tick(1200);
    assert(nodes[0].state.quiet && nodes[0].state.phase==P_LOBBY);
    tick(3000);assert(nodes[0].state.phase==P_LOBBY);
    join(3);tick(1000);assert(nodes[0].state.count==4 && !nodes[0].state.quiet);
    tick(5200);assert(nodes[0].state.phase==P_LOBBY); /* Newcomer is not ready. */
    click(3);tick(1000);assert(nodes[0].state.quiet);
    click(1);tick(1000);assert(!nodes[0].state.quiet); /* Cancel readiness resets timer. */
    tick(5200);assert(nodes[0].state.phase==P_LOBBY);
    click(1);tick(1000);assert(nodes[0].state.quiet);
    uint32_t quiet_at=nodes[0].state.quiet_at;
    while((uint32_t)(clock_us/1000)-quiet_at<4980)tick(20);
    assert(nodes[0].state.phase==P_LOBBY);
    drop_every=7;wait_phase(P_RUN,6000); /* Retransmit through deterministic loss. */
    join(4);tick(500);assert(!nodes[4].state.joined); /* Frozen roster rejects late join. */
    bool hit[4]={false};unsigned times[]={260,320,240,240};
    for(unsigned n=0;n<18000 && !all_phase(P_RESULTS);n+=20){
        for(unsigned i=0;i<4;i++){
            party_t *p=&nodes[i].state;
            if(!hit[i] && p->go && clock_us>=p->go+(int64_t)times[i]*1000){
                event(i,EC06_EVENT_ENC_DOWN,(uint64_t)(p->go+times[i]*1000));hit[i]=true;
            }
        }
        tick(20);
    }
    assert(all_phase(P_RESULTS));
    for(unsigned i=0;i<4;i++){
        assert(nodes[i].state.count==4);
        for(unsigned j=0;j<4;j++)assert(nodes[i].state.players[j].score==times[j]+1);
    }
    load(0);assert(party_rank(0)==3 && party_rank(1)==4 && party_rank(2)==1 && party_rank(3)==1);save();
    uint32_t old_round=nodes[0].state.round;
    for(unsigned i=0;i<4;i++)click(i);
    wait_phase(P_LOBBY,5000);tick(1000);
    for(unsigned i=0;i<4;i++)assert(nodes[i].state.round==old_round+1 && nodes[i].state.local_flags==0);

    /* Pure packet validation, malformed roster and stale sequence rejection. */
    load(0);party_send(P_SNAPSHOT);save();assert(bus_count);
    mesh_packet_t snapshot=bus[0].p;bus_count=0;assert(party_valid(&snapshot));
    mesh_packet_t bad=snapshot;bad.payload[0]=1;assert(!party_valid(&bad));
    bad=snapshot;bad.payload[19]=5;assert(!party_valid(&bad));
    bad=snapshot;memcpy(bad.payload+50,bad.payload+24,6);assert(!party_valid(&bad));
    bad=snapshot;bad.channel=1;assert(!party_valid(&bad));
    bad=snapshot;bad.payload[1]=P_PLAYER;assert(!party_valid(&bad));
    load(1);uint32_t seq=s_party.host_sequence;
    party_put(snapshot.payload+20,seq);snapshot.payload[18]=P_CLOSED;
    reaction_party_receive(&snapshot);reaction_party_proc();assert(s_party.phase==P_LOBBY);save();bus_count=0;

    /* Second round: queued pre-GO press is EARLY, missing players are DNF. */
    drop_every=0;for(unsigned i=0;i<4;i++)click(i);wait_phase(P_RUN,9000);
    event(1,EC06_EVENT_ENC_DOWN,clock_us);tick(20);assert(nodes[1].state.local_score==PARTY_EARLY);
    for(unsigned n=0;n<26000 && !all_phase(P_RESULTS);n+=20){
        party_t *p=&nodes[0].state;
        if(p->go && !p->local_score && clock_us>=p->go+200000)event(0,EC06_EVENT_ENC_DOWN,(uint64_t)(p->go+200000));
        tick(20);
    }
    assert(all_phase(P_RESULTS));
    for(unsigned i=0;i<4;i++)assert(nodes[0].state.players[i].score);
    assert(nodes[0].state.players[2].score==PARTY_DNF);
    /* Host disappearance: clients close instead of electing another room mid-round. */
    nodes[0].active=false;tick(8500);assert(nodes[1].state.phase==P_CLOSED);
    puts("PASS: 4 production instances, 5s stable-ready lobby, newcomer/reset, cancel ready, lock ACK/retries, late join, ranks/ties, new round, old sequence, early/DNF, host loss");
}

static void setup_four(uint8_t game){
    memset(nodes,0,sizeof(nodes));bus_count=0;clock_us=1000000;drop_every=0;test_game=game;
    open_node(0);click(0);for(unsigned i=1;i<4;i++)join(i);tick(2500);
    assert(nodes[0].state.count==4);
    for(unsigned i=0;i<4;i++)click(i);
    wait_phase(P_RUN,12000);
}
static void test_time(void){
    setup_four(PARTY_GAME_TIME);drop_every=7;
    unsigned target=nodes[0].state.target;assert(target>=1 && target<=10);
    int offsets[]={-100,80,-80,300};bool hit[4]={false};
    for(unsigned n=0;n<25000 && !all_phase(P_RESULTS);n+=20){
        for(unsigned i=0;i<4;i++){
            party_t *p=&nodes[i].state;assert(p->target==target);
            int elapsed=(int)target*1000+offsets[i];
            if(!hit[i] && p->go && clock_us>=p->go+(int64_t)elapsed*1000){
                event(i,EC06_EVENT_ENC_DOWN,(uint64_t)(p->go+(int64_t)elapsed*1000));hit[i]=true;
            }
        }
        tick(20);
    }
    assert(all_phase(P_RESULTS));
    for(unsigned i=0;i<4;i++)for(unsigned j=0;j<4;j++)assert(nodes[i].state.players[j].score==(unsigned)((int)target*1000+offsets[j]+1));
    load(0);assert(party_rank(0)==3 && party_rank(1)==1 && party_rank(2)==1 && party_rank(3)==4);
    assert(party_metric((uint16_t)(target*1000+1))==0);
    assert(party_metric(TIME_DNF)==UINT32_MAX);save();
    for(unsigned i=0;i<4;i++)click(i);
    wait_phase(P_LOBBY,5000);tick(1000);
    for(unsigned i=0;i<4;i++)click(i);
    wait_phase(P_RUN,10000);
    event(1,EC06_EVENT_ENC_DOWN,clock_us);tick(20);assert(nodes[1].state.local_score==TIME_EARLY);
    wait_phase(P_RESULTS,35000);assert(nodes[0].state.players[2].score==TIME_DNF);
    /* Score ranges differ by game; unknown game packets cannot enter this room. */
    assert(party_score_valid(MESH_TYPE_TIME_GUESS,20001));
    assert(!party_score_valid(MESH_TYPE_TIME_GUESS,20002));
    assert(party_score_valid(MESH_TYPE_TIME_GUESS,TIME_EARLY));
    load(0);party_send(P_SNAPSHOT);save();mesh_packet_t p=bus[0].p;bus_count=0;
    assert(party_valid(&p));p.payload[49]=11;assert(!party_valid(&p));
    /* Inclusive target endpoints and complete 1..10 generation coverage. */
    bool targets[11]={false};load(0);party_t original=s_party;
    for(unsigned n=0;n<10;n++){
        s_party=original;s_party.phase=P_LOBBY;s_party.count=2;s_party.local_flags=P_READY;s_party.local_score=0;
        s_party.players[1].flags=P_READY;s_party.players[1].seen=party_ms();
        s_party.quiet=true;s_party.quiet_at=party_ms()-5000;
        party_host_tick();assert(s_party.target>=1 && s_party.target<=10);targets[s_party.target]=true;
    }
    for(unsigned n=1;n<=10;n++)assert(targets[n]);
    s_party=original;
    p.type=MESH_TYPE_REACTION;p.payload[18]=P_LOBBY;p.payload[49]=0;
    for(unsigned i=0;i<p.payload[19];i++)party_put16(p.payload+24+i*26+23,0);
    assert(party_valid(&p));
    unsigned queued=s_party_count;reaction_party_receive(&p);assert(s_party_count==queued);
    save();bus_count=0;
    puts("PASS: 4-player random 1-10s target, same target snapshots, signed error/ties, retransmission, early/20s DNF, score bounds, rematch");
}
static void test_bomb(void){
    setup_four(PARTY_GAME_BOMB);drop_every=7;
    assert(nodes[0].state.bomb_low==1 && nodes[0].state.bomb_high==100 && nodes[0].state.bomb_turn==0);
    unsigned secret=nodes[0].state.bomb_secret;assert(secret>=1 && secret<=100);
    load(0);party_send(P_SNAPSHOT);save();assert(bus_count);
    mesh_packet_t snapshot=bus[0].p;bus_count=0;
    assert(snapshot.payload[49]==0); /* Secret not broadcast during play. */
    /* Out-of-turn and stale-revision commands must not advance the game. */
    load(0);uint32_t revision=s_party.revision;
    party_bomb_guess(1,50,revision);assert(s_party.revision==revision);
    party_bomb_guess(0,0,revision);assert(s_party.revision==revision);
    party_bomb_guess(0,50,revision-1);assert(s_party.revision==revision);save();
    unsigned previous_width=100;
    for(unsigned turn=0;turn<16 && nodes[0].state.phase==P_RUN;turn++){
        party_t *host=&nodes[0].state;unsigned player=host->bomb_turn;
        uint32_t before=host->revision;
        /* Wait until the active player has the authoritative latest range. */
        for(unsigned n=0;n<4000 && nodes[player].state.revision!=before;n+=20)tick(20);
        assert(nodes[player].state.revision==before);
        unsigned low=host->bomb_low,high=host->bomb_high,guess=(low+high)/2;
        if(low!=high && guess==secret)guess=guess<high?guess+1:guess-1;
        load(player);s_party.guess=(uint8_t)guess;save();click(player);
        for(unsigned n=0;n<5000 && nodes[0].state.revision==before;n+=20)tick(20);
        assert(nodes[0].state.revision!=before);
        if(nodes[0].state.phase==P_RUN){
            unsigned width=nodes[0].state.bomb_high-nodes[0].state.bomb_low+1;
            assert(width<previous_width);previous_width=width;
            load(0);uint32_t new_revision=s_party.revision;
            party_bomb_guess(player,guess,before);assert(s_party.revision==new_revision);save();
        }
    }
    wait_phase(P_RESULTS,5000);
    unsigned loser=nodes[0].state.bomb_turn;
    for(unsigned i=0;i<4;i++){
        assert(nodes[i].state.bomb_secret==secret && nodes[i].state.bomb_turn==loser);
        assert(nodes[i].state.players[loser].score==PARTY_DNF);
    }
    for(unsigned i=0;i<4;i++)click(i);
    wait_phase(P_LOBBY,5000);tick(1000);
    for(unsigned i=0;i<4;i++)click(i);
    wait_phase(P_RUN,10000);
    /* No guess within 20 seconds loses the turn and reveals the bomb. */
    wait_phase(P_RESULTS,26000);assert(nodes[0].state.bomb_turn==0);
    for(unsigned i=0;i<4;i++)click(i);
    wait_phase(P_LOBBY,5000);tick(1000);
    for(unsigned i=0;i<4;i++)click(i);
    wait_phase(P_RUN,10000);
    load(0);s_party.guess=s_party.bomb_secret==50?49:50;save();click(0);
    assert(nodes[0].state.bomb_turn==1);
    nodes[1].active=false;tick(12000);
    assert(nodes[0].state.phase==P_RESULTS && nodes[0].state.bomb_turn==1);
    assert(nodes[0].state.players[1].flags&P_DROPPED);
    puts("PASS: 4-player bomb, secret hidden, turn order, range shrink, invalid/stale/duplicate guess rejection, retries, reveal/rematch, 20s timeout");
}
int main(void){test_reaction();test_time();test_bomb();}
