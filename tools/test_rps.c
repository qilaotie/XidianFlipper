/* Runs the actual game state machine with mocked clock/radio/display. */
#define RPS_HOST_TEST
#include "../firmware/app/rps_game.c"
#include <assert.h>
static uint32_t clock_ms = 100, random_id = 100;
static char rendered[2][40];
static char opponent_line[40];
const char *nvs_config_get_device_name(void) { return "MYNAME"; }
int64_t esp_timer_get_time(void) { return (int64_t)clock_ms * 1000; }
uint32_t esp_random(void) { return ++random_id; }
bool ble_radio_get_my_mac(uint8_t *mac) { memset(mac, 1, 6); return true; }
bool ble_radio_is_ready(void) { return true; }
bool message_service_send_control(uint8_t t, const uint8_t *d, const void *p, uint8_t n) {
    (void)d; assert(t == MESH_TYPE_RPS && n == 19);
    assert(strcmp((const char *)p + 11, "MYNAME") == 0); return true;
}
void haptic_play(int e) { (void)e; }
void u8g2_SetDrawColor(void *d, int c) { (void)d; assert(c == 1); }
void u8g2_SetFont(void *d, const void *f) { (void)d; (void)f; }
void u8g2_DrawStr(void *d, int x, int y, const char *s) {
    (void)d; assert(x + strlen(s) * 5 <= 128);
    if (y == 7) snprintf(opponent_line, 40, "%s", s);
    else snprintf(rendered[y == 11 || y == 18 ? 0 : 1], 40, "%s", s);
}
static void incoming(uint8_t mac, int rssi, uint32_t token, uint32_t target, uint8_t op, uint8_t move) {
    mesh_packet_t p = {.len = 19};
    memset(p.src_mac, mac, 6);
    memset(p.dst_mac, op == HELLO ? 255 : 1, 6);
    p.payload[0] = 1; p.payload[1] = op;
    put32(p.payload + 2, token); put32(p.payload + 6, target); p.payload[10] = move;
    memcpy(p.payload + 11, "PLAYER", 7);
    rps_game_receive(&p, rssi);
}
static void key(uint8_t id) { btn.id = id; btn.pressed = true; rps_proc(); }
static void pair(void) {
    restart(); rps_proc();
    clock_ms += 2000;
    incoming(2, -75, 200, 0, HELLO, 0);
    incoming(3, -40, 300, 0, HELLO, 0);
    rps_proc(); clock_ms += 1600; rps_proc();
    assert(s_state == LINKING && s_peer.mac[0] == 3);
    incoming(3, -40, 300, s_token - 1, ACCEPT, 0); rps_proc();
    assert(s_state == LINKING); /* Old round cannot complete handshake. */
    incoming(3, -40, 300, s_token, ACCEPT, 0); rps_proc();
    assert(s_state == PLAY);
    assert(strcmp(s_peer.name, "PLAYER") == 0);
    assert(strcmp(opponent_line, "VS PLAYER") == 0);
}
int main(void) {
    const char *expected[3][3] = {
        {"DRAW", "WIN", "LOSE"}, {"LOSE", "DRAW", "WIN"}, {"WIN", "LOSE", "DRAW"}
    };
    for (uint8_t mine = 1; mine <= 3; ++mine) for (uint8_t theirs = 1; theirs <= 3; ++theirs) {
        pair();
        incoming(3, -40, 300, s_token, MOVE, 0); rps_proc(); assert(s_remote == 0);
        incoming(3, -40, 300, s_token, MOVE, theirs); rps_proc();
        assert(s_state == PLAY); /* Peer's move remains hidden until local lock. */
        s_choice = mine; key(BTN_ID_SP);
        assert(s_state == DONE && s_move == mine);
        assert(strncmp(rendered[0], expected[mine - 1][theirs - 1], strlen(expected[mine - 1][theirs - 1])) == 0);
        key(BTN_ID_CW); assert(s_move == mine);
        incoming(3, -40, 300, s_token, RESULT, theirs); rps_proc(); assert(s_confirmed);
        key(BTN_ID_SP); assert(s_state == SEARCH && s_remote == 0 && s_move == 0);
    }
    pair(); incoming(3, -40, 300, s_token, LEAVE, 0); rps_proc(); assert(s_state == LOST);
    key(BTN_ID_SP); assert(s_state == SEARCH);
    pair(); clock_ms += 25001; rps_proc(); assert(s_state == LOST);
    key(BTN_ID_BACK); assert(!s_active);
    puts("PASS: 9 outcomes, RSSI selection, stale rounds, invalid moves, lock, receipt, rematch, leave, timeout, display width");
    return 0;
}
