#include "crowd_activity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {uint8_t b[128],len;unsigned n;crowd_activity_t *s;} radio_t;
static bool send(void *ctx,const uint8_t *b,uint8_t n){radio_t *r=ctx;assert(n<=128);memcpy(r->b,b,n);r->len=n;r->n++;return true;}
static mesh_packet_t packet(radio_t *r){mesh_packet_t p;uint8_t dst[6]=MESH_DST_BROADCAST_MAC;assert(mesh_packet_pack(&p,r->s->self,dst,MESH_TYPE_COLLAB,r->b,r->len));p.channel=r->s->channel;p.session_id=r->s->boot;p.msg_id=r->n;p.flags=MESH_FLAG_ROUTED;p.crc16=mesh_packet_compute_crc(&p);return p;}
static void deliver(radio_t *r,crowd_activity_t *s){mesh_packet_t p=packet(r);crowd_receive(s,&p);}
int main(void){
 crowd_activity_t host,a,b;uint8_t mh[6]={2,0,0,0,0,1},ma[6]={2,0,0,0,0,2},mb[6]={2,0,0,0,0,3};radio_t rh={.s=&host},ra={.s=&a},rb={.s=&b};
 crowd_init(&host,mh,100,send,&rh);crowd_init(&a,ma,200,send,&ra);crowd_init(&b,mb,300,send,&rb);
 crowd_tick(&host,1,true,100);crowd_tick(&a,1,true,100);crowd_tick(&b,1,true,100);
 assert(!crowd_create(&host,0,""));assert(crowd_create(&host,0,"Meet here"));deliver(&rh,&a);deliver(&rh,&b);assert(!crowd_create(&a,0,"Takeover"));
 assert(crowd_request(&a,0,2));crowd_tick(&a,1,true,101);mesh_packet_t req=packet(&ra);crowd_receive(&host,&req);assert(host.rooms[0].count==1 && host.rooms[0].people[0].status==2);
 /* Lost ACK, replay returns original result without a second participant. */
 crowd_receive(&host,&req);deliver(&rh,&a);assert(a.result==TEAM_OK && a.confirmed==1 && !a.pending.active);
 assert(crowd_request(&a,0,1));crowd_tick(&a,1,true,1100);deliver(&ra,&host);deliver(&rh,&a);assert(host.rooms[0].count==1 && host.rooms[0].people[0].status==1);
 assert(crowd_request(&b,0,3));crowd_tick(&b,1,true,1100);deliver(&rb,&host);deliver(&rh,&b);assert(host.rooms[0].count==2);
 assert(crowd_close(&host,0));deliver(&rh,&a);assert(!a.rooms[0].open && !crowd_request(&a,0,2));
 /* One host-selected winner, two simultaneous requests, repeat cannot win again. */
 assert(crowd_create(&host,1,"Question one"));deliver(&rh,&a);deliver(&rh,&b);
 assert(crowd_request(&a,1,1));crowd_tick(&a,1,true,2100);mesh_packet_t first=packet(&ra);
 assert(crowd_request(&b,1,1));crowd_tick(&b,1,true,2100);mesh_packet_t second=packet(&rb);
 crowd_receive(&host,&first);deliver(&rh,&a);assert(a.result==TEAM_OK && !memcmp(host.rooms[1].winner,ma,6));
 crowd_receive(&host,&second);deliver(&rh,&b);assert(b.result==TEAM_REJECTED && !memcmp(host.rooms[1].winner,ma,6));
 crowd_receive(&host,&first);deliver(&rh,&a);assert(!host.rooms[1].open);
 /* A previous round cannot claim a newly opened round. */
 assert(crowd_create(&host,1,"Question two"));uint32_t id=host.rooms[1].id;crowd_receive(&host,&first);assert(host.rooms[1].open && host.rooms[1].id==id);
 deliver(&rh,&a); /* New round is pinned until old host freshness expires: safe, no premature entry. */
 crowd_tick(&a,1,true,20000);crowd_tick(&host,1,true,20000); /* periodic last frame is quiz */
 deliver(&rh,&a);assert(a.rooms[1].id==id);assert(crowd_request(&a,1,1));
 crowd_tick(&a,1,true,20001);crowd_tick(&a,1,true,21001);assert(a.retries==1);
 crowd_tick(&a,1,true,28000);assert(a.result==TEAM_TIMEOUT && a.timeouts==1);
 crowd_tick(&a,2,true,28001);assert(!a.rooms[0].active && !a.rooms[1].active);
 puts("PASS crowd: check-in update/dedup, one quiz winner, exact ACK, old round rejection, retry/timeout/channel reset");
}
