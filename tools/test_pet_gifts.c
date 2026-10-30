#include "pet_gifts.h"
#include "mesh_packet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static pet_gifts_t s[2];static gift_book_t disk[2];static int gold[2],items[2][3],disk_gold[2],disk_items[2][3];
static unsigned now,at,end,drop_op,fail_owner=99;static bool offline,fail_after_write;
static struct{unsigned from;uint8_t b[33];}q[512];static uint8_t last_commit[33];
static const uint8_t mac[2][6]={{2,0,0,0,0,1},{2,0,0,0,0,2}};
static bool store(void *ctx,const gift_book_t *b,unsigned asset,int delta){
    unsigned i=(unsigned)(uintptr_t)ctx;int value=asset?items[i][asset-1]:gold[i];
    if(value+delta<0 || value+delta>(asset?9:9999))return false;
    if(i==fail_owner && !fail_after_write)return false;
    disk[i]=*b;if(asset)disk_items[i][asset-1]=value+delta;else disk_gold[i]=value+delta;
    if(i==fail_owner)return false;
    if(asset)items[i][asset-1]=value+delta;else gold[i]=value+delta;return true;
}
static bool send(void *ctx,const uint8_t *b,uint8_t n,uint8_t ch,uint32_t scope){
    assert(n==33&&ch==1&&scope==77);unsigned i=(unsigned)(uintptr_t)ctx;if(b[1]==GIFT_COMMIT)memcpy(last_commit,b,33);
    if(b[1]==drop_op || offline)return true;assert(end-at<512);q[end%512].from=i;memcpy(q[end++%512].b,b,33);return true;
}
static void boot(unsigned i){gold[i]=disk_gold[i];memcpy(items[i],disk_items[i],sizeof(items[i]));pet_gifts_init(&s[i],&disk[i],store,send,(void *)(uintptr_t)i);}
static void pump(unsigned ms){for(unsigned t=0;t<ms;t+=50){now+=50;for(unsigned i=0;i<2;i++)pet_gifts_tick(&s[i],mac[i],i?"Bob":"Alice",1,77,true,true,now);while(at<end){unsigned pos=at++%512,j=q[pos].from^1;if(!offline)pet_gifts_receive(&s[j],mac[j^1],q[pos].b,33);}}}
static void setup(void){memset(disk,0,sizeof(disk));now=at=end=drop_op=0;fail_owner=99;offline=fail_after_write=false;for(unsigned i=0;i<2;i++){gold[i]=disk_gold[i]=100;for(unsigned j=0;j<3;j++)items[i][j]=disk_items[i][j]=3;boot(i);}pump(2500);}
static void offer(unsigned asset,unsigned amount){assert(pet_gifts_offer(&s[0],mac[1],asset,amount));pump(100);assert(s[1].offer);}
static void exchange(void){assert(pet_gifts_decide(&s[1],true));pump(10000);assert(s[0].book.outgoing.stage==GIFT_SENT);}
int main(void){
    setup();assert(pet_gifts_offer(&s[0],mac[1],0,10));assert(pet_gifts_offer(&s[1],mac[0],3,1));pump(100);assert(pet_gifts_decide(&s[0],true)&&pet_gifts_decide(&s[1],true));pump(10000);assert(s[0].book.outgoing.stage==GIFT_SENT&&s[1].book.outgoing.stage==GIFT_SENT&&gold[0]==90&&gold[1]==110&&items[0][2]==4&&items[1][2]==2);
    setup();offer(0,10);assert(gold[0]==90&&gold[1]==100);drop_op=GIFT_DONE;assert(pet_gifts_decide(&s[1],true));pump(7000);assert(gold[1]==110&&s[0].book.outgoing.stage==GIFT_COMMITTING);assert(!pet_gifts_cancel(&s[0]));
    boot(0);boot(1);pump(5000);assert(gold[0]==90&&gold[1]==110);drop_op=0;pump(4000);assert(s[0].book.outgoing.stage==GIFT_SENT);
    for(unsigned i=0;i<10;i++)pet_gifts_receive(&s[1],mac[0],last_commit,33);assert(gold[1]==110);
    offer(0,25);exchange();assert(gold[0]==65&&gold[1]==135);uint8_t old[33];memcpy(old,last_commit,33);old[8]=1;old[18]=10;pet_gifts_receive(&s[1],mac[0],old,33);assert(gold[1]==135);
    setup();offer(0,10);offline=true;assert(pet_gifts_cancel(&s[0]));pump(15000);assert(gold[0]==90&&s[0].book.outgoing.stage==GIFT_CANCELLING);boot(0);boot(1);offline=false;pump(6000);assert(gold[0]==100&&gold[1]==100&&s[0].book.outgoing.stage==GIFT_REFUNDED);
    setup();offer(1,1);items[1][0]=9;assert(pet_gifts_decide(&s[1],true));pump(5000);assert(items[0][0]==2&&items[1][0]==9&&s[0].book.outgoing.stage==GIFT_COMMITTING);items[1][0]--;pump(6000);assert(items[1][0]==9&&s[0].book.outgoing.stage==GIFT_SENT);
    setup();offer(0,50);assert(pet_gifts_decide(&s[1],false));pump(5000);assert(gold[0]==100&&gold[1]==100&&s[0].book.outgoing.stage==GIFT_REFUNDED);
    setup();offer(0,10);gold[0]=disk_gold[0]=9999;assert(pet_gifts_decide(&s[1],false));pump(5000);assert(gold[0]==9999&&s[0].book.outgoing.stage==GIFT_OFFERING);gold[0]=disk_gold[0]=9989;pump(5000);assert(gold[0]==9999&&s[0].book.outgoing.stage==GIFT_REFUNDED);
    setup();fail_owner=0;assert(!pet_gifts_offer(&s[0],mac[1],0,10));assert(gold[0]==100&&disk[0].sequence==0);fail_owner=99;
    offer(0,10);fail_owner=1;assert(!pet_gifts_decide(&s[1],true));pump(3000);assert(gold[1]==100&&s[0].book.outgoing.stage==GIFT_OFFERING);fail_owner=99;assert(pet_gifts_decide(&s[1],true));
    drop_op=GIFT_COMMIT;pump(3000);assert(s[0].book.outgoing.stage==GIFT_COMMITTING);fail_owner=1;drop_op=0;pump(3000);assert(gold[1]==100);fail_after_write=true;pump(3500);assert(gold[1]==100&&disk_gold[1]==110);boot(1);fail_owner=99;fail_after_write=false;pump(6000);assert(gold[1]==110&&s[0].book.outgoing.stage==GIFT_SENT);
    setup();for(unsigned i=0;i<GIFT_PEERS;i++){disk[1].receipts[i]=(gift_tx_t){.peer={2,0,0,0,1,i+3},.asset=0,.amount=1,.channel=1,.scope=77,.id=1,.stage=GIFT_CREDITED};}boot(1);pump(2500);assert(pet_gifts_offer(&s[0],mac[1],0,10));pump(6000);assert(gold[0]==100&&gold[1]==100&&s[0].book.outgoing.stage==GIFT_REFUNDED);
    /* Every durable transition survives both devices restarting and a lost step. */
    for(unsigned cut=0;cut<12;cut++){setup();offer(0,10);drop_op=cut%2?GIFT_ACCEPT:GIFT_DONE;assert(pet_gifts_decide(&s[1],true));pump(cut*100);at=end=0;boot(0);boot(1);drop_op=0;pump(15000);assert(gold[0]==90&&gold[1]==110&&s[0].book.outgoing.stage==GIFT_SENT);}
    puts("PASS gifts: bilateral consent, escrow, decline/cancel refund, no timeout refund, restart every phase, lost receipts, duplicate/old commit, capacity backpressure, failed/ambiguous writes, non-evicting ledger limit");
}
