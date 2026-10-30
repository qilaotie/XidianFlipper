#include "team_activity.h"
#include "contact_presence.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {uint8_t bytes[128],len;unsigned count;team_activity_t *source;} radio_t;
static bool send(void *ctx,const uint8_t *b,uint8_t n){radio_t *r=ctx;memcpy(r->bytes,b,n);r->len=n;r->count++;return true;}
static mesh_packet_t packet(radio_t *r){mesh_packet_t p;uint8_t dst[6]=MESH_DST_BROADCAST_MAC;assert(mesh_packet_pack(&p,r->source->self,dst,MESH_TYPE_COLLAB,r->bytes,r->len));p.channel=r->source->channel;p.session_id=r->source->boot;p.msg_id=r->count;p.flags=MESH_FLAG_ROUTED;p.crc16=mesh_packet_compute_crc(&p);return p;}
static void deliver(radio_t *r,team_activity_t *s){mesh_packet_t p=packet(r);team_receive(s,&p);}
int main(void){
 team_activity_t h,a,b;uint8_t mh[6]={2,0,0,0,0,1},ma[6]={2,0,0,0,0,2},mb[6]={2,0,0,0,0,3};radio_t rh={.source=&h},ra={.source=&a},rb={.source=&b};
 team_init(&h,mh,100,send,&rh);team_init(&a,ma,200,send,&ra);team_init(&b,mb,300,send,&rb);
 team_tick(&h,1,true,100);team_tick(&a,1,true,100);team_tick(&b,1,true,100);
 assert(!team_poll_create(&h,"",2));assert(!team_poll_create(&h,"Q",5));assert(team_poll_create(&h,"Lunch?",4));deliver(&rh,&a);deliver(&rh,&b);
 assert(a.poll.active && !a.poll.local && a.poll.options==4);assert(!team_poll_create(&a,"No takeover",2));
 assert(team_vote(&a,2));assert(a.result==TEAM_WAIT && !a.poll.voters);team_tick(&a,1,true,101);mesh_packet_t vote=packet(&ra);team_receive(&h,&vote);assert(h.poll.voters==1 && h.poll.counts[2]==1);
 /* Dropped ACK, same request is replayed: counts do not change and ACK repeats. */
 team_receive(&h,&vote);assert(h.poll.voters==1);deliver(&rh,&a);assert(a.result==TEAM_OK && !a.pending.active);
 assert(team_vote(&a,1));team_tick(&a,1,true,102);deliver(&ra,&h);deliver(&rh,&a);assert(a.result==TEAM_REJECTED && h.poll.voters==1);
 assert(team_vote(&b,0));team_tick(&b,1,true,103);deliver(&rb,&h);deliver(&rh,&b);assert(b.result==TEAM_OK && h.poll.voters==2);
 assert(team_poll_close(&h));deliver(&rh,&a);assert(!a.poll.open && a.poll.counts[2]==1 && a.poll.counts[0]==1);
 assert(team_board_create(&h,"Find beacon"));deliver(&rh,&a);deliver(&rh,&b);assert(a.board.tasks[0].state==1);
 /* Both claim revision 2. Exactly the first claim succeeds. */
 assert(team_task_request(&a,0,TEAM_CLAIM));team_tick(&a,1,true,104);mesh_packet_t claim=packet(&ra);
 assert(team_task_request(&b,0,TEAM_CLAIM));team_tick(&b,1,true,104);mesh_packet_t rival=packet(&rb);
 team_receive(&h,&claim);deliver(&rh,&a);assert(a.result==TEAM_OK);team_receive(&h,&rival);deliver(&rh,&b);assert(b.result==TEAM_REJECTED && !memcmp(h.board.tasks[0].owner,ma,6));
 team_tick(&h,1,true,2200);deliver(&rh,&a);deliver(&rh,&b);assert(a.board.tasks[0].state==2);
 assert(team_task_request(&b,0,TEAM_COMPLETE));team_tick(&b,1,true,2200);deliver(&rb,&h);deliver(&rh,&b);assert(b.result==TEAM_REJECTED);
 assert(team_task_request(&a,0,TEAM_COMPLETE));team_tick(&a,1,true,2200);deliver(&ra,&h);deliver(&rh,&a);assert(a.result==TEAM_OK && h.board.tasks[0].state==3);
 assert(team_task_add(&h,"Second"));assert(team_task_add(&h,"Third"));assert(team_task_add(&h,"Fourth"));assert(!team_task_add(&h,"Full"));
 /* Old state cannot regress the board; wrong-channel frames cannot mutate it. */
 mesh_packet_t state=packet(&rh);deliver(&rh,&a);uint32_t rev=a.board.rev;state.payload[8]=1;team_receive(&a,&state);assert(a.board.rev==rev);
 state.channel=2;team_receive(&b,&state);assert(b.board.rev<rev);
 /* No ACK means unknown result, never a fabricated successful vote. */
 assert(team_vote(&b,1));team_tick(&b,1,true,2300);team_tick(&b,1,true,10300);assert(b.result==TEAM_TIMEOUT && !b.pending.active);
 team_tick(&a,2,true,10500);assert(!a.poll.active && !a.board.active);team_tick(&h,2,true,10500);assert(!h.poll.active && !h.board.active);
 assert(team_poll_create(&h,"New channel",2));deliver(&rh,&a);assert(a.poll.active && !a.poll.local);
 /* Presence distinguishes direct origin from forwarding-radio RSSI. */
 contact_seen_t cache[32]={0};contact_presence_t status;mesh_packet_t p=packet(&rh);contact_observe(cache,&p,-42,100);contact_status(cache,mh,101,&status);assert(status.state==CONTACT_DIRECT && status.rssi==-42);
 p.flags|=1<<MESH_HOP_SHIFT;p.crc16=mesh_packet_compute_crc(&p);contact_observe(cache,&p,-20,16000);contact_status(cache,mh,16001,&status);assert(status.state==CONTACT_RECENT && status.hops==2 && !status.rssi);
 contact_status(cache,mh,76000,&status);assert(status.state==CONTACT_UNKNOWN);
 puts("PASS team: votes, retries, ACKs, rival claims, ownership, revisions, channel reset, presence");
}
