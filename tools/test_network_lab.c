#include "network_lab.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static network_lab_t s[2];static uint32_t now;static unsigned at,end,mode;
static struct{mesh_packet_t p;unsigned from;uint32_t due;}q[512];
static const uint8_t mac[2][6]={{2,0,0,0,0,1},{2,0,0,0,0,2}};
static uint32_t get32(const uint8_t *b){return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static bool send(void *ctx,const uint8_t *b,uint8_t n){
    unsigned from=(unsigned)(uintptr_t)ctx;if(mode==3)return false;if(mode==1&&b[1]==LAB_PONG)return true;
    if(mode==2&&b[1]==LAB_PONG&&get32(b+12)==1&&b[3]==2)return true;
    assert(end-at<512);unsigned slot=end++%512;q[slot].p=(mesh_packet_t){.type=MESH_TYPE_COLLAB,.flags=MESH_FLAG_ROUTED,.channel=1,.len=n};memcpy(q[slot].p.src_mac,mac[from],6);memset(q[slot].p.dst_mac,255,6);memcpy(q[slot].p.payload,b,n);q[slot].from=from;q[slot].due=now+(mode==2&&b[1]==LAB_PONG&&get32(b+12)==1&&b[3]==1?1680:40);return true;
}
static void pump(unsigned ms){for(unsigned t=0;t<ms;t+=20){now+=20;for(unsigned i=0;i<2;i++)network_lab_tick(&s[i],1,77,true,now);unsigned stop=end;for(unsigned i=at;i<stop;i++){unsigned slot=i%512;if(q[slot].p.len && (int32_t)(now-q[slot].due)>=0){network_lab_receive(&s[q[slot].from^1],&q[slot].p);q[slot].p.len=0;}}while(at<end&&!q[at%512].p.len)at++;}}
static void setup(unsigned kind){now=at=end=mode=0;memset(q,0,sizeof(q));for(unsigned i=0;i<2;i++){network_lab_init(&s[i],mac[i],send,(void *)(uintptr_t)i);network_lab_tick(&s[i],1,77,true,0);}assert(network_lab_start(&s[0],mac[1],kind,123));}
int main(void){
    setup(0);pump(20000);assert(!s[0].view.running&&s[0].view.confirmed==10&&s[0].view.attempts==10&&s[0].view.timeouts==0&&s[0].view.rtt_min==80&&s[0].view.rtt_max==80);
    setup(0);mode=1;pump(20000);assert(s[0].view.timeouts==10&&!s[0].view.confirmed&&s[0].view.attempts==10);
    setup(1);mode=1;pump(40000);assert(!s[0].view.running&&s[0].view.timeouts==10&&s[0].view.attempts==30&&s[0].view.retries==20);
    setup(1);mode=2;pump(20000);assert(s[0].view.confirmed==10&&s[0].view.retries==1&&s[0].view.rtt_max==1720);
    setup(0);pump(20);network_lab_stop(&s[0]);pump(2000);assert(!s[0].view.running&&!s[0].view.confirmed&&!s[0].view.timeouts);
    setup(0);mode=3;pump(5000);assert(s[0].view.local_error&&!s[0].view.running&&!s[0].view.attempts&&!s[0].view.timeouts);
    setup(2);network_lab_tick(&s[0],2,77,true,20);assert(!s[0].view.running);assert(!network_lab_start(&s[0],mac[0],0,124));
    setup(0);pump(20);mesh_packet_t bad=q[0].p;bad.payload[4]^=1;unsigned before=end;network_lab_receive(&s[1],&bad);assert(end==before);bad=q[0].p;bad.src_mac[5]^=9;network_lab_receive(&s[1],&bad);assert(end==before);
    puts("PASS network lab: 10/30 bounded probes, timeout versus ACK metrics, retry limits, matching-attempt RTT, delayed/duplicate protection, stop, scope/channel/source guards");
}
