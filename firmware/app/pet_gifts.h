#ifndef PET_GIFTS_H
#define PET_GIFTS_H
#include <stdbool.h>
#include <stdint.h>
#define GIFT_PEERS 16
enum {GIFT_HELLO=50,GIFT_OFFER,GIFT_ACCEPT,GIFT_COMMIT,GIFT_DONE,GIFT_CANCEL,GIFT_REJECT};
enum {GIFT_IDLE,GIFT_OFFERING,GIFT_COMMITTING,GIFT_CANCELLING,GIFT_SENT,GIFT_REFUNDED};
enum {GIFT_PREPARED=1,GIFT_CREDITED,GIFT_DECLINED};
typedef struct {uint8_t peer[6],asset,amount,channel,stage;uint32_t scope,id;} gift_tx_t;
typedef struct {uint32_t sequence;gift_tx_t outgoing,receipts[GIFT_PEERS];} gift_book_t;
typedef bool (*gift_save_fn)(void *,const gift_book_t *,unsigned asset,int delta);
typedef bool (*gift_send_fn)(void *,const uint8_t *,uint8_t,uint8_t,uint32_t);
typedef struct {uint8_t mac[6];char name[13];uint32_t seen;bool used;} gift_peer_t;
typedef struct {gift_book_t book;gift_tx_t incoming;char incoming_name[13],name[13];gift_peer_t peers[6];uint8_t self[6],channel;uint32_t scope,now,last_tx,last_hello,incoming_at;bool enabled,armed,offer,error;gift_save_fn save;gift_send_fn send;void *ctx;} pet_gifts_t;
bool gift_book_valid(const gift_book_t *);
void pet_gifts_init(pet_gifts_t *,const gift_book_t *,gift_save_fn,gift_send_fn,void *);
void pet_gifts_tick(pet_gifts_t *,const uint8_t self[6],const char *,uint8_t,uint32_t,bool,bool,uint32_t);
void pet_gifts_receive(pet_gifts_t *,const uint8_t src[6],const uint8_t *,unsigned);
bool pet_gifts_offer(pet_gifts_t *,const uint8_t peer[6],unsigned asset,unsigned amount);
bool pet_gifts_decide(pet_gifts_t *,bool accept);
bool pet_gifts_cancel(pet_gifts_t *);
#endif
