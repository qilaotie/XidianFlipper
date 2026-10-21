/**
 * @file    motor_pwm.h
 * @brief   振动马达 PWM 驱动（LEDC）— V5.4 阶段三 【已禁用】
 *
 * 硬件：MOTOR_PWM（GPIO22），外部 100k 下拉。
 * 使用 LEDC（LED PWM 控制器）— MCPWM 已由蜂鸣器占用。
 *
 * ⚠️ 马达功能已禁用（2026-06-16）。所有函数为空桩实现。
 *    恢复时取消 motor_pwm.c 中的 #if 0 并恢复 pins.h 中的 MOTOR_PWM 定义。
 */

#ifndef MOTOR_PWM_H
#define MOTOR_PWM_H

#include <stdint.h>
#include <stdbool.h>

/* ── 空桩函数（马达已禁用）────────────────────────────────── */

static inline void motor_init(void) { }

static inline void motor_start(void) { }

static inline void motor_stop(void) { }

static inline void motor_set_duty(uint8_t duty_pct) { (void)duty_pct; }

static inline bool motor_is_running(void) { return false; }

#endif /* MOTOR_PWM_H */
