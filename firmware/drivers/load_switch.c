/**
 * @file    load_switch.c
 * @brief   负载开关集中管理实现 — V7.0
 *
 * GPIO4 → WS4621C → OLED_3V3 + WS2812_VDD
 * 位掩码追踪两个客户端（OLED + RGB），双方都释放时关断。
 *
 * 上电时序：GPIO4 HIGH → 150ms（RC 稳定 + SSD1306 电荷泵）
 * 下电时序：GPIO4 LOW（OLED 的 Display OFF 由 oled_power 负责，
 *           调用 load_switch_release 之前发送）
 */

#include "load_switch.h"
#include "pins.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "load_sw";

/* ── 内部状态 ─────────────────────────────────────────────── */
static uint8_t  s_active_mask = 0x00;   /* bit 0=OLED, bit 1=RGB */
static bool     s_inited      = false;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_ready_us;

/* ── 上电稳定延时（RC τ=22ms, 5τ=110ms + 余量）─────────── */
#define LOAD_SW_STABILISE_MS   150

/* ══════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════ */

void load_switch_init(void)
{
    if (s_inited) return;

    /* 释放 DeepSleep 残留 Hold */
    gpio_hold_dis(OLED_EN);

    /* 默认关断 */
    gpio_set_direction(OLED_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(OLED_EN, 0);

    s_active_mask = 0x00;
    s_inited = true;

    ESP_LOGI(TAG, "Init: GPIO%d LOW (off)", OLED_EN);
}

void load_switch_acquire(load_switch_client_t client)
{
    if (!s_inited) {
        load_switch_init();
    }

    if (client >= LOAD_SW_COUNT) {
        ESP_LOGE(TAG, "acquire: invalid client %d", client);
        return;
    }

    portENTER_CRITICAL(&s_lock);
    if (!s_active_mask) {
        gpio_set_level(OLED_EN, 1);
        s_ready_us = esp_timer_get_time() + LOAD_SW_STABILISE_MS * 1000;
    }
    s_active_mask |= (uint8_t)(1u << client);
    int64_t wait_us = s_ready_us - esp_timer_get_time();
    portEXIT_CRITICAL(&s_lock);
    if (wait_us > 0) vTaskDelay(pdMS_TO_TICKS((wait_us + 999) / 1000));
}

void load_switch_release(load_switch_client_t client)
{
    if (!s_inited) return;

    if (client >= LOAD_SW_COUNT) {
        ESP_LOGE(TAG, "release: invalid client %d", client);
        return;
    }

    portENTER_CRITICAL(&s_lock);
    s_active_mask &= (uint8_t)~(1u << client);
    if (!s_active_mask) gpio_set_level(OLED_EN, 0);
    portEXIT_CRITICAL(&s_lock);
}

bool load_switch_is_on(void)
{
    portENTER_CRITICAL(&s_lock);
    bool on = s_active_mask != 0;
    portEXIT_CRITICAL(&s_lock);
    return on;
}

void load_switch_hold_for_sleep(void)
{
    if (!s_inited) {
        load_switch_init();
    }

    /* 强制关断 + Hold（无视引用计数） */
    gpio_set_level(OLED_EN, 0);
    gpio_set_direction(OLED_EN, GPIO_MODE_OUTPUT);
    gpio_hold_en(OLED_EN);

    s_active_mask = 0x00;

    ESP_LOGI(TAG, "GPIO%d HOLD LOW for DeepSleep", OLED_EN);
}
