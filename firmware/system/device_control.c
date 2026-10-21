/** USB control v2; old image/log envelope stays byte-for-byte compatible. */
#include "device_control.h"
#include "oled_mirror.h"
#include "ec06.h"
#include "rmt_ws2812.h"
#include "power_mgmt.h"
#include "haptic_feedback.h"
#include "driver/usb_serial_jtag.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>

#if defined(ENABLE_OLED_MIRROR) && ENABLE_OLED_MIRROR
typedef struct { uint32_t id; bool applied; } result_t;
static QueueHandle_t s_results;
static uint8_t s_rx[128];
static size_t s_used;
static uint16_t s_page;
static uint32_t s_ui_frames, s_fps_x10, s_render_us, s_display_us;
static uint8_t s_power_flags;
static int64_t s_last_rx, s_hello_at, s_telemetry_at;
static bool s_leased;
/* Duplicate IDs are acknowledged, never replayed. Cache covers serial retries. */
static uint32_t s_ids[32];
static uint8_t s_states[32], s_slot;

static uint16_t crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xffff;
    while (len--) {
        crc ^= (uint16_t)*data++ << 8;
        for (int bit = 0; bit < 8; ++bit) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}
static void send_json(uint8_t type, const char *json)
{
    uint8_t data[384];
    size_t len = strlen(json);
    if (len > sizeof(data) - 2) return;
    memcpy(data, json, len);
    uint16_t crc = crc16(data, len);
    data[len] = crc; data[len + 1] = crc >> 8;
    oled_mirror_send_packet(type, data, len + 2);
}
static void ack(uint32_t id, const char *status)
{
    char json[160];
    snprintf(json, sizeof(json), "{\"id\":%lu,\"status\":\"%s\",\"device_us\":%llu,\"frame\":%lu}",
             (unsigned long)id, status, (unsigned long long)esp_timer_get_time(),
             (unsigned long)oled_mirror_frame_count());
    send_json(OLED_MIRROR_FRAME_ACK, json);
}
static void hello(void)
{
    send_json(OLED_MIRROR_FRAME_HELLO,
        "{\"protocol\":2,\"width\":128,\"height\":32,\"control\":true,"
        "\"lease_ms\":1500,\"capabilities\":[\"rotate\",\"encoder\",\"back\",\"rgb\",\"ack\",\"power\",\"sound\"]}");
}
static void result_observer(uint32_t token, bool applied, void *ctx)
{
    (void)ctx;
    result_t result = { .id = token, .applied = applied };
    /* No USB I/O in input task. Bounded queue; a missing ACK is reported as timeout by PC. */
    if (s_results) xQueueSend(s_results, &result, 0);
}
static void command(const uint8_t *data)
{
    uint16_t supplied = data[8] | ((uint16_t)data[9] << 8);
    if (data[0] != 2 || supplied != crc16(data, 8)) return;
    uint8_t op = data[1];
    uint32_t id = data[2] | ((uint32_t)data[3] << 8) | ((uint32_t)data[4] << 16) |
                  ((uint32_t)data[5] << 24);
    int16_t value = (int16_t)(data[6] | ((uint16_t)data[7] << 8));
    if (!id) return;
    if (op < 1 || op > 10 || (op == 1 && (value == 0 || value < -32 || value > 32)) ||
        (op == 9 && (value < 0 || value > 3)) ||
        (op == 10 && (value < 0 || value >= HAPTIC_COUNT))) {
        ack(id, "invalid"); return;
    }
    s_last_rx = esp_timer_get_time();
    s_leased = true;
    ec06_remote_keepalive();
    if (op == 7) { ack(id, "executed"); return; } /* heartbeat */
    if (op == 8) { hello(); ack(id, "executed"); return; }
    /* Keep the v2 command shape, but never allow old clients to enable auto-off. */
    if (op == 9 && value == 2) { ack(id, "unsupported"); return; }
    for (int i = 0; i < 32; ++i) {
        if (s_ids[i] == id) {
            ack(id, s_states[i] == 2 ? "executed" : s_states[i] == 3 ? "cancelled" : "queued");
            return;
        }
    }
    if (op == 9 || op == 10) {
        if (op == 10) haptic_play((haptic_pattern_t)value);
        else if (value == 0) power_mgmt_notify_activity();
        else if (value == 1) power_mgmt_request_screen_off();
        /* value 3 (legacy auto_off) is an already-satisfied no-op. */
        s_ids[s_slot] = id; s_states[s_slot] = 2; s_slot = (s_slot + 1) % 32;
        ack(id, "executed");
        return;
    }
    if (op < 1 || op > 6 || (op == 1 && (value == 0 || value < -32 || value > 32))) {
        ack(id, "invalid"); return;
    }
    if (!ec06_remote_input((ec06_remote_action_t)op, value, id)) {
        ack(id, "busy"); return;
    }
    s_ids[s_slot] = id; s_states[s_slot] = 1; s_slot = (s_slot + 1) % 32;
    ack(id, "queued");
}
void device_control_init(void)
{
    if (!usb_serial_jtag_is_driver_installed()) return;
    if (s_results) return;
    s_results = xQueueCreate(64, sizeof(result_t));
    if (!s_results) return;
    ec06_set_remote_result_observer(result_observer, NULL);
    ec06_remote_release_all();
    if (usb_serial_jtag_is_connected()) hello();
}
void device_control_set_ui_state(uint16_t page) { s_page = page; }
void device_control_set_performance(uint32_t frames, uint32_t fps_x10,
                                    uint32_t render_us, uint32_t display_us,
                                    uint8_t power_flags)
{
    s_ui_frames=frames; s_fps_x10=fps_x10;
    s_render_us=render_us; s_display_us=display_us; s_power_flags=power_flags;
}
void device_control_tick(void)
{
    if (!s_results) return;
    if (!usb_serial_jtag_is_driver_installed() || !usb_serial_jtag_is_connected()) {
        if (s_leased) { s_leased = false; ec06_remote_release_all(); }
        s_used = 0;
        return;
    }
    uint8_t incoming[64];
    int count = usb_serial_jtag_read_bytes(incoming, sizeof(incoming), 0);
    for (int i = 0; i < count; ++i) {
        if (s_used == sizeof(s_rx)) { memmove(s_rx, s_rx + 1, --s_used); }
        s_rx[s_used++] = incoming[i];
        while (s_used >= 5) {
            if (s_rx[0] != 0xaa || s_rx[1] != 0x55 || s_rx[2] != OLED_MIRROR_FRAME_COMMAND ||
                s_rx[3] != 10 || s_rx[4] != 0) {
                memmove(s_rx, s_rx + 1, --s_used); continue;
            }
            if (s_used < 15) break;
            if (s_rx[5] != 2 || crc16(s_rx + 5, 8) != (uint16_t)(s_rx[13] | ((uint16_t)s_rx[14] << 8))) {
                memmove(s_rx, s_rx + 1, --s_used); continue;
            }
            command(s_rx + 5);
            s_used -= 15;
            memmove(s_rx, s_rx + 15, s_used);
        }
    }
    result_t result;
    while (xQueueReceive(s_results, &result, 0) == pdTRUE) {
        for (int i = 0; i < 32; ++i)
            if (s_ids[i] == result.id) s_states[i] = result.applied ? 2 : 3;
        ack(result.id, result.applied ? "executed" : "cancelled");
    }
    int64_t now = esp_timer_get_time();
    if (s_leased && now - s_last_rx > 1500000) {
        s_leased = false;
        ec06_remote_release_all();
    }
    if (now - s_hello_at >= 2000000) { hello(); s_hello_at = now; }
    if (now - s_telemetry_at >= 100000) {
        uint8_t r, g, b;
        rgb_get_output(&r, &g, &b);
        char json[384];
        snprintf(json, sizeof(json),
            "{\"uptime_ms\":%llu,\"page\":%u,\"frame\":%lu,\"rgb\":[%u,%u,%u],"
            "\"encoder\":%s,\"back\":%s,\"remote_lease\":%s,"
            "\"ui_frames\":%lu,\"ui_fps_x10\":%lu,\"render_us\":%lu,\"display_us\":%lu,"
            "\"screen_off\":%s,\"auto_standby\":%s}",
            (unsigned long long)(now / 1000), s_page, (unsigned long)oled_mirror_frame_count(),
            r, g, b, ec06_encoder_is_held() ? "true" : "false",
            ec06_is_key_held() ? "true" : "false", s_leased ? "true" : "false",
            (unsigned long)s_ui_frames, (unsigned long)s_fps_x10,
            (unsigned long)s_render_us, (unsigned long)s_display_us,
            s_power_flags&1 ? "true":"false", "false");
        send_json(OLED_MIRROR_FRAME_TELEMETRY, json);
        s_telemetry_at = now;
    }
}
#else
void device_control_init(void) {}
void device_control_tick(void) {}
void device_control_set_ui_state(uint16_t page) { (void)page; }
void device_control_set_performance(uint32_t frames,uint32_t fps,uint32_t render,
                                    uint32_t display,uint8_t flags)
{ (void)frames;(void)fps;(void)render;(void)display;(void)flags; }
#endif
