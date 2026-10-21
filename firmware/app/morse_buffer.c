/**
 * @file    morse_buffer.c
 * @brief   摩斯电码输入缓冲区实现 — V7.0 MORSE V1.1
 *
 * 累积 morse_engine 解码后的点/划模式，满足条件时触发发送。
 * stream_seq 每次 auto-send 后重新随机，避免全局递增回绕。
 */

#include "morse_buffer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include <string.h>

static const char *TAG = "morse_buf";

/* ── 内部状态 ────────────────────────────────────────────── */

static uint8_t  g_buf[MORSE_BUFFER_SIZE];   /**< 输入缓冲区 */
static uint16_t g_buf_len = 0;              /**< 当前有效长度 */
static uint32_t g_last_input_ms = 0;        /**< 上次输入的系统时间（ms） */
static uint16_t g_stream_seq = 0;           /**< 当前流序列号 */
static morse_buffer_send_cb_t g_send_cb = NULL;
static void    *g_user_ctx = NULL;
static bool     g_inited = false;

/* ── 辅助函数 ────────────────────────────────────────────── */

/** 生成新的随机 stream_seq */
static void renew_stream_seq(void)
{
    g_stream_seq = (uint16_t)(esp_random() & 0xFFFF);
    ESP_LOGD(TAG, "stream_seq renewed: %u", g_stream_seq);
}

/** 触发发送回调 */
static void trigger_send(void)
{
    if (!g_send_cb || g_buf_len == 0) return;

    uint16_t seq = g_stream_seq;
    ESP_LOGI(TAG, "Auto-send triggered: len=%u seq=%u", g_buf_len, seq);

    g_send_cb(g_buf, g_buf_len, seq, g_user_ctx);

    /* 发送后清空缓冲，递增并重新随机 seq */
    g_buf_len = 0;
    g_stream_seq++;
    renew_stream_seq();
    g_last_input_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* ── 公开 API ────────────────────────────────────────────── */

void morse_buffer_init(morse_buffer_send_cb_t send_cb, void *user_ctx)
{
    g_send_cb = send_cb;
    g_user_ctx = user_ctx;
    g_buf_len = 0;
    g_last_input_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    renew_stream_seq();
    g_inited = true;
    ESP_LOGI(TAG, "Morse buffer ready (capacity=%u, packet_max=%u, auto_send=%ums)",
             MORSE_BUFFER_SIZE, MORSE_BUF_PACKET_MAX, MORSE_AUTO_SEND_MS);
}

void morse_buffer_push_pattern(const char *pattern)
{
    if (!g_inited || !pattern || pattern[0] == '\0') return;

    uint16_t pat_len = (uint16_t)strlen(pattern);

    /* 计算所需空间：已有数据 + 空格(如需) + pattern + null */
    uint16_t need = g_buf_len;
    if (g_buf_len > 0) need += 1;   /* 前导空格 */
    need += pat_len;

    if (need >= MORSE_BUFFER_SIZE) {
        ESP_LOGW(TAG, "Buffer overflow prevented: need=%u max=%u", need, MORSE_BUFFER_SIZE);
        /* 强制发送以释放空间 */
        morse_buffer_flush();
        /* 刷新后重试 */
        need = pat_len;
        if (need >= MORSE_BUFFER_SIZE) return;
    }

    /* 追加前导空格（字符间隔） */
    if (g_buf_len > 0) {
        g_buf[g_buf_len++] = ' ';
    }

    /* 追加模式字符串 */
    memcpy(&g_buf[g_buf_len], pattern, pat_len);
    g_buf_len += pat_len;

    g_last_input_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

    ESP_LOGD(TAG, "Push '%s' → buf=%u/%u", pattern, g_buf_len, MORSE_BUFFER_SIZE);

    /* 检查是否达到单包上限 */
    if (g_buf_len >= MORSE_BUF_PACKET_MAX) {
        ESP_LOGI(TAG, "Buffer full (%u >= %u), auto-sending", g_buf_len, MORSE_BUF_PACKET_MAX);
        trigger_send();
    }
}

void morse_buffer_push_word_gap(void)
{
    if (!g_inited) return;

    /* 单词间隔：追加额外空格（已有字符间隔空格，再加一个形成双空格） */
    if (g_buf_len > 0 && g_buf_len + 1 < MORSE_BUFFER_SIZE) {
        g_buf[g_buf_len++] = ' ';
        g_last_input_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    }
}

void morse_buffer_tick(void)
{
    if (!g_inited || g_buf_len == 0) return;

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    uint32_t idle_ms = now_ms - g_last_input_ms;

    if (idle_ms >= MORSE_AUTO_SEND_MS) {
        ESP_LOGI(TAG, "Idle timeout (%lums >= %ums), auto-sending",
                 (unsigned long)idle_ms, MORSE_AUTO_SEND_MS);
        trigger_send();
    }
}

void morse_buffer_flush(void)
{
    if (!g_inited || g_buf_len == 0) return;
    ESP_LOGI(TAG, "Manual flush: len=%u seq=%u", g_buf_len, g_stream_seq);
    trigger_send();
}

void morse_buffer_clear(void)
{
    g_buf_len = 0;
    g_last_input_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    ESP_LOGD(TAG, "Buffer cleared");
}

uint16_t morse_buffer_get_fill(void)
{
    return g_buf_len;
}

uint16_t morse_buffer_get_stream_seq(void)
{
    return g_stream_seq;
}

const uint8_t *morse_buffer_get_data(void)
{
    return g_buf;
}

uint32_t morse_buffer_get_idle_ms(void)
{
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    return now_ms - g_last_input_ms;
}
