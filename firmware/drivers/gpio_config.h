/**
 * @file    gpio_config.h
 * @brief   GPIO 初始化与 DeepSleep Hold 策略 — V5.4
 *
 * GPIO 与 Hold 策略见 hardware/hardware.md、docs/lessons-learned.md。
 * 警告：错误的 Hold 配置可能导致：
 *   - 静态漏电流（µA 级）
 *   - 电平冲突 → GPIO 永久损坏
 *   - GPIO0（CHG_STAT）输出 Hold 会改变外部采样网络，应保持输入
 */

#ifndef GPIO_CONFIG_H
#define GPIO_CONFIG_H

#include "driver/gpio.h"
#include "pins.h"

/**
 * @brief 将所有 GPIO 初始化到安全的上电状态。
 *
 * 冷启动时调用一次。设置方向、上下拉和初始输出电平。
 * 尚未启用 Hold — Hold 仅在 gpio_prepare_deep_sleep() 中应用。
 */
void gpio_init_all(void);

/**
 * @brief 为 DeepSleep 入口准备 GPIO。
 *
 * 应用 Hold 策略：
 *   - 外部下拉引脚 → 输出 LOW + HOLD
 *   - 外部上拉引脚 → 输入模式 + 不 Hold
 *   - CHG_STAT（GPIO0）→ 输入模式 + 不 Hold（安全关键）
 *   - AUDIO_P/N → 输出 LOW + 不 Hold
 *
 * 必须在所有外设停止之后、esp_deep_sleep_start() 之前调用。
 */
void gpio_prepare_deep_sleep(void);

/**
 * @brief 释放所有 GPIO Hold（唤醒后、重新初始化前调用）。
 */
void gpio_release_deep_sleep_hold(void);

#endif /* GPIO_CONFIG_H */
