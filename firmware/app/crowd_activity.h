#ifndef CROWD_ACTIVITY_H
#define CROWD_ACTIVITY_H
#include "team_activity.h"
enum {CROWD_CHECKIN,CROWD_QUIZ};
typedef struct {
    bool active,local,open;uint8_t host[6],winner[6],count;
    uint32_t boot,id,rev,seen;char title[33];
    struct {uint8_t mac[6],status;} people[8];
} crowd_room_t;
typedef struct {
    crowd_room_t rooms[2];uint8_t self[6],channel,result;
    uint32_t boot,sequence,now,announce,confirmed,timeouts,retries,last_ack_ms;
    bool enabled;team_send_fn send;void *ctx;
    uint32_t status_boot[8],status_nonce[8];
    struct {bool active;uint8_t bytes[28],tries;uint32_t start,last;} pending;
    struct {bool used;uint8_t mac[6],op,status,result;uint32_t boot,nonce,id;} ledger[16];unsigned cursor;
} crowd_activity_t;
void crowd_init(crowd_activity_t *,const uint8_t self[6],uint32_t boot,team_send_fn,void *);
void crowd_tick(crowd_activity_t *,uint8_t channel,bool enabled,uint32_t now);
void crowd_receive(crowd_activity_t *,const mesh_packet_t *);
bool crowd_create(crowd_activity_t *,unsigned kind,const char *title);
bool crowd_close(crowd_activity_t *,unsigned kind);
bool crowd_request(crowd_activity_t *,unsigned kind,unsigned status);
#endif
