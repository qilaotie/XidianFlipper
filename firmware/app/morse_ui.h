#ifndef MORSE_UI_H
#define MORSE_UI_H
#include <stdint.h>
#include <stdbool.h>
typedef enum { MORSE_UI_CQ = 0, MORSE_UI_RXQ } morse_ui_state_t;
typedef enum { CQ_MODE_RX = 0, CQ_MODE_TX } cq_mode_t;
void morse_ui_init(void);
morse_ui_state_t morse_ui_get_state(void);
cq_mode_t morse_ui_get_cq_mode(void);
void morse_ui_key_down(void);
void morse_ui_key_up(uint32_t duration_ms);
void morse_ui_tick(void);
void morse_ui_render(void);
/* Explicit enqueue only. True means queued, never a delivery receipt. */
bool morse_ui_send_plain_text(const char *text, uint16_t len);
/* UI thread only. Reception never automatically plays or transmits. */
void morse_ui_rx_receive(const uint8_t src[6], uint8_t channel, uint32_t message_id,
                         const char *text, uint16_t len, int8_t rssi);
void morse_ui_rx_push(uint32_t src_id, int8_t rssi, const char *text, uint16_t len);
bool morse_ui_playback_is_active(void);
#endif