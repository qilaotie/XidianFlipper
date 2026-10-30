#ifndef NETWORK_LAB_H
#define NETWORK_LAB_H
#include "team_activity.h"
enum {LAB_PING=40,LAB_PONG};
typedef struct {bool running,enabled,local_error;uint8_t target[6],preset;uint32_t planned,started,confirmed,timeouts,attempts,retries,rtt_min,rtt_max,rtt_sum,last_rtt;} network_lab_view_t;
typedef struct {network_lab_view_t view;uint8_t self[6],channel,attempt;uint32_t scope,token,now,due,sent[3],response_at,queue_wait;bool inflight,responded,queue_waiting;team_send_fn send;void *ctx;} network_lab_t;
void network_lab_init(network_lab_t *,const uint8_t self[6],team_send_fn,void *);
void network_lab_tick(network_lab_t *,uint8_t,uint32_t,bool,uint32_t);
void network_lab_receive(network_lab_t *,const mesh_packet_t *);
bool network_lab_start(network_lab_t *,const uint8_t target[6],unsigned preset,uint32_t token);
void network_lab_stop(network_lab_t *);
#endif
