#include "gift_runtime.h"
#include "pet_service.h"
#include "message_service.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
static pet_gifts_t gifts;static bool initialized;
static portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
static struct {uint8_t src[6],bytes[33];uint32_t at;} queue[8];static unsigned head,count;
static bool persist(void *ctx,const gift_book_t *book,unsigned asset,int delta){(void)ctx;return pet_service_gift_commit(book,asset,delta);}
static bool transport(void *ctx,const uint8_t *b,uint8_t n,uint8_t ch,uint32_t scope){(void)ctx;return message_service_extras_send(b,n,ch,scope);}
void gift_runtime_receive(const mesh_packet_t *p){
    if(p->type!=MESH_TYPE_COLLAB || p->len!=33 || p->payload[0]!=1 || p->channel!=p->payload[2] || !(p->flags&MESH_FLAG_ROUTED) || p->flags&(MESH_FLAG_ACK_REQUEST|MESH_FLAG_ANNOUNCEMENT|MESH_FLAG_CUSTODY) || !mesh_packet_is_broadcast(p->dst_mac) || p->payload[1]<GIFT_HELLO || p->payload[1]>GIFT_REJECT)return;
    portENTER_CRITICAL(&guard);if(count<8){unsigned at=(head+count++)%8;memcpy(queue[at].src,p->src_mac,6);memcpy(queue[at].bytes,p->payload,33);queue[at].at=(uint32_t)(esp_timer_get_time()/1000);}portEXIT_CRITICAL(&guard);
}
void gift_runtime_tick(bool armed){
    if(!initialized){gift_book_t book;bool ok=pet_service_gift_book(&book);pet_gifts_init(&gifts,&book,persist,transport,NULL);if(!ok)gifts.error=true;initialized=true;}
    uint8_t self[6]={0},ch=0;uint32_t scope=0;bool enabled=message_service_extras_context(self,&ch,&scope);uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    pet_gifts_tick(&gifts,self,pet_service_name(),ch,scope,enabled,armed,now);
    for(unsigned i=0;i<8;i++){uint8_t src[6],b[33];uint32_t at=0;bool have;portENTER_CRITICAL(&guard);have=count!=0;if(have){memcpy(src,queue[head].src,6);memcpy(b,queue[head].bytes,33);at=queue[head].at;head=(head+1)%8;count--;}portEXIT_CRITICAL(&guard);if(!have)break;if(now-at<3000)pet_gifts_receive(&gifts,src,b,33);}
}
pet_gifts_t *gift_runtime_state(void){return &gifts;}
