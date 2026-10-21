#ifndef MESSAGE_SERVICE_H
#define MESSAGE_SERVICE_H
#include "mesh_packet.h"
#include "morse_live.h"
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
bool message_service_pop_notification(message_inbox_entry_t *out);
uint8_t message_service_get_channel(void);
bool message_service_set_channel(uint8_t channel);
void message_service_snapshot(message_service_snapshot_t *out);
bool message_service_prepare_sleep(void);
void message_service_resume(void);
/* Owner-only control-plane transport; prioritised, copied and bounded. */
bool message_service_send_control(uint8_t type, const uint8_t *dst_mac, const void *payload,
                                  uint8_t len);
#endif
