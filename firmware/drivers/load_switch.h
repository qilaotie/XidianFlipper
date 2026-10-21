/**
 * @file    load_switch.h
 * @brief   负载开关集中管理 — V7.0
 *
 * GPIO4 → WS4621C 负载开关 → OLED_3V3 + WS2812_VDD
 * OLED 和 WS2812 共用同一负载开关，通过本模块协调二者的电源管理。
 *
 * 使用模式：
 *   load_switch_acquire(LOAD_SW_OLED);   // OLED 需要电源
 *   load_switch_acquire(LOAD_SW_RGB);    // RGB 需要电源
 *   // ... 使用中 ...
 *   load_switch_release(LOAD_SW_RGB);    // RGB 用完了
 *   load_switch_release(LOAD_SW_OLED);   // OLED 也用完了 → GPIO4 LOW
 *
 * 约束：
 *   - acquire 时若当前关断，先开 GPIO4 + 等 150ms 稳定
 *   - release 时双方都释放后才关断 GPIO4，避免冲突
 *   - OLED 主动关断时需先发 SSD1306 Display OFF 命令（由 oled_power 处理）
 */

#ifndef LOAD_SWITCH_H
#define LOAD_SWITCH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 负载开关客户端类型 */
typedef enum {
    LOAD_SW_OLED = 0,       /**< OLED 显示屏 */
    LOAD_SW_RGB  = 1,       /**< WS2812 RGB LED */
    LOAD_SW_MORSE = 2,      /**< Morse playback owns its RGB power independently */
    LOAD_SW_COUNT
} load_switch_client_t;

/**
 * @brief 初始化负载开关模块。
 *
 * 配置 GPIO4 为输出 LOW（默认关断）。
 * 在 firmware_init_all() 早期调用。
 */
void load_switch_init(void);

/**
 * @brief 请求开启负载开关。
 *
 * 若当前 GPIO4 为 LOW → 拉高 + 等待 150ms 稳定。
 * 若已开启 → 仅增加引用计数，无延迟。
 *
 * @param client  请求方（LOAD_SW_OLED 或 LOAD_SW_RGB）
 */
void load_switch_acquire(load_switch_client_t client);

/**
 * @brief 释放负载开关。
 *
 * 清除对应客户端的活跃标记。
 * 若双方都已释放 → GPIO4 LOW（关断负载开关）。
 *
 * @param client  释放方
 */
void load_switch_release(load_switch_client_t client);

/**
 * @brief 查询负载开关当前是否开启。
 * @return true=GPIO4 HIGH, false=GPIO4 LOW
 */
bool load_switch_is_on(void);

/**
 * @brief 为 DeepSleep 关闭负载开关并 Hold GPIO4。
 *
 * 强制关断（无视引用计数）+ gpio_hold_en(OLED_EN)。
 * 必须在 esp_deep_sleep_start() 之前调用。
 */
void load_switch_hold_for_sleep(void);

#ifdef __cplusplus
}
#endif

#endif /* LOAD_SWITCH_H */
