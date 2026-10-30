#include "contact_presence.h"
#include <string.h>
void contact_observe(contact_seen_t cache[32],const mesh_packet_t *p,int8_t rssi,uint32_t now){
    if(!mesh_packet_verify_crc(p) || !mesh_mac_is_valid_peer(p->src_mac) || !p->session_id || !p->msg_id)return;
    unsigned slot=0;
    for(unsigned i=0;i<32;i++){if(cache[i].used && mesh_mac_equal(cache[i].mac,p->src_mac)){slot=i;break;}if(!cache[i].used || now-cache[i].heard>now-cache[slot].heard)slot=i;}
    contact_seen_t *c=&cache[slot];if(!c->used || !mesh_mac_equal(c->mac,p->src_mac))memset(c,0,sizeof(*c));
    c->used=true;memcpy(c->mac,p->src_mac,6);c->heard=now;c->hops=1+((p->flags&MESH_HOP_MASK)>>MESH_HOP_SHIFT);
    if(c->hops==1){c->direct=true;c->direct_at=now;c->rssi=rssi;}
}
void contact_status(const contact_seen_t cache[32],const uint8_t mac[6],uint32_t now,contact_presence_t *out){
    memset(out,0,sizeof(*out));for(unsigned i=0;i<32;i++)if(cache[i].used && mesh_mac_equal(cache[i].mac,mac)){
        const contact_seen_t *c=&cache[i];out->age_ms=now-c->heard;out->hops=c->hops;
        if(c->direct && now-c->direct_at<15000){out->state=CONTACT_DIRECT;out->rssi=c->rssi;}
        else if(out->age_ms<60000)out->state=CONTACT_RECENT;
        return;
    }
}
