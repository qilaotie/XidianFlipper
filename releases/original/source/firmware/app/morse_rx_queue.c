/**
 * @file    morse_rx_queue.c
 * @brief   摩斯电码接收队列实现 — V7.0 MORSE V1.1
 *
 * 环形 FIFO 缓冲，容量 32 条。入队满时覆盖最旧条目。
 * stream_seq 跳跃检测按 (src_id) 独立跟踪。
 */

#include "morse_rx_queue.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "morse_rxq";

/* ── 环形队列 ────────────────────────────────────────────── */

static morse_rx_msg_t g_queue[MORSE_RX_QUEUE_MAX];
static uint8_t  g_head = 0;     /**< 队首（下一个出队位置） */
static uint8_t  g_count = 0;    /**< 当前队列中条目数 */
static bool     g_full = false; /**< 队列是否已满（head 即最旧） */

/* ── 每源 stream_seq 跟踪（漏包检测）────────────────────── */

#define MISS_TRACK_MAX  8   /**< 最多同时跟踪的源节点数 */

typedef struct {
    uint32_t src_id;
    uint16_t last_seq;
    bool     in_use;
} miss_track_t;

static miss_track_t g_miss_track[MISS_TRACK_MAX];

/* ── 公开 API ────────────────────────────────────────────── */

void morse_rx_queue_init(void)
{
    memset(g_queue, 0, sizeof(g_queue));
    memset(g_miss_track, 0, sizeof(g_miss_track));
    g_head = 0;
    g_count = 0;
    g_full = false;
    ESP_LOGI(TAG, "RX queue ready (capacity=%u, mem=%u bytes)",
             MORSE_RX_QUEUE_MAX, (unsigned)sizeof(g_queue));
}

bool morse_rx_queue_push(const morse_rx_msg_t *msg)
{
    if (!msg) return false;

    uint8_t write_pos;

    if (g_full) {
        /* 队列满：覆盖最旧条目（head 位置） */
        write_pos = g_head;
        g_head = (g_head + 1) % MORSE_RX_QUEUE_MAX;
        ESP_LOGW(TAG, "Queue full, overwriting oldest (src=0x%08lX seq=%u)",
                 (unsigned long)g_queue[write_pos].src_id,
                 g_queue[write_pos].stream_seq);
    } else {
        write_pos = (g_head + g_count) % MORSE_RX_QUEUE_MAX;
        g_count++;
        if (g_count >= MORSE_RX_QUEUE_MAX) {
            g_full = true;
        }
    }

    memcpy(&g_queue[write_pos], msg, sizeof(morse_rx_msg_t));

    ESP_LOGD(TAG, "Push: src=0x%08lX seq=%u rssi=%d count=%u",
             (unsigned long)msg->src_id, msg->stream_seq, msg->rssi, g_count);

    return true;
}

bool morse_rx_queue_pop(morse_rx_msg_t *msg)
{
    if (g_count == 0) return false;

    if (msg) {
        memcpy(msg, &g_queue[g_head], sizeof(morse_rx_msg_t));
    }

    g_head = (g_head + 1) % MORSE_RX_QUEUE_MAX;
    g_count--;
    g_full = false;

    return true;
}

bool morse_rx_queue_peek(morse_rx_msg_t *msg)
{
    if (g_count == 0 || !msg) return false;
    memcpy(msg, &g_queue[g_head], sizeof(morse_rx_msg_t));
    return true;
}

uint8_t morse_rx_queue_count(void)
{
    return g_count;
}

bool morse_rx_queue_is_empty(void)
{
    return (g_count == 0);
}

void morse_rx_queue_clear(void)
{
    g_head = 0;
    g_count = 0;
    g_full = false;
    ESP_LOGD(TAG, "Queue cleared");
}

uint16_t morse_rx_queue_check_miss(uint32_t src_id, uint16_t stream_seq)
{
    /* 查找或分配跟踪槽位 */
    miss_track_t *track = NULL;
    int free_slot = -1;

    for (int i = 0; i < MISS_TRACK_MAX; i++) {
        if (g_miss_track[i].in_use && g_miss_track[i].src_id == src_id) {
            track = &g_miss_track[i];
            break;
        }
        if (!g_miss_track[i].in_use && free_slot < 0) {
            free_slot = i;
        }
    }

    if (!track) {
        if (free_slot < 0) {
            /* 跟踪槽满，淘汰第一个 */
            free_slot = 0;
        }
        track = &g_miss_track[free_slot];
        track->src_id = src_id;
        track->last_seq = stream_seq;
        track->in_use = true;
        return 0;  /* 首次收到，无历史可比较 */
    }

    uint16_t last = track->last_seq;
    track->last_seq = stream_seq;

    /* 处理回绕（uint16_t 溢出） */
    if (stream_seq == last) {
        return 0;  /* 重复包（去重应已处理） */
    }

    /* 正常情况：stream_seq = last + 1 */
    if (stream_seq == last + 1) {
        return 0;
    }

    /* 回绕检测：last 接近 65535 而 stream_seq 接近 0 */
    if (last > 0xFF00 && stream_seq < 0x00FF) {
        return 0;  /* 自然回绕，非漏包 */
    }

    /* 跳跃：计算缺失数量 */
    uint16_t missed = (stream_seq > last) ? (stream_seq - last - 1) : 0;

    if (missed > 0) {
        ESP_LOGW(TAG, "MISS: src=0x%08lX last=%u current=%u missed=%u",
                 (unsigned long)src_id, last, stream_seq, missed);
    }

    return missed;
}
