/* UI-thread Morse terminal: complete tree context, readable current route and ephemeral flow.
 * morse_input supplies timestamped keyer events. No page transition implicitly enables TX.
 */
#ifndef MORSE_TERMINAL_H
#define MORSE_TERMINAL_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MORSE_TERM_MSG_MAX 128
void morse_terminal_init(void);
void morse_terminal_exit(void);
void morse_terminal_tick(void);
void morse_terminal_render(void);
void morse_terminal_feed_rotate(int delta);
void morse_terminal_feed_key_up(uint32_t duration_ms); /* legacy; raw events own timing */
void morse_terminal_feed_double_click(void);
/* Practice forcibly disables TX. Live must be explicitly enabled after each init/entry. */
void morse_terminal_set_practice_mode(bool practice);
void morse_terminal_set_live_mode(bool live);
bool morse_terminal_is_keying(void);
uint32_t morse_terminal_get_dot_ms(void);
/* Compatibility snapshots are only the visible flow; they are not an editable send draft. */
const char *morse_terminal_get_message(void);
int morse_terminal_get_message_len(void);
bool morse_terminal_set_message(const char *text); /* local display only; never transmits */
void morse_terminal_clear_message(void);
void morse_terminal_clear_input(void);
void morse_terminal_backspace(void); /* cancel only the incomplete character */
#ifdef __cplusplus
}
#endif
#endif
