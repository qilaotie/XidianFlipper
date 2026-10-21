#include "morse_playback.h"
#include "mcpwm_buzzer.h"
#include "haptic_feedback.h"
#include "nvs_config.h"
#include "rmt_ws2812.h"
#include "rgb_led.h"
#include "load_switch.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
static esp_timer_handle_t s_timer;
static SemaphoreHandle_t s_mutex;
static morse_stream_t s_stream;
static morse_segment_t s_segment;
static bool s_active;
static uint32_t s_dot_ms;
static uint64_t s_deadline_us;
static bool s_led_claimed;
static void release_led_locked(void)
{
    if (!s_led_claimed) return;
    rgb_off();
    load_switch_release(LOAD_SW_MORSE);
    s_led_claimed = false;
}
static void advance_locked(void)
{
    if (!s_active) return;
    if (!morse_stream_next(&s_stream, &s_segment)) {
        s_active = false; buzzer_stop(); release_led_locked(); haptic_set_morse_priority(false);
        memset(&s_segment, 0, sizeof(s_segment));
        return;
    }
    if (s_segment.tone) buzzer_start(); else buzzer_stop();
    if (s_led_claimed) {
        if (s_segment.tone && g_config.led_enabled) rgb_set_color(0, 100, 255);
        else rgb_off();
    }
    uint64_t duration = (uint64_t)s_segment.units * s_dot_ms * 1000;
    s_deadline_us = esp_timer_get_time() + duration;
    if (esp_timer_start_once(s_timer, duration) != ESP_OK) {
        s_active = false; buzzer_stop(); release_led_locked(); haptic_set_morse_priority(false);
    }
}
static void timer_callback(void *context)
{
    (void)context;
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        /* Ignore a callback already queued before a stop/restart. */
        if (s_active && (uint64_t)esp_timer_get_time() >= s_deadline_us) {
            /* A previously dispatched callback can run after restart. If the
             * new segment is already due, disarm it before advancing once. */
            esp_timer_stop(s_timer);
            advance_locked();
        }
        xSemaphoreGive(s_mutex);
    }
}
void morse_playback_init(void)
{
    if (s_timer) return;
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return;
    esp_timer_create_args_t args = { .callback = timer_callback, .name = "morse_audio" };
    if (esp_timer_create(&args, &s_timer) != ESP_OK) s_timer = NULL;
}
bool morse_playback_start(const char *text, uint16_t len, uint32_t dot_ms)
{
    if (!morse_codec_validate(text, len)) return false;
    morse_playback_init();
    if (!s_timer || !s_mutex) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_timer_stop(s_timer);
    release_led_locked();
    haptic_set_morse_priority(true);
    rgb_led_stop();
    if (g_config.led_enabled) {
        load_switch_acquire(LOAD_SW_MORSE);
        s_led_claimed = true;
    }
    morse_stream_init(&s_stream, text, len);
    s_dot_ms = dot_ms < 40 ? 40 : dot_ms > 300 ? 300 : dot_ms;
    s_active = true;
    advance_locked();
    bool ok = s_active;
    xSemaphoreGive(s_mutex);
    return ok;
}
void morse_playback_stop(void)
{
    if (!s_mutex) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_timer) esp_timer_stop(s_timer);
    if (s_active) buzzer_stop();
    s_active = false; release_led_locked(); haptic_set_morse_priority(false);
    memset(&s_segment, 0, sizeof(s_segment));
    xSemaphoreGive(s_mutex);
}
bool morse_playback_is_active(void)
{
    if (!s_mutex) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool active = s_active;
    xSemaphoreGive(s_mutex);
    return active;
}
void morse_playback_get_segment(morse_segment_t *out)
{
    if (!out) return;
    if (!s_mutex) { memset(out, 0, sizeof(*out)); return; }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_segment;
    xSemaphoreGive(s_mutex);
}
