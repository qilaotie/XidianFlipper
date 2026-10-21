/* Two-person pairing over advertisements. Only durable bilateral consent can become ACTIVE.
 * A reboot replays the journal; duplicates resend responses. Revocation wins over commit.
 * The last radio acknowledgement can be lost: COMMITTED/SYNCING explicitly represents that
 * uncertainty, and anti-entropy finishes or revokes the same transaction on next contact.
 */
#include "friend_mgr.h"
#include "ble_radio.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "message_service.h"
#include "nvs_config.h"
#include <stdio.h>
#include <string.h>
#define FRIEND_FRAME_SIZE 37
#define CANDIDATES 6
#define HELLO_INTERVAL_MS 350
#define STABLE_MIN_MS 650
#define CANDIDATE_FRESH_MS 1600
#define RSSI_MARGIN_DB 7
typedef enum {
    F_HELLO = 1,
    F_ACCEPT,
    F_READY,
    F_COMMIT,
    F_COMMIT_ACK,
    F_FINAL,
    F_CANCEL,
    F_SYNC,
    F_REVOKE,
    F_REVOKE_ACK
} frame_op_t;
typedef enum { C_ENTER, C_CONFIRM, C_CANCEL, C_REMOVE, C_THRESHOLD } cmd_op_t;
typedef struct {
    cmd_op_t op;
    uint8_t mac[6];
    int8_t threshold;
} friend_command_t;
typedef struct {
    uint8_t mac[6];
    uint32_t nonce, first, last, sample_ms;
    int16_t rssi;
    unsigned samples;
    char name[14];
} candidate_t;
static QueueHandle_t s_commands;
static SemaphoreHandle_t s_lock;
static friend_mgr_snapshot_t s_view;
static candidate_t s_candidates[CANDIDATES];
static bool s_inited, s_peer_locked;
/* RAM deny-list closes the authorization window before the owner commits deletion. */
static uint8_t s_revoking[FRIEND_STORE_MAX][6];
static bool revoking(const uint8_t *mac) {
    for (unsigned i = 0; i < FRIEND_STORE_MAX; i++)
        if (mesh_mac_equal(s_revoking[i], mac))
            return true;
    return false;
}
static void set_revoking(const uint8_t *mac, bool pending) {
    if (pending && revoking(mac))
        return;
    for (unsigned i = 0; i < FRIEND_STORE_MAX; i++) {
        if (pending ? !mesh_mac_is_valid_peer(s_revoking[i]) : mesh_mac_equal(s_revoking[i], mac)) {
            if (pending)
                memcpy(s_revoking[i], mac, 6);
            else
                memset(s_revoking[i], 0, 6);
            return;
        }
    }
}
static volatile bool s_enter_queued;
static uint32_t s_nonce, s_peer_nonce, s_deadline, s_last_hello, s_last_control, s_last_sync,
    s_revision, s_ui_revision;
static uint64_t s_transaction;
static uint8_t s_mac[6];
static unsigned s_sync_cursor;
static int8_t s_rssi_threshold = FRIEND_RSSI_MIN_DBM;
static friend_mgr_state_cb_t s_state_cb;
static friend_mgr_added_cb_t s_added_cb;
static void *s_cb_context;
#define FLOCK() xSemaphoreTakeRecursive(s_lock, portMAX_DELAY)
#define FUNLOCK() xSemaphoreGiveRecursive(s_lock)
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
static uint64_t get64(const uint8_t *p) {
    return (uint64_t)get32(p) | ((uint64_t)get32(p + 4) << 32);
}
static void put64(uint8_t *p, uint64_t v) {
    put32(p, (uint32_t)v);
    put32(p + 4, (uint32_t)(v >> 32));
}
static bool active_state(friend_mgr_state_t state) {
    return state == FRIEND_MGR_DISCOVERY || state == FRIEND_MGR_WAIT_USER_CONFIRM ||
           state == FRIEND_MGR_WAIT_PEER_CONFIRM || state == FRIEND_MGR_COMMITTING ||
           state == FRIEND_MGR_SYNCING;
}
static void state(friend_mgr_state_t next, const char *error) {
    if (next != s_view.state)
        s_revision++;
    s_view.state = next;
    snprintf(s_view.error, sizeof(s_view.error), "%s", error ? error : "");
}
static uint32_t short_id(const uint8_t *mac) {
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < 6; i++)
        h = (h ^ mac[i]) * 16777619u;
    return h;
}
static bool send_frame(frame_op_t op, const uint8_t *dst, uint64_t transaction,
                       uint8_t record_state) {
    uint8_t b[FRIEND_FRAME_SIZE] = {0};
    b[0] = (uint8_t)op;
    put32(b + 1, op == F_SYNC ? record_state : s_nonce);
    put32(b + 5, s_peer_nonce);
    put64(b + 9, transaction);
    const char *name = nvs_config_get_device_name();
    if (name)
        snprintf((char *)b + 17, 14, "%s", name);
    memcpy(b + 31, s_view.peer_mac, 6);
    return message_service_send_control(MESH_TYPE_FRIEND_REQ, dst, b, sizeof(b));
}
static bool current_peer(const uint8_t *mac, uint64_t id) {
    return id && id == s_transaction && mesh_mac_equal(mac, s_view.peer_mac);
}
static void done_if_current(const friend_record_t *r) {
    if (current_peer(r->mac, r->transaction_id) && active_state(s_view.state)) {
        s_view.local_confirmed = true;
        s_view.peer_confirmed = true;
        s_deadline = 0;
        state(FRIEND_MGR_DONE, NULL);
    }
}
static bool save_state(friend_record_t *r, uint8_t next) {
    if (r->state == next)
        return true;
    friend_record_t copy = *r;
    copy.state = next;
    if (!friend_store_put(&copy)) {
        if (current_peer(r->mac, r->transaction_id))
            state(FRIEND_MGR_ERROR, "Storage write failed");
        return false;
    }
    *r = copy;
    return true;
}
static void cancel_current(bool timeout) {
    if (mesh_mac_is_valid_peer(s_view.peer_mac) && s_transaction &&
        (active_state(s_view.state) || s_view.state == FRIEND_MGR_ERROR)) {
        friend_record_t r;
        if (friend_store_get(s_view.peer_mac, s_transaction, &r) &&
            r.state < FRIEND_RECORD_REVOKED) {
            set_revoking(r.mac, true);
            if (!save_state(&r, FRIEND_RECORD_REVOKED))
                return;
            set_revoking(r.mac, false);
            send_frame(F_REVOKE, r.mac, r.transaction_id, 0);
        } else
            send_frame(F_CANCEL, s_view.peer_mac, s_transaction, 0);
    }
    s_deadline = 0;
    state(timeout ? FRIEND_MGR_TIMEOUT : FRIEND_MGR_IDLE, timeout ? "Pairing timed out" : NULL);
}
static bool commit_local(friend_record_t *r) {
    if (r->state == FRIEND_RECORD_PREPARED && !save_state(r, FRIEND_RECORD_COMMITTED))
        return false;
    if (current_peer(r->mac, r->transaction_id) && active_state(s_view.state)) {
        s_view.local_confirmed = true;
        s_view.peer_confirmed = true;
        s_deadline = 0;
        state(FRIEND_MGR_COMMITTING, NULL);
    }
    return true;
}
static void accept_local(void) {
    if (s_view.state != FRIEND_MGR_WAIT_USER_CONFIRM || !mesh_mac_is_valid_peer(s_view.peer_mac) ||
        !s_peer_locked)
        return;
    friend_record_t r = {0};
    memcpy(r.mac, s_view.peer_mac, 6);
    r.transaction_id = s_transaction;
    memcpy(r.name, s_view.peer_name, sizeof(r.name));
    r.state = FRIEND_RECORD_PREPARED;
    if (!friend_store_put(&r)) {
        state(FRIEND_MGR_ERROR, "Friend storage full/error");
        return;
    }
    s_view.local_confirmed = true;
    state(FRIEND_MGR_WAIT_PEER_CONFIRM, NULL);
    send_frame(F_ACCEPT, r.mac, r.transaction_id, 0);
    if (s_view.peer_confirmed && commit_local(&r))
        send_frame(F_READY, r.mac, r.transaction_id, 0);
}
static void enter(void) {
    s_enter_queued = false;
    if (active_state(s_view.state))
        return;
    memset(&s_view, 0, sizeof(s_view));
    s_view.rssi = -100;
    memset(s_candidates, 0, sizeof(s_candidates));
    s_transaction = 0;
    s_peer_nonce = 0;
    s_peer_locked = false;
    s_nonce = esp_random();
    if (!s_nonce)
        s_nonce = 1;
    s_deadline = now_ms() + FRIEND_DISCOVERY_TIMEOUT_MS;
    s_last_hello = 0;
    s_last_control = 0;
    if (!friend_store_ready()) {
        state(FRIEND_MGR_ERROR, "Friend storage unavailable");
        return;
    }
    if (!friend_store_has_capacity()) {
        state(FRIEND_MGR_ERROR, "Friend storage is full");
        return;
    }
    state(FRIEND_MGR_DISCOVERY, NULL);
}
void friend_mgr_init(void) {
    if (s_inited)
        return;
    if (!s_lock)
        s_lock = xSemaphoreCreateRecursiveMutex();
    if (!s_commands)
        s_commands = xQueueCreate(8, sizeof(friend_command_t));
    if (!s_lock || !s_commands)
        return;
    friend_store_init();
    memset(&s_view, 0, sizeof(s_view));
    s_view.rssi = -100;
    s_inited = true;
    /* Recover durable, unfinished consent. No discovery or automatic acceptance on boot. */
    for (unsigned i = 0; i < FRIEND_STORE_RECORD_MAX; i++) {
        friend_record_t r;
        if (friend_store_get_record(i, &r) &&
            (r.state == FRIEND_RECORD_PREPARED || r.state == FRIEND_RECORD_COMMITTED)) {
            s_transaction = r.transaction_id;
            memcpy(s_view.peer_mac, r.mac, 6);
            memcpy(s_view.peer_name, r.name, sizeof(r.name));
            s_view.local_confirmed = true;
            s_view.peer_confirmed = r.state == FRIEND_RECORD_COMMITTED;
            state(FRIEND_MGR_SYNCING, "Restoring pairing");
            break;
        }
    }
}
static void choose_candidate(void) {
    int best = -1;
    uint32_t now = now_ms();
    for (unsigned i = 0; i < CANDIDATES; i++) {
        candidate_t *c = &s_candidates[i];
        if (!c->nonce || now - c->last > CANDIDATE_FRESH_MS || c->samples < 4 ||
            now - c->first < STABLE_MIN_MS || c->rssi < s_rssi_threshold)
            continue;
        if (best < 0 || c->rssi > s_candidates[best].rssi)
            best = (int)i;
    }
    if (best < 0)
        return;
    candidate_t *c = &s_candidates[best];
    for (unsigned i = 0; i < CANDIDATES; i++)
        if ((int)i != best && s_candidates[i].nonce &&
            now - s_candidates[i].last < CANDIDATE_FRESH_MS &&
            c->rssi - s_candidates[i].rssi < RSSI_MARGIN_DB) {
            snprintf(s_view.error, sizeof(s_view.error), "Separate devices");
            return;
        }
    memcpy(s_view.peer_mac, c->mac, 6);
    memcpy(s_view.peer_name, c->name, sizeof(c->name));
    s_view.rssi = (int8_t)c->rssi;
    s_peer_nonce = c->nonce;
    s_transaction = memcmp(s_mac, c->mac, 6) < 0 ? ((uint64_t)s_nonce << 32) | s_peer_nonce
                                                 : ((uint64_t)s_peer_nonce << 32) | s_nonce;
    s_peer_locked = false;
    s_deadline = now + FRIEND_CONFIRM_TIMEOUT_MS;
    state(FRIEND_MGR_WAIT_USER_CONFIRM, NULL);
    send_frame(F_HELLO, NULL, 0, 0);
}
static void hello(const mesh_packet_t *p, int8_t rssi) {
    uint32_t nonce = get32(p->payload + 1);
    if (!nonce)
        return;
    const uint8_t *locked = p->payload + 31;
    static const uint8_t zero[6] = {0};
    if (memcmp(locked, zero, 6) && !mesh_mac_equal(locked, s_mac))
        return;
    if (!active_state(s_view.state) || s_view.state == FRIEND_MGR_SYNCING)
        return;
    if (mesh_mac_equal(p->src_mac, s_view.peer_mac) && nonce == s_peer_nonce) {
        s_view.rssi = rssi;
        if (mesh_mac_equal(locked, s_mac))
            s_peer_locked = true;
        return;
    }
    if (s_view.state != FRIEND_MGR_DISCOVERY || friend_store_get_active(p->src_mac, NULL))
        return;
    uint32_t now = now_ms();
    int slot = -1;
    for (unsigned i = 0; i < CANDIDATES; i++)
        if (s_candidates[i].nonce && mesh_mac_equal(s_candidates[i].mac, p->src_mac)) {
            slot = (int)i;
            break;
        }
    if (slot < 0)
        for (unsigned i = 0; i < CANDIDATES; i++)
            if (!s_candidates[i].nonce || now - s_candidates[i].last > CANDIDATE_FRESH_MS) {
                slot = (int)i;
                break;
            }
    if (slot < 0)
        return;
    candidate_t *c = &s_candidates[slot];
    if (c->nonce != nonce || now - c->last > CANDIDATE_FRESH_MS) {
        memset(c, 0, sizeof(*c));
        memcpy(c->mac, p->src_mac, 6);
        c->nonce = nonce;
        c->rssi = rssi;
        c->first = now;
    }
    c->last = now;
    if (!c->samples || now - c->sample_ms >= 60) {
        c->sample_ms = now;
        c->rssi = (c->rssi * 3 + rssi) / 4;
        if (rssi >= s_rssi_threshold)
            c->samples++;
        else {
            c->samples = 0;
            c->first = now;
        }
    }
    memcpy(c->name, p->payload + 17, sizeof(c->name));
    s_view.rssi = rssi;
    choose_candidate();
}
void friend_mgr_on_peer_found(const mesh_packet_t *p, int8_t rssi) {
    if (!s_inited || !p || p->len != FRIEND_FRAME_SIZE || !ble_radio_get_my_mac(s_mac) ||
        !memchr(p->payload + 17, 0, 14))
        return;
    frame_op_t op = (frame_op_t)p->payload[0];
    if (op < F_HELLO || op > F_REVOKE_ACK)
        return;
    FLOCK();
    if (op == F_HELLO) {
        hello(p, rssi);
        FUNLOCK();
        return;
    }
    uint64_t id = get64(p->payload + 9);
    if (!id || !mesh_mac_equal(p->dst_mac, s_mac)) {
        FUNLOCK();
        return;
    }
    bool current = current_peer(p->src_mac, id);
    friend_record_t r;
    bool have = friend_store_get(p->src_mac, id, &r);
    if (revoking(p->src_mac) && (!have || r.state < FRIEND_RECORD_REVOKED)) {
        FUNLOCK();
        return;
    }
    if (op == F_REVOKE || op == F_CANCEL) {
        if (have && r.state < FRIEND_RECORD_REVOKED) {
            if (!save_state(&r, FRIEND_RECORD_REVOKED_ACKED)) {
                FUNLOCK();
                return;
            }
        }
        if (have || current)
            send_frame(F_REVOKE_ACK, p->src_mac, id, 0);
        if (current) {
            s_deadline = 0;
            state(FRIEND_MGR_ERROR, op == F_CANCEL ? "Peer cancelled" : "Friendship revoked");
        }
        FUNLOCK();
        return;
    }
    if (op == F_REVOKE_ACK) {
        if (have && r.state == FRIEND_RECORD_REVOKED)
            save_state(&r, FRIEND_RECORD_REVOKED_ACKED);
        FUNLOCK();
        return;
    }
    if (have && r.state >= FRIEND_RECORD_REVOKED) {
        send_frame(F_REVOKE, p->src_mac, id, 0);
        FUNLOCK();
        return;
    }
    if (op == F_ACCEPT) {
        if (current && active_state(s_view.state) && get32(p->payload + 1) == s_peer_nonce &&
            get32(p->payload + 5) == s_nonce) {
            s_view.peer_confirmed = true;
            s_peer_locked = true;
            if (have && r.state == FRIEND_RECORD_PREPARED && commit_local(&r))
                send_frame(F_READY, r.mac, id, 0);
        } else if (have && r.state < FRIEND_RECORD_REVOKED) {
            if (commit_local(&r))
                send_frame(F_READY, r.mac, id, 0);
        }
        FUNLOCK();
        return;
    }
    if (!have) {
        /* The other person may still be reading the confirmation prompt. */
        if (current && active_state(s_view.state)) {
            if (op == F_SYNC && get32(p->payload + 1) < FRIEND_RECORD_REVOKED)
                s_view.peer_confirmed = true;
            FUNLOCK();
            return;
        }
        /* A peer claiming an old relationship never creates a local friendship. */
        if (op == F_SYNC || op == F_READY || op == F_COMMIT || op == F_COMMIT_ACK || op == F_FINAL)
            send_frame(F_REVOKE, p->src_mac, id, 0);
        FUNLOCK();
        return;
    }
    if (op == F_SYNC) {
        uint32_t remote = get32(p->payload + 1);
        if (remote < FRIEND_RECORD_PREPARED || remote > FRIEND_RECORD_REVOKED_ACKED) {
            FUNLOCK();
            return;
        }
        if (remote >= FRIEND_RECORD_REVOKED) {
            if (save_state(&r, FRIEND_RECORD_REVOKED_ACKED))
                send_frame(F_REVOKE_ACK, r.mac, id, 0);
        } else if (remote == FRIEND_RECORD_ACTIVE) {
            if (commit_local(&r) && save_state(&r, FRIEND_RECORD_ACTIVE)) {
                send_frame(F_FINAL, r.mac, id, 0);
                done_if_current(&r);
            }
        } else {
            if (commit_local(&r)) {
                if (r.state == FRIEND_RECORD_ACTIVE || memcmp(s_mac, r.mac, 6) < 0)
                    send_frame(F_COMMIT, r.mac, id, 0);
                else
                    send_frame(F_READY, r.mac, id, 0);
            }
        }
        FUNLOCK();
        return;
    }
    if (op == F_READY) {
        if (commit_local(&r)) {
            if (r.state == FRIEND_RECORD_ACTIVE || memcmp(s_mac, r.mac, 6) < 0)
                send_frame(F_COMMIT, r.mac, id, 0);
            else
                send_frame(F_READY, r.mac, id, 0);
        }
    } else if (op == F_COMMIT) {
        /* The follower stays unauthorized until FINAL (or the peer's durable ACTIVE SYNC). */
        if (commit_local(&r)) {
            send_frame(F_COMMIT_ACK, r.mac, id, 0);
            if (current && active_state(s_view.state))
                state(FRIEND_MGR_SYNCING, "Waiting final receipt");
        }
    } else if (op == F_COMMIT_ACK) {
        if (commit_local(&r) && save_state(&r, FRIEND_RECORD_ACTIVE)) {
            send_frame(F_FINAL, r.mac, id, 0);
            done_if_current(&r);
        }
    } else if (op == F_FINAL) {
        if (commit_local(&r) && save_state(&r, FRIEND_RECORD_ACTIVE))
            done_if_current(&r);
    }
    FUNLOCK();
}
void friend_mgr_tick(void) {
    if (!s_inited)
        return;
    FLOCK();
    friend_command_t c;
    while (xQueueReceive(s_commands, &c, 0) == pdTRUE) {
        switch (c.op) {
        case C_ENTER:
            enter();
            break;
        case C_CONFIRM:
            accept_local();
            break;
        case C_CANCEL:
            s_enter_queued = false;
            cancel_current(false);
            break;
        case C_THRESHOLD:
            s_rssi_threshold = c.threshold;
            break;
        case C_REMOVE: {
            friend_record_t r;
            if (friend_store_get_active(c.mac, &r)) {
                if (save_state(&r, FRIEND_RECORD_REVOKED)) {
                    set_revoking(c.mac, false);
                    send_frame(F_REVOKE, r.mac, r.transaction_id, 0);
                } else
                    state(FRIEND_MGR_ERROR, "Delete failed; retry");
            }
            break;
        }
        }
    }
    if (!ble_radio_get_my_mac(s_mac)) {
        FUNLOCK();
        return;
    }
    uint32_t now = now_ms();
    if (s_deadline && (int32_t)(now - s_deadline) >= 0 && active_state(s_view.state))
        cancel_current(true);
    if (active_state(s_view.state) && s_nonce && s_view.state != FRIEND_MGR_SYNCING &&
        now - s_last_hello >= HELLO_INTERVAL_MS) {
        if (send_frame(F_HELLO, NULL, 0, 0))
            s_last_hello = now;
    }
    if (active_state(s_view.state) && s_view.local_confirmed && s_transaction &&
        now - s_last_control >= 350) {
        friend_record_t r;
        if (friend_store_get(s_view.peer_mac, s_transaction, &r)) {
            frame_op_t op = r.state == FRIEND_RECORD_PREPARED    ? F_ACCEPT
                            : r.state == FRIEND_RECORD_COMMITTED ? F_READY
                                                                 : F_SYNC;
            if (send_frame(op, r.mac, r.transaction_id, r.state))
                s_last_control = now;
        }
    }
    /* Round-robin reconciliation also runs outside the pairing screen, including screen-off. */
    if (now - s_last_sync >= 700) {
        for (unsigned n = 0; n < FRIEND_STORE_RECORD_MAX; n++) {
            friend_record_t r;
            unsigned i = s_sync_cursor++ % FRIEND_STORE_RECORD_MAX;
            if (!friend_store_get_record(i, &r) || r.state == FRIEND_RECORD_REVOKED_ACKED)
                continue;
            if (send_frame(r.state == FRIEND_RECORD_REVOKED ? F_REVOKE : F_SYNC, r.mac,
                           r.transaction_id, r.state))
                s_last_sync = now;
            break;
        }
    }
    FUNLOCK();
}
bool friend_mgr_enter_add_mode(friend_mgr_state_cb_t cb, friend_mgr_added_cb_t added, void *ctx) {
    if (!s_inited)
        return false;
    FLOCK();
    if (s_enter_queued || active_state(s_view.state)) {
        FUNLOCK();
        return false;
    }
    s_state_cb = cb;
    s_added_cb = added;
    s_cb_context = ctx;
    friend_command_t c = {.op = C_ENTER};
    s_enter_queued = xQueueSend(s_commands, &c, 0) == pdTRUE;
    bool ok = s_enter_queued;
    FUNLOCK();
    return ok;
}
bool friend_mgr_user_confirm(void) {
    if (!s_inited)
        return false;
    FLOCK();
    bool allowed = s_view.state == FRIEND_MGR_WAIT_USER_CONFIRM && s_peer_locked;
    friend_command_t c = {.op = C_CONFIRM};
    bool ok = allowed && xQueueSend(s_commands, &c, 0) == pdTRUE;
    FUNLOCK();
    return ok;
}
void friend_mgr_user_cancel(void) {
    if (s_commands) {
        FLOCK();
        /* Cancel also discards a not-yet-processed enter/confirm. */
        friend_command_t old;
        unsigned count = uxQueueMessagesWaiting(s_commands);
        for (unsigned i = 0; i < count; i++) {
            if (xQueueReceive(s_commands, &old, 0) != pdTRUE)
                break;
            if (old.op != C_ENTER && old.op != C_CONFIRM)
                xQueueSend(s_commands, &old, 0);
        }
        s_enter_queued = false;
        friend_command_t c = {.op = C_CANCEL};
        xQueueSendToFront(s_commands, &c, 0);
        FUNLOCK();
    }
}
void friend_mgr_exit_add_mode(void) { friend_mgr_user_cancel(); }
bool friend_mgr_is_active(void) {
    if (!s_inited)
        return false;
    FLOCK();
    bool a = s_enter_queued || active_state(s_view.state);
    for (unsigned i = 0; i < FRIEND_STORE_MAX; i++)
        if (mesh_mac_is_valid_peer(s_revoking[i]))
            a = true;
    FUNLOCK();
    return a;
}
void friend_mgr_snapshot(friend_mgr_snapshot_t *out) {
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (!s_inited)
        return;
    FLOCK();
    *out = s_view;
    uint32_t now = now_ms();
    out->remaining_seconds =
        s_deadline && (int32_t)(s_deadline - now) > 0 ? (s_deadline - now + 999) / 1000 : 0;
    FUNLOCK();
}
friend_mgr_state_t friend_mgr_get_state(void) {
    friend_mgr_snapshot_t v;
    friend_mgr_snapshot(&v);
    return v.state;
}
bool friend_mgr_is_friend(const uint8_t mac[6]) {
    if (!s_inited || !mac)
        return false;
    FLOCK();
    bool ok = !revoking(mac) && friend_store_get_active(mac, NULL);
    FUNLOCK();
    return ok;
}
unsigned friend_mgr_friend_count(void) { return friend_store_count(); }
bool friend_mgr_get_friend(unsigned index, friend_record_t *out) {
    return friend_store_get_friend(index, out);
}
bool friend_mgr_remove_friend(const uint8_t mac[6]) {
    if (!s_commands || !mac || !friend_store_get_active(mac, NULL))
        return false;
    FLOCK();
    friend_command_t c = {.op = C_REMOVE};
    memcpy(c.mac, mac, 6);
    bool ok = xQueueSend(s_commands, &c, 0) == pdTRUE;
    if (ok)
        set_revoking(mac, true);
    FUNLOCK();
    return ok;
}
int8_t friend_mgr_get_rssi_threshold(void) {
    if (!s_inited)
        return FRIEND_RSSI_MIN_DBM;
    FLOCK();
    int8_t r = s_rssi_threshold;
    FUNLOCK();
    return r;
}
bool friend_mgr_set_rssi_threshold(int8_t dbm) {
    if (!s_commands || dbm < -90 || dbm > -20)
        return false;
    friend_command_t c = {.op = C_THRESHOLD, .threshold = dbm};
    return xQueueSend(s_commands, &c, 0) == pdTRUE;
}
int8_t friend_mgr_get_last_rssi(void) {
    friend_mgr_snapshot_t v;
    friend_mgr_snapshot(&v);
    return v.rssi;
}
const char *friend_mgr_get_peer_name(void) {
    static char name[14];
    friend_mgr_snapshot_t v;
    friend_mgr_snapshot(&v);
    memcpy(name, v.peer_name, sizeof(name));
    return name;
}
const char *friend_mgr_get_discovered_peer_name(void) { return friend_mgr_get_peer_name(); }
uint32_t friend_mgr_get_discovered_peer_id(void) {
    friend_mgr_snapshot_t v;
    friend_mgr_snapshot(&v);
    return mesh_mac_is_valid_peer(v.peer_mac) ? short_id(v.peer_mac) : 0;
}
uint32_t friend_mgr_get_remaining_seconds(void) {
    friend_mgr_snapshot_t v;
    friend_mgr_snapshot(&v);
    return v.remaining_seconds;
}
void friend_mgr_process_ui_callbacks(void) {
    if (!s_inited)
        return;
    FLOCK();
    if (s_ui_revision == s_revision) {
        FUNLOCK();
        return;
    }
    s_ui_revision = s_revision;
    friend_mgr_snapshot_t v = s_view;
    friend_mgr_state_cb_t cb = s_state_cb;
    friend_mgr_added_cb_t added = s_added_cb;
    void *ctx = s_cb_context;
    FUNLOCK();
    if (cb)
        cb(v.state, ctx);
    if (v.state == FRIEND_MGR_DONE && added)
        added(short_id(v.peer_mac), v.peer_name, ctx);
}
#ifdef FRIEND_MGR_TEST
void friend_mgr_test_reboot(void) {
    if (s_commands)
        xQueueReset(s_commands);
    s_inited = false;
    s_nonce = 0;
    s_peer_nonce = 0;
    s_transaction = 0;
    s_deadline = 0;
    s_last_hello = 0;
    s_last_control = 0;
    s_last_sync = 0;
    s_peer_locked = false;
    s_enter_queued = false;
    memset(s_candidates, 0, sizeof(s_candidates));
    memset(s_revoking, 0, sizeof(s_revoking));
    friend_mgr_init();
}
#endif
