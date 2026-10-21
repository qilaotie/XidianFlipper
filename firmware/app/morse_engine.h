/**
 * @file    morse_engine.h
 * @brief   莫尔斯电码编码/解码器 — V5.7 阶段八 (WPM 动态化)
 *
 * 使用 KEY（GPIO19）边沿中断 + RTC 定时器实现精确定时。
 * 将点/划序列解码为字符。
 * 支持运行时 WPM 调整（morse_engine_set_wpm），所有时序阈值动态跟随。
 * 空闲超时后自动发送，或双击 ENC_BTN 强制发送。
 *
 * WPM 与时序关系：
 *   T = 1200 / WPM（毫秒）
 *   点 ≤ 1.33T、划 > 1.33T、元素间隔 = 2.5T、自动发送 = 10T
 */

#ifndef MORSE_ENGINE_H
#define MORSE_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

/* ── 莫尔斯时序常量（默认 12 WPM）────────────────────────── */
#define MORSE_DEFAULT_WPM       12      /**< 默认码速（词/分钟） */
#define MORSE_DOT_MIN_MS        20      /**< 最小点持续时间（消抖下限） */

/* ── 莫尔斯符号常量 ──────────────────────────────────────── */
#define MORSE_MAX_LEN           8       /**< 每字符最大点数/划数 */

/** 莫尔斯字符解码回调 */
typedef void (*morse_decode_cb_t)(char ch, void *user_ctx);

/** 完整消息准备发送回调 */
typedef void (*morse_send_cb_t)(const char *sequence, void *user_ctx);

/** 单符号解码回调（教学模式用：每输入一个符号就回调） */
typedef void (*morse_symbol_cb_t)(char symbol, void *user_ctx);

/**
 * @brief 获取当前 WPM 对应的基础单位 T（毫秒）。
 * @return T = 1200 / current_wpm
 */
uint32_t morse_engine_get_base_t_ms(void);

/**
 * @brief 获取当前点/划判别阈值（毫秒）。
 * @return 1.33 × base_t
 */
uint32_t morse_engine_get_threshold_ms(void);

/**
 * @brief 设置莫尔斯码速。
 *
 * 动态更新所有时序阈值（点/划判别、元素间隔、自动发送）。
 * @param wpm  码速（词/分钟），范围 5-40，自动钳位。
 */
void morse_engine_set_wpm(uint8_t wpm);

/**
 * @brief 获取当前 WPM 设置。
 */
uint8_t morse_engine_get_wpm(void);

/**
 * @brief 初始化莫尔斯引擎。
 *
 * 注册来自输入子系统的 KEY_DOWN/KEY_UP 事件。
 * 从 g_config.morse_wpm 读取初始 WPM。
 * @param decode_cb  字符解码时调用。
 * @param send_cb    自动发送触发时调用。
 * @param user_ctx   传递给回调的用户上下文。
 */
void morse_engine_init(morse_decode_cb_t decode_cb,
                       morse_send_cb_t send_cb,
                       void *user_ctx);

/**
 * @brief 向莫尔斯引擎送入按键事件。
 *
 * 由 ui_task 或输入消费者调用。非 ISR 函数。
 * @param evt_type    INPUT_EVT_KEY_DOWN 或 INPUT_EVT_KEY_UP。
 * @param duration_ms 按压持续时间（用于 KEY_UP）。
 */
void morse_engine_feed(int evt_type, uint32_t duration_ms);

/**
 * @brief 清空输入缓冲区。
 */
void morse_engine_clear(void);

/**
 * @brief 获取当前输入缓冲区长度。
 */
uint8_t morse_engine_get_buffer_len(void);

/**
 * @brief 根据点/划字符串查找对应字符（公开给译码端使用）。
 * @param pattern  点/划字符串（如 ".-"）。
 * @return         匹配的字符，未找到返回 '?'。
 */
char morse_lookup(const char *pattern);

/* ── 解码历史缓冲区（V5.8：自动译码功能）─────────────────── */

/** 解码历史最大字符数 */
#define MORSE_DECODE_HISTORY_MAX    256

/**
 * @brief 将解码字符追加到历史缓冲区（环形缓冲，满时覆盖最旧）。
 * @param ch  解码出的字符。
 */
void morse_decode_push(char ch);

/**
 * @brief 按索引获取历史缓冲区中的字符。
 * @param index  0 = 最旧，count-1 = 最新。
 * @return       字符，索引越界返回 '\0'。
 */
char morse_decode_get_char(uint16_t index);

/**
 * @brief 获取历史缓冲区中当前字符总数。
 */
uint16_t morse_decode_get_count(void);

/**
 * @brief 清空解码历史缓冲区。
 */
void morse_decode_clear(void);

#endif /* MORSE_ENGINE_H */
