#ifndef FRIEND_MGR_H
#define FRIEND_MGR_H
#include "friend_store.h"
#include "mesh_packet.h"
#include <stdbool.h>
#include <stdint.h>
#define FRIEND_DISCOVERY_TIMEOUT_MS 30000
#define FRIEND_CONFIRM_TIMEOUT_MS 30000
#define FRIEND_RSSI_MIN_DBM (-40)
typedef enum {
    FRIEND_MGR_IDLE = 0,
    FRIEND_MGR_DISCOVERY = 1,
    FRIEND_MGR_WAIT_USER_CONFIRM = 2,
    FRIEND_MGR_DONE = 3,
    FRIEND_MGR_TIMEOUT = 4,
    FRIEND_MGR_ERROR = 5,
    FRIEND_MGR_WAIT_PEER_CONFIRM = 6,
    FRIEND_MGR_COMMITTING = 7,
    FRIEND_MGR_SYNCING = 8
} friend_mgr_state_t;
typedef struct {
    friend_mgr_state_t state;
    uint8_t peer_mac[6];
    char peer_name[FRIEND_NAME_MAX + 1];
    int8_t rssi;
    bool local_confirmed, peer_confirmed;
    uint32_t remaining_seconds;
    char error[40];
} friend_mgr_snapshot_t;
typedef void (*friend_mgr_state_cb_t)(friend_mgr_state_t state, void *ctx);
typedef void (*friend_mgr_added_cb_t)(uint32_t node_id, const char *name, void *ctx);
void friend_mgr_init(void);
bool friend_mgr_enter_add_mode(friend_mgr_state_cb_t state_cb, friend_mgr_added_cb_t added_cb,
                               void *ctx);
void friend_mgr_exit_add_mode(void);
friend_mgr_state_t friend_mgr_get_state(void);
void friend_mgr_snapshot(friend_mgr_snapshot_t *out);
void friend_mgr_tick(void); /* message_service owner only */
void friend_mgr_on_peer_found(const mesh_packet_t *pkt, int8_t rssi); /* owner only */
void friend_mgr_process_ui_callbacks(void); /* UI context, optional compatibility callbacks */
bool friend_mgr_user_confirm(void);
void friend_mgr_user_cancel(void);
bool friend_mgr_is_active(void);
bool friend_mgr_is_friend(const uint8_t mac[6]);
unsigned friend_mgr_friend_count(void);
bool friend_mgr_get_friend(unsigned index, friend_record_t *out);
bool friend_mgr_remove_friend(const uint8_t mac[6]);
int8_t friend_mgr_get_rssi_threshold(void);
bool friend_mgr_set_rssi_threshold(int8_t dbm); /* debug calibration: -90 .. -20 dBm */
int8_t friend_mgr_get_last_rssi(void);
const char *friend_mgr_get_peer_name(void);
uint32_t friend_mgr_get_discovered_peer_id(void);
const char *friend_mgr_get_discovered_peer_name(void);
uint32_t friend_mgr_get_remaining_seconds(void);
#endif
