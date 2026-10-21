#ifndef HAPTIC_FEEDBACK_H
#define HAPTIC_FEEDBACK_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Semantic sound effects, stable existing IDs. 2-6 kHz differential piezo;
 * no motor and no extra settings schema. Pass these IDs to haptic_play(). */
typedef enum {
    HAPTIC_TICK = 0,
    HAPTIC_BUMP = 1,
    HAPTIC_WIN_VALUE = 2,
    HAPTIC_CONFIRM = 3,
    HAPTIC_BACK = 4,
    HAPTIC_TOGGLE_ON = 5,
    HAPTIC_TOGGLE_OFF = 6,
    HAPTIC_WINDOW_OPEN = 7,
    HAPTIC_WINDOW_CLOSE = 8,
    HAPTIC_MSG_SENT = 9,
    HAPTIC_MSG_RX = 10,
    HAPTIC_EMERGENCY = 11,
    HAPTIC_FRIEND_FOUND = 12,
    HAPTIC_FRIEND_TIMEOUT = 13,
    HAPTIC_BOOT = 14, /* coin: 3951 Hz, 12 ms gap, 5274 Hz */
    HAPTIC_SLEEP = 15,
    HAPTIC_ERROR = 16,
    HAPTIC_CHARGE_DETECT = 17,
    HAPTIC_MORSE_CORRECT = 18,
    HAPTIC_MORSE_WRONG = 19,
    HAPTIC_MORSE_LESSON_DONE = 20,
    HAPTIC_MORSE_TIER_DONE = 21,
    HAPTIC_MORSE_STAR = 22,
    HAPTIC_REVEAL = 23,
    HAPTIC_RADAR = 24,
    HAPTIC_PET_CALL = 25,
    HAPTIC_COUNT
} haptic_pattern_t;

void haptic_init(void); /* call once after buzzer_init */
void haptic_play(haptic_pattern_t pattern); /* no waits; muted by buzzer_enabled */
void haptic_beep_only(haptic_pattern_t pattern);
void haptic_vibrate_only(haptic_pattern_t pattern); /* compatibility no-op */
bool haptic_is_busy(void); /* includes gaps and the final note */
void haptic_stop_all(void); /* cancel owned feedback; does not stop newer Morse */
void haptic_set_morse_priority(bool active); /* claiming cancels pending feedback */
#ifdef __cplusplus
}
#endif
#endif
