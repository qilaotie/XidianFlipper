/**
 * @file    morse_buffer.h
 * @brief   摩斯电码输入缓冲区 — V7.0 MORSE V1.1
 *
 * 位于 morse_engine（点划解码）与 morse_tx（发送）之间。
 * 累积多个字符的点/划模式，满足条件时自动触发发送。
 *
 * 自动发送条件（满足任一即触发）：
 *   1. 静默超过 3 秒（morse_buffer_tick 检测）
 *   2. 缓冲数据达到 60 字节（MORSE_PAYLOAD_DATA_MAX）
 *
 * stream_seq 每次 auto-send 后重新随机，禁止全局递增。
 */

#ifndef MORSE_BUFFER_H
#define MORSE_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 常量 ────────────────────────────────────────────────── */

/** 输入缓冲区大小（字节） */
#define MORSE_BUFFER_SIZE       512

/** 自动发送空闲超时（毫秒） */
#define MORSE_AUTO_SEND_MS      3000

/** 单包最大数据量（与 morse_payload_t.morse_data 容量一致） */
#define MORSE_BUF_PACKET_MAX    60

/* ── 回调类型 ────────────────────────────────────────────── */

/**
 * @brief 自动发送回调。
 * @param data        待发送的点/划模式文本（如 ".- -... -.-."）。
 * @param len         数据长度（字节）。
 * @param stream_seq  本次发送的流序列号。
 * @param user_ctx    用户上下文指针。
 */
typedef void (*morse_buffer_send_cb_t)(const uint8_t *data, uint16_t len,
                                       uint16_t stream_seq, void *user_ctx);

/* ── API ─────────────────────────────────────────────────── */

/**
 * @brief 初始化摩斯缓冲区。
 *
 * 清空缓冲区，生成随机初始 stream_seq。
 *
 * @param send_cb   自动发送触发时的回调（不可为 NULL）。
 * @param user_ctx  传递给回调的用户上下文。
 */
void morse_buffer_init(morse_buffer_send_cb_t send_cb, void *user_ctx);

/**
 * @brief 追加一个解码后的点/划模式。
 *
 * 内部自动在模式间插入空格分隔。
 * 若追加后缓冲区达到 MORSE_BUF_PACKET_MAX，立即触发发送。
 *
 * @param pattern  点/划字符串（如 ".-" 表示 'A'）。不可为 NULL。
 */
void morse_buffer_push_pattern(const char *pattern);

/**
 * @brief 追加一个空格（单词间隔）。
 *
 * 在缓冲区中插入额外空格，用于分隔单词。
 */
void morse_buffer_push_word_gap(void);

/**
 * @brief 周期性检查（由主循环调用，~100ms 一次）。
 *
 * 检测空闲超时（last_input_ms + MORSE_AUTO_SEND_MS），
 * 超时且有数据则自动触发发送。
 */
void morse_buffer_tick(void);

/**
 * @brief 强制立即发送当前缓冲区内容。
 *
 * 发送后清空缓冲区并重新随机 stream_seq。
 */
void morse_buffer_flush(void);

/**
 * @brief 清空缓冲区（不发送）。
 */
void morse_buffer_clear(void);

/**
 * @brief 获取当前缓冲区填充量（字节）。
 */
uint16_t morse_buffer_get_fill(void);

/**
 * @brief 获取当前 stream_seq。
 */
uint16_t morse_buffer_get_stream_seq(void);

/**
 * @brief 获取缓冲区原始指针（只读，供 UI 渲染用）。
 */
const uint8_t *morse_buffer_get_data(void);

/**
 * @brief 获取自上次输入以来的静默时间（毫秒）。
 */
uint32_t morse_buffer_get_idle_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* MORSE_BUFFER_H */
