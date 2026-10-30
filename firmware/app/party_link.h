#ifndef PARTY_LINK_H
#define PARTY_LINK_H
#include "party_rules.h"
#include "team_activity.h"
enum {PARTY_JOIN=1,PARTY_LEAVE,PARTY_START,PARTY_MOVE,PARTY_CLOSE,PARTY_READY,PARTY_INVITE};
#define PARTY_TABLES 8
#define PARTY_TABLE_TTL 15000u
typedef struct {
    bool active,modern,invited;
    uint8_t host[6],kind,phase,n,round;
    char name[7];
    uint32_t id,host_boot,revision,seen;
    uint8_t ready_mask;
    uint32_t invite_nonce,invite_seen;
} party_table_t;
typedef struct {
    bool active,local,enabled,ready,pending,stale,watching;
    uint8_t ready_mask;
    uint32_t id,revision,seen,host_boot;uint8_t result;
    party_view_t game;
    char names[8][7];
    uint8_t host[6];
    party_table_t tables[PARTY_TABLES];
} party_snapshot_t;
typedef struct {
    party_rules_t rules;
    party_view_t view,stage;
    uint8_t self[6],host[6],members[8][6],channel,n,stage_slot,publish_cycle,result;
    char names[8][7],name[7];
    uint32_t boot,host_boot,id,revision,sequence,now,seen,scope,publish_at,private_rev,roster_rev;
    bool active,local,enabled,ready,have_public,joined,watching;
    uint8_t ready_mask;
    uint32_t invite_nonce,invite_sent;
    bool invite_sent_once;
    uint8_t private_bytes[57];
    uint32_t member_boot[8],member_nonce[8];uint8_t member_result[8];
    struct {bool active;uint8_t bytes[40],tries;uint32_t start,last;} pending;
    party_table_t tables[PARTY_TABLES];
    team_send_fn send;void *ctx;
} party_link_t;
void party_link_init(party_link_t *,const uint8_t self[6],uint32_t boot,team_send_fn,void *);
void party_link_tick(party_link_t *,uint8_t channel,uint32_t scope,bool enabled,uint32_t now);
void party_link_receive(party_link_t *,const mesh_packet_t *);
bool party_link_create(party_link_t *,unsigned kind,uint32_t seed,const char *name);
bool party_link_join_table(party_link_t *,const uint8_t host[6],uint32_t boot,uint32_t id,const char *name);
bool party_link_watch_table(party_link_t *,const uint8_t host[6],uint32_t boot,uint32_t id);
bool party_link_stop_watching(party_link_t *);
bool party_link_request(party_link_t *,unsigned op,unsigned action,unsigned a,unsigned b,unsigned c,const char *name);
void party_link_snapshot(party_link_t *,party_snapshot_t *);
#endif
