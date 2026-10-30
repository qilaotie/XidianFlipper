#include "party_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static party_link_t s[8];
static struct {mesh_packet_t packet;unsigned sender;} queue[512];
static unsigned at,end,clock_ms,serial,count,drop_ack,drop_every,send_count;
static mesh_packet_t last_request,last_state,last_private;
static mesh_packet_t last_invite;
static mesh_packet_t states[8],privates[8],rosters[8];
bool mesh_packet_is_broadcast(const uint8_t *m){for(unsigned i=0;i<6;i++)if(m[i]!=255)return false;return true;}
bool mesh_mac_is_valid_peer(const uint8_t *m){unsigned nonzero=0;for(unsigned i=0;i<6;i++)nonzero|=m[i];return nonzero && !mesh_packet_is_broadcast(m) && !(m[0]&1);}
static bool send(void *ctx,const uint8_t *b,uint8_t n){
    unsigned sender=(unsigned)(uintptr_t)ctx;assert(n<=128);mesh_packet_t p={.type=MESH_TYPE_COLLAB,.len=n,.flags=MESH_FLAG_ROUTED,.channel=s[sender].channel,.session_id=s[sender].boot,.msg_id=++serial};
    memcpy(p.src_mac,s[sender].self,6);memset(p.dst_mac,255,6);memcpy(p.payload,b,n);
    if(b[1]==22)last_request=p;if(b[1]==20)last_state=p;if(b[1]==21)last_private=p;
    if(b[1]==20)states[sender]=p;if(b[1]==21)privates[sender]=p;if(b[1]==24)rosters[sender]=p;
    if(b[1]==26)last_invite=p;
    if((b[1]==23 && drop_ack) || (drop_every && ++send_count%drop_every==0))return true;
    assert(end-at<512);queue[end%512].packet=p;queue[end++%512].sender=sender;return true;
}
static void drain(void){while(at<end){unsigned i=at++%512;for(unsigned j=0;j<count;j++)if(j!=queue[i].sender)party_link_receive(&s[j],&queue[i].packet);}}
static void pump(unsigned duration){for(unsigned t=0;t<duration;t+=50){clock_ms+=50;for(unsigned i=0;i<count;i++)party_link_tick(&s[i],1,0,true,clock_ms);drain();}}
static void setup(unsigned n){count=n;at=end=clock_ms=serial=drop_ack=drop_every=send_count=0;for(unsigned i=0;i<n;i++){uint8_t mac[6]={2,3,4,5,6,i+1};party_link_init(&s[i],mac,100+i,send,(void *)(uintptr_t)i);party_link_tick(&s[i],1,0,true,1);}}
static void choose(unsigned i,unsigned host){assert(party_link_join_table(&s[i],s[host].self,s[host].boot,s[host].id,"friend"));}
static void join(unsigned i){choose(i,0);pump(9000);printf("join %u n%u pending%d result%u tries%u drop%u\n",i,s[0].n,s[i].pending.active,s[i].result,s[i].pending.tries,drop_every);fflush(stdout);assert(!s[i].pending.active&&s[i].result==TEAM_OK&&s[0].n==i+1);}
static void ready(unsigned i){party_snapshot_t v;party_link_snapshot(&s[i],&v);assert(v.active&&v.ready&&!v.stale&&v.game.me==i&&v.game.n==s[0].n);}
static unsigned table_count(unsigned i){party_snapshot_t v;party_link_snapshot(&s[i],&v);unsigned n=0;for(unsigned j=0;j<PARTY_TABLES;j++)n+=v.tables[j].active;return n;}
static bool start(unsigned host){
    for(unsigned i=0;i<count;i++)if(i!=host && s[i].active && !s[i].watching && !memcmp(s[i].host,s[host].self,6) && s[i].view.me<s[host].n){pump(5000);assert(party_link_request(&s[i],PARTY_READY,0,1,0,0,""));pump(5000);}
    return party_link_request(&s[host],PARTY_START,0,0,0,0,"");
}
static void multitable(void){
    setup(8);s[1].boot=s[0].boot; /* Same boot and game ID still differ by host MAC. */
    assert(party_link_create(&s[0],PG_TIC,1,"Alpha"));assert(party_link_create(&s[1],PG_TIC,2,"Beta"));pump(6000);
    assert(!s[2].active && table_count(2)==2 && s[0].local && s[1].local);
    assert(!party_link_create(&s[0],PG_UNO,1,"Alpha"));
    choose(2,1);choose(3,0);pump(9000);
    assert(s[0].n==2 && s[1].n==2 && s[2].ready && s[3].ready && s[2].joined && s[3].joined);
    assert(s[2].view.me==1 && !memcmp(s[2].host,s[1].self,6) && !strcmp(s[2].names[0],"Beta"));
    assert(!party_link_join_table(&s[2],s[0].self,s[0].boot,s[0].id,"no"));assert(!party_link_create(&s[2],PG_UNO,1,"no"));
    assert(start(0));assert(start(1));pump(6000);
    assert(party_link_request(&s[0],PARTY_MOVE,PG_CARD,0,0,0,""));assert(party_link_request(&s[1],PARTY_MOVE,PG_CARD,4,0,0,""));pump(6000);
    assert(s[3].view.board[0]==1 && !s[3].view.board[4] && s[2].view.board[4]==1 && !s[2].view.board[0]);
    party_view_t saved=s[2].view;uint32_t revision=s[2].revision;
    party_link_receive(&s[2],&states[0]);mesh_packet_t wrong=privates[0];memcpy(wrong.payload+22,s[2].self,6);party_link_receive(&s[2],&wrong);party_link_receive(&s[2],&rosters[0]);
    assert(!memcmp(&saved,&s[2].view,sizeof(saved)) && s[2].revision==revision && !memcmp(s[2].host,s[1].self,6));
    assert(party_link_create(&s[4],PG_LOVE,5,"Gamma"));pump(5000);assert(table_count(5)==3 && !s[5].active);
    assert(!party_link_join_table(&s[5],s[4].self,s[4].boot,s[4].id+1,"bad"));assert(!s[5].active);
    choose(5,4);pump(8000);assert(s[4].n==2 && s[5].view.kind==PG_LOVE && s[5].view.me==1);
    /* An acknowledged JOIN remains locked before the private state arrives. */
    s[5].view.me=255;s[5].ready=false;assert(!party_link_create(&s[5],PG_UNO,1,"no"));pump(5000);
    assert(party_link_request(&s[1],PARTY_CLOSE,0,0,0,0,""));pump(5000);assert(s[2].view.phase==PG_FINISHED);
    choose(2,4);pump(8000);assert(s[4].n==3 && s[2].ready && s[2].view.kind==PG_LOVE && s[2].view.me==2);
    /* Expiry removes only the silent table and never auto-selects another. */
    for(unsigned t=0;t<17000;t+=50){clock_ms+=50;for(unsigned i=0;i<count;i++)if(i!=4)party_link_tick(&s[i],1,0,true,clock_ms);drain();}
    party_snapshot_t v;party_link_snapshot(&s[2],&v);assert(v.stale && v.game.kind==PG_LOVE && !memcmp(v.host,s[4].self,6));
    assert(!party_link_join_table(&s[6],s[4].self,s[4].boot,s[4].id,"late"));
    party_link_tick(&s[6],2,99,true,clock_ms);assert(!s[6].active && table_count(6)==0);
    setup(8);for(unsigned i=0;i<8;i++)assert(party_link_create(&s[i],PG_UNO,i+1,"Host"));pump(8000);
    for(unsigned i=0;i<8;i++)assert(table_count(i)==PARTY_TABLES && s[i].local && s[i].n==1);
    puts("PASS multi-table: explicit selection, same-game host identity isolation, parallel independent matches, private/state filtering, membership lock, different games, expiry, scope clear, eight-table discovery");
}
static unsigned invited(unsigned i){unsigned n=0;for(unsigned j=0;j<PARTY_TABLES;j++)n+=s[i].tables[j].active&&s[i].tables[j].invited;return n;}
static void lobby_features(void){
    setup(4);assert(party_link_create(&s[0],PG_UNO,50,"Host"));pump(5000);
    assert(party_link_request(&s[0],PARTY_INVITE,0,0,0,0,""));drain();assert(invited(1)==1 && !s[1].active);
    assert(!party_link_request(&s[0],PARTY_INVITE,0,0,0,0,""));mesh_packet_t old_invite=last_invite;old_invite.len--;party_link_receive(&s[1],&old_invite);assert(invited(1)==1);
    old_invite=last_invite;pump(61000);assert(!invited(1));party_link_receive(&s[1],&old_invite);assert(!invited(1));
    assert(party_link_request(&s[0],PARTY_INVITE,0,0,0,0,""));drain();assert(invited(1)==1);choose(1,0);pump(9000);
    assert(s[0].ready_mask==1 && !party_link_request(&s[0],PARTY_START,0,0,0,0,""));assert(!party_link_request(&s[1],PARTY_INVITE,0,0,0,0,""));
    drop_ack=1;assert(party_link_request(&s[1],PARTY_READY,0,1,0,0,""));pump(3500);assert(s[0].ready_mask==3 && s[1].pending.active);unsigned revision=s[0].revision;party_link_receive(&s[0],&last_request);assert(s[0].revision==revision);drain();drop_ack=0;pump(7000);assert(!s[1].pending.active && s[1].ready_mask==3);
    assert(party_link_request(&s[1],PARTY_READY,0,0,0,0,""));pump(6000);assert(s[0].ready_mask==1);assert(party_link_request(&s[1],PARTY_READY,0,1,0,0,""));pump(6000);mesh_packet_t late=last_request;
    choose(2,0);pump(9000);assert(s[0].n==3 && s[0].ready_mask==1);
    uint32_t nonce=++s[1].sequence;for(unsigned i=0;i<4;i++)late.payload[28+i]=(uint8_t)(nonce>>(8*i));party_link_receive(&s[0],&late);drain();assert(s[0].ready_mask==1);assert(start(0));pump(7000);
    assert(!invited(1));unsigned queued=end;assert(party_link_watch_table(&s[3],s[0].self,s[0].boot,s[0].id));assert(end==queued);pump(7000);
    party_snapshot_t v;party_link_snapshot(&s[3],&v);assert(v.watching && v.ready && v.game.me==255 && !v.game.nh && !v.game.role && s[0].n==3);
    for(unsigned i=0;i<32;i++)assert(!v.game.hand[i]);for(unsigned i=0;i<16;i++)assert(!v.game.extra[i]);
    mesh_packet_t private_frame=privates[0];memcpy(private_frame.payload+22,s[3].self,6);party_link_receive(&s[3],&private_frame);assert(s[3].view.me==255 && !s[3].view.nh);
    for(unsigned op=PARTY_JOIN;op<=PARTY_INVITE;op++)assert(!party_link_request(&s[3],op,PG_CARD,0,0,0,""));
    assert(party_link_request(&s[0],PARTY_MOVE,PG_DRAW,0,0,0,""));pump(7000);assert(s[3].revision==s[0].revision && s[3].ready && s[3].view.nh==0);
    assert(party_link_stop_watching(&s[3]));assert(!s[3].active && s[0].rules.phase==PG_PLAYING && s[0].n==3);
    puts("PASS lobby: invitation rate/expiry/replay, no auto-join, all-ready gate, ready cancel/retry dedup, new-member invalidation/late ready rejection, spectator public sync/private isolation/read-only/exit");
}
int main(void){
    lobby_features();
    multitable();
    for(unsigned kind=0;kind<TABLETOP_GAMES;kind++){
        if(!party_game_available(kind))continue;
        unsigned n=party_min(kind);setup(n);assert(party_link_create(&s[0],kind,kind+1,"host"));pump(2000);
        for(unsigned i=1;i<n;i++)join(i);
        assert(start(0));pump(7000);
        for(unsigned i=0;i<n;i++){ready(i);if(i)assert(s[i].view.kind==kind&&s[i].view.phase==s[0].rules.phase);}
    }
    setup(2);for(unsigned k=0;k<TABLETOP_GAMES;k++)if(!party_game_available(k))assert(!party_link_create(&s[0],k,1,"host"));
    setup(3);assert(party_link_create(&s[0],PG_YACHT,123,"Host"));pump(2000);join(1);join(2);for(unsigned i=0;i<3;i++)ready(i);
    assert(start(0));pump(4000);assert(s[1].view.phase==PG_PLAYING);
    assert(party_link_request(&s[0],PARTY_MOVE,PG_ROLL,0,0,0,""));assert(party_link_request(&s[0],PARTY_MOVE,PG_SCORE,0,0,0,""));pump(5000);assert(s[1].view.turn==1);
    drop_ack=1;assert(party_link_request(&s[1],PARTY_MOVE,PG_ROLL,0,0,0,""));pump(3500);assert(s[0].rules.rolls==1&&s[1].pending.active);unsigned rev=s[0].revision;
    for(unsigned i=0;i<5;i++)party_link_receive(&s[0],&last_request);assert(s[0].revision==rev&&s[0].rules.rolls==1);drain();drop_ack=0;pump(4500);assert(!s[1].pending.active&&s[1].result==TEAM_OK);ready(1);
    /* An old publication and another player's private frame cannot overwrite view. */
    party_view_t saved=s[1].view;mesh_packet_t malformed=last_private;memcpy(malformed.payload+22,s[2].self,6);party_link_receive(&s[1],&malformed);assert(!memcmp(&saved,&s[1].view,sizeof(saved)));
    malformed=last_state;malformed.session_id++;party_link_receive(&s[1],&malformed);assert(!memcmp(&saved,&s[1].view,sizeof(saved)));
    malformed=last_state;malformed.payload[64]=255;party_link_receive(&s[1],&malformed);assert(!memcmp(&saved,&s[1].view,sizeof(saved)));
    malformed=last_state;malformed.payload[22]=PG_CODE;malformed.payload[56]=255;party_link_receive(&s[1],&malformed);assert(!memcmp(&saved,&s[1].view,sizeof(saved)));
    assert(party_link_request(&s[1],PARTY_MOVE,PG_SCORE,1,0,0,""));pump(4500);assert(s[0].rules.turn==2);
    /* Rebooted client gets its existing seat after rediscovery and JOIN. */
    uint8_t mac[6];memcpy(mac,s[2].self,6);party_link_init(&s[2],mac,202,send,(void *)(uintptr_t)2);pump(5000);assert(!s[2].active);choose(2,0);pump(5000);ready(2);assert(party_link_request(&s[2],PARTY_JOIN,0,0,0,0,"new"));pump(5000);ready(2);assert(s[0].n==3);
    /* Room/channel change invalidates host ownership and old pending operations. */
    party_link_tick(&s[2],1,77,true,clock_ms);assert(!s[2].active&&!s[2].local&&!s[2].ready);party_link_tick(&s[2],1,0,true,clock_ms);pump(5000);assert(!s[2].active);choose(2,0);pump(5000);
    assert(party_link_request(&s[2],PARTY_JOIN,0,0,0,0,""));pump(4000);assert(party_link_request(&s[2],PARTY_LEAVE,0,0,0,0,""));pump(5000);assert(s[0].rules.phase==PG_FINISHED&&s[1].view.phase==PG_FINISHED);
    setup(8);assert(party_link_create(&s[0],PG_UNO,8,"host"));pump(2000);for(unsigned i=1;i<8;i++)join(i);pump(6000);for(unsigned i=0;i<8;i++)ready(i);
    assert(start(0));pump(7000);for(unsigned i=0;i<8;i++)ready(i);
    /* Every opcode/length with malformed data is bounded and non-mutating. */
    party_rules_t rules=s[0].rules;
    for(unsigned len=0;len<=128;len++){mesh_packet_t bad={.type=MESH_TYPE_COLLAB,.channel=1,.flags=MESH_FLAG_ROUTED,.len=len};memset(bad.dst_mac,255,6);for(unsigned op=20;op<=26;op++){bad.payload[0]=1;bad.payload[1]=op;bad.payload[2]=1;party_link_receive(&s[0],&bad);assert(!memcmp(&rules,&s[0].rules,sizeof(rules)));}}
    setup(2);drop_every=5;assert(party_link_create(&s[0],PG_TIC,1,"host"));pump(6000);join(1);assert(start(0));pump(6000);
    unsigned moves[]={0,3,1,4,2};for(unsigned i=0;i<5;i++){unsigned p=s[0].rules.turn;assert(party_link_request(&s[p],PARTY_MOVE,PG_CARD,moves[i],0,0,""));pump(7000);}assert(s[0].rules.winner==1&&s[1].view.winner==1&&s[1].view.phase==PG_FINISHED);
    puts("PASS transport: 2/3/8 simulated devices, ACK loss/retry/dedup, stale/target validation, reboot rejoin, scope reset, eight-player state, packet loss, completed tic-tac-toe");
    party_link_init(&s[2],s[1].self,400,send,(void *)(uintptr_t)2);party_link_tick(&s[2],1,0,true,clock_ms);mesh_packet_t withdrawn=last_state;withdrawn.payload[22]=PG_REVERSI;party_link_receive(&s[2],&withdrawn);assert(!s[2].active);
}
