/* Differential piezo output. GPIO matrix ownership stays with MCPWM for life. */
#include "mcpwm_buzzer.h"
#include "pins.h"
#include "nvs_config.h"
#include "driver/mcpwm_timer.h"
#include "driver/mcpwm_oper.h"
#include "driver/mcpwm_cmpr.h"
#include "driver/mcpwm_gen.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

#define TIMER_RESOLUTION_HZ 1000000U
#define MORSE_FREQUENCY_HZ 4000U
#define MIN_DURATION_MS 5U
static const char *TAG = "buzzer";
static mcpwm_timer_handle_t s_pwm;
static mcpwm_oper_handle_t s_oper;
static mcpwm_cmpr_handle_t s_compare;
static mcpwm_gen_handle_t s_positive, s_negative;
static esp_timer_handle_t s_sequence_timer;
static SemaphoreHandle_t s_lock;
static buzzer_note_t s_notes[BUZZER_SEQUENCE_MAX];
static uint8_t s_count, s_index;
static bool s_running, s_ready;
static uint32_t s_generation;
static uint64_t s_deadline_us;

static void silence_locked(void)
{
    /* gpio_set_direction(OUTPUT) would disconnect the peripheral in IDF 5.5.
     * Force both generator outputs LOW without touching GPIO matrix routing. */
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(s_positive, 0, true));
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(s_negative, 0, true));
    s_running = false;
}

static void invalidate_locked(void)
{
    esp_timer_stop(s_sequence_timer);
    s_count = s_index = 0;
    s_deadline_us = 0;
    if (!++s_generation) ++s_generation;
}

static void output_locked(uint16_t frequency_hz)
{
    if (!frequency_hz || !g_config.buzzer_enabled) {
        silence_locked();
        return;
    }
    if (frequency_hz < BUZZER_MIN_FREQUENCY_HZ) frequency_hz = BUZZER_MIN_FREQUENCY_HZ;
    if (frequency_hz > BUZZER_MAX_FREQUENCY_HZ) frequency_hz = BUZZER_MAX_FREQUENCY_HZ;
    uint32_t period = TIMER_RESOLUTION_HZ / frequency_hz;
    ESP_ERROR_CHECK(mcpwm_timer_set_period(s_pwm, period));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(s_compare, period / 2));
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(s_positive, -1, true));
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(s_negative, -1, true));
    s_running = true;
}

static bool play_note_locked(void)
{
    if (s_index >= s_count || !g_config.buzzer_enabled) {
        invalidate_locked();
        silence_locked();
        return false;
    }
    const buzzer_note_t *note = &s_notes[s_index];
    output_locked(note->frequency_hz);
    uint64_t delay_us = (uint64_t)note->duration_ms * 1000;
    s_deadline_us = (uint64_t)esp_timer_get_time() + delay_us;
    if (esp_timer_start_once(s_sequence_timer, delay_us) != ESP_OK) {
        invalidate_locked();
        silence_locked();
        return false;
    }
    return true;
}

static void sequence_callback(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    /* stop() cannot preempt a callback which was already dispatched. Such a
     * callback may advance only the currently due sequence, never an old note. */
    if (s_count && (uint64_t)esp_timer_get_time() >= s_deadline_us) {
        esp_timer_stop(s_sequence_timer);
        ++s_index;
        play_note_locked();
    }
    xSemaphoreGive(s_lock);
}

void buzzer_init(void)
{
    if (s_ready) return;
    s_lock = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(s_lock ? ESP_OK : ESP_ERR_NO_MEM);
    const mcpwm_timer_config_t timer_cfg = {
        .group_id = 0, .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = TIMER_RESOLUTION_HZ,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = TIMER_RESOLUTION_HZ / MORSE_FREQUENCY_HZ,
        .flags.update_period_on_empty = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_cfg, &s_pwm));
    const mcpwm_operator_config_t oper_cfg = {.group_id = 0};
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_cfg, &s_oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(s_oper, s_pwm));
    const mcpwm_comparator_config_t compare_cfg = {.flags.update_cmp_on_tez = true};
    ESP_ERROR_CHECK(mcpwm_new_comparator(s_oper, &compare_cfg, &s_compare));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(s_compare,
                                                     TIMER_RESOLUTION_HZ / MORSE_FREQUENCY_HZ / 2));
    const mcpwm_generator_config_t positive_cfg = {.gen_gpio_num = AUDIO_P};
    const mcpwm_generator_config_t negative_cfg = {.gen_gpio_num = AUDIO_N};
    ESP_ERROR_CHECK(mcpwm_new_generator(s_oper, &positive_cfg, &s_positive));
    ESP_ERROR_CHECK(mcpwm_new_generator(s_oper, &negative_cfg, &s_negative));
    silence_locked();
    /* One comparator, complementary 50% duty; no GPIO inversion, so LOW/LOW
     * is also an unambiguous zero-voltage idle state across the piezo. */
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(s_positive,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(s_positive,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, s_compare, MCPWM_GEN_ACTION_LOW)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(s_negative,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_LOW)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(s_negative,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, s_compare, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_timer_enable(s_pwm));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(s_pwm, MCPWM_TIMER_START_NO_STOP));
    const esp_timer_create_args_t args = {.callback = sequence_callback, .name = "piezo_sequence"};
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_sequence_timer));
    s_ready = true;
    ESP_LOGI(TAG, "Differential piezo ready, 2-6 kHz, timer-owned sequences");
}

bool buzzer_sequence_start(const buzzer_note_t *notes, uint8_t count, uint32_t *token)
{
    if (!s_ready || !notes || !count || count > BUZZER_SEQUENCE_MAX) return false;
    for (unsigned i = 0; i < count; ++i)
        if (!notes[i].duration_ms) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    invalidate_locked();
    memcpy(s_notes, notes, count * sizeof(*notes));
    s_count = count;
    bool ok = play_note_locked();
    if (token) *token = ok ? s_generation : 0;
    xSemaphoreGive(s_lock);
    return ok;
}

bool buzzer_sequence_is_active(uint32_t token)
{
    if (!s_ready || !token) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool active = s_count && token == s_generation;
    xSemaphoreGive(s_lock);
    return active;
}

void buzzer_sequence_cancel(uint32_t token)
{
    if (!s_ready || !token) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_count && token == s_generation) {
        invalidate_locked();
        silence_locked();
    }
    xSemaphoreGive(s_lock);
}

void buzzer_beep(uint16_t duration_ms) { buzzer_tone(MORSE_FREQUENCY_HZ, duration_ms); }

void buzzer_tone(uint16_t frequency_hz, uint16_t duration_ms)
{
    if (!frequency_hz) { buzzer_stop(); return; }
    buzzer_note_t note = {frequency_hz, duration_ms < MIN_DURATION_MS ? MIN_DURATION_MS : duration_ms};
    buzzer_sequence_start(&note, 1, NULL);
}

void buzzer_start(void)
{
    if (!s_ready) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    invalidate_locked();
    output_locked(MORSE_FREQUENCY_HZ);
    xSemaphoreGive(s_lock);
}

void buzzer_stop(void)
{
    if (!s_ready) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    invalidate_locked();
    silence_locked();
    xSemaphoreGive(s_lock);
}

bool buzzer_is_playing(void)
{
    if (!s_ready) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool playing = s_running;
    xSemaphoreGive(s_lock);
    return playing;
}
