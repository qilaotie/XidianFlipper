/**
 * @file    pins.h
 * @brief   GPIO 引脚定义 — V5.4 最终锁定版
 *
 * 引脚分配按 hardware/hardware.md 与原始网表核对；变更时同步硬件标识和文档。
 */

#ifndef PINS_H
#define PINS_H

#include "driver/gpio.h"

/* ── OLED 显示屏（SSD1306，128×32；实际 I2C 时钟见 HAL）── */
#define OLED_EN         GPIO_NUM_4      /* 电源使能，外部 100k 下拉 */
#define I2C_SDA         GPIO_NUM_5      /* 网表 R7=4.7k 上拉到 OLED_3V3 */
#define I2C_SCL         GPIO_NUM_6      /* 网表 R8=4.7k 上拉到 OLED_3V3 */
#define I2C_NUM         0               /* I2C 端口 0 */
#define I2C_FREQ_HZ     600000UL        /* 与 HAL 的 700kHz 定义不一致；待实机核验，本次不改值 */

/* ── RGB LED（WS2812）───────────────────────────────────── */
#define RGB_DIN         GPIO_NUM_7      /* 网表直接接 LED1.3，R21=10k 下拉，未列串联 1k */
/* RGB 与 OLED 共用 WS4621C 负载开关（GPIO4/OLED_EN），无需独立电源控制脚 */

/* ── 旋转编码器 ─────────────────────────────────────────── */
#define ENC_A           GPIO_NUM_2      /* 外部 100k 上拉 */
#define ENC_B           GPIO_NUM_3      /* 外部 100k 上拉 */
#define ENC_BTN         GPIO_NUM_18     /* 外部 100k 上拉；C6 非 RTC GPIO，深睡使用定时轮询 */

/* ── 独立按键 ────────────────────────────────────────────── */
#define KEY             GPIO_NUM_19     /* 外部 100k 上拉；C6 非 RTC GPIO，深睡使用定时轮询 */

/* ── 按键内部上拉开关 ─────────────────────────────────────
 * 设为 1：启用内部上拉（GPIO2/3/18/19），与外部上拉并联
 * 设为 0：仅依赖外部上拉（默认硬件设计）
 * 注意：内部上拉约 45kΩ，与外部 100kΩ 并联后约 31kΩ，不影响正常使用
 * ───────────────────────────────────────────────────────── */
#define INPUT_PULLUP_ENABLE  0

/* ── 初始化阶段临时上下拉开关 ─────────────────────────────
 * 设为 1：gpio_init_all() 开始时，对所有标注了外部上下拉的引脚
 *         先应用匹配的内部弱上下拉，短暂稳定后再执行正式配置。
 *         确保引脚在外部电路稳定前处于安全电平。
 * 设为 0：跳过临时上下拉，直接执行正式配置。
 *
 * 涉及的引脚：
 *   临时上拉：ENC_A/B(GPIO2/3), ENC_BTN(GPIO18), KEY(GPIO19),
 *             I2C_SDA/SCL(GPIO5/6) — 匹配外部上拉电阻
 *   临时下拉：OLED_EN(GPIO4), RGB_DIN(GPIO7),
 *             AUDIO_P/N(GPIO20/21) — 匹配外部下拉电阻
 *   跳过：CHG_STAT(GPIO0) — 已有专用临时上拉逻辑(adc_battery.c)
 *         MOTOR_PWM(GPIO22) — 已禁用
 * ───────────────────────────────────────────────────────── */
#define TEMP_PULL_INIT_ENABLE  1

/* ── 蜂鸣器（压电式，MCPWM 互补输出）─────────────────────── */
#define AUDIO_P         GPIO_NUM_20     /* 外部 100Ω 串联 + 100k 下拉 */
#define AUDIO_N         GPIO_NUM_21     /* 外部 100Ω 串联 + 100k 下拉 */

/* ── 振动马达（已禁用）──────────────────────────────────────
 * #define MOTOR_PWM       GPIO_NUM_22     // 外部 100k 下拉
 * ── */

/* ── USB ────────────────────────────────────────────────────── */
#define USB_DN          GPIO_NUM_12
#define USB_DP          GPIO_NUM_13

/* ── 调试 / 启动引脚 ─────────────────────────────────────── */
//#define DEBUG_GPIO      GPIO_NUM_0       /* 已停止使用 */

/* ── 充电状态检测（MCP73831 STAT 经分压电阻）──────────────── */
#define CHG_STAT        GPIO_NUM_0      /* U7.12；网表 R4=1MΩ 到 STAT，R5=2MΩ 到 GND。仅输入，不 Hold。 */

/* ── 历史兼容掩码：含 C6 不支持 EXT1 的 GPIO18/19，不可直接配置唤醒 ── */
#define WAKEUP_EXT1_MASK    ((1ULL << ENC_BTN) | (1ULL << KEY) | (1ULL << ENC_A) | (1ULL << ENC_B))

/* 历史兼容按键掩码；当前 DeepSleep 通过 500ms timer 检查 GPIO18/19。
 * 保留常量以兼容旧代码，不代表这些引脚支持 C6 EXT1。 */
#define WAKEUP_DEEPSLEEP_MASK   ((1ULL << ENC_BTN) | (1ULL << KEY))

/* ── Hold 掩码：DeepSleep 期间需要保持低电平的引脚 ──────── */
#define HOLD_LOW_MASK       ((1ULL << OLED_EN) | (1ULL << RGB_DIN))

#endif /* PINS_H */
