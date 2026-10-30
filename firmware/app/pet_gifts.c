/* Escrow -> recipient prepare -> durable commit -> exactly-once credit -> receipt.
 * No refund from timeouts. Persistent per-peer high watermarks are never evicted. */
#include "pet_gifts.h"
#include "mesh_packet.h"
#include <string.h>
static uint32_t get32(const uint8_t *b){return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static void put32(uint8_t *b,uint32_t n){for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(n>>(8*i));}
static bool amount_valid(unsigned asset,unsigned n){return asset<=3 && n && (asset? n==1:n<=50);}
static bool tx_valid(const gift_tx_t *t){return mesh_mac_is_valid_peer(t->peer) && t->id && t->channel>=1 && t->channel<=16 && amount_valid(t->asset,t->amount);}
static bool tx_empty(const gift_tx_t *t){static const uint8_t zero[6]={0};return !t->id && !t->asset && !t->amount && !t->channel && !t->scope && !memcmp(t->peer,zero,6);}
bool gift_book_valid(const gift_book_t *b){
    if(!b->outgoing.stage && !tx_empty(&b->outgoing))return false;
    if(b->outgoing.stage && (!tx_valid(&b->outgoing) || b->outgoing.stage>GIFT_REFUNDED || b->outgoing.id>b->sequence))return false;
    for(unsigned i=0;i<GIFT_PEERS;i++){
        const gift_tx_t *t=&b->receipts[i];if(!t->stage){if(!tx_empty(t))return false;continue;}
        if(!tx_valid(t) || t->stage>GIFT_DECLINED)return false;
        for(unsigned j=0;j<i;j++)if(b->receipts[j].stage && mesh_mac_equal(t->peer,b->receipts[j].peer))return false;
    }return true;
}
static bool save(pet_gifts_t *s,const gift_book_t *next,unsigned asset,int delta){if(!s->save || !s->save(s->ctx,next,asset,delta)){s->error=true;return false;}s->book=*next;s->error=false;return true;}
static bool same(const gift_tx_t *a,const gift_tx_t *b){return a->id==b->id && a->asset==b->asset && a->amount==b->amount && a->channel==b->channel && a->scope==b->scope && mesh_mac_equal(a->peer,b->peer);}
static int receipt(const pet_gifts_t *s,const uint8_t peer[6]){int empty=-1;for(unsigned i=0;i<GIFT_PEERS;i++){if(s->book.receipts[i].stage && mesh_mac_equal(s->book.receipts[i].peer,peer))return (int)i;if(!s->book.receipts[i].stage)empty=(int)i;}return empty;}
static bool send(pet_gifts_t *s,unsigned op,const gift_tx_t *t){
    uint8_t b[33]={1,(uint8_t)op,t->channel,t->asset};put32(b+4,t->scope);put32(b+8,t->id);memcpy(b+12,t->peer,6);b[18]=t->amount;memcpy(b+20,s->name,13);
    return s->send && s->send(s->ctx,b,sizeof(b),t->channel,t->scope);
}
void pet_gifts_init(pet_gifts_t *s,const gift_book_t *book,gift_save_fn persist,gift_send_fn transport,void *ctx){memset(s,0,sizeof(*s));s->save=persist;s->send=transport;s->ctx=ctx;if(book && gift_book_valid(book))s->book=*book;else s->error=true;}
void pet_gifts_tick(pet_gifts_t *s,const uint8_t self[6],const char *name,uint8_t ch,uint32_t scope,bool enabled,bool armed,uint32_t now){
    if(s->channel!=ch || s->scope!=scope){memset(s->peers,0,sizeof(s->peers));s->offer=false;}
    s->channel=ch;s->scope=scope;s->now=now;s->enabled=enabled;s->armed=armed;memcpy(s->self,self,6);memset(s->name,0,13);
    for(unsigned i=0;name && name[i] && i<12;i++)s->name[i]=(uint8_t)name[i]>=32 && (uint8_t)name[i]<=126?name[i]:'?';
    if(!armed || now-s->incoming_at>=15000)s->offer=false;
    if(!enabled || !mesh_mac_is_valid_peer(self))return;
    if(armed && now-s->last_hello>=2000){gift_tx_t hello={.channel=ch,.scope=scope};memset(hello.peer,255,6);if(send(s,GIFT_HELLO,&hello))s->last_hello=now;}
    const gift_tx_t *t=&s->book.outgoing;
    if(t->stage>=GIFT_OFFERING && t->stage<=GIFT_CANCELLING && t->channel==ch && t->scope==scope && now-s->last_tx>=1500){unsigned op=t->stage==GIFT_OFFERING?GIFT_OFFER:t->stage==GIFT_COMMITTING?GIFT_COMMIT:GIFT_CANCEL;if(send(s,op,t))s->last_tx=now;}
}
bool pet_gifts_offer(pet_gifts_t *s,const uint8_t peer[6],unsigned asset,unsigned amount){
    if(!s->enabled || !s->armed || !peer || mesh_mac_equal(peer,s->self) || !amount_valid(asset,amount) || s->book.sequence==UINT32_MAX || (s->book.outgoing.stage>=GIFT_OFFERING && s->book.outgoing.stage<=GIFT_CANCELLING))return false;
    bool known=false;for(unsigned i=0;i<6;i++)if(s->peers[i].used && mesh_mac_equal(peer,s->peers[i].mac) && s->now-s->peers[i].seen<15000)known=true;if(!known)return false;
    gift_book_t next=s->book;gift_tx_t t={.channel=s->channel,.scope=s->scope,.id=++next.sequence,.asset=asset,.amount=amount,.stage=GIFT_OFFERING};memcpy(t.peer,peer,6);next.outgoing=t;
    if(!save(s,&next,asset,-(int)amount))return false;
    s->last_tx=s->now-1500;return true;
}
bool pet_gifts_cancel(pet_gifts_t *s){if(s->book.outgoing.stage!=GIFT_OFFERING)return false;gift_book_t next=s->book;next.outgoing.stage=GIFT_CANCELLING;if(!save(s,&next,0,0))return false;s->last_tx=s->now-1500;return true;}
bool pet_gifts_decide(pet_gifts_t *s,bool accept){
    if(!s->enabled || !s->armed || !s->offer || s->now-s->incoming_at>=15000)return false;
    gift_tx_t t=s->incoming;int slot=receipt(s,t.peer);if(slot<0)return false;const gift_tx_t *old=&s->book.receipts[slot];
    if(old->stage && (old->id>=t.id || old->stage==GIFT_PREPARED))return false;
    gift_book_t next=s->book;t.stage=accept?GIFT_PREPARED:GIFT_DECLINED;next.receipts[slot]=t;
    if(!save(s,&next,0,0))return false;
    s->offer=false;send(s,accept?GIFT_ACCEPT:GIFT_REJECT,&t);return true;
}
void pet_gifts_receive(pet_gifts_t *s,const uint8_t src[6],const uint8_t *b,unsigned len){
    if(!s->enabled || len!=33 || b[0]!=1 || b[1]<GIFT_HELLO || b[1]>GIFT_REJECT || b[2]!=s->channel || get32(b+4)!=s->scope || b[19] || b[32] || !mesh_mac_is_valid_peer(src) || mesh_mac_equal(src,s->self))return;
    for(unsigned i=20;i<32;i++)if(b[i] && (b[i]<32 || b[i]>126))return;
    unsigned op=b[1];
    if(op==GIFT_HELLO){if(!s->armed || !mesh_packet_is_broadcast(b+12) || get32(b+8) || b[3] || b[18])return;unsigned at=0;for(unsigned i=0;i<6;i++){if(s->peers[i].used && mesh_mac_equal(s->peers[i].mac,src)){at=i;break;}if(!s->peers[i].used || s->now-s->peers[i].seen>s->now-s->peers[at].seen)at=i;}gift_peer_t *p=&s->peers[at];p->used=true;p->seen=s->now;memcpy(p->mac,src,6);memcpy(p->name,b+20,13);return;}
    if(!mesh_mac_equal(b+12,s->self) || !get32(b+8) || !amount_valid(b[3],b[18]))return;
    gift_tx_t t={.channel=b[2],.scope=get32(b+4),.id=get32(b+8),.asset=b[3],.amount=b[18]};memcpy(t.peer,src,6);
    if(op==GIFT_ACCEPT || op==GIFT_DONE || op==GIFT_REJECT){
        const gift_tx_t *out=&s->book.outgoing;if(!same(out,&t))return;gift_book_t next=s->book;
        if(op==GIFT_ACCEPT && out->stage==GIFT_OFFERING){next.outgoing.stage=GIFT_COMMITTING;if(save(s,&next,0,0))s->last_tx=s->now-1500;}
        if(op==GIFT_DONE && out->stage==GIFT_COMMITTING){next.outgoing.stage=GIFT_SENT;save(s,&next,0,0);}
        if(op==GIFT_REJECT && (out->stage==GIFT_OFFERING || out->stage==GIFT_CANCELLING)){next.outgoing.stage=GIFT_REFUNDED;save(s,&next,t.asset,t.amount);}return;
    }
    int slot=receipt(s,src);if(slot<0){
        /* A permanently full, non-evicting ledger has NEVER prepared this peer.
         * It cannot accept any future replay either. No COMMIT was authorized. */
        s->error=true;if(op==GIFT_OFFER || op==GIFT_CANCEL)send(s,GIFT_REJECT,&t);return;
    }const gift_tx_t *old=&s->book.receipts[slot];
    if(old->stage && old->id==t.id){
        if(!same(old,&t))return;
        if(old->stage==GIFT_CREDITED){send(s,GIFT_DONE,old);return;}if(old->stage==GIFT_DECLINED){send(s,GIFT_REJECT,old);return;}
        if(op==GIFT_OFFER){send(s,GIFT_ACCEPT,old);return;}
        if(op==GIFT_COMMIT){gift_book_t next=s->book;next.receipts[slot].stage=GIFT_CREDITED;if(save(s,&next,t.asset,t.amount))send(s,GIFT_DONE,&t);return;}
    }else if(old->stage && (old->id>t.id || old->stage==GIFT_PREPARED))return;
    if(op==GIFT_CANCEL){gift_book_t next=s->book;t.stage=GIFT_DECLINED;next.receipts[slot]=t;if(save(s,&next,0,0)){if(s->offer && same(&s->incoming,&t))s->offer=false;send(s,GIFT_REJECT,&t);}return;}
    if(op==GIFT_OFFER && s->armed && (!s->offer || same(&s->incoming,&t))){s->incoming=t;memcpy(s->incoming_name,b+20,13);s->incoming_at=s->now;s->offer=true;}
}
