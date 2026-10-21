#include "oled_mirror.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_timer.h"

#if defined(ENABLE_OLED_MIRROR) && ENABLE_OLED_MIRROR
typedef struct { uint16_t len; uint8_t data[5 + OLED_MIRROR_MAX_PAYLOAD]; } packet_t;
typedef struct { uint16_t len; uint8_t data[260]; } log_packet_t;
static u8g2_t *s_display;
static oled_mirror_send_t s_send;
static SemaphoreHandle_t s_lock;
static QueueHandle_t s_controls, s_logs;
static TaskHandle_t s_worker;
static uint32_t s_frames, s_replaced, s_dropped, s_failures;
static packet_t s_image, s_inflight, s_build;
static bool s_image_pending, s_inflight_pending, s_retrying;
static int64_t s_failed_since;
static unsigned s_control_burst;

static void packet_fill(packet_t *packet, uint8_t type, const uint8_t *data, size_t len)
{
    packet->len = (uint16_t)(len + 5);
    packet->data[0] = 0xaa; packet->data[1] = 0x55; packet->data[2] = type;
    packet->data[3] = (uint8_t)len; packet->data[4] = (uint8_t)(len >> 8);
    if (len) memcpy(packet->data + 5, data, len);
}

/* Only the TX owner calls USB. Callback false means zero bytes were accepted:
 * IDF USB Serial/JTAG's byte ring buffer accepts a whole write or returns 0. */
static bool transmit_once(void)
{
    if (!s_inflight_pending) {
        if (s_control_burst < 4 && xQueueReceive(s_controls, &s_inflight, 0) == pdTRUE) {
            ++s_control_burst;
            s_inflight_pending = true;
        } else if (xSemaphoreTake(s_lock, 0) == pdTRUE) {
            if (s_image_pending) {
                s_inflight = s_image;
                s_image_pending = false;
                s_inflight_pending = true;
                s_control_burst = 0;
            }
            xSemaphoreGive(s_lock);
        }
        if (!s_inflight_pending && xQueueReceive(s_controls, &s_inflight, 0) == pdTRUE) {
            s_inflight_pending = true;
            s_control_burst = 0;
        }
        if (!s_inflight_pending && s_logs) {
            log_packet_t log;
            if (xQueueReceive(s_logs, &log, 0) == pdTRUE) {
                s_inflight.len = log.len;
                memcpy(s_inflight.data, log.data, log.len);
                s_inflight_pending = true;
                s_control_burst = 0;
            }
        }
    }
    if (!s_inflight_pending) return false;
    if (s_send(s_inflight.data, s_inflight.len)) {
        if (s_inflight.data[2] == OLED_MIRROR_FRAME_IMAGE) ++s_frames;
        s_inflight_pending = s_retrying = false;
        return true;
    }
    ++s_failures;
    int64_t now = esp_timer_get_time();
    if (!s_retrying) { s_retrying = true; s_failed_since = now; }
    if (now - s_failed_since >= 100000) {
        /* Bound stale data after a closed COM port; preserve the newest screen. */
        s_dropped += 1 + uxQueueMessagesWaiting(s_controls);
        xQueueReset(s_controls);
        if (s_logs) { s_dropped += uxQueueMessagesWaiting(s_logs); xQueueReset(s_logs); }
        s_inflight_pending = s_retrying = false;
    }
    return false;
}

static void mirror_tx_task(void *arg)
{
    (void)arg;
    while (1) {
        if (!transmit_once()) ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
    }
}

void oled_mirror_begin(u8g2_t *u8g2, oled_mirror_send_t send_fn)
{
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_controls) s_controls = xQueueCreate(16, sizeof(packet_t));
    if (!s_logs) s_logs = xQueueCreate(8, sizeof(log_packet_t));
    s_display = u8g2;
    s_send = send_fn;
    if (s_lock && s_controls && send_fn && !s_worker)
        xTaskCreate(mirror_tx_task, "mirror_tx", 3072, NULL, 2, &s_worker);
}

bool oled_mirror_send_packet(uint8_t type, const uint8_t *data, size_t len)
{
    if (!s_send || !s_worker || (!data && len) || len > OLED_MIRROR_MAX_PAYLOAD) return false;
    if (type == OLED_MIRROR_FRAME_IMAGE) {
        if (xSemaphoreTake(s_lock, 0) != pdTRUE) { ++s_dropped; return false; }
        if (s_image_pending) ++s_replaced;
        packet_fill(&s_image, type, data, len);
        s_image_pending = true;
        xSemaphoreGive(s_lock);
    } else if (type == OLED_MIRROR_FRAME_LOG) {
        if (!s_logs || len > 255) return false;
        log_packet_t log;
        log.len = (uint16_t)(len + 5);
        log.data[0] = 0xaa; log.data[1] = 0x55; log.data[2] = type;
        log.data[3] = (uint8_t)len; log.data[4] = 0;
        if (len) memcpy(log.data + 5, data, len);
        if (xQueueSend(s_logs, &log, 0) != pdTRUE) { ++s_dropped; return false; }
    } else {
        if (xSemaphoreTake(s_lock, 0) != pdTRUE) { ++s_dropped; return false; }
        packet_fill(&s_build, type, data, len);
        bool accepted = xQueueSend(s_controls, &s_build, 0) == pdTRUE;
        xSemaphoreGive(s_lock);
        if (!accepted) { ++s_dropped; return false; }
    }
    xTaskNotifyGive(s_worker);
    return true; /* Queue acceptance; only the TX owner counts successful USB frames. */
}

void oled_mirror_send_frame(void)
{
    if (!s_display) return;
    size_t len = (size_t)u8g2_GetBufferTileWidth(s_display) *
                 u8g2_GetBufferTileHeight(s_display) * 8;
    if (len) oled_mirror_send_packet(OLED_MIRROR_FRAME_IMAGE, u8g2_GetBufferPtr(s_display), len);
}
uint32_t oled_mirror_frame_count(void) { return s_frames; }
void oled_mirror_get_stats(oled_mirror_stats_t *stats)
{
    if (!stats) return;
    *stats = (oled_mirror_stats_t){ s_frames, s_replaced, s_dropped, s_failures,
        s_controls ? (uint16_t)uxQueueMessagesWaiting(s_controls) : 0 };
}
void oled_mirror_send_log(const char *msg)
{
    if (!msg) return;
    size_t len = 0;
    while (len < 255 && msg[len]) ++len;
    oled_mirror_send_packet(OLED_MIRROR_FRAME_LOG, (const uint8_t *)msg, len);
}
#endif
