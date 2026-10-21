/* Shared physical/remote key input. Observer only queues timestamped edges. */
#include "morse_input.h"
#include "ec06.h"
#include "nvs_config.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
typedef struct { bool pressed; uint64_t timestamp_us; } edge_t;
typedef struct { morse_input_event_t event; uint32_t duration; } output_t;
static QueueHandle_t s_edges;
static morse_keyer_t s_keyer;
static output_t s_output[192];
static unsigned s_read, s_write;
static uint32_t s_duration, s_overflows;
static volatile bool s_edge_overflow;
static void on_key(bool pressed, int64_t timestamp, void *context)
{
    (void)context;
    edge_t edge = { pressed, (uint64_t)timestamp };
    if (s_edges && xQueueSend(s_edges, &edge, 0) != pdTRUE) s_edge_overflow = true;
}
static void on_event(morse_input_event_t event, uint32_t duration, void *context)
{
    (void)context;
    unsigned next = (s_write + 1) % 192;
    if (next == s_read) { ++s_overflows; return; }
    s_output[s_write] = (output_t){ event, duration };
    s_write = next;
}
void morse_input_init(uint8_t gpio)
{
    (void)gpio;
    if (!s_edges) s_edges = xQueueCreate(64, sizeof(edge_t));
    morse_keyer_init(&s_keyer, g_config.morse_dot_ms ? g_config.morse_dot_ms : 200,
                     ec06_encoder_is_held(), on_event, NULL);
    ec06_set_raw_encoder_observer(on_key, NULL);
}
void morse_input_reset(void)
{
    if (s_edges) xQueueReset(s_edges);
    s_read = s_write = 0;
    s_edge_overflow = false;
    s_duration = 0;
    morse_keyer_init(&s_keyer, s_keyer.dot_ms ? s_keyer.dot_ms : 200,
                     ec06_encoder_is_held(), on_event, NULL);
}
morse_input_event_t morse_input_poll(void)
{
    if (s_read == s_write) {
        if (s_edge_overflow) {
            ++s_overflows;
            ESP_LOGW("morse_input", "Edge queue overflow; discarding incomplete character");
            morse_input_reset();
            return MORSE_IN_WORD_GAP;
        }
        edge_t edge;
        while (s_edges && xQueueReceive(s_edges, &edge, 0) == pdTRUE)
            morse_keyer_edge(&s_keyer, edge.pressed, edge.timestamp_us);
        morse_keyer_advance(&s_keyer, (uint64_t)esp_timer_get_time());
    }
    if (s_read == s_write) return MORSE_IN_NONE;
    output_t output = s_output[s_read];
    s_read = (s_read + 1) % 192;
    s_duration = output.duration;
    return output.event;
}
uint32_t morse_input_get_duration_ms(void) { return s_duration; }
void morse_input_set_dot_ms(uint32_t ms) { morse_keyer_set_speed(&s_keyer, ms); }
uint32_t morse_input_get_dot_ms(void) { return s_keyer.dot_ms ? s_keyer.dot_ms : 200; }
bool morse_input_is_held(void) { return s_keyer.pressed; }
uint32_t morse_input_get_overflows(void) { return s_overflows; }