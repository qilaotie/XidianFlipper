#include "mesh_route.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t A[6]={2,0,0,0,0,1},B[6]={2,0,0,0,0,2},C[6]={2,0,0,0,0,3},D[6]={2,0,0,0,0,4};
static const uint8_t ALL[6]=MESH_DST_BROADCAST_MAC;
static mesh_packet_t packet(const uint8_t *src,const uint8_t *dst,uint8_t type,uint32_t id){
    mesh_packet_t p;assert(mesh_packet_pack(&p,src,dst,type,(const uint8_t *)"Hi (^_^)",8));
    p.session_id=42;p.msg_id=id;p.flags=MESH_FLAG_ROUTED;p.channel=mesh_packet_is_broadcast(dst)?1:0;
    p.crc16=mesh_packet_compute_crc(&p);return p;
}
static void crc(mesh_packet_t *p){p->crc16=mesh_packet_compute_crc(p);}
int main(void){
    mesh_route_t b={0},c={0},d={0};mesh_packet_t p=packet(A,C,MESH_TYPE_QUICK_MSG,1),q,r;
    mesh_route_receive(&b,&p,B,1,-65,0,0);assert(!mesh_route_pop(&b,&q,500));
    mesh_route_enable(&b,true);mesh_route_enable(&c,true);mesh_route_enable(&d,true);
    mesh_route_receive(&b,&p,B,1,-65,0,0);assert(!mesh_route_pop(&b,&q,79));assert(mesh_route_pop(&b,&q,80));
    assert(mesh_packet_verify_crc(&q)&&((q.flags&MESH_HOP_MASK)>>2)==1&&!memcmp(q.payload,p.payload,8));
    assert(mesh_mac_equal(q.src_mac,A)&&mesh_mac_equal(q.dst_mac,C)&&q.session_id==42&&q.msg_id==1);
    mesh_route_receive(&c,&q,C,1,-70,100,0);assert(c.last_rx_hops==2&&!mesh_route_pop(&c,&r,500));
    assert(c.events[0].used && c.events[0].hops==2 && c.events[0].rssi==-70 && !memcmp(c.events[0].src,A,6));
    assert(mesh_route_peers(&c,100)==0); /* A is an origin, not a direct beacon neighbor. */
    /* Original sender may retry after a lost receiver write/receipt; each local burst is deduped. */
    mesh_route_receive(&b,&p,B,1,-65,180,0);assert(b.duplicates==1&&!mesh_route_pop(&b,&r,500));
    assert(b.event_cursor==1); /* Duplicate observation does not flood the event ring. */
    mesh_route_receive(&b,&p,B,1,-65,3000,0);assert(mesh_route_pop(&b,&r,3080));
    /* Reverse receipt follows the same bounded path, with endpoint identity untouched. */
    p=packet(C,A,MESH_TYPE_RECEIPT,2);mesh_route_receive(&b,&p,B,1,-65,4000,0);assert(mesh_route_pop(&b,&q,4080));
    mesh_route_t a={0};mesh_route_enable(&a,true);mesh_route_receive(&a,&q,A,1,-65,4100,0);assert(a.last_rx_hops==2);
    /* Broadcast chain is limited to three links; max payload survives each encode/decode. */
    p=packet(A,ALL,MESH_TYPE_QUICK_MSG,3);p.len=128;memset(p.payload,'Z',128);crc(&p);
    uint8_t bytes[MESH_PACKET_MAX_LEN];size_t n=mesh_packet_to_bytes(&p,bytes,sizeof(bytes));assert(n==157);
    assert(mesh_packet_unpack(&p,bytes,n));mesh_route_receive(&b,&p,B,1,-60,5000,0);assert(mesh_route_pop(&b,&q,5080));
    mesh_route_receive(&c,&q,C,1,-60,5100,0);assert(mesh_route_pop(&c,&r,5180));
    mesh_route_receive(&d,&r,D,1,-60,5200,0);assert(d.last_rx_hops==3&&!mesh_route_pop(&d,&q,6000));
    assert(r.len==128&&!memcmp(r.payload,p.payload,128));
    /* Neither legacy packets nor games/live Morse are eligible. */
    mesh_route_enable(&b,false);mesh_route_enable(&b,true);
    p=packet(A,ALL,MESH_TYPE_QUICK_MSG,4);p.flags=0;crc(&p);mesh_route_receive(&b,&p,B,1,-65,7000,0);assert(!mesh_route_pop(&b,&q,7500));
    p=packet(A,ALL,MESH_TYPE_RPS,5);assert(!mesh_packet_verify_crc(&p));
    p=packet(A,ALL,MESH_TYPE_QUICK_MSG,6);p.flags|=3<<2;crc(&p);assert(!mesh_packet_to_bytes(&p,bytes,sizeof(bytes)));
    p=packet(A,ALL,MESH_TYPE_QUICK_MSG,7);mesh_route_receive(&b,&p,B,2,-65,8000,0);assert(!mesh_route_pop(&b,&q,8500));
    p.crc16^=1;mesh_route_receive(&b,&p,B,1,-65,8000,0);assert(!mesh_route_pop(&b,&q,8500));
    /* Fixed queue, global rate limit, expiry and off switch. */
    for(unsigned i=0;i<10;i++){p=packet(A,ALL,MESH_TYPE_QUICK_MSG,100+i);mesh_route_receive(&b,&p,B,1,-65,9000,0);}
    assert(b.dropped>=2);assert(mesh_route_pop(&b,&q,9080));assert(!mesh_route_pop(&b,&q,9100));assert(mesh_route_pop(&b,&q,9330));
    mesh_route_enable(&b,false);assert(!mesh_route_pop(&b,&q,10000));mesh_route_enable(&b,true);
    p=packet(A,ALL,MESH_TYPE_QUICK_MSG,200);mesh_route_receive(&b,&p,B,1,-65,11000,0);assert(!mesh_route_pop(&b,&q,14001));
    /* Discovery uses direct-only, validated expiring beacons. */
    uint8_t hello[15]={1};memcpy(hello+1,"Alice",5);assert(mesh_packet_pack(&p,A,ALL,MESH_TYPE_NET_BEACON,hello,15));
    p.channel=1;p.session_id=42;p.msg_id=300;crc(&p);
    mesh_route_receive(&b,&p,B,1,-42,15000,0);assert(mesh_route_peers(&b,15001)==1);
    bool found=false;for(unsigned i=0;i<ROUTE_PEERS;i++)if(b.peers[i].used && !strcmp(b.peers[i].name,"Alice"))found=true;assert(found);
    assert(!mesh_route_pop(&b,&q,16000)&&mesh_route_peers(&b,30000)==0);
    assert(mesh_route_beacon_due(&b,0)&&!mesh_route_beacon_due(&b,4999)&&mesh_route_beacon_due(&b,5000));
    /* Unsigned time wrap must not extend queue lifetime. */
    mesh_route_enable(&b,false);mesh_route_enable(&b,true);p=packet(A,ALL,MESH_TYPE_QUICK_MSG,400);
    mesh_route_receive(&b,&p,B,1,-65,0xfffffff0,0);assert(!mesh_route_pop(&b,&q,20));assert(mesh_route_pop(&b,&q,80));
    puts("PASS production routing: A-B-C/reverse ACK, TTL3, full128, retries/dedup, queues/rate/expiry, discovery/off/wrap; host only");
    /* Announcement crosses channel boundaries; ordinary group traffic never does. */
    mesh_route_enable(&b,false);mesh_route_enable(&b,true);mesh_route_enable(&c,false);mesh_route_enable(&c,true);
    p=packet(A,ALL,MESH_TYPE_QUICK_MSG,500);p.channel=0;p.flags|=MESH_FLAG_ANNOUNCEMENT;crc(&p);
    assert(mesh_packet_verify_crc(&p));mesh_route_receive(&b,&p,B,2,-50,1000,0);assert(mesh_route_pop(&b,&q,1080));
    mesh_route_receive(&c,&q,C,16,-50,1200,0);assert(c.last_rx_hops==2&&mesh_route_pop(&c,&r,1280));
    p=packet(A,ALL,MESH_TYPE_QUICK_MSG,501);mesh_route_receive(&b,&p,B,2,-50,2000,0);assert(!mesh_route_pop(&b,&q,2500));
    p.flags|=MESH_FLAG_ANNOUNCEMENT;crc(&p);assert(!mesh_packet_to_bytes(&p,bytes,sizeof(bytes))); /* wrong channel */
    p=packet(A,C,MESH_TYPE_QUICK_MSG,502);p.flags|=MESH_FLAG_ANNOUNCEMENT;crc(&p);assert(!mesh_packet_to_bytes(&p,bytes,sizeof(bytes)));
    p=packet(A,ALL,MESH_TYPE_QUICK_MSG,503);p.flags=MESH_FLAG_ANNOUNCEMENT;p.channel=0;crc(&p);assert(!mesh_packet_to_bytes(&p,bytes,sizeof(bytes)));
    puts("PASS announcement cross-channel hop2 and invalid channel/unicast/non-routed rejection; group isolation");
}
