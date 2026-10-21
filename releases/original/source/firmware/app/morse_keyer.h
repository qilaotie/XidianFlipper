#ifndef MORSE_KEYER_H
#define MORSE_KEYER_H
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    MORSE_IN_NONE = 0, MORSE_IN_KEY_DOWN, MORSE_IN_KEY_UP,
    MORSE_IN_DOT, MORSE_IN_DASH, MORSE_IN_CHAR_GAP, MORSE_IN_WORD_GAP
} morse_input_event_t;
typedef void (*morse_keyer_emit_t)(morse_input_event_t event, uint32_t duration_ms, void *context);
typedef struct {
    uint32_t dot_ms;
    uint64_t raw_since_us, press_us, release_us;
    bool raw_pressed, pressed, has_release, char_fired, word_fired, suppress;
    morse_keyer_emit_t emit;
    void *context;
} morse_keyer_t;
void morse_keyer_init(morse_keyer_t *keyer, uint32_t dot_ms, bool held,
                      morse_keyer_emit_t emit, void *context);
void morse_keyer_edge(morse_keyer_t *keyer, bool pressed, uint64_t timestamp_us);
void morse_keyer_advance(morse_keyer_t *keyer, uint64_t timestamp_us);
void morse_keyer_set_speed(morse_keyer_t *keyer, uint32_t dot_ms);
#endif
