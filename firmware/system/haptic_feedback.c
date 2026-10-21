/* Semantic UI sounds. All notes/gaps run in the buzzer's single sequencer. */
#include "haptic_feedback.h"
#include "mcpwm_buzzer.h"
#include "nvs_config.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    uint8_t priority, count;
    buzzer_note_t notes[BUZZER_SEQUENCE_MAX];
} sound_pattern_t;

/* Low navigation ticks never truncate a functional result or the boot coin.
 * Frequency and rhythm both carry meaning; every pitched note fits 2-6 kHz. */
static const sound_pattern_t s_patterns[HAPTIC_COUNT] = {
    [HAPTIC_TICK]              = {0, 1, {{4000, 8}}},
    [HAPTIC_RADAR]             = {0, 1, {{4400, 30}}},
    [HAPTIC_PET_CALL]          = {0, 3, {{3200, 35}, {0, 80}, {4000, 35}}},
    [HAPTIC_BUMP]              = {0, 1, {{2400, 25}}},
    [HAPTIC_WIN_VALUE]         = {0, 1, {{4400, 10}}},
    [HAPTIC_CONFIRM]           = {1, 3, {{3200, 28}, {0, 12}, {4400, 45}}},
    [HAPTIC_BACK]              = {1, 3, {{4000, 25}, {0, 10}, {2800, 40}}},
    [HAPTIC_TOGGLE_ON]         = {1, 3, {{3200, 25}, {0, 25}, {4800, 35}}},
    [HAPTIC_TOGGLE_OFF]        = {1, 3, {{4000, 25}, {0, 25}, {2400, 45}}},
    [HAPTIC_WINDOW_OPEN]       = {0, 1, {{4400, 22}}},
    [HAPTIC_WINDOW_CLOSE]      = {0, 1, {{3000, 18}}},
    [HAPTIC_MSG_SENT]          = {1, 3, {{4000, 35}, {0, 35}, {5200, 65}}},
    [HAPTIC_MSG_RX]            = {1, 3, {{5200, 55}, {0, 70}, {4000, 55}}},
    [HAPTIC_EMERGENCY]         = {2, 5, {{5600, 55}, {0, 40}, {3000, 55}, {0, 40}, {5600, 90}}},
    [HAPTIC_FRIEND_FOUND]      = {2, 5, {{3200, 40}, {0, 20}, {4000, 40}, {0, 20}, {5200, 100}}},
    [HAPTIC_FRIEND_TIMEOUT]    = {1, 3, {{3000, 65}, {0, 65}, {2200, 90}}},
    /* Coin pickup, B7 -> E8: the familiar interval transposed into piezo range. */
    [HAPTIC_BOOT]              = {2, 3, {{3951, 75}, {0, 12}, {5274, 180}}},
    [HAPTIC_SLEEP]             = {1, 3, {{4000, 35}, {0, 15}, {2200, 65}}},
    [HAPTIC_ERROR]             = {1, 3, {{2600, 65}, {0, 30}, {2000, 95}}},
    [HAPTIC_CHARGE_DETECT]     = {0, 3, {{3600, 25}, {0, 15}, {4800, 55}}},
    [HAPTIC_MORSE_CORRECT]     = {1, 3, {{3600, 35}, {0, 30}, {4800, 60}}},
    [HAPTIC_MORSE_WRONG]       = {1, 3, {{2800, 40}, {0, 20}, {2200, 65}}},
    [HAPTIC_MORSE_LESSON_DONE] = {2, 5, {{3200, 35}, {0, 20}, {4000, 35}, {0, 20}, {5200, 100}}},
    [HAPTIC_MORSE_TIER_DONE]   = {2, 7, {{3000, 40}, {0, 20}, {3800, 40}, {0, 20}, {4600, 40}, {0, 20}, {5800, 110}}},
    [HAPTIC_MORSE_STAR]        = {0, 1, {{5200, 25}}},
    [HAPTIC_REVEAL]            = {2, 5, {{3200, 30}, {0, 60}, {4000, 30}, {0, 60}, {5400, 140}}},
};

static SemaphoreHandle_t s_lock;
static uint32_t s_token;
static uint8_t s_priority;
static uint64_t s_last_tick_us;
static bool s_had_tick, s_morse_priority;

void haptic_init(void)
{
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
}

void haptic_play(haptic_pattern_t pattern)
{
    if (!s_lock || (unsigned)pattern >= HAPTIC_COUNT) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_morse_priority) {
        xSemaphoreGive(s_lock);
        return;
    }
    if (!g_config.buzzer_enabled) {
        buzzer_sequence_cancel(s_token);
        s_token = 0;
        xSemaphoreGive(s_lock);
        return;
    }
    const sound_pattern_t *def = &s_patterns[pattern];
    bool busy = buzzer_sequence_is_active(s_token);
    uint64_t now = (uint64_t)esp_timer_get_time();
    if ((busy && def->priority < s_priority) ||
        (pattern == HAPTIC_TICK && s_had_tick && now - s_last_tick_us < 30000)) {
        xSemaphoreGive(s_lock);
        return;
    }
    if (buzzer_sequence_start(def->notes, def->count, &s_token)) {
        s_priority = def->priority;
        if (pattern == HAPTIC_TICK) { s_last_tick_us = now; s_had_tick = true; }
    }
    xSemaphoreGive(s_lock);
}

void haptic_beep_only(haptic_pattern_t pattern) { haptic_play(pattern); }
void haptic_vibrate_only(haptic_pattern_t pattern) { (void)pattern; }

bool haptic_is_busy(void)
{
    if (!s_lock) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool busy = buzzer_sequence_is_active(s_token);
    xSemaphoreGive(s_lock);
    return busy;
}

void haptic_stop_all(void)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    /* An obsolete feedback token must never cancel a newer Morse key press. */
    buzzer_sequence_cancel(s_token);
    s_token = 0;
    xSemaphoreGive(s_lock);
}

void haptic_set_morse_priority(bool active)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_morse_priority = active;
    if (active) {
        buzzer_sequence_cancel(s_token);
        s_token = 0;
    }
    xSemaphoreGive(s_lock);
}
