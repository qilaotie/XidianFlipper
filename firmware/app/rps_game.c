/* UI owns game state. The message worker only copies into a bounded mailbox. */
#include "rps_game.h"
#ifdef RPS_HOST_TEST
#include "../../tools/rps_host_stubs.h"
#else
#include "ui_zh.h"
#include "ui_language.h"
#include "message_service.h"
#include "ble_radio.h"
#include "wououi.h"
#include "haptic_feedback.h"
#include "nvs_config.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#endif
#include <string.h>
#include <stdio.h>

enum { HELLO = 1, INVITE, ACCEPT, MOVE, RESULT, LEAVE };
enum { SEARCH, LINKING, PLAY, DONE, LOST };
typedef struct { uint8_t mac[6], data[19]; int8_t rssi; uint32_t at; } event_t;
typedef struct { uint8_t mac[6]; char name[8]; uint32_t token, seen; int rssi; } peer_t;
static portMUX_TYPE s_guard = portMUX_INITIALIZER_UNLOCKED;
static event_t s_events[16];
static unsigned s_head, s_count;
static bool s_active;
static peer_t s_peers[8], s_peer;
static uint8_t s_mac[6], s_state, s_choice, s_move, s_remote;
static uint32_t s_token, s_started, s_last_tx, s_last_rx;
static bool s_confirmed;
static bool s_discovery_turn;
static const char *const s_names[] = { "?", "ROCK", "SCISSORS", "PAPER" };
static uint32_t millis(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
void rps_game_receive(const mesh_packet_t *p, int8_t rssi) {
    if ((p->len != 11 && p->len != 19) || p->channel || p->flags || p->payload[0] != 1 ||
        p->payload[1] < HELLO || p->payload[1] > LEAVE || !get32(p->payload + 2)) return;
    if ((p->payload[1] == HELLO) != mesh_packet_is_broadcast(p->dst_mac)) return;
    if ((p->payload[1] == MOVE || p->payload[1] == RESULT) &&
        (p->payload[10] < 1 || p->payload[10] > 3)) return;
    uint32_t now = millis();
    portENTER_CRITICAL(&s_guard);
    if (s_active && s_count < 16) {
        event_t *e = &s_events[(s_head + s_count++) % 16];
        memcpy(e->mac, p->src_mac, 6);
        memset(e->data, 0, sizeof(e->data));
        memcpy(e->data, p->payload, p->len);
        e->rssi = rssi; e->at = now;
    }
    portEXIT_CRITICAL(&s_guard);
}
static bool send_frame(uint8_t op) {
    uint8_t b[19] = {1, op};
    put32(b + 2, s_token);
    put32(b + 6, op == HELLO ? 0 : s_peer.token);
    b[10] = s_move;
    const char *name = nvs_config_get_device_name();
    if (name) snprintf((char *)b + 11, 8, "%.7s", name);
    return message_service_send_control(MESH_TYPE_RPS, op == HELLO ? NULL : s_peer.mac, b, sizeof(b));
}
void rps_game_leave(void) {
    portENTER_CRITICAL(&s_guard);
    s_active = false; s_count = s_head = 0;
    portEXIT_CRITICAL(&s_guard);
}
static void restart(void) {
    rps_game_leave();
    memset(s_peers, 0, sizeof(s_peers));
    memset(&s_peer, 0, sizeof(s_peer));
    s_token = esp_random();
    if (!s_token) s_token = 1;
    s_state = SEARCH; s_move = s_remote = 0; s_choice = 1; s_confirmed = false;
    s_discovery_turn = false;
    s_started = s_last_rx = millis(); s_last_tx = s_started - 700;
    portENTER_CRITICAL(&s_guard);
    s_active = true;
    portEXIT_CRITICAL(&s_guard);
}
static void receive_events(uint32_t now) {
    for (unsigned n = 0; n < 16; ++n) {
        event_t e;
        portENTER_CRITICAL(&s_guard);
        bool have = s_count != 0;
        if (have) { e = s_events[s_head]; s_head = (s_head + 1) % 16; --s_count; }
        portEXIT_CRITICAL(&s_guard);
        if (!have) break;
        if (now - e.at > 2000) continue;
        uint8_t op = e.data[1];
        uint32_t token = get32(e.data + 2);
        if (op == HELLO) {
            if (s_state != SEARCH) continue;
            unsigned slot = 0;
            for (unsigned i = 0; i < 8; ++i) {
                if (!memcmp(s_peers[i].mac, e.mac, 6)) { slot = i; break; }
                if (s_peers[i].seen < s_peers[slot].seen) slot = i;
            }
            peer_t *p = &s_peers[slot];
            bool same = p->token == token && !memcmp(p->mac, e.mac, 6);
            p->rssi = same ? (3 * p->rssi + e.rssi) / 4 : e.rssi;
            memcpy(p->mac, e.mac, 6); p->token = token; p->seen = now;
            memcpy(p->name, e.data + 11, 7); p->name[7] = '\0';
            for (unsigned i = 0; i < 7 && p->name[i]; ++i)
                if ((unsigned char)p->name[i] < 32 || (unsigned char)p->name[i] > 126) p->name[i] = '?';
            continue;
        }
        if (get32(e.data + 6) != s_token || token != s_peer.token ||
            memcmp(e.mac, s_peer.mac, 6)) continue;
        s_last_rx = now;
        if (op == LEAVE) { s_state = LOST; continue; }
        if (op == INVITE && (s_state == LINKING || s_state == PLAY)) {
            s_state = PLAY; send_frame(ACCEPT);
        } else if (op == ACCEPT && s_state == LINKING) s_state = PLAY;
        else if ((op == MOVE || op == RESULT) && (s_state == PLAY || s_state == DONE)) {
            if (!s_remote) s_remote = e.data[10];
            if (e.data[10] != s_remote) continue;
            if (op == RESULT) s_confirmed = true;
        }
    }
}
void rps_proc(void) {
    if (!s_active) restart();
    uint32_t now = millis();
    bool ready = ble_radio_get_my_mac(s_mac) && ble_radio_is_ready();
    receive_events(now);
    if (btn.pressed) {
        uint8_t key = btn.id; btn.pressed = false;
        if (key == BTN_ID_BACK) {
            if (s_peer.token) send_frame(LEAVE);
            rps_game_leave(); haptic_play(HAPTIC_BACK);
            ui.index = ui.parent_index[ui.layer]; ui.state = S_LAYER_OUT;
            return;
        }
        if (key == BTN_ID_SP && (s_state == LOST || (s_state == DONE && s_confirmed))) restart();
        else if (s_state == PLAY && !s_move) {
            if (key == BTN_ID_CW) s_choice = s_choice % 3 + 1;
            if (key == BTN_ID_CC) s_choice = (s_choice + 1) % 3 + 1;
            if (key == BTN_ID_SP) { s_move = s_choice; s_last_tx = now - 700; haptic_play(HAPTIC_TICK); }
        }
    }
    if (s_state == SEARCH && now - s_started >= 3500 && ready) {
        int best = -1;
        for (unsigned i = 0; i < 8; ++i)
            if (s_peers[i].token && now - s_peers[i].seen < 2200 &&
                (best < 0 || s_peers[i].rssi > s_peers[best].rssi ||
                 (s_peers[i].rssi == s_peers[best].rssi && memcmp(s_peers[i].mac, s_peers[best].mac, 6) < 0))) best = i;
        if (best >= 0) {
            s_peer = s_peers[best]; s_state = LINKING; s_last_rx = now; s_last_tx = now - 700;
        }
    }
    if (s_state == PLAY && s_move && s_remote) {
        s_state = DONE; haptic_play(HAPTIC_REVEAL); s_last_tx = now - 700;
    }
    if ((s_state == LINKING || s_state == PLAY || (s_state == DONE && !s_confirmed)) &&
        now - s_last_rx > 25000) s_state = LOST;
    if (ready && now - s_last_tx >= 700) {
        uint8_t op = 0;
        if (s_state == SEARCH) op = HELLO;
        if (s_state == LINKING) {
            /* Keep announcing while the peer completes its discovery window. */
            s_discovery_turn = !s_discovery_turn;
            op = s_discovery_turn ? HELLO : memcmp(s_mac, s_peer.mac, 6) < 0 ? INVITE : ACCEPT;
        }
        if (s_state == PLAY) op = s_move ? MOVE : ACCEPT;
        if (s_state == DONE) op = RESULT;
        if (op && send_frame(op)) s_last_tx = now;
    }
    char top[96], bottom[96];
    if (!ready) { snprintf(top, sizeof(top), "RADIO NOT READY"); snprintf(bottom, sizeof(bottom), "BACK: EXIT"); }
    else if (s_state == SEARCH) { snprintf(top, sizeof(top), "RPS: SEARCH NEARBY"); snprintf(bottom, sizeof(bottom), "OPEN RPS ON PEER"); }
    else if (s_state == LINKING) { snprintf(top, sizeof(top), "LINK %02X%02X %ddBm", s_peer.mac[4], s_peer.mac[5], s_peer.rssi); snprintf(bottom, sizeof(bottom), "MATCHING..."); }
    else if (s_state == LOST) { snprintf(top, sizeof(top), "PEER LEFT / TIMEOUT"); snprintf(bottom, sizeof(bottom), "SP: SEARCH AGAIN"); }
    else if (s_state == DONE) {
        const char *result = s_move == s_remote ? "DRAW" : s_move % 3 + 1 == s_remote ? "WIN" : "LOSE";
        snprintf(top, sizeof(top), "%s %s/%s", ui_zh_lookup(result), ui_zh_lookup(s_names[s_move]), ui_zh_lookup(s_names[s_remote]));
        snprintf(bottom, sizeof(bottom), "%s", s_confirmed ? "SP: NEXT MATCH" : "CONFIRMING RESULT...");
    } else {
        snprintf(top, sizeof(top), "%s: %s", ui_zh_lookup(s_move ? (ui_language_chinese()?"PICK LOCKED":"LOCKED") : "PICK"), ui_zh_lookup(s_names[s_move ? s_move : s_choice]));
        snprintf(bottom, sizeof(bottom), "%s", s_move ? "WAIT FOR PEER..." : "TURN:PICK SP:LOCK");
    }
    u8g2_SetDrawColor(&u8g2, 1); u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
    if (s_peer.token) {
        char opponent[26];
        if (s_peer.name[0]) snprintf(opponent, sizeof(opponent), "VS %s", s_peer.name);
        else snprintf(opponent, sizeof(opponent), "VS %02X%02X", s_peer.mac[4], s_peer.mac[5]);
        u8g2_DrawStr(&u8g2, 2, 7, opponent);
        ui_zh_draw_fit(&u8g2, 2, 18, top,124);
        ui_zh_draw_fit(&u8g2, 2, 30, bottom,124);
    } else {
        ui_zh_draw_fit(&u8g2, 2, 11, top,124); ui_zh_draw_fit(&u8g2, 2, 27, bottom,124);
    }
}
