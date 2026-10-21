/**
 * @file    morse_engine.c
 * @brief   莫尔斯电码编码/解码器 — V5.7 阶段八 (WPM 动态化)
 *
 * 依据架构文档 §14.四.1/4 实现：
 *   - 20ms 消抖下限
 *   - 点/划判别阈值 = 1.33 × T（T = 1200 / WPM）
 *   - 元素间隔 = 2.5T，触发字符解码
 *   - 自动发送 = 10T 空闲超时
 *   - 所有阈值随 WPM 动态调整
 */

#include "morse_engine.h"
#include "morse_codec.h"
#include "ec06.h"
#include "nvs_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "morse";

/* ── 运行时 WPM 状态 ────────────────────────────────────── */
static uint8_t  g_wpm = MORSE_DEFAULT_WPM;
static uint32_t g_base_t_ms = 100;          /* T = 1200 / WPM */
static uint32_t g_threshold_ms = 133;        /* 1.33T（点/划分界） */
static uint32_t g_element_gap_ms = 250;      /* 2.5T（元素间隔） */
static uint32_t g_auto_send_ms = 1000;       /* 10T（自动发送） */

static void recalc_thresholds(void)
{
    g_base_t_ms      = 1200 / g_wpm;
    if (g_base_t_ms < 30) g_base_t_ms = 30;
    if (g_base_t_ms > 240) g_base_t_ms = 240;
    g_threshold_ms   = (g_base_t_ms * 4) / 3;   /* 1.33T */
    g_element_gap_ms = (g_base_t_ms * 5) / 2;    /* 2.5T */
    g_auto_send_ms   = g_base_t_ms * 10;          /* 10T */
    ESP_LOGI(TAG, "WPM=%u → T=%lums, threshold=%lums, gap=%lums, auto_send=%lums",
             g_wpm, (unsigned long)g_base_t_ms, (unsigned long)g_threshold_ms,
             (unsigned long)g_element_gap_ms, (unsigned long)g_auto_send_ms);
}

/* ── 莫尔斯电码查找表 ────────────────────────────────────── */
typedef struct {
    char   ch;
    const char *pattern;  /* 如 ".-" */
} morse_entry_t;

/* ── 内部状态 ────────────────────────────────────────────── */
static char   g_buffer[MORSE_MAX_LEN + 1];  /* 当前点/划序列 */
static uint8_t g_buffer_len = 0;
static uint64_t g_last_event_us = 0;
static bool g_key_was_down = false;
static morse_decode_cb_t g_decode_cb = NULL;
static morse_send_cb_t   g_send_cb = NULL;
static void              *g_user_ctx = NULL;

/* ── 辅助函数 ────────────────────────────────────────────── */

char morse_lookup(const char *pattern)
{
    char ch = morse_codec_decode(pattern);
    return ch ? ch : '?';
}

static void morse_decode_and_report(void)
{
    if (g_buffer_len == 0) return;
    g_buffer[g_buffer_len] = '\0';
    char ch = morse_lookup(g_buffer);
    ESP_LOGI(TAG, "Decoded: '%s' → '%c'", g_buffer, ch);
    if (g_decode_cb) g_decode_cb(ch, g_user_ctx);
    g_buffer_len = 0;
}

/* ── 公开 API ────────────────────────────────────────────── */

uint32_t morse_engine_get_base_t_ms(void)  { return g_base_t_ms; }
uint32_t morse_engine_get_threshold_ms(void) { return g_threshold_ms; }

void morse_engine_set_wpm(uint8_t wpm)
{
    if (wpm < 5) wpm = 5;
    if (wpm > 40) wpm = 40;
    if (wpm == g_wpm) return;
    g_wpm = wpm;
    recalc_thresholds();
}

uint8_t morse_engine_get_wpm(void) { return g_wpm; }

void morse_engine_init(morse_decode_cb_t decode_cb,
                       morse_send_cb_t send_cb,
                       void *user_ctx)
{
    g_decode_cb = decode_cb;
    g_send_cb   = send_cb;
    g_user_ctx  = user_ctx;
    g_buffer_len = 0;
    g_key_was_down = false;
    g_last_event_us = esp_timer_get_time();

    /* V7.7: 从 dot_ms 反算 WPM（优先 g_config，fallback 默认 200ms→6WPM） */
    if (g_config.morse_dot_ms >= 40 && g_config.morse_dot_ms <= 300) {
        g_wpm = (uint8_t)(1200 / g_config.morse_dot_ms);
        if (g_wpm < 5) g_wpm = 5;
        if (g_wpm > 40) g_wpm = 40;
    } else if (g_config.morse_wpm >= 5 && g_config.morse_wpm <= 40) {
        g_wpm = g_config.morse_wpm;
    }
    recalc_thresholds();
    ESP_LOGI(TAG, "Morse engine ready (WPM=%u, T=%lums)", g_wpm, (unsigned long)g_base_t_ms);
}

void morse_engine_feed(int evt_type, uint32_t duration_ms)
{
    uint64_t now_us = esp_timer_get_time();
    uint32_t gap_ms = (uint32_t)((now_us - g_last_event_us) / 1000ULL);
    g_last_event_us = now_us;

    if (evt_type == EC06_EVENT_ENC_DOWN) {
        /* 检查间隔是否超过自动发送阈值 */
        if (gap_ms > g_auto_send_ms && g_buffer_len > 0) {
            ESP_LOGI(TAG, "Auto-send triggered (gap=%lums)", (unsigned long)gap_ms);
            if (g_send_cb) {
                g_buffer[g_buffer_len] = '\0';
                g_send_cb(g_buffer, g_user_ctx);
            }
            g_buffer_len = 0;
        }

        /* 检查间隔是否超过元素解码阈值 */
        if (gap_ms > g_element_gap_ms && g_buffer_len > 0) {
            morse_decode_and_report();
        }
        g_key_was_down = true;

    } else if (evt_type == EC06_EVENT_ENC_UP) {
        if (!g_key_was_down) return;
        g_key_was_down = false;

        /* 分类按压时长（使用动态阈值） */
        if (duration_ms < MORSE_DOT_MIN_MS) {
            /* 消抖：太短，丢弃 */
        } else if (duration_ms <= g_threshold_ms) {
            /* 点 */
            if (g_buffer_len < MORSE_MAX_LEN) {
                g_buffer[g_buffer_len++] = '.';
            }
            ESP_LOGD(TAG, "Dot (%lums, thresh=%lums)",
                     (unsigned long)duration_ms, (unsigned long)g_threshold_ms);
        } else {
            /* 划 */
            if (g_buffer_len < MORSE_MAX_LEN) {
                g_buffer[g_buffer_len++] = '-';
            }
            ESP_LOGD(TAG, "Dash (%lums, thresh=%lums)",
                     (unsigned long)duration_ms, (unsigned long)g_threshold_ms);
        }
    }
}

void morse_engine_clear(void)
{
    g_buffer_len = 0;
}

uint8_t morse_engine_get_buffer_len(void)
{
    return g_buffer_len;
}

/* ══════════════════════════════════════════════════════════════
 *  解码历史缓冲区（V5.8：自动译码）
 *  环形缓冲，最多 MORSE_DECODE_HISTORY_MAX 字符。
 *  满时覆盖最旧条目。
 * ══════════════════════════════════════════════════════════════ */

static char  g_decode_history[MORSE_DECODE_HISTORY_MAX];
static uint16_t g_decode_head = 0;    /* 写入位置 */
static uint16_t g_decode_count = 0;   /* 缓冲区中字符数 */
static bool  g_decode_full = false;   /* 是否已达最大容量 */

void morse_decode_push(char ch)
{
    if (ch < ' ' || ch > '~') return;  /* 可打印 ASCII 范围 */

    g_decode_history[g_decode_head] = ch;
    g_decode_head = (g_decode_head + 1) % MORSE_DECODE_HISTORY_MAX;

    if (g_decode_full) {
        /* 满：head 已推进，无需调整 count */
    } else {
        g_decode_count++;
        if (g_decode_count >= MORSE_DECODE_HISTORY_MAX) {
            g_decode_full = true;
        }
    }
}

char morse_decode_get_char(uint16_t index)
{
    if (index >= g_decode_count) return '\0';

    uint16_t start;
    if (g_decode_full) {
        /* 最旧字符在 head 位置（下一个将被覆盖的位置） */
        start = g_decode_head;
    } else {
        start = 0;
    }
    uint16_t actual = (start + index) % MORSE_DECODE_HISTORY_MAX;
    return g_decode_history[actual];
}

uint16_t morse_decode_get_count(void)
{
    return g_decode_count;
}

void morse_decode_clear(void)
{
    g_decode_head = 0;
    g_decode_count = 0;
    g_decode_full = false;
}
