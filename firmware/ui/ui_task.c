/* UI owns OLED I2C and standby transitions; radio runs independently. */
#include "ui_task.h"
#include "wououi.h"
#include "local_pages.h"
#include "pet_service.h"
#include "pet_notice.h"
#include "ec06.h"
#include "power_mgmt.h"
#include "mesh_notify.h"
#include "rgb_led.h"
#include "morse_input.h"
#include "device_control.h"
#include "oled_mirror.h"
#include "u8g2_esp32_hal.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void ui_task_fn(void *arg)
{
    (void)arg;
    uint32_t last_rgb_ms = 0;
    uint32_t frames = 0, fps_x10 = 0, measured_frames = 0;
    uint32_t render_us = 0, display_us = 0;
    int64_t measured_at = esp_timer_get_time();
    TickType_t next_frame = xTaskGetTickCount();
    while (1) {
        device_control_set_performance(frames,fps_x10,render_us,display_us,
            (power_mgmt_is_screen_off()?1:0) | (power_mgmt_auto_standby_enabled()?2:0));
        device_control_set_ui_state(ui.index);
        device_control_tick();
        pet_service_tick();
        power_mgmt_tick();
        if (g_power_off_in_progress) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        /* Storage/receipts are already handled by message_service's owner task. */
        mesh_notify_process();
        pet_notice_tick();
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        if (now_ms - last_rgb_ms >= 1000) {
            last_rgb_ms = now_ms;
            rgb_led_unread_tick();
        }
        if (power_mgmt_is_screen_off()) {
            next_frame = xTaskGetTickCount();
            fps_x10 = 0;
            measured_at = esp_timer_get_time();
            measured_frames = frames;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        /* Clean up the previous page before interpreting its release as a menu click. */
        local_pages_sync_page();
        bool guard = power_mgmt_input_guard();
        if (guard) {
            while (ec06_pop_event().type != EC06_EVENT_NONE) {}
            morse_input_reset();
            btn.pressed = false;
        } else if (!local_pages_uses_raw_input(ui.index)) {
            ec06_to_btn();
        }
        int64_t frame_start = esp_timer_get_time();
        u8g2_ClearBuffer(&u8g2);
        if (!guard) ui_proc();
        else local_pages_render_waking();
        if(!guard)pet_notice_draw();
        int64_t rendered_at = esp_timer_get_time();
        render_us = (uint32_t)(rendered_at - frame_start);
        if (oled_is_available()) u8g2_SendBuffer(&u8g2);
        display_us = (uint32_t)(esp_timer_get_time() - rendered_at);
        oled_mirror_send_frame();
        ++frames;
        int64_t finished_at = esp_timer_get_time();
        if (finished_at - measured_at >= 1000000) {
            fps_x10 = (uint32_t)((uint64_t)(frames-measured_frames)*10000000 /
                                (finished_at-measured_at));
            measured_frames = frames;
            measured_at = finished_at;
        }
        device_control_set_ui_state(ui.index);
        if (esp_timer_get_time() - frame_start > 100000)
            ESP_LOGW("ui_task", "Slow frame: %lld us", esp_timer_get_time() - frame_start);
        /* 50 Hz includes rendering/I2C time; do not add a full sleep after it. */
        vTaskDelayUntil(&next_frame, pdMS_TO_TICKS(20));
    }
}

void ui_task_create(void)
{
    if (xTaskCreate(ui_task_fn, "ui_task", 6144, NULL, 5, NULL) != pdPASS)
        ESP_LOGE("ui_task", "Could not create UI task");
}
