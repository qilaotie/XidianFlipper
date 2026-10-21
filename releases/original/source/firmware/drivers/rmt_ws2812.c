/**
 * @file    rmt_ws2812.c
 * @brief   WS2812C RGB LED RMT 驱动
 *
 *   - rmt_copy_encoder + 24 个数据符号，帧前/后各 400us 低电平
 *   - eot_level=0 只保证空闲电平，不保证连续发送之间的复位时长
 *   - 持久缓冲区在超时后仍有效，旧传输完成前禁止复用
 *   - 保留全局亮度控制供上层 rgb_led 引擎使用
 *   - FreeRTOS 互斥锁串行化多上下文并发传输（ui_task/NimBLE/Tmr Svc/main），
 *     所有 RMT API 错误仅记日志，绝不 abort
 *
 * WS2812C-2020-V1 timing (datasheet p4, 1 bit = 1.2 µs):
 *   T0H = 0.3 µs,  T0L = 0.9 µs   (code "0")
 *   T1H = 0.6 µs,  T1L = 0.6 µs   (code "1")
 *   RES  = 400 µs (low, including WS2812C variants requiring >280 µs)
 *
 * Data format: GRB (Green-Red-Blue), 24 bits per LED, MSB first.
 * 硬件: GPIO7 → WS2812C-2020-V1 DIN，网表中有 10k 下拉
 */

#include "rmt_ws2812.h"
#include "pins.h"
#include "load_switch.h"
#include "nvs_config.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "ws2812";

/* ── RMT handles ────────────────────────────────────────────── */
static rmt_channel_handle_t     rmt_tx_ch    = NULL;
static rmt_encoder_handle_t     rmt_encoder  = NULL;
static bool                     inited       = false;

/* ── 并发保护 ─────────────────────────────────────────────────
 * rgb_set_color 可从 4 个上下文并发进入 (ui_task / NimBLE host /
 * Tmr Svc 定时器回调 / main)，而 RMT 通道 trans_queue_depth=1，
 * 并发 transmit 会失败。互斥锁串行化所有传输；拿不到锁就放弃
 * 本次帧并记日志，绝不允许 ESP_ERROR_CHECK abort。 */
static SemaphoreHandle_t        s_rmt_mutex  = NULL;
#define RMT_MUTEX_TIMEOUT_MS    50   /* 取锁超时: 覆盖最坏情况一帧传输 */

/* ── Global brightness 0-100 ───────────────────────────────── */
static uint8_t                  s_brightness = 100;
static uint32_t                 s_output_rgb = 0;

/* ── WS2812 timing in RMT ticks (target 10 MHz ≈ 0.1 µs/tick) ── */
#define WS2812_T0H_TICKS   3   /* 0.3 µs HIGH (specified 0.22..0.38) */
#define WS2812_T0L_TICKS   9   /* 0.9 µs LOW  (specified 0.58..1.0)  */
#define WS2812_T1H_TICKS   6   /* 0.6 µs HIGH (specified 0.58..1.0)  */
#define WS2812_T1L_TICKS   6   /* 0.6 µs LOW  (specified 0.58..1.0)  */
#define WS2812_RST_HALF_TICKS 2000 /* 2 x 200us = 400us low */

/* RMT retains the payload pointer until completion, even after a wait timeout.
 * All accesses are protected by s_rmt_mutex. 26 symbols fit in C6's 48-word RAM. */
static rmt_symbol_word_t s_symbols[26];
static bool s_tx_pending;
static uint32_t s_pending_rgb;

/* ── Pre-built RMT symbols for bit 0 and bit 1 ──────────────── */
static rmt_symbol_word_t ws2812_bit0;
static rmt_symbol_word_t ws2812_bit1;

static const rmt_transmit_config_t rmt_tx_cfg = {
    .loop_count       = 0,
    .flags.eot_level  = 0,   /* 传输结束后保持低电平 → WS2812 自然复位 */
};

/* ══════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════ */

void rgb_init(void)
{
    if (inited) return;

    ESP_LOGI(TAG, "Init WS2812C RMT driver (400us frame reset)");

    /* 先创建互斥锁 — 必须在任何传输路径可用之前就绪 */
    if (!s_rmt_mutex) {
        s_rmt_mutex = xSemaphoreCreateMutex();
        if (!s_rmt_mutex) {
            ESP_LOGE(TAG, "Failed to create RMT mutex!");
            return;
        }
    }

    /* 初始化全程持锁，避免与并发的 rgb_set_color 交叉；
     * 等锁期间别的上下文可能已完成初始化 → 复查 inited */
    xSemaphoreTake(s_rmt_mutex, portMAX_DELAY);
    if (inited) {
        xSemaphoreGive(s_rmt_mutex);
        return;
    }

    /* 构建 RMT 符号 */
    ws2812_bit0.duration0 = WS2812_T0H_TICKS;
    ws2812_bit0.level0    = 1;
    ws2812_bit0.duration1 = WS2812_T0L_TICKS;
    ws2812_bit0.level1    = 0;

    ws2812_bit1.duration0 = WS2812_T1H_TICKS;
    ws2812_bit1.level0    = 1;
    ws2812_bit1.duration1 = WS2812_T1L_TICKS;
    ws2812_bit1.level1    = 0;

    /* 配置 RMT TX 通道 (ESP32-C6 最多 48 符号/通道) */
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num           = RGB_DIN,
        .clk_src            = RMT_CLK_SRC_DEFAULT,
        .resolution_hz      = 10 * 1000 * 1000,   /* 目标 10 MHz */
        .mem_block_symbols  = 48,                   /* ESP32-C6 硬件上限 */
        .trans_queue_depth  = 1,
        .flags.invert_out   = false,
        .flags.with_dma     = false,
    };
    esp_err_t err = rmt_new_tx_channel(&tx_cfg, &rmt_tx_ch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_tx_channel failed: %s", esp_err_to_name(err));
        goto init_fail;
    }

    /* copy encoder: 将预构建的 RMT 符号原样传输 */
    rmt_copy_encoder_config_t copy_cfg = {};
    err = rmt_new_copy_encoder(&copy_cfg, &rmt_encoder);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_copy_encoder failed: %s", esp_err_to_name(err));
        goto init_fail;
    }

    /* 使能通道 */
    err = rmt_enable(rmt_tx_ch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_enable failed: %s", esp_err_to_name(err));
        goto init_fail;
    }

    /* Every data frame includes reset. Startup sends black after the shared
     * OLED/RGB supply is ready; do not transmit a stack buffer while unpowered. */

    inited = true;
    xSemaphoreGive(s_rmt_mutex);
    ESP_LOGI(TAG, "WS2812 ready (GPIO%d)", (int)RGB_DIN);
    return;

init_fail:
    /* 清理已创建的资源，允许下次调用重试 */
    if (rmt_encoder) { rmt_del_encoder(rmt_encoder); rmt_encoder = NULL; }
    if (rmt_tx_ch)   { rmt_del_channel(rmt_tx_ch);   rmt_tx_ch   = NULL; }
    xSemaphoreGive(s_rmt_mutex);
}

/**
 * @brief 发送一个 WS2812 LED 的 GRB 数据（24 bits）
 *
 * WS2812 数据顺序: G7..G0, R7..R0, B7..B0
 * 每 bit 1 个 RMT 符号，MSB first。
 * 数据前后各包含完整复位，快速相邻调用也不会合并成一帧。
 */
static void ws2812_send_grb(uint8_t g, uint8_t r, uint8_t b)
{
    /* 驱动未就绪（rgb_init 失败或被跳过）→ 安全丢弃 */
    if (!inited || !rmt_tx_ch || !rmt_encoder) {
        ESP_LOGW(TAG, "send_grb: driver not ready, frame dropped");
        return;
    }

    /* 串行化多上下文并发传输；超时放弃本次帧，绝不 abort */
    if (!s_rmt_mutex ||
        xSemaphoreTake(s_rmt_mutex, pdMS_TO_TICKS(RMT_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "send_grb: RMT busy (>%ums), frame dropped", RMT_MUTEX_TIMEOUT_MS);
        return;
    }

    /* A timed-out transaction may still be reading s_symbols. Never overwrite
     * it, or queue another frame, until the driver confirms completion. */
    if (s_tx_pending) {
        esp_err_t pending_err = rmt_tx_wait_all_done(rmt_tx_ch, 100);
        if (pending_err != ESP_OK) {
            ESP_LOGW(TAG, "previous RGB frame still pending: %s", esp_err_to_name(pending_err));
            xSemaphoreGive(s_rmt_mutex);
            return;
        }
        s_tx_pending = false;
        s_output_rgb = s_pending_rgb;
    }

    const rmt_symbol_word_t reset = {
        .duration0 = WS2812_RST_HALF_TICKS, .level0 = 0,
        .duration1 = WS2812_RST_HALF_TICKS, .level1 = 0,
    };
    s_symbols[0] = reset;
    s_symbols[25] = reset;
    uint8_t bytes[3] = { g, r, b };

    for (int i = 0; i < 3; i++) {
        for (int bit = 0; bit < 8; bit++) {
            s_symbols[1 + i * 8 + (7 - bit)] =
                (bytes[i] >> bit) & 1 ? ws2812_bit1 : ws2812_bit0;
        }
    }

    esp_err_t err = rmt_transmit(rmt_tx_ch, rmt_encoder, s_symbols,
                                  sizeof(s_symbols), &rmt_tx_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_transmit failed: %s", esp_err_to_name(err));
    } else {
        s_tx_pending = true;
        s_pending_rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        /* Data + reset <1ms; keep the payload alive if the bounded wait fails. */
        err = rmt_tx_wait_all_done(rmt_tx_ch, 100);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "rmt_tx_wait_all_done failed: %s", esp_err_to_name(err));
        }
    }

    if (err == ESP_OK) {
        s_tx_pending = false;
        s_output_rgb = s_pending_rgb;
    }
    xSemaphoreGive(s_rmt_mutex);
}

void rgb_get_output(uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint32_t value = load_switch_is_on() ? s_output_rgb : 0;
    if (r) *r = (uint8_t)(value >> 16);
    if (g) *g = (uint8_t)(value >> 8);
    if (b) *b = (uint8_t)value;
}

void rgb_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    if (!g_config.led_enabled) { rgb_set_color_direct(0, 0, 0); return; }
    /* Apply global brightness scaling */
    if (s_brightness < 100) {
        r = (uint8_t)(((uint16_t)r * s_brightness) / 100);
        g = (uint8_t)(((uint16_t)g * s_brightness) / 100);
        b = (uint8_t)(((uint16_t)b * s_brightness) / 100);
    }
    rgb_set_color_direct(r, g, b);
}

void rgb_set_color_direct(uint8_t r, uint8_t g, uint8_t b)
{
    if (!inited) {
        rgb_init();
    }
    ws2812_send_grb(g, r, b);
}

void rgb_off(void)
{
    if (!inited) return;
    ws2812_send_grb(0, 0, 0);
    ESP_LOGI(TAG, "RGB OFF (all-black sent)");
}

void rgb_set_brightness(uint8_t pct)
{
    if (pct > 100) pct = 100;
    s_brightness = pct;
    ESP_LOGD(TAG, "Brightness set to %u%%", pct);
}

uint8_t rgb_get_brightness(void)
{
    return s_brightness;
}

void rgb_flash(uint32_t duration_ms)
{
    /* 弃用 — 请使用 rgb_led_trigger() 非阻塞引擎。
     * 保留此函数供 Phase1 自检使用。 */
    rgb_set_color(255, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    rgb_off();
}
