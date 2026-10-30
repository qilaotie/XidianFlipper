#ifndef MESSAGE_SERVICE_H
#define MESSAGE_SERVICE_H
#include "mesh_packet.h"
#include "morse_live.h"
#include "mesh_route.h"
#include "message_outbox.h"
#include "team_activity.h"
#include "contact_presence.h"
#include "crowd_activity.h"
#include "activity_log.h"
#include "social.h"
#include "relay_mail.h"
#include "network_lab.h"
bool message_service_extras_context(uint8_t self[6],uint8_t *channel,uint32_t *scope);
bool message_service_extras_send(const uint8_t *,uint8_t len,uint8_t channel,uint32_t scope);
void message_service_lab_view(network_lab_view_t *);
bool message_service_lab_start(const uint8_t target[6],unsigned preset);
void message_service_lab_stop(void);
#include <stdbool.h>
#include <stdint.h>
#define MESSAGE_TEXT_MAX 128
#define MESSAGE_INBOX_MAX 32
#define MESSAGE_TX_HISTORY_MAX 8
typedef enum {
    MESSAGE_KIND_QUICK = MESH_TYPE_QUICK_MSG,
    MESSAGE_KIND_MORSE = MESH_TYPE_MORSE
} message_kind_t;
typedef enum {
    MESSAGE_TX_UNKNOWN = 0,
    MESSAGE_TX_QUEUED,
    MESSAGE_TX_SENDING,
    MESSAGE_TX_SENT,
    MESSAGE_TX_WAIT_RECEIPT,
    MESSAGE_TX_DELIVERED,
    MESSAGE_TX_FAILED
} message_tx_status_t;
typedef struct {
    uint32_t local_id;
    uint8_t src_mac[6];
    uint32_t session_id, msg_id;
    uint8_t channel, kind;
    uint16_t len;
    int8_t rssi;
    bool unread;
    char text[MESSAGE_TEXT_MAX + 1];
} message_inbox_entry_t;
typedef struct {
    uint32_t rx_accepted, rx_duplicates, rx_rejected, rx_queue_dropped, storage_errors;
    uint32_t tx_sent, tx_delivered, tx_failed;
    bool ready, paused, scanning, advertising;
    uint8_t channel;
} message_service_snapshot_t;
/* Creates the sole owner task. Call after NVS initialization; BLE may still be syncing. */
bool message_service_init(void);
void message_service_tick(void); /* compatibility; maintenance belongs to the owner task */
bool message_service_send_text(message_kind_t kind, const uint8_t *dst_mac, uint8_t channel,
                               const char *text, uint16_t len, bool request_receipt,
                               uint32_t *out_id);
bool message_service_get_tx(uint32_t id, message_tx_status_t *out);
bool message_service_retry(uint32_t id);
/* Public live characters bypass persistent inbox; true means queued, never delivery confirmed. */
bool message_service_send_live_char(char character, uint32_t *sequence);
void message_service_live_snapshot(morse_live_snapshot_t *out);
bool message_service_rx_enqueue(const mesh_packet_t *packet, int8_t rssi);
unsigned message_service_inbox_count(void);
unsigned message_service_unread_count(void);
/* Newest first; copies data so callers never retain pointers into a mutable ring. */
bool message_service_inbox_get(unsigned index, message_inbox_entry_t *out);
bool message_service_inbox_get_by_id(uint32_t id, message_inbox_entry_t *out);
bool message_service_inbox_mark_read(uint32_t id);
/* Confirmed single-message removal; commit before changing RAM. */
bool message_service_inbox_remove(uint32_t id);
bool message_service_pop_notification(message_inbox_entry_t *out);
uint8_t message_service_get_channel(void);
bool message_service_set_channel(uint8_t channel);
void message_service_snapshot(message_service_snapshot_t *out);
bool message_service_prepare_sleep(void);
/* Exclusive tool lease: retain background control frames for resume. */
bool message_service_pause_for_tools(void);
void message_service_resume(void);
/* Owner-only control-plane transport; prioritised, copied and bounded. */
bool message_service_send_control(uint8_t type, const uint8_t *dst_mac, const void *payload,
                                  uint8_t len);
typedef struct {
    bool enabled,paused;uint8_t last_rx_hops;
    uint32_t forwarded,duplicates,dropped,now;
    route_peer_t peers[ROUTE_PEERS];
    route_event_t events[ROUTE_EVENTS];
} message_network_view_t;
void message_service_network_view(message_network_view_t *out);
bool message_service_network_enable(bool enabled);
/* Explicit network actions; never silently enable relay/discovery. */
bool message_service_group_text(const char *text,bool announcement,uint32_t *id);
int message_service_offline_text(const uint8_t dst[6],const char *text);
bool message_service_outbox_get(unsigned slot,outbox_entry_t *out,bool *storage_error);
bool message_service_outbox_retry(unsigned slot);
bool message_service_outbox_remove(unsigned slot);
void message_service_contact(const uint8_t mac[6],contact_presence_t *out);
typedef struct {team_poll_t poll;team_board_t board;uint8_t result;bool enabled;} message_team_view_t;
void message_service_team_view(message_team_view_t *);
enum {MESSAGE_POLL_CREATE,MESSAGE_POLL_CLOSE,MESSAGE_POLL_VOTE,MESSAGE_BOARD_CREATE,MESSAGE_TASK_ADD,MESSAGE_TASK_ACTION};
bool message_service_team_action(unsigned action,const char *text,unsigned index,unsigned value);
typedef struct {crowd_room_t rooms[2];uint8_t result;bool enabled;} message_crowd_view_t;
enum {MESSAGE_CROWD_CREATE,MESSAGE_CROWD_CLOSE,MESSAGE_CROWD_STATUS};
void message_service_crowd_view(message_crowd_view_t *);
bool message_service_crowd_action(unsigned action,unsigned kind,const char *title,unsigned status);
typedef struct {uint32_t team_ack_ms,crowd_ack_ms,confirmed,timeouts,retries;unsigned records;bool log_error;} message_diagnostics_t;
void message_service_diagnostics(message_diagnostics_t *);
bool message_service_activity_get(unsigned index,activity_record_t *);
void message_service_social_view(social_view_t *);
bool message_service_room(uint32_t code,const char *name);
bool message_service_card_edit(unsigned field,const char *text);
bool message_service_card_share(bool);
bool message_service_passport_visit(const uint8_t mac[6],const char *name,uint32_t token);
void message_service_relay_enable(bool);
void message_service_relay_view(relay_mail_view_t *);
bool message_service_relay_get(unsigned,relay_mail_entry_t *);
bool message_service_relay_remove(unsigned);
int message_service_custody_text(const uint8_t dst[6],const char *text);
void message_service_ensemble_arm(bool,unsigned role);
bool message_service_ensemble_start(unsigned tune,unsigned bpm);
bool message_service_ensemble_event(uint32_t *,unsigned *,unsigned *,unsigned *);
#include "party_link.h"
void message_service_party_snapshot(party_snapshot_t *);
bool message_service_party_create(unsigned kind);
bool message_service_party_join_table(const uint8_t host[6],uint32_t boot,uint32_t id);
bool message_service_party_watch_table(const uint8_t host[6],uint32_t boot,uint32_t id);
bool message_service_party_stop_watching(void);
bool message_service_party_request(unsigned op,unsigned action,unsigned a,unsigned b,unsigned c);
#endif
