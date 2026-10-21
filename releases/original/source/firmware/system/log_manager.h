/**
 * @file    log_manager.h
 * @brief   日志封装宏 + 运行时管理 API + 统计收集器 — V0.10.0
 *
 * 三层过滤架构：
 *   1. 编译期：LOG_MASTER_ENABLE → 模块/阶段开关 → LOG_LEVEL
 *   2. 运行时：log_manager_set_level() / log_manager_set_channel()
 *   3. ESP-IDF：esp_log_level_set() 底层控制
 *
 * 使用方式：
 *   LOG_TAG("mesh", LOG_INFO, "RX pkt src=0x%08lX", src_id);
 *   LOG_PHASE(2, "mesh", LOG_INFO, "Relay decision: %u%%", prob);
 *   LOG_DIAG("sys", "Free heap: %lu KB", heap_kb);
 */

#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "log_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════
 *  日志级别常量（与 ESP_LOG 对齐）
 * ══════════════════════════════════════════════════════════════ */

#define LOG_LVL_NONE                0
#define LOG_LVL_ERROR               1
#define LOG_LVL_WARN                2
#define LOG_LVL_INFO                3
#define LOG_LVL_DEBUG               4
#define LOG_LVL_VERBOSE             5

/* ══════════════════════════════════════════════════════════════
 *  编译期级别检查宏（内部使用）
 * ══════════════════════════════════════════════════════════════ */

#define LOG_LEVEL_ALLOWS(lvl)       ((lvl) <= LOG_LEVEL)

/* ══════════════════════════════════════════════════════════════
 *  日志封装宏
 *
 *  LOG_TAG(tag, level, fmt, ...)
 *    - 通用模块级日志，受 LOG_MASTER_ENABLE + LOG_LEVEL 控制
 *    - 推荐新代码使用此宏替代原始 ESP_LOGx
 *
 *  LOG_PHASE(phase, tag, level, fmt, ...)
 *    - 按阶段过滤的日志，额外受 LOG_PHASEn_ENABLE 控制
 *    - 用于各阶段自检代码
 *
 *  LOG_DIAG(tag, fmt, ...)
 *    - 诊断专用日志，固定 INFO 级别，受 LOG_DIAG_ENABLE 控制
 *    - 用于系统健康快照、统计汇总等非错误/告警类输出
 * ══════════════════════════════════════════════════════════════ */

#if LOG_MASTER_ENABLE

  /* ── LOG_TAG ──────────────────────────────────────────── */
  #define LOG_TAG(tag, level, fmt, ...)                                       \
      do {                                                                    \
          if (LOG_LEVEL_ALLOWS(level)) {                                      \
              if (level == LOG_LVL_ERROR)                                     \
                  ESP_LOGE(tag, fmt, ##__VA_ARGS__);                          \
              else if (level == LOG_LVL_WARN)                                 \
                  ESP_LOGW(tag, fmt, ##__VA_ARGS__);                          \
              else if (level == LOG_LVL_INFO)                                 \
                  ESP_LOGI(tag, fmt, ##__VA_ARGS__);                          \
              else if (level == LOG_LVL_DEBUG)                                \
                  ESP_LOGD(tag, fmt, ##__VA_ARGS__);                          \
              else                                                            \
                  ESP_LOGV(tag, fmt, ##__VA_ARGS__);                          \
          }                                                                   \
      } while (0)

  /* ── LOG_PHASE ────────────────────────────────────────── */
  #define LOG_PHASE(phase, tag, level, fmt, ...)                              \
      do {                                                                    \
          if (LOG_PHASE_ENABLED(phase) && LOG_LEVEL_ALLOWS(level)) {          \
              if (level == LOG_LVL_ERROR)                                     \
                  ESP_LOGE(tag, "[P%u] " fmt, (unsigned)(phase), ##__VA_ARGS__);\
              else if (level == LOG_LVL_WARN)                                 \
                  ESP_LOGW(tag, "[P%u] " fmt, (unsigned)(phase), ##__VA_ARGS__);\
              else if (level == LOG_LVL_INFO)                                 \
                  ESP_LOGI(tag, "[P%u] " fmt, (unsigned)(phase), ##__VA_ARGS__);\
              else if (level == LOG_LVL_DEBUG)                                \
                  ESP_LOGD(tag, "[P%u] " fmt, (unsigned)(phase), ##__VA_ARGS__);\
              else                                                            \
                  ESP_LOGV(tag, "[P%u] " fmt, (unsigned)(phase), ##__VA_ARGS__);\
          }                                                                   \
      } while (0)

  /* ── LOG_DIAG ─────────────────────────────────────────── */
  #if LOG_DIAG_ENABLE
    #define LOG_DIAG(tag, fmt, ...)   ESP_LOGI(tag, "[DIAG] " fmt, ##__VA_ARGS__)
  #else
    #define LOG_DIAG(tag, fmt, ...)   ((void)0)
  #endif

  /* ── 阶段启动 dump 宏 ────────────────────────────────── */
  #define LOG_PHASE_BANNER(phase, name)                                        \
      do {                                                                     \
          if (LOG_PHASE_ENABLED(phase)) {                                      \
              ESP_LOGI("main", "══════ Phase %u: %s ══════",                  \
                       (unsigned)(phase), name);                                \
          }                                                                    \
      } while (0)

#else /* LOG_MASTER_ENABLE == 0 */

  /* 全部展开为空 */
  #define LOG_TAG(tag, level, fmt, ...)           ((void)0)
  #define LOG_PHASE(phase, tag, level, fmt, ...)  ((void)0)
  #define LOG_DIAG(tag, fmt, ...)                 ((void)0)
  #define LOG_PHASE_BANNER(phase, name)           ((void)0)

#endif /* LOG_MASTER_ENABLE */

/* ── 阶段开关辅助宏 ──────────────────────────────────────── */
#define LOG_PHASE_ENABLED(p)                                                  \
    (((p) == 0 && LOG_PHASE0_ENABLE) ||                                        \
     ((p) == 1 && LOG_PHASE1_ENABLE) ||                                        \
     ((p) == 2 && LOG_PHASE2_ENABLE) ||                                        \
     ((p) == 3 && LOG_PHASE3_ENABLE) ||                                        \
     ((p) == 4 && LOG_PHASE4_ENABLE) ||                                        \
     ((p) == 5 && LOG_PHASE5_ENABLE) ||                                        \
     ((p) == 6 && LOG_PHASE6_ENABLE))

/* ══════════════════════════════════════════════════════════════
 *  运行时管理 API
 * ══════════════════════════════════════════════════════════════ */

/**
 * @brief 统计收集器结构体
 *
 * 部分字段使用 RTC_DATA_ATTR 持久化（boot_count、deepsleep_* 等）。
 */
typedef struct {
    /* ── 日志统计 ──────────────────────────────────────── */
    uint32_t log_bytes_written;         /**< USB 总输出字节数 */
    uint32_t log_lines_dropped;         /**< 因缓冲区满丢弃行数 */
    uint32_t log_rate_per_sec;          /**< 近期输出速率（行/秒） */

    /* ── Mesh 统计镜像 ─────────────────────────────────── */
    uint32_t mesh_pkt_rx;               /**< Mesh 包总接收数 */
    uint32_t mesh_pkt_tx;               /**< Mesh 包总发送数 */

    /* ── GATT 统计 ─────────────────────────────────────── */
    uint32_t gatt_connect_count;        /**< GATT 累计连接次数 */
    uint32_t gatt_disconnect_count;     /**< GATT 累计断开次数 */

    /* ── 电源统计（RTC 持久化）────────────────────────── */
    uint32_t deepsleep_enter_count;     /**< 进入 DeepSleep 次数 */
    uint32_t deepsleep_total_ms;        /**< 累计 DeepSleep 时长(ms) */

    /* ── 系统统计（NVS 持久化）─────────────────────────── */
    uint32_t boot_count;                /**< 设备启动次数 */
    uint32_t reset_reason;              /**< 上次复位原因（esp_reset_reason_t） */

    /* ── FreeRTOS 水位线快照（上次 system_diag 时的值）─── */
    uint32_t task_watermark_radio;      /**< radio_task 栈高水位（字节） */
    uint32_t task_watermark_ui;         /**< ui_task 栈高水位（字节） */
    uint32_t task_watermark_battery;    /**< battery_task 栈高水位（字节） */
    uint32_t task_watermark_anim;       /**< animation_task 栈高水位（字节） */

    /* ── 时间戳 ────────────────────────────────────────── */
    uint32_t last_diag_timestamp_ms;    /**< 上次 system_diag 时间戳 */
} log_diag_stats_t;

/**
 * @brief 日志通道位掩码（运行时控制）
 */
typedef enum {
    LOG_CH_USB      = (1 << 0),     /**< USB CDC 输出 */
    LOG_CH_MESH     = (1 << 1),     /**< Mesh 相关日志 */
    LOG_CH_GATT     = (1 << 2),     /**< GATT 相关日志 */
    LOG_CH_UI       = (1 << 3),     /**< UI 相关日志 */
    LOG_CH_POWER    = (1 << 4),     /**< 电源相关日志 */
    LOG_CH_SYSTEM   = (1 << 5),     /**< 系统诊断日志 */
    LOG_CH_INPUT    = (1 << 6),     /**< 输入事件日志（按键/编码器） */
} log_channel_t;

/**
 * @brief 初始化日志管理器。
 *
 * 必须在事件系统之后、其他模块之前调用。
 * 恢复 RTC 持久化统计，dump 编译期开关矩阵到 USB。
 */
void log_manager_init(void);

/**
 * @brief 设置运行时日志级别。
 * @param level  0=NONE ~ 5=VERBOSE
 */
void log_manager_set_level(uint8_t level);

/**
 * @brief 获取当前运行时日志级别。
 * @return 0-5
 */
uint8_t log_manager_get_level(void);

/**
 * @brief 设置日志通道开关。
 * @param channel  通道名（"usb"/"mesh"/"gatt"/"ui"/"power"/"system"）
 * @param enabled  true=开启, false=关闭
 */
void log_manager_set_channel(const char *channel, bool enabled);

/**
 * @brief 查询日志通道是否开启。
 * @param channel  通道名
 * @return true=开启
 */
bool log_manager_get_channel(const char *channel);

/**
 * @brief 获取诊断统计结构体（只读）。
 * @return 指向内部统计结构体的指针
 */
const log_diag_stats_t *log_manager_get_stats(void);

/**
 * @brief 记录事件到统计收集器。
 * @param event_type  事件类型字符串（"mesh_rx"/"mesh_tx"/"gatt_connect"等）
 * @param value       事件附加值
 */
void log_manager_record_event(const char *event_type, uint32_t value);

/**
 * @brief 输出系统健康全景（一次性 dump）。
 *
 * 输出内容：
 *   - Free heap (DRAM)、RTC Fast Memory 用量
 *   - 各 FreeRTOS Task 栈高水位
 *   - NVS 已用条目/扇区状态
 *   - Boot Count、复位原因、运行时长
 *   - 日志统计（丢帧/速率）
 *
 * 受 LOG_SYSTEM_ENABLE 编译开关和 LOG_CH_SYSTEM 通道开关双重控制。
 */
void log_manager_system_diag(void);

/**
 * @brief Dump 编译期开关矩阵到 USB（启动时调用一次）。
 */
void log_manager_dump_build_config(void);

#ifdef __cplusplus
}
#endif

#endif /* LOG_MANAGER_H */
