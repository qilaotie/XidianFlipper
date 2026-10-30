#include "network_lab.h"
#include <string.h>
static uint32_t get32(const uint8_t *b){return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static void put32(uint8_t *b,uint32_t n){for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(n>>(i*8));}
static unsigned max_attempts(const network_lab_t *s){return s->view.preset==0?1:s->view.preset==1?3:2;}
static void finish_probe(network_lab_t *s){s->inflight=false;s->due=s->now+(s->view.preset==2?300:500);if(s->view.started==s->view.planned)s->view.running=false;}
void network_lab_init(network_lab_t *s,const uint8_t self[6],team_send_fn send,void *ctx){memset(s,0,sizeof(*s));memcpy(s->self,self,6);s->send=send;s->ctx=ctx;}
void network_lab_stop(network_lab_t *s){s->view.running=false;s->inflight=s->queue_waiting=false;}
bool network_lab_start(network_lab_t *s,const uint8_t target[6],unsigned preset,uint32_t token){
    if(!s->view.enabled || s->view.running || !target || !mesh_mac_is_valid_peer(s->self) || !mesh_mac_is_valid_peer(target) || mesh_mac_equal(target,s->self) || preset>2 || !token)return false;
    s->view=(network_lab_view_t){.running=true,.enabled=true,.preset=preset,.planned=preset==2?30:10};memcpy(s->view.target,target,6);s->token=token;s->due=s->now;s->inflight=false;return true;
}
void network_lab_tick(network_lab_t *s,uint8_t ch,uint32_t scope,bool enabled,uint32_t now){
    if(s->channel!=ch || s->scope!=scope || !enabled)network_lab_stop(s);
    s->channel=ch;s->scope=scope;s->view.enabled=enabled;s->now=now;
    if(!s->view.running)return;
    if(s->inflight && now-s->sent[s->attempt-1]>=1000){if(s->attempt>=max_attempts(s)){s->view.timeouts++;finish_probe(s);}else s->inflight=false;}
    if(!s->view.running || s->inflight || (int32_t)(now-s->due)<0)return;
    bool retry=s->attempt && s->view.started>s->view.confirmed+s->view.timeouts;
    unsigned attempt=retry?s->attempt+1:1;uint32_t seq=retry?s->view.started:s->view.started+1;
    uint8_t b[28]={1,LAB_PING,ch,(uint8_t)attempt};put32(b+4,scope);put32(b+8,s->token);put32(b+12,seq);memcpy(b+16,s->self,6);memcpy(b+22,s->view.target,6);
    if(!s->send || !s->send(s->ctx,b,sizeof(b))){if(!s->queue_waiting){s->queue_waiting=true;s->queue_wait=now;}if(now-s->queue_wait>=3000){s->view.local_error=true;network_lab_stop(s);}else s->due=now+200;return;}
    s->queue_waiting=false;
    if(!retry)s->view.started++;else s->view.retries++;
    s->attempt=attempt;s->sent[attempt-1]=now;s->inflight=true;s->view.attempts++;
}
void network_lab_receive(network_lab_t *s,const mesh_packet_t *p){
    const uint8_t *b=p->payload;
    if(!s->view.enabled || p->type!=MESH_TYPE_COLLAB || p->len!=28 || p->channel!=s->channel || !(p->flags&MESH_FLAG_ROUTED) || p->flags&(MESH_FLAG_ACK_REQUEST|MESH_FLAG_ANNOUNCEMENT|MESH_FLAG_CUSTODY) || !mesh_packet_is_broadcast(p->dst_mac) || b[0]!=1 || (b[1]!=LAB_PING && b[1]!=LAB_PONG) || b[2]!=s->channel || get32(b+4)!=s->scope || !get32(b+8) || !get32(b+12) || b[3]<1 || b[3]>3 || !mesh_mac_is_valid_peer(b+16) || !mesh_mac_is_valid_peer(b+22))return;
    if(b[1]==LAB_PING){if(!mesh_mac_equal(p->src_mac,b+16) || !mesh_mac_equal(s->self,b+22) || (s->responded && s->now-s->response_at<100))return;uint8_t reply[28];memcpy(reply,b,28);reply[1]=LAB_PONG;if(s->send && s->send(s->ctx,reply,28)){s->responded=true;s->response_at=s->now;}return;}
    if(!s->view.running || !s->inflight || !mesh_mac_equal(b+16,s->self) || !mesh_mac_equal(p->src_mac,s->view.target) || !mesh_mac_equal(b+22,s->view.target) || get32(b+8)!=s->token || get32(b+12)!=s->view.started || b[3]>s->attempt)return;
    unsigned rtt=s->now-s->sent[b[3]-1];s->view.last_rtt=rtt;if(!s->view.confirmed || rtt<s->view.rtt_min)s->view.rtt_min=rtt;if(rtt>s->view.rtt_max)s->view.rtt_max=rtt;s->view.rtt_sum+=rtt;s->view.confirmed++;s->attempt=0;finish_probe(s);
}
