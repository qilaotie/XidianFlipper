#include "message_service.h"
#include "ble_radio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "friend_mgr.h"
#include "morse_codec.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>

#define RX_QUEUE_SIZE 24
#define CONTROL_QUEUE_SIZE 12
#define COMMAND_QUEUE_SIZE 16
#define TX_ATTEMPTS 4
#define TX_BURST_MS 180
#define TX_RECEIPT_WAIT_MS 650
#define SEEN_COUNT 64
typedef struct {
    mesh_packet_t packet;
    int8_t rssi;
} rx_event_t;
typedef enum { CMD_SEND, CMD_RETRY, CMD_READ, CMD_CHANNEL, CMD_SLEEP } command_type_t;
typedef struct {
    command_type_t type;
    uint32_t id;
    uint8_t channel;
} command_t;
typedef struct {
    mesh_packet_t packet;
    message_tx_status_t status;
    uint32_t next_try, created;
    uint8_t attempts;
} tx_entry_t;
typedef struct {
    uint32_t schema;
    message_inbox_entry_t entry;
} inbox_disk_t;
typedef struct {
    uint8_t mac[6];
    uint32_t session, id, seen_ms;
} seen_t;
typedef struct {
    char ch;
    uint32_t sequence, stream, queued_ms;
} live_input_t;
static QueueHandle_t s_rx_queue, s_controls, s_commands, s_notifications;
static QueueHandle_t s_live_inputs;
static SemaphoreHandle_t s_lock, s_sleep_ack;
static TaskHandle_t s_task;
static bool s_initialized, s_store_ok, s_accepting = true, s_paused, s_sleep_result,
                                       s_sleep_pending;
static message_inbox_entry_t s_inbox[MESSAGE_INBOX_MAX];
static tx_entry_t s_tx[MESSAGE_TX_HISTORY_MAX];
static seen_t s_seen[SEEN_COUNT];
static unsigned s_seen_cursor;
static uint32_t s_session, s_next_message, s_next_inbox;
static uint32_t s_sleep_generation;
static unsigned s_control_streak;
static uint8_t s_channel = MESH_CHANNEL_DEFAULT;
static message_service_snapshot_t s_stats;
static morse_live_t s_live;
static uint32_t s_rx_queue_dropped;
static portMUX_TYPE s_publish_lock = portMUX_INITIALIZER_UNLOCKED;
static struct {
    morse_live_snapshot_t live;
    message_service_snapshot_t service;
    unsigned inbox_count, unread_count;
    bool accepting;
} s_published;
static uint32_t s_live_input_sequence;
static struct {
    bool active;
    int tx_index;
    uint32_t start;
    mesh_packet_t packet;
} s_air;
static const uint8_t s_broadcast[6] = MESH_DST_BROADCAST_MAC;
static const char *TAG = "messages";
#define LOCK() xSemaphoreTakeRecursive(s_lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGiveRecursive(s_lock)
static void publish_snapshot(void) {
    /* Build outside the short critical section. Never hold this lock during NVS or NimBLE. */
    morse_live_snapshot_t live = s_live.view;
    live.pending = (uint8_t)(s_live.tx_count + uxQueueMessagesWaiting(s_live_inputs));
    message_service_snapshot_t service = s_stats;
    service.rx_queue_dropped = __atomic_load_n(&s_rx_queue_dropped, __ATOMIC_RELAXED);
    service.ready = s_store_ok && ble_radio_is_ready();
    service.paused = s_paused;
    service.scanning = ble_radio_is_scanning();
    service.advertising = ble_radio_is_advertising();
    service.channel = s_channel;
    unsigned count = 0, unread = 0;
    for (unsigned i = 0; i < MESSAGE_INBOX_MAX; i++)
        if (s_inbox[i].local_id) {
            count++;
            if (s_inbox[i].unread)
                unread++;
        }
    portENTER_CRITICAL(&s_publish_lock);
    if (s_published.live.stream != live.stream)
        s_live_input_sequence = 0;
    s_published.live = live;
    s_published.service = service;
    s_published.inbox_count = count;
    s_published.unread_count = unread;
    s_published.accepting = s_accepting;
    portEXIT_CRITICAL(&s_publish_lock);
}
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static bool pending(message_tx_status_t s) {
    return s == MESSAGE_TX_QUEUED || s == MESSAGE_TX_SENDING || s == MESSAGE_TX_WAIT_RECEIPT;
}
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int find_tx(uint32_t id) {
    for (unsigned i = 0; i < MESSAGE_TX_HISTORY_MAX; i++)
        if (s_tx[i].status && s_tx[i].packet.msg_id == id)
            return (int)i;
    return -1;
}
static bool text_valid(message_kind_t kind, const char *p, uint16_t n) {
    if (!p || !n || n > MESSAGE_TEXT_MAX)
        return false;
    if (kind != MESSAGE_KIND_QUICK && kind != MESSAGE_KIND_MORSE)
        return false;
    for (unsigned i = 0; i < n; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c == 0 || c < 32 || c == 127)
            return false;
    }
    /* Quick text is UTF-8. Reject malformed sequences, overlong encodings and surrogates. */
    if (kind == MESSAGE_KIND_QUICK)
        for (unsigned i = 0; i < n;) {
            uint8_t c = (uint8_t)p[i++];
            if (c < 128)
                continue;
            unsigned more;
            uint32_t value, min;
            if (c >= 0xc2 && c <= 0xdf) {
                more = 1;
                value = c & 31;
                min = 0x80;
            } else if (c >= 0xe0 && c <= 0xef) {
                more = 2;
                value = c & 15;
                min = 0x800;
            } else if (c >= 0xf0 && c <= 0xf4) {
                more = 3;
                value = c & 7;
                min = 0x10000;
            } else
                return false;
            if (i + more > n)
                return false;
            while (more--) {
                c = (uint8_t)p[i++];
                if ((c & 0xc0) != 0x80)
                    return false;
                value = (value << 6) | (c & 63);
            }
            if (value < min || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
                return false;
        }
    return kind != MESSAGE_KIND_MORSE || morse_codec_validate(p, n);
}
static bool disk_save(unsigned slot, const message_inbox_entry_t *e) {
    inbox_disk_t d = {.schema = 1, .entry = *e};
    char key[16];
    snprintf(key, sizeof(key), "m%02u", slot);
    nvs_handle_t h;
    esp_err_t rc = nvs_open("messages_v3", NVS_READWRITE, &h);
    if (rc == ESP_OK) {
        rc = nvs_set_blob(h, key, &d, sizeof(d));
        if (rc == ESP_OK)
            rc = nvs_commit(h);
        nvs_close(h);
    }
    if (rc != ESP_OK) {
        s_stats.storage_errors++;
        ESP_LOGE(TAG, "inbox commit %s: %d", key, rc);
    }
    return rc == ESP_OK;
}
static bool load_store(void) {
    nvs_handle_t h;
    esp_err_t rc = nvs_open("messages_v3", NVS_READWRITE, &h);
    if (rc != ESP_OK)
        return false;
    uint8_t channel = 1;
    rc = nvs_get_u8(h, "channel", &channel);
    if (rc == ESP_OK && channel >= 1 && channel <= 16)
        s_channel = channel;
    else if (rc != ESP_OK && rc != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        return false;
    }
    for (unsigned i = 0; i < MESSAGE_INBOX_MAX; i++) {
        char key[16];
        snprintf(key, sizeof(key), "m%02u", i);
        inbox_disk_t d = {0};
        size_t n = sizeof(d);
        rc = nvs_get_blob(h, key, &d, &n);
        if (rc == ESP_ERR_NVS_NOT_FOUND)
            continue;
        if (rc != ESP_OK || n != sizeof(d) || d.schema != 1 || !d.entry.local_id ||
            !mesh_mac_is_valid_peer(d.entry.src_mac) || d.entry.channel > 16 ||
            !text_valid((message_kind_t)d.entry.kind, d.entry.text, d.entry.len) ||
            d.entry.text[d.entry.len] != 0) {
            nvs_close(h);
            return false;
        }
        s_inbox[i] = d.entry;
        if (d.entry.local_id > s_next_inbox)
            s_next_inbox = d.entry.local_id;
    }
    nvs_close(h);
    return true;
}
static bool same_message(const message_inbox_entry_t *e, const mesh_packet_t *p) {
    return e->local_id && e->session_id == p->session_id && e->msg_id == p->msg_id &&
           mesh_mac_equal(e->src_mac, p->src_mac);
}
static bool duplicate(const mesh_packet_t *p) {
    for (unsigned i = 0; i < MESSAGE_INBOX_MAX; i++)
        if (same_message(&s_inbox[i], p))
            return true;
    uint32_t now = now_ms();
    for (unsigned i = 0; i < SEEN_COUNT; i++)
        if (s_seen[i].id == p->msg_id && s_seen[i].session == p->session_id &&
            mesh_mac_equal(s_seen[i].mac, p->src_mac) && now - s_seen[i].seen_ms < 120000)
            return true;
    return false;
}
static void remember(const mesh_packet_t *p) {
    seen_t *s = &s_seen[s_seen_cursor++ % SEEN_COUNT];
    memcpy(s->mac, p->src_mac, 6);
    s->session = p->session_id;
    s->id = p->msg_id;
    s->seen_ms = now_ms();
}
bool message_service_send_control(uint8_t type, const uint8_t *dst, const void *payload,
                                  uint8_t len) {
    if (!s_initialized || s_paused || !ble_radio_is_ready() || len > MESH_PAYLOAD_MAX)
        return false;
    uint8_t mac[6];
    if (!ble_radio_get_my_mac(mac))
        return false;
    mesh_packet_t p;
    if (!mesh_packet_pack(&p, mac, dst ? dst : s_broadcast, type, payload, len))
        return false;
    LOCK();
    /* Coalesce repeated receipts/handshakes without suppressing future retries. */
    mesh_packet_t queued;
    bool exists = s_air.active && s_air.packet.type == p.type && s_air.packet.len == p.len &&
                  mesh_mac_equal(s_air.packet.dst_mac, p.dst_mac) &&
                  !memcmp(s_air.packet.payload, p.payload, p.len);
    unsigned count = uxQueueMessagesWaiting(s_controls);
    for (unsigned i = 0; i < count; i++) {
        if (xQueueReceive(s_controls, &queued, 0) != pdTRUE)
            break;
        if (queued.type == p.type && queued.len == p.len &&
            mesh_mac_equal(queued.dst_mac, p.dst_mac) && !memcmp(queued.payload, p.payload, p.len))
            exists = true;
        xQueueSend(s_controls, &queued, 0);
    }
    if (exists) {
        UNLOCK();
        return true;
    }
    p.session_id = s_session;
    p.msg_id = ++s_next_message;
    if (!p.msg_id)
        p.msg_id = ++s_next_message;
    bool ok = xQueueSend(s_controls, &p, 0) == pdTRUE;
    UNLOCK();
    return ok;
}
static void receipt(const mesh_packet_t *p) {
    if (!(p->flags & MESH_FLAG_ACK_REQUEST) || mesh_packet_is_broadcast(p->dst_mac))
        return;
    uint8_t b[8];
    put32(b, p->session_id);
    put32(b + 4, p->msg_id);
    message_service_send_control(MESH_TYPE_RECEIPT, p->src_mac, b, sizeof(b));
}
static void receive(const rx_event_t *e) {
    const mesh_packet_t *p = &e->packet;
    uint8_t mac[6];
    if (!ble_radio_get_my_mac(mac) || !mesh_mac_is_valid_peer(p->src_mac) ||
        mesh_mac_equal(p->src_mac, mac) || !p->session_id || !p->msg_id ||
        (!mesh_mac_equal(p->dst_mac, mac) && !mesh_packet_is_broadcast(p->dst_mac))) {
        s_stats.rx_rejected++;
        return;
    }
    if (p->type == MESH_TYPE_FRIEND_REQ || p->type == MESH_TYPE_FRIEND_ACK) {
        friend_mgr_on_peer_found(p, e->rssi);
        return; /* duplicate control packets must elicit replies */
    }
    if (p->type == MESH_TYPE_MORSE_LIVE) {
        if (!mesh_packet_is_broadcast(p->dst_mac) || p->channel != s_channel || !p->channel ||
            p->flags ||
            !morse_live_receive(&s_live, p->src_mac, p->session_id, p->payload, p->len, now_ms()))
            s_stats.rx_rejected++;
        return; /* Ephemeral stream: never persist a character or create an inbox notification. */
    }
    if (p->type == MESH_TYPE_RECEIPT) {
        if (p->len != 8 || p->channel || !mesh_mac_equal(p->dst_mac, mac) ||
            !friend_mgr_is_friend(p->src_mac))
            return;
        uint32_t session = get32(p->payload), id = get32(p->payload + 4);
        int i = find_tx(id);
        if (i >= 0 && s_tx[i].packet.session_id == session &&
            (s_tx[i].packet.flags & MESH_FLAG_ACK_REQUEST) &&
            mesh_mac_equal(p->src_mac, s_tx[i].packet.dst_mac) &&
            s_tx[i].status != MESSAGE_TX_DELIVERED) {
            s_tx[i].status = MESSAGE_TX_DELIVERED;
            s_stats.tx_delivered++;
        }
        return;
    }
    bool broadcast = mesh_packet_is_broadcast(p->dst_mac);
    bool public_channel = broadcast && p->channel >= 1 && p->channel <= 16;
    if (public_channel) {
        if (p->channel != s_channel) {
            s_stats.rx_rejected++;
            return;
        }
    } else if (p->channel != 0 || !friend_mgr_is_friend(p->src_mac)) {
        s_stats.rx_rejected++;
        return;
    }
    if ((p->type != MESH_TYPE_QUICK_MSG && p->type != MESH_TYPE_MORSE) ||
        !text_valid((message_kind_t)p->type, (const char *)p->payload, p->len)) {
        s_stats.rx_rejected++;
        return;
    }
    if (duplicate(p)) {
        s_stats.rx_duplicates++;
        receipt(p);
        return;
    }
    if (!s_store_ok) {
        s_stats.storage_errors++;
        return;
    }
    unsigned slot = 0;
    for (unsigned i = 0; i < MESSAGE_INBOX_MAX; i++)
        if (!s_inbox[i].local_id) {
            slot = i;
            break;
        } else if (s_inbox[i].local_id < s_inbox[slot].local_id)
            slot = i;
    message_inbox_entry_t v = {0};
    v.local_id = s_next_inbox + 1;
    if (!v.local_id) {
        s_stats.storage_errors++;
        return;
    }
    memcpy(v.src_mac, p->src_mac, 6);
    v.session_id = p->session_id;
    v.msg_id = p->msg_id;
    v.kind = p->type;
    v.channel = p->channel;
    v.len = p->len;
    v.rssi = e->rssi;
    v.unread = true;
    memcpy(v.text, p->payload, p->len);
    if (!disk_save(slot, &v))
        return; /* No receipt and no dedup until durable storage succeeds. */
    s_inbox[slot] = v;
    s_next_inbox = v.local_id;
    remember(p);
    s_stats.rx_accepted++;
    if (xQueueSend(s_notifications, &v.local_id, 0) != pdTRUE) {
        uint32_t old;
        xQueueReceive(s_notifications, &old, 0);
        xQueueSend(s_notifications, &v.local_id, 0);
    }
    receipt(p);
}
static void fail_tx(tx_entry_t *t) {
    if (t->status != MESSAGE_TX_FAILED) {
        t->status = MESSAGE_TX_FAILED;
        s_stats.tx_failed++;
    }
}
static void process_command(const command_t *c) {
    switch (c->type) {
    case CMD_SEND:
        break; /* slot already reserved and copied by the calling thread */
    case CMD_RETRY:
        break; /* retry reserves its slot before returning to the calling thread */
    case CMD_READ:
        for (unsigned k = 0; k < MESSAGE_INBOX_MAX; k++)
            if (s_inbox[k].local_id == c->id && s_inbox[k].unread) {
                message_inbox_entry_t v = s_inbox[k];
                v.unread = false;
                if (disk_save(k, &v))
                    s_inbox[k] = v;
                break;
            }
        break;
    case CMD_CHANNEL: {
        nvs_handle_t h;
        esp_err_t rc = nvs_open("messages_v3", NVS_READWRITE, &h);
        if (rc == ESP_OK) {
            rc = nvs_set_u8(h, "channel", c->channel);
            if (rc == ESP_OK)
                rc = nvs_commit(h);
            nvs_close(h);
        }
        if (rc == ESP_OK) {
            s_channel = c->channel;
            /* Channel changes start a new stream and cannot leak queued characters across channels.
             */
            morse_live_init(&s_live, esp_random(), now_ms());
        } else
            s_stats.storage_errors++;
        break;
    }
    case CMD_SLEEP: {
        if (!s_sleep_pending || c->id != s_sleep_generation)
            break;
        bool busy = friend_mgr_is_active() || s_air.active ||
                    uxQueueMessagesWaiting(s_rx_queue) > 0 ||
                    uxQueueMessagesWaiting(s_controls) > 0;
        busy = busy || s_live.tx_count > 0;
        busy = busy || uxQueueMessagesWaiting(s_live_inputs) > 0;
        for (unsigned k = 0; k < MESSAGE_TX_HISTORY_MAX; k++)
            if (pending(s_tx[k].status))
                busy = true;
        s_sleep_result = !busy && s_store_ok && friend_store_ready();
        if (s_sleep_result) {
            s_paused = true;
            ble_radio_advertise_stop();
            ble_radio_scan_stop();
        } else
            s_accepting = true;
        s_sleep_pending = false;
        xSemaphoreGive(s_sleep_ack);
        break;
    }
    }
}
static void service_radio(void) {
    uint32_t now = now_ms();
    if (s_air.active) {
        if (!ble_radio_is_advertising() || now - s_air.start > TX_BURST_MS + 500) {
            bool ok = ble_radio_is_ready() && !ble_radio_is_advertising() &&
                      ble_radio_last_advertising_ok();
            if (ble_radio_is_advertising())
                ble_radio_advertise_stop();
            if (s_air.tx_index >= 0) {
                tx_entry_t *t = &s_tx[s_air.tx_index];
                if (t->status != MESSAGE_TX_DELIVERED) {
                    if (ok) {
                        s_stats.tx_sent++;
                        t->status = (t->packet.flags & MESH_FLAG_ACK_REQUEST)
                                        ? MESSAGE_TX_WAIT_RECEIPT
                                        : MESSAGE_TX_SENT;
                    } else if (t->attempts >= TX_ATTEMPTS)
                        fail_tx(t);
                    else
                        t->status = MESSAGE_TX_QUEUED;
                    t->next_try = now + TX_RECEIPT_WAIT_MS;
                }
            }
            if (s_air.tx_index == -2)
                morse_live_complete_packet(&s_live, ok, now);
            s_air.active = false;
        }
    }
    if (!ble_radio_is_ready()) {
        for (unsigned i = 0; i < MESSAGE_TX_HISTORY_MAX; i++)
            if (pending(s_tx[i].status) && now - s_tx[i].created > 10000)
                fail_tx(&s_tx[i]);
        return;
    }
    if (!ble_radio_is_scanning())
        ble_radio_scan_continuous_start(30, BLE_PHY_1M);
    if (s_air.active)
        return;
    /* A receipt or pairing frame gets the next slot; data is not held for 6 seconds. */
    mesh_packet_t p;
    int chosen = -1;
    bool have = s_control_streak < 3 && xQueueReceive(s_controls, &p, 0) == pdTRUE;
    if (!have)
        for (unsigned i = 0; i < MESSAGE_TX_HISTORY_MAX; i++) {
            tx_entry_t *t = &s_tx[i];
            if (t->status != MESSAGE_TX_QUEUED && t->status != MESSAGE_TX_WAIT_RECEIPT)
                continue;
            if (t->next_try && (int32_t)(now - t->next_try) < 0)
                continue;
            if (t->attempts >= TX_ATTEMPTS) {
                fail_tx(t);
                continue;
            }
            if (!mesh_packet_is_broadcast(t->packet.dst_mac) &&
                !friend_mgr_is_friend(t->packet.dst_mac)) {
                fail_tx(t);
                continue;
            }
            chosen = (int)i;
            p = t->packet;
            have = true;
            break;
        }
    if (!have) {
        uint8_t payload[MORSE_LIVE_PAYLOAD_MAX], length;
        if (morse_live_prepare_packet(&s_live, now, payload, &length)) {
            uint8_t mac[6];
            if (ble_radio_get_my_mac(mac) &&
                mesh_packet_pack(&p, mac, s_broadcast, MESH_TYPE_MORSE_LIVE, payload, length)) {
                p.channel = s_channel;
                p.session_id = s_session;
                p.msg_id = ++s_next_message;
                if (!p.msg_id)
                    p.msg_id = ++s_next_message;
                chosen = -2;
                have = true;
            } else
                morse_live_complete_packet(&s_live, false, now);
        }
    }
    if (!have)
        have = xQueueReceive(s_controls, &p, 0) == pdTRUE;
    if (!have)
        return;
    s_control_streak = chosen != -1 ? 0 : s_control_streak + 1;
    uint8_t b[MESH_PACKET_MAX_LEN];
    size_t n = mesh_packet_to_bytes(&p, b, sizeof(b));
    bool ok = n && ble_radio_advertise_timed(b, n, TX_BURST_MS);
    if (chosen == -2 && !ok)
        morse_live_complete_packet(&s_live, false, now);
    if (chosen >= 0) {
        tx_entry_t *t = &s_tx[chosen];
        t->attempts++;
        t->status = ok ? MESSAGE_TX_SENDING : MESSAGE_TX_QUEUED;
        t->next_try = now + TX_RECEIPT_WAIT_MS;
        if (!ok && t->attempts >= TX_ATTEMPTS)
            fail_tx(t);
    }
    if (ok) {
        s_air.active = true;
        s_air.tx_index = chosen;
        s_air.start = now;
        s_air.packet = p;
    }
}
static void process_live_inputs(void) {
    live_input_t input;
    for (unsigned i = 0; i < 16 && xQueueReceive(s_live_inputs, &input, 0) == pdTRUE; i++) {
        if (input.stream != s_live.stream) {
            s_live.view.failed++;
            s_live.view.last_failure_ms = now_ms();
            continue;
        }
        if (input.sequence <= s_live.next_sequence)
            continue;
        s_live.next_sequence = input.sequence - 1;
        if (now_ms() - input.queued_ms >= MORSE_LIVE_QUEUE_MS) {
            s_live.next_sequence = input.sequence;
            s_live.view.failed++;
            s_live.view.last_failure_ms = now_ms();
            morse_live_flow_push(&s_live.view.flow, input.ch, input.sequence, MORSE_LIVE_FAILED);
        } else
            morse_live_push(&s_live, input.ch, input.queued_ms, NULL);
    }
}
static void worker(void *arg) {
    (void)arg;
    for (;;) {
        LOCK();
        command_t c;
        for (unsigned i = 0; i < 8 && xQueueReceive(s_commands, &c, 0) == pdTRUE; i++)
            process_command(&c);
        if (!s_paused) {
            process_live_inputs();
            morse_live_tick(&s_live, now_ms());
            rx_event_t rx;
            for (unsigned i = 0; i < 12 && xQueueReceive(s_rx_queue, &rx, 0) == pdTRUE; i++)
                receive(&rx);
            friend_mgr_tick();
            service_radio();
        }
        publish_snapshot();
        UNLOCK();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
bool message_service_init(void) {
    if (s_initialized)
        return true;
    s_lock = xSemaphoreCreateRecursiveMutex();
    s_sleep_ack = xSemaphoreCreateBinary();
    s_rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(rx_event_t));
    s_controls = xQueueCreate(CONTROL_QUEUE_SIZE, sizeof(mesh_packet_t));
    s_commands = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(command_t));
    s_notifications = xQueueCreate(MESSAGE_INBOX_MAX, sizeof(uint32_t));
    s_live_inputs = xQueueCreate(MORSE_LIVE_TX_MAX, sizeof(live_input_t));
    if (!s_lock || !s_sleep_ack || !s_rx_queue || !s_controls || !s_commands || !s_notifications ||
        !s_live_inputs)
        return false;
    s_session = esp_random();
    if (!s_session)
        s_session = 1;
    s_store_ok = load_store();
    morse_live_init(&s_live, esp_random(), now_ms());
    if (!s_store_ok)
        ESP_LOGE(TAG, "inbox unavailable; preserving existing NVS");
    friend_mgr_init();
    publish_snapshot();
    s_initialized = true;
    if (xTaskCreate(worker, "message_service", 6144, NULL, 4, &s_task) != pdPASS) {
        s_initialized = false;
        return false;
    }
    return true;
}
void message_service_tick(void) {}
bool message_service_rx_enqueue(const mesh_packet_t *p, int8_t rssi) {
    if (!s_initialized || !p || p->len > MESH_PAYLOAD_MAX)
        return false;
    rx_event_t e = {.packet = *p, .rssi = rssi};
    if (xQueueSend(s_rx_queue, &e, 0) == pdTRUE)
        return true;
    /* The producer must never block NimBLE. Counters are diagnostic, not persistence. */
    __atomic_fetch_add(&s_rx_queue_dropped, 1, __ATOMIC_RELAXED);
    return false;
}
bool message_service_send_text(message_kind_t kind, const uint8_t *dst, uint8_t channel,
                               const char *text, uint16_t len, bool ack, uint32_t *id) {
    if (!s_initialized || !text_valid(kind, text, len))
        return false;
    bool broadcast = !dst || mesh_packet_is_broadcast(dst);
    if (channel > 16 || (!broadcast && channel != 0) ||
        (kind == MESSAGE_KIND_MORSE && broadcast && channel == 0) ||
        (!broadcast && !friend_mgr_is_friend(dst)))
        return false;
    uint8_t mac[6];
    if (!ble_radio_get_my_mac(mac))
        return false;
    LOCK();
    if (!s_accepting || s_paused) {
        UNLOCK();
        return false;
    }
    int slot = -1;
    uint32_t oldest = UINT32_MAX;
    for (unsigned i = 0; i < MESSAGE_TX_HISTORY_MAX; i++) {
        /* An early receipt can mark DELIVERED while its advertising burst is still running. */
        if (s_air.active && s_air.tx_index == (int)i)
            continue;
        if (!s_tx[i].status) {
            slot = (int)i;
            break;
        }
        if (!pending(s_tx[i].status) && s_tx[i].created <= oldest) {
            oldest = s_tx[i].created;
            slot = (int)i;
        }
    }
    if (slot < 0) {
        UNLOCK();
        return false;
    }
    tx_entry_t v = {0};
    if (!mesh_packet_pack(&v.packet, mac, broadcast ? s_broadcast : dst, (uint8_t)kind,
                          (const uint8_t *)text, (uint8_t)len)) {
        UNLOCK();
        return false;
    }
    v.packet.channel = channel;
    v.packet.session_id = s_session;
    v.packet.msg_id = ++s_next_message;
    if (!v.packet.msg_id)
        v.packet.msg_id = ++s_next_message;
    if (ack && !broadcast)
        v.packet.flags = MESH_FLAG_ACK_REQUEST;
    if (kind == MESSAGE_KIND_MORSE)
        for (unsigned i = 0; i < len; i++)
            if (v.packet.payload[i] >= 'a' && v.packet.payload[i] <= 'z')
                v.packet.payload[i] -= 'a' - 'A';
    v.status = MESSAGE_TX_QUEUED;
    v.created = now_ms();
    command_t c = {.type = CMD_SEND, .id = v.packet.msg_id};
    if (xQueueSend(s_commands, &c, 0) != pdTRUE) {
        UNLOCK();
        return false;
    }
    s_tx[slot] = v;
    if (id)
        *id = v.packet.msg_id;
    UNLOCK();
    return true;
}
bool message_service_send_live_char(char ch, uint32_t *sequence) {
    if (!s_initialized || (ch != ' ' && !morse_codec_encode(ch)))
        return false;
    live_input_t input = {.ch = ch, .queued_ms = now_ms()};
    /* Nonblocking FreeRTOS copy. Sequence allocation and queue insertion have one order. */
    portENTER_CRITICAL(&s_publish_lock);
    input.sequence = s_live_input_sequence + 1;
    input.stream = s_published.live.stream;
    bool ok = s_published.accepting && !s_published.service.paused && input.sequence &&
              input.sequence < UINT32_MAX && xQueueSend(s_live_inputs, &input, 0) == pdTRUE;
    if (ok) {
        s_live_input_sequence = input.sequence;
        if (sequence)
            *sequence = input.sequence;
    }
    portEXIT_CRITICAL(&s_publish_lock);
    return ok;
}
void message_service_live_snapshot(morse_live_snapshot_t *out) {
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (!s_initialized)
        return;
    portENTER_CRITICAL(&s_publish_lock);
    *out = s_published.live;
    portEXIT_CRITICAL(&s_publish_lock);
}
bool message_service_get_tx(uint32_t id, message_tx_status_t *out) {
    if (!s_initialized || !out)
        return false;
    LOCK();
    int i = find_tx(id);
    *out = i < 0 ? MESSAGE_TX_UNKNOWN : s_tx[i].status;
    UNLOCK();
    return i >= 0;
}
bool message_service_retry(uint32_t id) {
    if (!s_initialized)
        return false;
    LOCK();
    int i = find_tx(id);
    bool ok = s_accepting && i >= 0 && s_tx[i].status == MESSAGE_TX_FAILED;
    command_t c = {.type = CMD_RETRY, .id = id};
    if (ok)
        ok = xQueueSend(s_commands, &c, 0) == pdTRUE;
    if (ok) {
        s_tx[i].status = MESSAGE_TX_QUEUED;
        s_tx[i].attempts = 0;
        s_tx[i].created = now_ms();
        s_tx[i].next_try = 0;
    }
    UNLOCK();
    return ok;
}
unsigned message_service_inbox_count(void) {
    portENTER_CRITICAL(&s_publish_lock);
    unsigned count = s_published.inbox_count;
    portEXIT_CRITICAL(&s_publish_lock);
    return count;
}
unsigned message_service_unread_count(void) {
    portENTER_CRITICAL(&s_publish_lock);
    unsigned count = s_published.unread_count;
    portEXIT_CRITICAL(&s_publish_lock);
    return count;
}
bool message_service_inbox_get(unsigned index, message_inbox_entry_t *out) {
    if (!s_initialized || !out || index >= MESSAGE_INBOX_MAX)
        return false;
    LOCK();
    uint64_t before = (uint64_t)UINT32_MAX + 1;
    int slot = -1;
    for (unsigned rank = 0; rank <= index; rank++) {
        uint32_t best = 0;
        slot = -1;
        for (unsigned i = 0; i < MESSAGE_INBOX_MAX; i++)
            if (s_inbox[i].local_id > best && s_inbox[i].local_id < before) {
                best = s_inbox[i].local_id;
                slot = (int)i;
            }
        if (slot < 0)
            break;
        before = s_inbox[slot].local_id;
    }
    if (slot >= 0)
        *out = s_inbox[slot];
    UNLOCK();
    return slot >= 0;
}
bool message_service_inbox_get_by_id(uint32_t id, message_inbox_entry_t *out) {
    if (!s_initialized || !out || !id)
        return false;
    LOCK();
    bool ok = false;
    for (unsigned i = 0; i < MESSAGE_INBOX_MAX; i++)
        if (s_inbox[i].local_id == id) {
            *out = s_inbox[i];
            ok = true;
            break;
        }
    UNLOCK();
    return ok;
}
bool message_service_inbox_mark_read(uint32_t id) {
    if (!s_initialized || !id)
        return false;
    command_t c = {.type = CMD_READ, .id = id};
    LOCK();
    bool ok = s_accepting && s_store_ok && xQueueSend(s_commands, &c, 0) == pdTRUE;
    UNLOCK();
    return ok;
}
bool message_service_pop_notification(message_inbox_entry_t *out) {
    if (!s_initialized || !out)
        return false;
    uint32_t id;
    while (xQueueReceive(s_notifications, &id, 0) == pdTRUE)
        if (message_service_inbox_get_by_id(id, out))
            return true;
    return false;
}
uint8_t message_service_get_channel(void) {
    portENTER_CRITICAL(&s_publish_lock);
    uint8_t channel = s_published.service.channel;
    portEXIT_CRITICAL(&s_publish_lock);
    return channel ? channel : MESH_CHANNEL_DEFAULT;
}
bool message_service_set_channel(uint8_t channel) {
    if (!s_initialized || channel < 1 || channel > 16)
        return false;
    command_t c = {.type = CMD_CHANNEL, .channel = channel};
    LOCK();
    bool ok = s_accepting && s_store_ok && xQueueSend(s_commands, &c, 0) == pdTRUE;
    UNLOCK();
    return ok;
}
void message_service_snapshot(message_service_snapshot_t *out) {
    if (!out)
        return;
    portENTER_CRITICAL(&s_publish_lock);
    *out = s_published.service;
    portEXIT_CRITICAL(&s_publish_lock);
}
bool message_service_prepare_sleep(void) {
    if (!s_initialized)
        return false;
    LOCK();
    if (s_sleep_pending) {
        UNLOCK();
        return false;
    }
    if (s_paused) {
        UNLOCK();
        return true;
    }
    s_accepting = false;
    portENTER_CRITICAL(&s_publish_lock);
    s_published.accepting = false;
    portEXIT_CRITICAL(&s_publish_lock);
    s_sleep_pending = true;
    xSemaphoreTake(s_sleep_ack, 0);
    command_t c = {.type = CMD_SLEEP, .id = ++s_sleep_generation};
    bool queued = xQueueSend(s_commands, &c, 0) == pdTRUE;
    if (!queued) {
        s_accepting = true;
        s_sleep_pending = false;
    }
    UNLOCK();
    if (!queued)
        return false;
    if (xSemaphoreTake(s_sleep_ack, pdMS_TO_TICKS(2000)) != pdTRUE) {
        message_service_resume();
        return false;
    }
    LOCK();
    bool ok = s_sleep_result;
    UNLOCK();
    return ok;
}
void message_service_resume(void) {
    if (!s_initialized)
        return;
    LOCK();
    s_accepting = true;
    s_paused = false;
    s_sleep_pending = false;
    s_sleep_generation++;
    portENTER_CRITICAL(&s_publish_lock);
    s_published.accepting = true;
    s_published.service.paused = false;
    portEXIT_CRITICAL(&s_publish_lock);
    UNLOCK();
}
