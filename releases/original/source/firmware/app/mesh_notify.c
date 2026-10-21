#include "mesh_notify.h"
#include "friend_mgr.h"
#include "mcpwm_buzzer.h"
#include "morse_ui.h"
#include "rgb_led.h"
static mesh_notify_allowed_fn_t s_allowed;
static mesh_notify_message_fn_t s_handler;
extern void msg_rx_trigger(const char *text, uint32_t src_id, bool emergency);
void mesh_notify_init(void) {}
void mesh_notify_set_handlers(mesh_notify_allowed_fn_t allowed, mesh_notify_message_fn_t message) {
    s_allowed = allowed;
    s_handler = message;
}
void mesh_notify_dispatch(const mesh_packet_t *p, int8_t rssi) {
    message_service_rx_enqueue(p, rssi);
}
void mesh_notify_process(void) {
    friend_mgr_process_ui_callbacks();
    if (s_allowed && !s_allowed())
        return;
    message_inbox_entry_t e;
    /* One notification per frame. A full queue never loses the persistent inbox. */
    if (!message_service_pop_notification(&e))
        return;
    if (s_handler) {
        s_handler(&e);
        return;
    }
    if (e.kind == MESSAGE_KIND_MORSE)
        morse_ui_rx_receive(e.src_mac, e.channel, e.msg_id, e.text, e.len, e.rssi);
    else {
        uint32_t id = 2166136261u;
        for (unsigned i = 0; i < 6; i++)
            id = (id ^ e.src_mac[i]) * 16777619u;
        msg_rx_trigger(e.text, id, false);
    }
    rgb_led_trigger(RGB_EVENT_MSG_RECEIVED);
    buzzer_beep(70);
}