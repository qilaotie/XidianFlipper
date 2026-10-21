/**
 * @file    dev_mode.c
 * @brief   开发者模式实现：阶段 0 硬件测试 + 阶段 1-6 诊断 + 系统诊断 — V0.10.0
 *
 * OLED 128×32 分块渲染规范：
 *   顶部 8px：标题行
 *   中部 16px：状态/数值
 *   底部 8px：操作提示
 */

#include "dev_mode.h"

#include "log_config.h"

#if LOG_DEV_MODE_ENABLE

#include "ec06.h"
#include "nvs_config.h"
#include "pins.h"
#include "constants.h"
#include "log_manager.h"
#include "haptic_feedback.h"
#include "rmt_ws2812.h"
#include "mcpwm_buzzer.h"
#include "motor_pwm.h"
#include "adc_battery.h"
#include "power_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"

/* 诊断数据源头文件 */
#include "mesh_forward.h"
#include "neighbor_cache.h"
#include "wake_slot.h"
#include "ble_radio.h"
#include "gatt_server.h"
#include "config_mode.h"
#include "friend_mgr.h"
#include "menu_content.h"    /* V6.14: friends_menu_refresh() */

/* 系统诊断需要的 ESP-IDF 头文件 */
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════
 *  U8G2 兼容宏（原 i2c_oled.h API → U8G2 C API）
 * ══════════════════════════════════════════════════════════════ */
#define oled_fb_clear()              u8g2_ClearBuffer(&u8g2)
#define oled_fb_flush()              u8g2_SendBuffer(&u8g2)
#define oled_write_string(c, p, s)   do { \
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr); \
    u8g2_DrawStr(&u8g2, (c), (p) * 8 + 7, (s)); \
} while(0)
#define oled_fill_rect(x, y, w, h, color)  do { \
    u8g2_SetDrawColor(&u8g2, (color)); \
    u8g2_DrawBox(&u8g2, (x), (y), (w), (h)); \
} while(0)
#define oled_draw_hline(x, y, w, color)    do { \
    u8g2_SetDrawColor(&u8g2, (color)); \
    u8g2_DrawHLine(&u8g2, (x), (y), (w)); \
} while(0)
#define oled_draw_vline(x, y, h, color)    do { \
    u8g2_SetDrawColor(&u8g2, (color)); \
    u8g2_DrawVLine(&u8g2, (x), (y), (h)); \
} while(0)

static const char *TAG = "dev_mode";

/* ══════════════════════════════════════════════════════════════
 *  内部状态
 * ══════════════════════════════════════════════════════════════ */

static hw_test_state_t s_hw_state = HW_TEST_MENU;
static dev_page_t      s_dev_page = DEV_PAGE_MAIN;  /* V6.15: 文件级, 供 BACK 判断 */
static hw_oled_step_t   s_oled_step = HW_STEP_OLED_NONE;
static hw_rgb_step_t    s_rgb_step = HW_STEP_RGB_NONE;
static hw_buzzer_step_t s_buzzer_step = HW_STEP_BUZZER_NONE;
static hw_motor_step_t  s_motor_step = HW_STEP_MOTOR_NONE;

/* 编码器测试计数器 */
static int32_t  s_enc_cw_count = 0;
static int32_t  s_enc_ccw_count = 0;
static int32_t  s_enc_dir_changes = 0;
static bool     s_enc_last_cw = true;

/* KEY 测试 */
static bool     s_key_pressed = false;
static uint32_t s_key_press_start_ms = 0;
static uint32_t s_key_last_duration_ms = 0;

/* ENC_BTN 测试 */
static int32_t  s_ebtn_short_count = 0;
static int32_t  s_ebtn_long_count = 0;
static int32_t  s_ebtn_double_count = 0;
static const char *s_ebtn_last_event = "none";

/* 诊断页滚动偏移 */
static int16_t  s_diag_scroll = 0;

/* 充电检测上次状态 */
static charge_state_t s_last_charge_state = CHARGE_OFF;

/* ══════════════════════════════════════════════════════════════
 *  硬件测试：OLED 显示模式步进
 * ══════════════════════════════════════════════════════════════ */

static void hw_oled_step_next(void)
{
    s_oled_step = (hw_oled_step_t)((int)s_oled_step + 1);
    if (s_oled_step >= HW_STEP_OLED_MAX) s_oled_step = HW_STEP_OLED_ALL_ON;

    oled_fb_clear();
    switch (s_oled_step) {
    case HW_STEP_OLED_ALL_ON:
        oled_fill_rect(0, 0, OLED_WIDTH, OLED_HEIGHT, 1);
        break;
    case HW_STEP_OLED_ALL_OFF:
        /* already cleared */
        break;
    case HW_STEP_OLED_CHECKER:
        for (uint8_t y = 0; y < OLED_HEIGHT; y += 8) {
            for (uint8_t x = 0; x < OLED_WIDTH; x += 8) {
                if (((x / 8) + (y / 8)) & 1) {
                    oled_fill_rect(x, y, 8, 8, 1);
                }
            }
        }
        break;
    case HW_STEP_OLED_VLINE:
        for (uint8_t x = 0; x < OLED_WIDTH; x += 4) {
            oled_draw_vline(x, 0, OLED_HEIGHT, 1);
        }
        break;
    case HW_STEP_OLED_HLINE:
        for (uint8_t y = 0; y < OLED_HEIGHT; y += 4) {
            oled_draw_hline(0, y, OLED_WIDTH, 1);
        }
        break;
    case HW_STEP_OLED_NORMAL:
    default:
        /* Render helper text */
        oled_write_string(0, 0, "OLED Test PASSED");
        oled_write_string(0, 2, "Press KEY to cycle");
        break;
    }
    oled_fb_flush();
}

static const char *hw_oled_step_name(void)
{
    switch (s_oled_step) {
    case HW_STEP_OLED_ALL_ON:   return "All On";
    case HW_STEP_OLED_ALL_OFF:  return "All Off";
    case HW_STEP_OLED_CHECKER:  return "Checker";
    case HW_STEP_OLED_VLINE:    return "V Lines";
    case HW_STEP_OLED_HLINE:    return "H Lines";
    case HW_STEP_OLED_NORMAL:   return "Normal";
    default:                    return "?";
    }
}

/* ══════════════════════════════════════════════════════════════
 *  硬件测试：RGB LED 颜色步进
 * ══════════════════════════════════════════════════════════════ */

static void hw_rgb_step_next(void)
{
    s_rgb_step = (hw_rgb_step_t)((int)s_rgb_step + 1);
    if (s_rgb_step >= HW_STEP_RGB_MAX) s_rgb_step = HW_STEP_RGB_RED;

    switch (s_rgb_step) {
    case HW_STEP_RGB_RED:     rgb_set_color(255, 0, 0);   break;
    case HW_STEP_RGB_GREEN:   rgb_set_color(0, 255, 0);   break;
    case HW_STEP_RGB_BLUE:    rgb_set_color(0, 0, 255);   break;
    case HW_STEP_RGB_WHITE:   rgb_set_color(255, 255, 255); break;
    case HW_STEP_RGB_YELLOW:  rgb_set_color(255, 255, 0); break;
    case HW_STEP_RGB_CYAN:    rgb_set_color(0, 255, 255); break;
    case HW_STEP_RGB_MAGENTA: rgb_set_color(255, 0, 255); break;
    case HW_STEP_RGB_OFF:     rgb_off();                  break;
    default: break;
    }
}

static const char *hw_rgb_step_name(void)
{
    switch (s_rgb_step) {
    case HW_STEP_RGB_RED:     return "Red";
    case HW_STEP_RGB_GREEN:   return "Green";
    case HW_STEP_RGB_BLUE:    return "Blue";
    case HW_STEP_RGB_WHITE:   return "White";
    case HW_STEP_RGB_YELLOW:  return "Yellow";
    case HW_STEP_RGB_CYAN:    return "Cyan";
    case HW_STEP_RGB_MAGENTA: return "Magenta";
    case HW_STEP_RGB_OFF:     return "Off";
    default:                  return "?";
    }
}

/* ══════════════════════════════════════════════════════════════
 *  硬件测试：蜂鸣器步进
 * ══════════════════════════════════════════════════════════════ */

static void hw_buzzer_step_next(void)
{
    s_buzzer_step = (hw_buzzer_step_t)((int)s_buzzer_step + 1);
    if (s_buzzer_step >= HW_STEP_BUZZER_MAX) s_buzzer_step = HW_STEP_BUZZER_SHORT;

    switch (s_buzzer_step) {
    case HW_STEP_BUZZER_SHORT:
        buzzer_beep(50);
        break;
    case HW_STEP_BUZZER_LONG:
        buzzer_beep(200);
        break;
    case HW_STEP_BUZZER_DOUBLE:
        buzzer_beep(50);
        vTaskDelay(pdMS_TO_TICKS(100));
        buzzer_beep(50);
        break;
    case HW_STEP_BUZZER_TRIPLE:
        buzzer_beep(40);
        vTaskDelay(pdMS_TO_TICKS(80));
        buzzer_beep(40);
        vTaskDelay(pdMS_TO_TICKS(80));
        buzzer_beep(40);
        break;
    default: break;
    }
}

static const char *hw_buzzer_step_name(void)
{
    switch (s_buzzer_step) {
    case HW_STEP_BUZZER_SHORT:  return "Short 4kHz 50ms";
    case HW_STEP_BUZZER_LONG:   return "Long 4kHz 200ms";
    case HW_STEP_BUZZER_DOUBLE: return "Double 4kHz x2";
    case HW_STEP_BUZZER_TRIPLE: return "Triple 4kHz x3";
    case HW_STEP_BUZZER_LONG:   return "Long 200ms";
    case HW_STEP_BUZZER_DOUBLE: return "Double";
    case HW_STEP_BUZZER_TRIPLE: return "Triple";
    default:                    return "?";
    }
}

/* ══════════════════════════════════════════════════════════════
 *  硬件测试：震动马达步进
 * ══════════════════════════════════════════════════════════════ */

static void hw_motor_step_next(void)
{
    s_motor_step = (hw_motor_step_t)((int)s_motor_step + 1);
    if (s_motor_step >= HW_STEP_MOTOR_MAX) s_motor_step = HW_STEP_MOTOR_LIGHT;

    switch (s_motor_step) {
    case HW_STEP_MOTOR_LIGHT:
        motor_start();
        motor_set_duty(30);
        vTaskDelay(pdMS_TO_TICKS(100));
        motor_stop();
        break;
    case HW_STEP_MOTOR_MEDIUM:
        motor_start();
        motor_set_duty(50);
        vTaskDelay(pdMS_TO_TICKS(200));
        motor_stop();
        break;
    case HW_STEP_MOTOR_STRONG:
        motor_start();
        motor_set_duty(80);
        vTaskDelay(pdMS_TO_TICKS(300));
        motor_stop();
        break;
    case HW_STEP_MOTOR_LONG:
        motor_start();
        motor_set_duty(50);
        vTaskDelay(pdMS_TO_TICKS(500));
        motor_stop();
        break;
    default: break;
    }
}

static const char *hw_motor_step_name(void)
{
    switch (s_motor_step) {
    case HW_STEP_MOTOR_LIGHT:  return "Light 30%";
    case HW_STEP_MOTOR_MEDIUM: return "Med 50%";
    case HW_STEP_MOTOR_STRONG: return "Strong 80%";
    case HW_STEP_MOTOR_LONG:   return "Long 500ms";
    default:                   return "?";
    }
}

/* ══════════════════════════════════════════════════════════════
 *  硬件测试：总控
 * ══════════════════════════════════════════════════════════════ */

static void hw_test_enter(hw_test_state_t state)
{
    s_hw_state = state;
    switch (state) {
    case HW_TEST_OLED:
        s_oled_step = HW_STEP_OLED_NONE;
        hw_oled_step_next();
        break;
    case HW_TEST_RGB:
        s_rgb_step = HW_STEP_RGB_NONE;
        rgb_off();
        break;
    case HW_TEST_BUZZER:
        s_buzzer_step = HW_STEP_BUZZER_NONE;
        buzzer_stop();
        break;
    case HW_TEST_MOTOR:
        s_motor_step = HW_STEP_MOTOR_NONE;
        motor_stop();
        break;
    case HW_TEST_ENCODER:
        s_enc_cw_count = 0;
        s_enc_ccw_count = 0;
        s_enc_dir_changes = 0;
        s_enc_last_cw = true;
        break;
    case HW_TEST_KEY:
        s_key_pressed = false;
        s_key_press_start_ms = 0;
        s_key_last_duration_ms = 0;
        break;
    case HW_TEST_ENC_BTN:
        s_ebtn_short_count = 0;
        s_ebtn_long_count = 0;
        s_ebtn_double_count = 0;
        s_ebtn_last_event = "none";
        break;
    case HW_TEST_CHARGE:
        s_last_charge_state = charge_status_read();
        break;
    default:
        break;
    }
}

static void hw_test_exit(void)
{
    rgb_off();
    buzzer_stop();
    motor_stop();
    s_hw_state = HW_TEST_MENU;
}

/* ══════════════════════════════════════════════════════════════
 *  硬件测试：编码器事件处理（由 ui_core 回调）
 * ══════════════════════════════════════════════════════════════ */

#if LOG_HW_TEST_ENABLE
void dev_mode_notify_encoder(bool cw)
{
    if (s_hw_state != HW_TEST_ENCODER) return;

    if (cw) {
        s_enc_cw_count++;
        if (!s_enc_last_cw) {
            s_enc_dir_changes++;
            s_enc_last_cw = true;
        }
    } else {
        s_enc_ccw_count++;
        if (s_enc_last_cw) {
            s_enc_dir_changes++;
            s_enc_last_cw = false;
        }
    }
}

void dev_mode_notify_key(bool pressed, uint32_t duration_ms)
{
    if (s_hw_state == HW_TEST_KEY) {
        s_key_pressed = pressed;
        if (pressed) {
            s_key_press_start_ms = (uint32_t)(esp_timer_get_time() / 1000);
        } else {
            s_key_last_duration_ms = duration_ms;
        }
    }

    if (s_hw_state == HW_TEST_ENC_BTN) {
        /* 事件类型由 input_encoder 判断，这里用简化逻辑 */
        if (!pressed) {
            if (duration_ms < 200) {
                s_ebtn_short_count++;
                s_ebtn_last_event = "short";
            } else if (duration_ms < 600) {
                s_ebtn_long_count++;
                s_ebtn_last_event = "long";
            } else {
                s_ebtn_double_count++;
                s_ebtn_last_event = "double";
            }
        }
    }
}
#endif /* LOG_HW_TEST_ENABLE */

/* ══════════════════════════════════════════════════════════════
 *  硬件测试：渲染
 * ══════════════════════════════════════════════════════════════ */

static void dev_render_hw_test(void)
{
    oled_fb_clear();
    char buf[22];
    (void)buf;

    switch (s_hw_state) {
    case HW_TEST_MENU:
        oled_write_string(0, 0, "[ Hardware Test ]");
        oled_write_string(0, 1, "OLED RGB Buzzer");
        oled_write_string(0, 2, "Motor Enc KEY Btn");
        oled_write_string(0, 3, "Charge    KEY:back");
        break;

    case HW_TEST_OLED:
        /* Title already written by hw_oled_step_next() in some cases */
        snprintf(buf, sizeof(buf), "OLED: %s", hw_oled_step_name());
        oled_write_string(0, 3, "KEY:next Btn:back");
        break;

    case HW_TEST_RGB:
        oled_write_string(0, 0, "[ RGB LED Test ]");
        snprintf(buf, sizeof(buf), "Color: %s", hw_rgb_step_name());
        oled_write_string(0, 2, buf);
        oled_write_string(0, 3, "KEY:next Btn:next test");
        break;

    case HW_TEST_BUZZER:
        oled_write_string(0, 0, "[ Buzzer Test ]");
        snprintf(buf, sizeof(buf), "Mode: %s", hw_buzzer_step_name());
        oled_write_string(0, 2, buf);
        oled_write_string(0, 3, "KEY:next Btn:next test");
        break;

    case HW_TEST_MOTOR:
        oled_write_string(0, 0, "[ Motor Test ]");
        snprintf(buf, sizeof(buf), "Intensity: %s", hw_motor_step_name());
        oled_write_string(0, 2, buf);
        oled_write_string(0, 3, "KEY:next Btn:next test");
        break;

    case HW_TEST_ENCODER:
        oled_write_string(0, 0, "[ Encoder Test ]");
        snprintf(buf, sizeof(buf), "CW:%ld CCW:%ld", (long)s_enc_cw_count, (long)s_enc_ccw_count);
        oled_write_string(0, 1, buf);
        snprintf(buf, sizeof(buf), "Dir chg: %ld", (long)s_enc_dir_changes);
        oled_write_string(0, 2, buf);
        oled_write_string(0, 3, "Rotate! Btn:next");
        break;

    case HW_TEST_KEY:
        oled_write_string(0, 0, "[ KEY Button Test ]");
        snprintf(buf, sizeof(buf), "State: %s", s_key_pressed ? "PRESSED" : "released");
        oled_write_string(0, 1, buf);
        if (!s_key_pressed && s_key_last_duration_ms > 0) {
            snprintf(buf, sizeof(buf), "Last: %lu ms", (unsigned long)s_key_last_duration_ms);
            oled_write_string(0, 2, buf);
        }
        oled_write_string(0, 3, "Press KEY! Btn:next");
        break;

    case HW_TEST_ENC_BTN:
        oled_write_string(0, 0, "[ ENC_BTN Test ]");
        snprintf(buf, sizeof(buf), "S:%ld L:%ld D:%ld",
                 (long)s_ebtn_short_count, (long)s_ebtn_long_count,
                 (long)s_ebtn_double_count);
        oled_write_string(0, 1, buf);
        snprintf(buf, sizeof(buf), "Last: %s", s_ebtn_last_event);
        oled_write_string(0, 2, buf);
        oled_write_string(0, 3, "Press Btn! Long:back");
        break;

    case HW_TEST_CHARGE: {
        oled_write_string(0, 0, "[ Charge Detect ]");
        charge_state_t cs = charge_status_read();
        const char *cs_str = "Unknown";
        switch (cs) {
        case CHARGE_OFF:      cs_str = "No Charger";    break;
        case CHARGE_CHARGING: cs_str = "CHARGING...";   break;
        case CHARGE_DONE:     cs_str = "DONE (full)";   break;
        case CHARGE_ERROR:    cs_str = "ERROR!";        break;
        }
        oled_write_string(0, 1, cs_str);
        snprintf(buf, sizeof(buf), "GPIO1: %s",
                 (cs == CHARGE_DONE) ? "HIGH" : "LOW");
        oled_write_string(0, 2, buf);
        oled_write_string(0, 3, "Btn:next test");
        break;
    }

    default:
        break;
    }

    oled_fb_flush();
}

/* ══════════════════════════════════════════════════════════════
 *  诊断页面渲染：Phase 1 — GPIO / 电源
 * ══════════════════════════════════════════════════════════════ */

static void dev_render_phase1(void)
{
    oled_fb_clear();
    oled_write_string(0, 0, "[ P1: GPIO/Power ]");

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    const char *wc_str = "ColdBoot";
    switch (cause) {
    case ESP_SLEEP_WAKEUP_EXT1:      wc_str = "EXT1(key)";  break;
    case ESP_SLEEP_WAKEUP_TIMER:     wc_str = "Timer";       break;
    case ESP_SLEEP_WAKEUP_UNDEFINED: wc_str = "ColdBoot";    break;
    default:                         wc_str = "Other";       break;
    }

    charge_state_t cs = charge_status_read();
    const char *cs_str = "Off";
    switch (cs) {
    case CHARGE_CHARGING: cs_str = "Chg";   break;
    case CHARGE_DONE:     cs_str = "Full";  break;
    default:              cs_str = "Off";   break;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "Wake:%s Chg:%s", wc_str, cs_str);
    oled_write_string(0, 1, buf);

    snprintf(buf, sizeof(buf), "DS:%us Hold:OE%d/R%d/M%d",
             DEFAULT_DEEPSLEEP_SEC,
             (int)gpio_get_level(OLED_EN),
             (int)gpio_get_level(RGB_DIN),
             -1 /* (int)gpio_get_level(MOTOR_PWM) 马达已禁用 */);
    oled_write_string(0, 2, buf);

    oled_write_string(0, 3, "KEY:back");
    oled_fb_flush();
}

/* ══════════════════════════════════════════════════════════════
 *  诊断页面渲染：Phase 2 — Mesh 统计
 * ══════════════════════════════════════════════════════════════ */

static void dev_render_phase2(void)
{
    oled_fb_clear();
    oled_write_string(0, 0, "[ P2: Mesh Stats ]");

#if LOG_MESH_ENABLE
    mesh_stats_t ms;
    mesh_forward_get_stats(&ms);

    char buf[22];
    /* Line 1: RX/TX counts + Drops */
    snprintf(buf, sizeof(buf), "RX:%lu TX:%lu Dr:%lu",
             (unsigned long)ms.packets_rx, (unsigned long)ms.packets_tx,
             (unsigned long)ms.packets_dropped_dup);
    oled_write_string(0, 1, buf);

    /* Line 2: Relay + ACK */
    snprintf(buf, sizeof(buf), "Rly:%lu ACK:%lu/%lu",
             (unsigned long)ms.packets_relayed,
             (unsigned long)ms.ack_success,
             (unsigned long)ms.ack_failed);
    oled_write_string(0, 2, buf);

    /* Line 3: Action prompt */
    oled_write_string(0, 3, "SP:Reset   KEY:back");
#else
    oled_write_string(0, 1, "Mesh log disabled");
    oled_write_string(0, 3, "SP:Reset   KEY:back");
#endif

    oled_fb_flush();
}

/* ══════════════════════════════════════════════════════════════
 *  诊断页面渲染：Phase 3 — UI
 * ══════════════════════════════════════════════════════════════ */

static void dev_render_phase3(void)
{
    oled_fb_clear();
    oled_write_string(0, 0, "[ P3: UI Diag ]");

    const log_diag_stats_t *st = log_manager_get_stats();
    char buf[22];

    /* 使用 ui_core 追踪状态 */
    snprintf(buf, sizeof(buf), "Page:active Mode:Dev");
    oled_write_string(0, 1, buf);

    snprintf(buf, sizeof(buf), "LogDrp:%lu",
             (unsigned long)st->log_lines_dropped);
    oled_write_string(0, 2, buf);

    oled_write_string(0, 3, "KEY:back");
    oled_fb_flush();
}

/* ══════════════════════════════════════════════════════════════
 *  诊断页面渲染：Phase 4 — GATT
 * ══════════════════════════════════════════════════════════════ */

static void dev_render_phase4(void)
{
    oled_fb_clear();
    oled_write_string(0, 0, "[ P4: GATT Diag ]");

    gatt_srv_state_t gs = gatt_server_get_state();
    const char *gs_str = "Off";
    switch (gs) {
    case GATT_SRV_OFF:       gs_str = "Off";       break;
    case GATT_SRV_IDLE:      gs_str = "Idle";      break;
    case GATT_SRV_CONNECTED: gs_str = "Connected"; break;
    case GATT_SRV_OTA:       gs_str = "OTA";       break;
    }

    uint32_t ota_free = gatt_ota_get_free_space();

    char buf[32];
    snprintf(buf, sizeof(buf), "GATT:%s OTA:%luK",
             gs_str, (unsigned long)(ota_free / 1024));
    oled_write_string(0, 1, buf);

    snprintf(buf, sizeof(buf), "Mode:%s",
             config_mode_get_name());
    oled_write_string(0, 2, buf);

    oled_write_string(0, 3, "SP:Reset   KEY:back");
    oled_fb_flush();
}

/* ══════════════════════════════════════════════════════════════
 *  诊断页面渲染：Phase 5 — OTA
 * ══════════════════════════════════════════════════════════════ */

static void dev_render_phase5(void)
{
    oled_fb_clear();
    oled_write_string(0, 0, "[ P5: OTA Status ]");

    const log_diag_stats_t *st = log_manager_get_stats();
    const esp_partition_t *running = esp_ota_get_running_partition();

    char buf[32];
    if (running) {
        snprintf(buf, sizeof(buf), "Run: %s", running->label[0] ? running->label : "?");
    } else {
        snprintf(buf, sizeof(buf), "Run: unknown");
    }
    oled_write_string(0, 1, buf);

    snprintf(buf, sizeof(buf), "Boot:%lu Rollback:%s",
             (unsigned long)st->boot_count,
             "N/A");
    oled_write_string(0, 2, buf);

    /* Show free space */
    uint32_t free_kb = gatt_ota_get_free_space() / 1024;
    snprintf(buf, sizeof(buf), "OTA free:%luK", (unsigned long)free_kb);
    oled_write_string(0, 3, buf);

    oled_fb_flush();
}

/* ══════════════════════════════════════════════════════════════
 *  诊断页面渲染：Phase 6 — Friends
 * ══════════════════════════════════════════════════════════════ */

static void dev_render_phase6(void)
{
    oled_fb_clear();
    oled_write_string(0, 0, "[ P6: Friends ]");

    char buf[24];  /* V6.35: 增大容纳 WaitConfirm 状态名 */
    uint8_t cnt = g_config.contact_count;
    snprintf(buf, sizeof(buf), "Cnt:%u/32", (unsigned)cnt);
    oled_write_string(0, 1, buf);

    friend_mgr_state_t fs = friend_mgr_get_state();
    const char *fs_str = "Idle";
    switch (fs) {
    case FRIEND_MGR_IDLE:         fs_str = "Idle";        break;
    case FRIEND_MGR_SCANNING:     fs_str = "Scanning";    break;
    case FRIEND_MGR_WAIT_CONFIRM: fs_str = "WaitConfirm"; break;
    case FRIEND_MGR_EXCHANGING:   fs_str = "Exchange";    break;
    case FRIEND_MGR_DONE:         fs_str = "Done";        break;
    case FRIEND_MGR_TIMEOUT:      fs_str = "Timeout";     break;
    case FRIEND_MGR_ERROR:        fs_str = "Error";       break;
    }

    uint8_t pending = nvs_config_get_pending_sync_count();
    snprintf(buf, sizeof(buf), "St:%s Sync:%u", fs_str, (unsigned)pending);
    oled_write_string(0, 2, buf);

    oled_write_string(0, 3, "SP:ClearAll KEY:back");
    oled_fb_flush();
}

/* ══════════════════════════════════════════════════════════════
 *  系统诊断页面渲染
 * ══════════════════════════════════════════════════════════════ */

static void dev_render_sys_diag(void)
{
    oled_fb_clear();
    oled_write_string(0, 0, "[ System Diag ]");

    const log_diag_stats_t *st = log_manager_get_stats();
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);

    char buf[22];

    /* Line 1: Heap */
    snprintf(buf, sizeof(buf), "Heap:%lu/%luK",
             (unsigned long)(free_heap / 1024),
             (unsigned long)(min_heap / 1024));
    oled_write_string(0, 1, buf);

    /* Line 2: Boot + Uptime */
    uint32_t uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
    snprintf(buf, sizeof(buf), "Boot#%lu Up:%lus",
             (unsigned long)st->boot_count, (unsigned long)uptime_s);
    oled_write_string(0, 2, buf);

    /* Line 3: Task watermarks */
    snprintf(buf, sizeof(buf), "R:%lu U:%lu",
             (unsigned long)st->task_watermark_radio,
             (unsigned long)st->task_watermark_ui);
    oled_write_string(0, 3, buf);

    oled_fb_flush();
}

/* ══════════════════════════════════════════════════════════════
 *  公开 API
 * ══════════════════════════════════════════════════════════════ */

void dev_mode_init(void)
{
    s_hw_state = HW_TEST_MENU;
    s_diag_scroll = 0;
    ESP_LOGI(TAG, "Developer mode initialized");
}

void dev_mode_render(dev_page_t page)
{
    switch (page) {
    case DEV_PAGE_HW_TEST:
        dev_render_hw_test();
        break;
    case DEV_PAGE_PHASE1:
        dev_render_phase1();
        break;
    case DEV_PAGE_PHASE2:
        dev_render_phase2();
        break;
    case DEV_PAGE_PHASE3:
        dev_render_phase3();
        break;
    case DEV_PAGE_PHASE4:
        dev_render_phase4();
        break;
    case DEV_PAGE_PHASE5:
        dev_render_phase5();
        break;
    case DEV_PAGE_PHASE6:
        dev_render_phase6();
        break;
    case DEV_PAGE_SYS_DIAG:
        dev_render_sys_diag();
        break;
    default:
        break;
    }
}

bool dev_mode_handle_input(dev_page_t page, const ec06_event_t *evt)
{
    if (!evt) return false;

    /* ── 硬件测试页面输入处理 ──────────────────────── */
    if (page == DEV_PAGE_HW_TEST) {
        switch (s_hw_state) {
        case HW_TEST_MENU:
            /* KEY 返回上级，短按无操作 */
            if (evt->type == EC06_EVENT_KEY_DOWN) return true; /* 由 ui_core 处理返回 */
            if (evt->type == EC06_EVENT_LONG_PRESS) { hw_test_exit(); return true; }
            break;

        case HW_TEST_OLED:
        case HW_TEST_RGB:
        case HW_TEST_BUZZER:
        case HW_TEST_MOTOR:
            if (evt->type == EC06_EVENT_KEY_DOWN) {
                /* KEY 步进 */
                if (s_hw_state == HW_TEST_OLED)      hw_oled_step_next();
                else if (s_hw_state == HW_TEST_RGB)   hw_rgb_step_next();
                else if (s_hw_state == HW_TEST_BUZZER) hw_buzzer_step_next();
                else if (s_hw_state == HW_TEST_MOTOR)  hw_motor_step_next();
                return true;
            }
            if (evt->type == EC06_EVENT_SHORT_PRESS) {
                /* ENC_BTN 进入下一子测试 */
                if (s_hw_state == HW_TEST_OLED)      hw_test_enter(HW_TEST_RGB);
                else if (s_hw_state == HW_TEST_RGB)   hw_test_enter(HW_TEST_BUZZER);
                else if (s_hw_state == HW_TEST_BUZZER) hw_test_enter(HW_TEST_MOTOR);
                else if (s_hw_state == HW_TEST_MOTOR)  hw_test_enter(HW_TEST_ENCODER);
                return true;
            }
            if (evt->type == EC06_EVENT_LONG_PRESS) { hw_test_exit(); return true; }
            break;

        case HW_TEST_ENCODER:
        case HW_TEST_KEY:
            if (evt->type == EC06_EVENT_SHORT_PRESS) {
                if (s_hw_state == HW_TEST_ENCODER) hw_test_enter(HW_TEST_KEY);
                else if (s_hw_state == HW_TEST_KEY) hw_test_enter(HW_TEST_ENC_BTN);
                return true;
            }
            if (evt->type == EC06_EVENT_LONG_PRESS) { hw_test_exit(); return true; }
            break;

        case HW_TEST_ENC_BTN:
            if (evt->type == EC06_EVENT_SHORT_PRESS) {
                hw_test_enter(HW_TEST_CHARGE);
                return true;
            }
            if (evt->type == EC06_EVENT_LONG_PRESS) { hw_test_exit(); return true; }
            break;

        case HW_TEST_CHARGE:
            if (evt->type == EC06_EVENT_SHORT_PRESS || evt->type == EC06_EVENT_LONG_PRESS) {
                hw_test_exit();
                return true;
            }
            break;

        default:
            break;
        }
    }

    /* ── V6.14: 诊断页面输入处理（SP=操作, KEY=返回）────── */
    if (page >= DEV_PAGE_PHASE1 && page <= DEV_PAGE_SYS_DIAG) {
        if (evt->type == EC06_EVENT_KEY_DOWN || evt->type == EC06_EVENT_LONG_PRESS) {
            return true; /* 由 dev_mode_page_proc 处理 BACK → 主菜单 */
        }
        if (evt->type == EC06_EVENT_SHORT_PRESS) {
            /* SP 触发各页操作 */
            switch (page) {
            case DEV_PAGE_PHASE2: {
                /* P2: 清零 Mesh 统计 */
                mesh_forward_reset_stats();
                ESP_LOGI(TAG, "Mesh stats reset");
                break;
            }
            case DEV_PAGE_PHASE4: {
                /* P4: 切换 Config Mode (Normal ↔ Config) */
                device_mode_t cur = config_mode_get();
                device_mode_t next = (cur == MODE_CONFIG) ? MODE_MESH : MODE_CONFIG;
                config_mode_request(next);
                ESP_LOGI(TAG, "Config mode: %s → %s",
                         config_mode_get_name(), (next == MODE_CONFIG) ? "Config" : "Normal");
                break;
            }
            case DEV_PAGE_PHASE5: {
                /* P5: 显示 OTA 提示（不实际触发，防止误操作） */
                ESP_LOGI(TAG, "OTA rollback NOT triggered (dev safeguard)");
                break;
            }
            case DEV_PAGE_PHASE6: {
                /* P6: 清除全部好友联系人 */
                extern nvs_config_t g_config;
                for (int i = 0; i < NVS_CONTACT_MAX; i++) {
                    memset(&g_config.contacts[i], 0, sizeof(g_config.contacts[i]));
                }
                g_config.contact_count = 0;
                nvs_config_mark_dirty();
                friends_menu_refresh();
                ESP_LOGI(TAG, "All contacts cleared");
                break;
            }
            case DEV_PAGE_SYS_DIAG: {
                /* SysDiag: 重启设备 */
                ESP_LOGI(TAG, "Rebooting via dev mode...");
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_restart();
                break;
            }
            default: break;
            }
            return true;
        }
    }

    return false;
}

hw_test_state_t dev_mode_get_hw_test_state(void)
{
    return s_hw_state;
}

bool dev_mode_is_active(void)
{
    return (s_hw_state != HW_TEST_MENU && s_hw_state != HW_TEST_DONE);
}

void dev_mode_enter_hw_test(void)
{
    hw_test_enter(HW_TEST_MENU);
}

void dev_mode_hw_cleanup(void)
{
    rgb_off();
    buzzer_stop();
    motor_stop();
}

bool dev_mode_is_in_sub_page(void)
{
    return (s_dev_page != DEV_PAGE_MAIN);
}

void dev_mode_back_to_main(void)
{
    s_dev_page = DEV_PAGE_MAIN;
    s_hw_state = HW_TEST_MENU;
    rgb_off();
    buzzer_stop();
    motor_stop();
}

void dev_mode_set_page(dev_page_t page)
{
    s_dev_page = page;
}

dev_page_t dev_mode_get_page(void)
{
    return s_dev_page;
}

#endif /* LOG_DEV_MODE_ENABLE */
