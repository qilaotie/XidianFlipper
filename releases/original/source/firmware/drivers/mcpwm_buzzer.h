#ifndef MCPWM_BUZZER_H
#define MCPWM_BUZZER_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

/* GPIO20/21 complementary square waves. Existing buzzer_enabled NVS setting
 * remains the sole mute setting. All APIs are task-context, without audio waits. */
#define BUZZER_MIN_FREQUENCY_HZ 2000
#define BUZZER_MAX_FREQUENCY_HZ 6000
#define BUZZER_SEQUENCE_MAX 12
typedef struct {
    uint16_t frequency_hz; /* 0 = silent gap; nonzero clamped to 2-6 kHz */
    uint16_t duration_ms;  /* strictly positive */
} buzzer_note_t;

void buzzer_init(void);
void buzzer_beep(uint16_t duration_ms); /* 4 kHz, at least 5 ms */
void buzzer_tone(uint16_t frequency_hz, uint16_t duration_ms); /* 0 Hz cancels */
void buzzer_start(void); /* continuous 4 kHz; replaces any sequence */
void buzzer_stop(void);  /* cancel timers, immediately force both outputs LOW */
bool buzzer_is_playing(void);

/* Copy a bounded sequence; a new start replaces the previous output. A token
 * allows an old feedback owner to cancel itself without stopping newer Morse.
 * Sequence remains active through silent gaps and until the final note ends. */
bool buzzer_sequence_start(const buzzer_note_t *notes, uint8_t count, uint32_t *token);
bool buzzer_sequence_is_active(uint32_t token);
void buzzer_sequence_cancel(uint32_t token);
#ifdef __cplusplus
}
#endif
#endif
