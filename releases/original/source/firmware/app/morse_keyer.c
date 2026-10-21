#include "morse_keyer.h"
#include <string.h>
#define DEBOUNCE_US 3000ULL
static void emit(morse_keyer_t *k, morse_input_event_t e, uint32_t duration)
{ if (k->emit) k->emit(e, duration, k->context); }
void morse_keyer_set_speed(morse_keyer_t *k, uint32_t ms)
{ k->dot_ms = ms < 40 ? 40 : ms > 300 ? 300 : ms; }
void morse_keyer_init(morse_keyer_t *k, uint32_t ms, bool held,
                      morse_keyer_emit_t callback, void *context)
{
    memset(k, 0, sizeof(*k));
    morse_keyer_set_speed(k, ms);
    k->raw_pressed = k->pressed = k->suppress = held;
    k->emit = callback; k->context = context;
}
static void gaps(morse_keyer_t *k, uint64_t now)
{
    if (k->pressed || !k->has_release || now < k->release_us) return;
    uint64_t elapsed = now - k->release_us;
    /* Emit both in chronological order when a slow frame spans both thresholds. */
    if (!k->char_fired && elapsed >= (uint64_t)k->dot_ms * 3000) {
        k->char_fired = true; emit(k, MORSE_IN_CHAR_GAP, 0);
    }
    if (!k->word_fired && elapsed >= (uint64_t)k->dot_ms * 7000) {
        k->word_fired = true; emit(k, MORSE_IN_WORD_GAP, 0);
    }
}
void morse_keyer_advance(morse_keyer_t *k, uint64_t now)
{
    if (k->raw_pressed != k->pressed && now >= k->raw_since_us + DEBOUNCE_US) {
        uint64_t edge = k->raw_since_us;
        gaps(k, edge);
        k->pressed = k->raw_pressed;
        if (k->pressed) {
            k->press_us = edge;
            if (!k->suppress) emit(k, MORSE_IN_KEY_DOWN, 0);
        } else if (k->suppress) {
            k->suppress = false;
        } else {
            uint32_t duration = (uint32_t)((edge - k->press_us) / 1000);
            emit(k, MORSE_IN_KEY_UP, duration);
            if (duration >= 10) {
                emit(k, duration <= 2 * k->dot_ms ? MORSE_IN_DOT : MORSE_IN_DASH, duration);
                k->release_us = edge;
                k->has_release = true;
                k->char_fired = k->word_fired = false;
            }
        }
    }
    /* A pending down edge blocks gaps after that edge, before debounce completes. */
    gaps(k, k->raw_pressed && !k->pressed ? k->raw_since_us : now);
}
void morse_keyer_edge(morse_keyer_t *k, bool pressed, uint64_t ts)
{
    morse_keyer_advance(k, ts);
    if (pressed != k->raw_pressed) { k->raw_pressed = pressed; k->raw_since_us = ts; }
}
