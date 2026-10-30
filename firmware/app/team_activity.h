/* Bounded, ephemeral, single-host channel activities. All calls have one owner. */
#ifndef TEAM_ACTIVITY_H
#define TEAM_ACTIVITY_H
#include <stdbool.h>
#include <stdint.h>
#include "mesh_packet.h"
enum {TEAM_IDLE,TEAM_WAIT,TEAM_OK,TEAM_REJECTED,TEAM_TIMEOUT};
enum {TEAM_CLAIM=1,TEAM_COMPLETE,TEAM_RELEASE};
typedef struct {uint8_t state,owner[6];char title[19];} team_task_t;
typedef struct {
    bool active,local,open;uint8_t host[6],options,counts[4],voters;
    uint32_t boot,id,rev,seen;char question[49];
} team_poll_t;
typedef struct {bool active,local;uint8_t host[6];uint32_t boot,id,rev,seen;team_task_t tasks[4];} team_board_t;
typedef bool (*team_send_fn)(void *,const uint8_t *,uint8_t);
typedef struct {
    team_poll_t poll;team_board_t board;
    uint8_t self[6],channel,result;uint32_t boot,sequence,now,announce;
    bool enabled;team_send_fn send;void *ctx;
    uint32_t confirmed,timeouts,retries,last_ack_ms;
    uint8_t voter_mac[8][6];
    struct {bool active;uint8_t bytes[32],len,tries;uint32_t started,last;} pending;
    struct {bool used;uint8_t mac[6],op,action,result;uint32_t boot,nonce,id;} ledger[16];
    unsigned ledger_next;
} team_activity_t;
void team_init(team_activity_t *,const uint8_t self[6],uint32_t boot,team_send_fn,void *);
void team_tick(team_activity_t *,uint8_t channel,bool enabled,uint32_t now);
void team_receive(team_activity_t *,const mesh_packet_t *);
bool team_poll_create(team_activity_t *,const char *,unsigned options);
bool team_poll_close(team_activity_t *);
bool team_vote(team_activity_t *,unsigned choice);
bool team_board_create(team_activity_t *,const char *);
bool team_task_add(team_activity_t *,const char *);
bool team_task_request(team_activity_t *,unsigned task,unsigned action);
#endif
