/** EC06: ISR/USB -> one timestamped queue -> raw observer + debounced UI events. */
#include "ec06.h"
#include "pins.h"
#include "power_mgmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_attr.h"

enum { RAW_ROTARY, RAW_BUTTON, RAW_REMOTE };
typedef struct {
    int64_t us;
    uint32_t token, generation;
    int16_t value;
    uint8_t kind, id;
} raw_input_t;
typedef struct {
    bool physical, remote, raw, stable, long_sent, last_short, startup;
    int64_t edge_us, down_us, last_release_us;
    uint32_t last_duration_ms;
} button_t;

static ec06_config_t s_cfg;
static QueueHandle_t s_raw_queue, s_events;
static button_t s_buttons[2];
static uint8_t s_rotary_state;
static int8_t s_rotary_accum;
static int32_t s_count;
static ec06_raw_encoder_observer_t s_raw_observer;
static void *s_raw_ctx;
static ec06_remote_result_t s_result_observer;
static void *s_result_ctx;
static volatile uint32_t s_generation;
static volatile bool s_release_pending, s_resync;
static int64_t s_lease_us;
static uint32_t s_input_drops, s_event_drops;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static bool emit(ec06_event_type_t type, int64_t us, uint32_t duration)
{
    ec06_event_t event = { .type = type, .timestamp_us = (uint64_t)us,
                          .duration_ms = duration };
    if (xQueueSend(s_events, &event, 0) != pdTRUE) {
        ++s_event_drops;
        return false;
    }
    power_mgmt_notify_activity();
    return true;
}

static void advance_buttons(int64_t now)
{
    for (unsigned id = 0; id < 2; ++id) {
        button_t *b = &s_buttons[id];
        int64_t debounce = (int64_t)(id ? s_cfg.key_debounce_ms : s_cfg.debounce_ms) * 1000;
        if (b->raw != b->stable && now - b->edge_us >= debounce) {
            b->stable = b->raw;
            if (b->stable) {
                b->down_us = b->edge_us;
                b->long_sent = false;
                b->startup = false;
                emit(id ? EC06_EVENT_KEY_DOWN : EC06_EVENT_ENC_DOWN, b->edge_us, 0);
            } else {
                uint32_t dur = (uint32_t)((b->edge_us - b->down_us) / 1000);
                b->last_duration_ms = dur;
                emit(id ? EC06_EVENT_KEY_UP : EC06_EVENT_ENC_UP, b->edge_us, dur);
                if (!id && !b->startup && !b->long_sent && dur >= s_cfg.short_press_ms) {
                    int64_t gap = b->down_us - b->last_release_us;
                    if (b->last_short && gap > (int64_t)s_cfg.debounce_ms * 1000 &&
                        gap < (int64_t)s_cfg.double_click_ms * 1000) {
                        emit(EC06_EVENT_DOUBLE_CLICK, b->edge_us, dur);
                        b->last_short = false;
                    } else {
                        emit(EC06_EVENT_SHORT_PRESS, b->edge_us, dur);
                        b->last_short = true;
                    }
                } else b->last_short = false;
                b->last_release_us = b->edge_us;
                b->startup = false;
            }
        }
        if (!id && b->stable && b->raw && !b->startup && !b->long_sent &&
            now - b->down_us >= (int64_t)s_cfg.long_press_ms * 1000) {
            emit(EC06_EVENT_LONG_PRESS, b->down_us + (int64_t)s_cfg.long_press_ms * 1000,
                 s_cfg.long_press_ms);
            b->long_sent = true;
            b->last_short = false;
        }
    }
}

static void button_edge(unsigned id, bool pressed, bool remote, int64_t us)
{
    button_t *b = &s_buttons[id];
    if (remote) b->remote = pressed;
    else b->physical = pressed;
    bool combined = b->physical || b->remote;
    if (combined == b->raw) return;
    b->raw = combined;
    b->edge_us = us;
    if (!id && s_raw_observer) s_raw_observer(combined, us, s_raw_ctx);
    power_mgmt_notify_activity();
}

static bool rotate(int steps, int64_t us)
{
    if (uxQueueSpacesAvailable(s_events) < (UBaseType_t)(steps < 0 ? -steps : steps)) {
        ++s_event_drops;
        return false;
    }
    while (steps) {
        int direction = steps > 0 ? 1 : -1;
        if (emit(direction > 0 ? EC06_EVENT_CW : EC06_EVENT_CCW, us, 0)) {
            portENTER_CRITICAL(&s_lock);
            s_count += direction;
            portEXIT_CRITICAL(&s_lock);
        }
        steps -= direction;
    }
    return true;
}

static void IRAM_ATTR isr_input(void *arg)
{
    uintptr_t id = (uintptr_t)arg;
    raw_input_t event = { .us = esp_timer_get_time() };
    if (id == 2) {
        event.kind = RAW_ROTARY;
        event.value = (gpio_get_level(s_cfg.gpio_a) << 1) | gpio_get_level(s_cfg.gpio_b);
    } else {
        event.kind = RAW_BUTTON;
        event.id = id;
        event.value = gpio_get_level(id ? s_cfg.gpio_key : s_cfg.gpio_btn) == 0;
    }
    BaseType_t wake = pdFALSE;
    if (xQueueSendFromISR(s_raw_queue, &event, &wake) != pdTRUE) {
        ++s_input_drops;
        s_resync = true;
    }
    if (wake) portYIELD_FROM_ISR();
}

static void input_task(void *arg)
{
    (void)arg;
    raw_input_t event;
    int64_t last_us = 0;
    for (;;) {
        portENTER_CRITICAL(&s_lock);
        bool expired = s_lease_us && esp_timer_get_time() - s_lease_us > 1500000;
        if (expired) s_lease_us = 0;
        portEXIT_CRITICAL(&s_lock);
        if (expired) ec06_remote_release_all();
        if (xQueueReceive(s_raw_queue, &event, 1) == pdTRUE) {
            /* Producers may be preempted before enqueue; never move the clock backwards. */
            if (event.us < last_us) event.us = last_us;
            last_us = event.us;
            advance_buttons(event.us);
            if (event.kind == RAW_BUTTON) button_edge(event.id, event.value != 0, false, event.us);
            else if (event.kind == RAW_ROTARY) {
                uint8_t prev = s_rotary_state, next = (uint8_t)event.value;
                s_rotary_state = next;
                /* Gray-code reversals cancel bounce; invalid two-bit jumps reset.
                   Two transitions per step retain the previous half-step resolution. */
                static const int8_t transitions[16] = {0,1,-1,0,-1,0,0,1,1,0,0,-1,0,-1,1,0};
                int direction = transitions[(prev << 2) | next];
                if (!direction && prev != next) s_rotary_accum = 0;
                else s_rotary_accum += direction;
                if ((next == 0 || next == 3) && (s_rotary_accum >= 2 || s_rotary_accum <= -2)) {
                    int step = s_rotary_accum > 0 ? 1 : -1;
                    rotate(s_cfg.invert_dir ? -step : step, event.us);
                    s_rotary_accum = 0;
                }
            } else {
                bool applied = event.generation == s_generation;
                if (applied) {
                    switch (event.id) {
                    case EC06_REMOTE_ROTATE: applied = rotate(event.value, event.us); break;
                    case EC06_REMOTE_ENCODER_DOWN: button_edge(0, true, true, event.us); break;
                    case EC06_REMOTE_ENCODER_UP: button_edge(0, false, true, event.us); break;
                    case EC06_REMOTE_BACK_DOWN: button_edge(1, true, true, event.us); break;
                    case EC06_REMOTE_BACK_UP: button_edge(1, false, true, event.us); break;
                    case EC06_REMOTE_RELEASE_ALL:
                        button_edge(0, false, true, event.us);
                        button_edge(1, false, true, event.us);
                        break;
                    default: applied = false; break;
                    }
                }
                if (s_result_observer) s_result_observer(event.token, applied, s_result_ctx);
            }
        } else advance_buttons(esp_timer_get_time());
        /* Drain older recorded edges before generating a watchdog release. */
        if (s_release_pending && uxQueueMessagesWaiting(s_raw_queue) == 0) {
            s_release_pending = false;
            int64_t now = esp_timer_get_time();
            advance_buttons(now);
            button_edge(0, false, true, now);
            button_edge(1, false, true, now);
            last_us = now;
        }
        if (s_resync && uxQueueMessagesWaiting(s_raw_queue) == 0) {
            s_resync = false;
            int64_t now = esp_timer_get_time();
            button_edge(0, gpio_get_level(s_cfg.gpio_btn) == 0, false, now);
            button_edge(1, gpio_get_level(s_cfg.gpio_key) == 0, false, now);
        }
    }
}

void ec06_init(const ec06_config_t *cfg)
{
    if (s_raw_queue) return;
    ec06_config_t defaults = EC06_CONFIG_DEFAULT;
    s_cfg = cfg ? *cfg : defaults;
    s_raw_queue = xQueueCreate(128, sizeof(raw_input_t));
    s_events = xQueueCreate(64, sizeof(ec06_event_t));
    if (!s_raw_queue || !s_events) {
        ESP_LOGE("ec06", "input queues unavailable");
        return;
    }
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << s_cfg.gpio_a) | (1ULL << s_cfg.gpio_b) |
                        (1ULL << s_cfg.gpio_btn) | (1ULL << s_cfg.gpio_key),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = INPUT_PULLUP_ENABLE ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    s_rotary_state = (gpio_get_level(s_cfg.gpio_a) << 1) | gpio_get_level(s_cfg.gpio_b);
    for (unsigned id = 0; id < 2; ++id) {
        button_t *b = &s_buttons[id];
        b->physical = b->raw = b->stable = gpio_get_level(id ? s_cfg.gpio_key : s_cfg.gpio_btn) == 0;
        b->down_us = b->edge_us = esp_timer_get_time();
        b->startup = b->stable;
    }
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(gpio_isr_handler_add(s_cfg.gpio_a, isr_input, (void *)2));
    ESP_ERROR_CHECK(gpio_isr_handler_add(s_cfg.gpio_b, isr_input, (void *)2));
    ESP_ERROR_CHECK(gpio_isr_handler_add(s_cfg.gpio_btn, isr_input, (void *)0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(s_cfg.gpio_key, isr_input, (void *)1));
    xTaskCreate(input_task, "ec06", 3584, NULL, 5, NULL);
}

ec06_event_t ec06_pop_event(void)
{
    ec06_event_t event = {0};
    if (s_events) xQueueReceive(s_events, &event, 0);
    return event;
}
int32_t ec06_get_count(void)
{
    portENTER_CRITICAL(&s_lock);
    int32_t value = s_count;
    s_count = 0;
    portEXIT_CRITICAL(&s_lock);
    return value;
}
uint32_t ec06_get_key_duration_ms(void)
{
    return s_buttons[1].stable ? (uint32_t)((esp_timer_get_time() - s_buttons[1].down_us) / 1000)
                               : s_buttons[1].last_duration_ms;
}
bool ec06_is_key_held(void) { return s_buttons[1].stable; }
bool ec06_encoder_is_held(void) { return s_buttons[0].raw; }
void ec06_set_press_thresholds(uint32_t short_ms, uint32_t long_ms)
{
    s_cfg.short_press_ms = short_ms ? short_ms : 1;
    s_cfg.long_press_ms = long_ms > s_cfg.short_press_ms ? long_ms : s_cfg.short_press_ms + 1;
}
void ec06_set_raw_encoder_observer(ec06_raw_encoder_observer_t observer, void *ctx)
{
    s_raw_ctx = ctx;
    s_raw_observer = observer;
}
void ec06_set_remote_result_observer(ec06_remote_result_t observer, void *ctx)
{
    s_result_ctx = ctx;
    s_result_observer = observer;
}
bool ec06_remote_input(ec06_remote_action_t action, int16_t value, uint32_t token)
{
    if (!s_raw_queue || action < 1 || action > EC06_REMOTE_RELEASE_ALL ||
        (action == EC06_REMOTE_ROTATE && (value == 0 || value < -32 || value > 32))) return false;
    /* On ESP32-C6 this also prevents a GPIO ISR from overtaking the timestamp
       before enqueue, so physical and remote edges retain one chronological chain. */
    portENTER_CRITICAL(&s_lock);
    raw_input_t event = { .kind = RAW_REMOTE, .id = action, .value = value,
        .token = token, .us = esp_timer_get_time(), .generation = s_generation };
    bool queued = xQueueSend(s_raw_queue, &event, 0) == pdTRUE;
    portEXIT_CRITICAL(&s_lock);
    return queued;
}
void ec06_remote_release_all(void)
{
    portENTER_CRITICAL(&s_lock);
    ++s_generation;
    s_release_pending = true;
    portEXIT_CRITICAL(&s_lock);
}
void ec06_remote_keepalive(void)
{
    portENTER_CRITICAL(&s_lock);
    s_lease_us = esp_timer_get_time();
    portEXIT_CRITICAL(&s_lock);
}
void ec06_dump_stats(void)
{
    ESP_LOGI("ec06", "input_drops=%lu event_drops=%lu", (unsigned long)s_input_drops,
             (unsigned long)s_event_drops);
}
