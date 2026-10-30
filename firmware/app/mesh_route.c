/* Bounded opt-in flooding, not Bluetooth Mesh or an authenticated router. */
#include "mesh_route.h"
#include <string.h>
void mesh_route_enable(mesh_route_t *s,bool enabled){
    if(s->enabled==enabled)return;
    s->enabled=enabled;
    memset(s->pending,0,sizeof(s->pending));
    memset(s->seen,0,sizeof(s->seen));
    memset(s->peers,0,sizeof(s->peers));
    memset(s->events,0,sizeof(s->events));s->event_cursor=0;
    s->beacon_sent=false;s->last_rx_hops=0;
}
unsigned mesh_route_peers(const mesh_route_t *s,uint32_t now){
    unsigned n=0;for(unsigned i=0;i<ROUTE_PEERS;i++)if(s->peers[i].used && now-s->peers[i].seen<15000)n++;
    return n;
}
void mesh_route_receive(mesh_route_t *s,const mesh_packet_t *p,const uint8_t self[6],uint8_t channel,int8_t rssi,uint32_t now,uint32_t random){
    if(!s->enabled || !mesh_packet_verify_crc(p) || !p->session_id || !p->msg_id ||
       !mesh_mac_is_valid_peer(p->src_mac) || mesh_mac_equal(self,p->src_mac))return;
    bool broadcast=mesh_packet_is_broadcast(p->dst_mac);
    if(p->type==MESH_TYPE_NET_BEACON){
        if(!broadcast || p->channel!=channel || p->flags || p->len!=15 || p->payload[0]!=1 || p->payload[14])return;
        for(unsigned j=1;j<14;j++)if(p->payload[j] && (p->payload[j]<32 || p->payload[j]>126))return;
        unsigned slot=0;
        for(unsigned i=0;i<ROUTE_PEERS;i++){
            if(s->peers[i].used && mesh_mac_equal(s->peers[i].mac,p->src_mac)){slot=i;break;}
            if(!s->peers[i].used || now-s->peers[i].seen>now-s->peers[slot].seen)slot=i;
        }
        route_peer_t *peer=&s->peers[slot];peer->used=true;peer->seen=now;peer->rssi=rssi;
        memcpy(peer->mac,p->src_mac,6);memcpy(peer->name,p->payload+1,14);return;
    }
    if(!(p->flags&MESH_FLAG_ROUTED) ||
       (p->type!=MESH_TYPE_QUICK_MSG && p->type!=MESH_TYPE_MORSE && p->type!=MESH_TYPE_RECEIPT && p->type!=MESH_TYPE_COLLAB && p->type!=MESH_TYPE_SOCIAL && p->type!=MESH_TYPE_CUSTODY_CANCEL))return;
    bool announcement=(p->flags&MESH_FLAG_ANNOUNCEMENT)!=0;
    bool cancel=p->type==MESH_TYPE_CUSTODY_CANCEL && p->channel==0 && p->len==14;
    if(broadcast ? ((!announcement && !cancel && (p->channel!=channel || !channel)) || p->type==MESH_TYPE_RECEIPT) : p->channel!=0)return;
    if(p->type==MESH_TYPE_RECEIPT ? p->len!=8 : !p->len)return;
    unsigned hops=((p->flags&MESH_HOP_MASK)>>MESH_HOP_SHIFT)+1;
    bool recorded=false;
    for(unsigned i=0;i<ROUTE_EVENTS;i++){const route_event_t *e=&s->events[i];if(e->used && e->session==p->session_id && e->id==p->msg_id && e->type==p->type && mesh_mac_equal(e->src,p->src_mac) && now-e->at<3000){recorded=true;break;}}
    if(!recorded){route_event_t *e=&s->events[s->event_cursor++%ROUTE_EVENTS];*e=(route_event_t){.used=true,.type=p->type,.hops=hops,.rssi=rssi,.session=p->session_id,.id=p->msg_id,.at=now};memcpy(e->src,p->src_mac,6);memcpy(e->dst,p->dst_mac,6);}
    if(mesh_mac_equal(p->dst_mac,self) || broadcast)s->last_rx_hops=(uint8_t)hops;
    if(mesh_mac_equal(p->dst_mac,self) || hops>=MESH_TOTAL_HOPS_MAX)return;
    for(unsigned i=0;i<ROUTE_SEEN;i++){
        route_seen_t *seen=&s->seen[i];
        if(seen->used && seen->id==p->msg_id && seen->session==p->session_id && seen->type==p->type &&
           mesh_mac_equal(seen->mac,p->src_mac) && now-seen->time<1800){s->duplicates++;return;}
    }
    for(unsigned i=0;i<ROUTE_PENDING;i++)if(s->pending[i].used && now-s->pending[i].created>=3000)s->pending[i].used=false;
    unsigned slot=ROUTE_PENDING;
    for(unsigned i=0;i<ROUTE_PENDING;i++)if(!s->pending[i].used){slot=i;break;}
    if(slot==ROUTE_PENDING){s->dropped++;return;}
    route_seen_t *seen=&s->seen[s->cursor++%ROUTE_SEEN];
    *seen=(route_seen_t){.used=true,.session=p->session_id,.id=p->msg_id,.type=p->type,.time=now};memcpy(seen->mac,p->src_mac,6);
    route_pending_t *next=&s->pending[slot];next->used=true;next->created=now;next->due=now+80+random%161;next->packet=*p;
    next->packet.flags=(uint8_t)((p->flags&~MESH_HOP_MASK)|(hops<<MESH_HOP_SHIFT));
    next->packet.crc16=mesh_packet_compute_crc(&next->packet);
}
bool mesh_route_pop(mesh_route_t *s,mesh_packet_t *p,uint32_t now){
    if(!s->enabled || (s->forward_sent && now-s->last_forward<250))return false;
    for(unsigned i=0;i<ROUTE_PENDING;i++)if(s->pending[i].used){
        route_pending_t *next=&s->pending[i];
        if(now-next->created>=3000){next->used=false;s->dropped++;continue;}
        if((int32_t)(now-next->due)<0)continue;
        *p=next->packet;next->used=false;s->last_forward=now;s->forward_sent=true;return true;
    }
    return false;
}
bool mesh_route_beacon_due(mesh_route_t *s,uint32_t now){
    if(!s->enabled || (s->beacon_sent && now-s->beacon_at<5000))return false;
    s->beacon_sent=true;s->beacon_at=now;return true;
}
