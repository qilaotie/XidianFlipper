#ifndef MORSE_INPUT_H
#define MORSE_INPUT_H
#include "morse_keyer.h"
#define MORSE_IN_DOT_MIN_MS 10
#define MORSE_IN_DOT_DEF_MS 200
#define MORSE_IN_DOT_MAX_MS 300
#define MORSE_IN_DOT_MIN_SET 40
#define MORSE_IN_DOT_STEP 10
/* Initialise once after ec06_init; installs physical/remote observer. */
void morse_input_init(uint8_t gpio);
/* Reset on session entry, never when committing an individual character. */
void morse_input_reset(void);
morse_input_event_t morse_input_poll(void);
uint32_t morse_input_get_duration_ms(void);
void morse_input_set_dot_ms(uint32_t ms);
uint32_t morse_input_get_dot_ms(void);
bool morse_input_is_held(void);
uint32_t morse_input_get_overflows(void);
#endif