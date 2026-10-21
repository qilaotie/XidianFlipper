/**
 * @file    oled_mirror.h
 * @brief   OLED 无感投屏模块 — 编译开关控制 | 完全解耦
 *
 * 设备端仅做一件事：从 U8g2 帧缓冲取出数据，原样通过注入的发送回调发出。
 * 不解析、不封装像素、不关心分辨率和页面布局。
 *
 * 集成方式：
 *   1. CMake 定义 ENABLE_OLED_MIRROR 宏
 *   2. U8g2 初始化后调用 oled_mirror_begin(&u8g2, your_send_fn)
 *   3. 每次 u8g2_SendBuffer() 后调用 oled_mirror_send_frame()
 *
 * 协议（增强版，防控制台日志误触发）：
 *   [0xAA 0x55] [TYPE 1B] [LEN 2B LE] [PAYLOAD N bytes]
 *   TYPE: 0x00=图像帧  0x01=日志帧
 */

#ifndef OLED_MIRROR_H
#define OLED_MIRROR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 发送回调原型 ────────────────────────────────────────── */
/** 由应用层注入；仅完整帧被底层接受时返回 true。 */
typedef bool (*oled_mirror_send_t)(const uint8_t *data, size_t len);

/* ── 帧类型枚举 ──────────────────────────────────────────── */
#define OLED_MIRROR_FRAME_IMAGE  0x00  /**< U8g2 图像帧 */
#define OLED_MIRROR_FRAME_LOG    0x01  /**< 调试日志帧 */
#define OLED_MIRROR_FRAME_HELLO  0x10
#define OLED_MIRROR_FRAME_COMMAND 0x11
#define OLED_MIRROR_FRAME_ACK    0x12
#define OLED_MIRROR_FRAME_TELEMETRY 0x13
#define OLED_MIRROR_MAX_PAYLOAD 1024
typedef struct {
    uint32_t frames_sent, frames_replaced, packets_dropped, tx_failures;
    uint16_t control_pending;
} oled_mirror_stats_t;

/* ── 公开 API ───────────────────────────────────────────── */

#if defined(ENABLE_OLED_MIRROR) && ENABLE_OLED_MIRROR

/**
 * @brief 初始化投屏模块，绑定 U8g2 实例和发送回调
 * @param u8g2     U8g2 句柄指针（需已完成 InitDisplay）
 * @param send_fn  底层发送函数（如 usb_serial_jtag_write_bytes 的包装）
 */
void oled_mirror_begin(u8g2_t *u8g2, oled_mirror_send_t send_fn);

/**
 * @brief 发送当前帧缓冲（必须在 u8g2_SendBuffer() 之后调用）
 *
 * 自动从 U8g2 获取 buffer 指针和大小，添加同步头后发送。
 * 若未调用 oled_mirror_begin() 或 send_fn 为 NULL，则静默返回。
 */
void oled_mirror_send_frame(void);

/**
 * @brief 发送调试日志到 PC 端（仅镜像开启时有效）
 * @param msg  UTF-8 字符串，最大 255 字节
 */
void oled_mirror_send_log(const char *msg);
/** 有界异步封包；true表示已入队，USB成功帧数由TX任务更新。 */
bool oled_mirror_send_packet(uint8_t type, const uint8_t *data, size_t len);
uint32_t oled_mirror_frame_count(void);
void oled_mirror_get_stats(oled_mirror_stats_t *stats);

#else  /* ENABLE_OLED_MIRROR=0 或未定义 — 全部空内联 */

static inline void oled_mirror_begin(u8g2_t *u8g2, oled_mirror_send_t send_fn) {
    (void)u8g2; (void)send_fn;
}
static inline void oled_mirror_send_frame(void) {}
static inline void oled_mirror_send_log(const char *msg) { (void)msg; }
static inline bool oled_mirror_send_packet(uint8_t type, const uint8_t *data, size_t len) {
    (void)type; (void)data; (void)len; return false;
}
static inline uint32_t oled_mirror_frame_count(void) { return 0; }
static inline void oled_mirror_get_stats(oled_mirror_stats_t *stats) {
    if (stats) *stats = (oled_mirror_stats_t){0};
}

#endif /* ENABLE_OLED_MIRROR */

#ifdef __cplusplus
}
#endif

#endif /* OLED_MIRROR_H */
