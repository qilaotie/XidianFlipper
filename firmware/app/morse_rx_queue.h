/**
 * @file    morse_rx_queue.h
 * @brief   摩斯电码接收队列 — V7.0 MORSE V1.1
 *
 * FIFO 队列，容量 32 条。接收端收到 TYPE_MORSE 包后经过去重，
 * 入队等待蜂鸣器回放和 OLED 显示。先收到先播放。
 *
 * 同时提供 stream_seq 跳跃检测（漏包提示）。
 */

#ifndef MORSE_RX_QUEUE_H
#define MORSE_RX_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 常量 ────────────────────────────────────────────────── */

/** 接收队列最大容量 */
#define MORSE_RX_QUEUE_MAX          32

/** 单条消息数据最大长度 */
#define MORSE_RX_DATA_MAX           64

/* ── 队列条目 ────────────────────────────────────────────── */

typedef struct {
    uint32_t src_id;                        /**< 发送方节点 ID */
    uint32_t msg_id;                        /**< Mesh 层消息 ID */
    uint16_t stream_seq;                    /**< 流序列号（漏包检测用） */
    int8_t   rssi;                          /**< 接收信号强度（dBm） */
    uint32_t rx_time;                       /**< 接收时刻（系统 ms） */
    uint16_t len;                           /**< morse_data 有效长度 */
    uint8_t  data[MORSE_RX_DATA_MAX];       /**< 摩斯点/划编码数据 */
} morse_rx_msg_t;

/* ── API ─────────────────────────────────────────────────── */

/**
 * @brief 初始化接收队列。
 */
void morse_rx_queue_init(void);

/**
 * @brief 入队一条消息（FIFO，满时丢弃最旧）。
 *
 * @param msg  待入队的消息指针（深拷贝）。
 * @return     入队成功返回 true，队列满返回 false。
 */
bool morse_rx_queue_push(const morse_rx_msg_t *msg);

/**
 * @brief 出队一条消息（从队首移除）。
 *
 * @param[out] msg  出队消息的拷贝目标。可为 NULL（仅移除不读取）。
 * @return          出队成功返回 true，队列空返回 false。
 */
bool morse_rx_queue_pop(morse_rx_msg_t *msg);

/**
 * @brief 查看队首消息（不出队）。
 *
 * @param[out] msg  消息拷贝目标。
 * @return          队列非空返回 true。
 */
bool morse_rx_queue_peek(morse_rx_msg_t *msg);

/**
 * @brief 获取当前队列中消息数量。
 */
uint8_t morse_rx_queue_count(void);

/**
 * @brief 检查队列是否为空。
 */
bool morse_rx_queue_is_empty(void);

/**
 * @brief 清空队列。
 */
void morse_rx_queue_clear(void);

/**
 * @brief 检测 stream_seq 跳跃（漏包）。
 *
 * 对比上一包的 stream_seq 与本包，若跳跃 > 1 则表示有包丢失。
 * 调用者应先 push 再 check_miss，传入当前最新 seq。
 *
 * @param src_id      发送方节点 ID。
 * @param stream_seq  当前收到的 stream_seq。
 * @return            缺失的包数（0 = 无缺失，>0 = 缺失数量）。
 */
uint16_t morse_rx_queue_check_miss(uint32_t src_id, uint16_t stream_seq);

#ifdef __cplusplus
}
#endif

#endif /* MORSE_RX_QUEUE_H */
