/* Compatibility facade. Transport is explicit and owned by message_service.
   No timer, page transition or received packet can implicitly send a draft. */
#include "morse_ui.h"
#include "morse_terminal.h"
#include "morse_playback.h"
#include "morse_codec.h"
#include "morse_input.h"
#include "message_service.h"
#include "wououi.h"
#include <string.h>
#include <stdio.h>
static cq_mode_t s_mode = CQ_MODE_RX;
static char s_last_text[MORSE_TEXT_MAX + 1];
static uint32_t s_last_id;
static int8_t s_last_rssi;
void morse_ui_init(void)
{
    s_mode = CQ_MODE_RX; s_last_text[0] = 0; s_last_id = 0;
}
morse_ui_state_t morse_ui_get_state(void) { return MORSE_UI_CQ; }
cq_mode_t morse_ui_get_cq_mode(void) { return s_mode; }
void morse_ui_key_down(void)
{
    if (morse_playback_is_active()) { morse_playback_stop(); return; }
    if (s_mode == CQ_MODE_RX) { morse_terminal_init(); s_mode = CQ_MODE_TX; }
}
void morse_ui_key_up(uint32_t duration_ms) { (void)duration_ms; }
void morse_ui_tick(void) { if (s_mode == CQ_MODE_TX) morse_terminal_tick(); }
void morse_ui_render(void)
{
    if (s_mode == CQ_MODE_TX) { morse_terminal_render(); return; }
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    char title[32];
    snprintf(title, sizeof(title), "CH%02u  #%lu  %ddBm", message_service_get_channel(),
             (unsigned long)s_last_id, s_last_rssi);
    u8g2_DrawStr(&u8g2, 0, 7, title);
    u8g2_DrawStr(&u8g2, 0, 19, s_last_text[0] ? s_last_text : "No channel messages");
    u8g2_DrawStr(&u8g2, 0, 31, "Press to compose");
}
bool morse_ui_send_plain_text(const char *text, uint16_t len)
{
    if (!morse_codec_validate(text, len)) return false;
    return message_service_send_text(MESSAGE_KIND_MORSE, NULL, message_service_get_channel(),
                                      text, len, false, NULL);
}
void morse_ui_rx_receive(const uint8_t src[6], uint8_t channel, uint32_t message_id,
                         const char *text, uint16_t len, int8_t rssi)
{
    (void)src;
    if (channel != message_service_get_channel() || !morse_codec_validate(text, len)) return;
    memcpy(s_last_text, text, len); s_last_text[len] = 0;
    s_last_id = message_id; s_last_rssi = rssi;
    /* Receipt/history belongs to message_service. Replay is an explicit UI action. */
}
void morse_ui_rx_push(uint32_t src_id, int8_t rssi, const char *text, uint16_t len)
{
    uint8_t src[6] = {0};
    memcpy(src + 2, &src_id, 4);
    morse_ui_rx_receive(src, message_service_get_channel(), ++s_last_id, text, len, rssi);
}
bool morse_ui_playback_is_active(void) { return morse_playback_is_active(); }