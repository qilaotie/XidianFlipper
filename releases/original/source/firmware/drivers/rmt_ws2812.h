/**
 * @file    rmt_ws2812.h
 * @brief   通过 RMT 驱动的 WS2812C RGB LED
 *
 * 硬件连接：
 *   RGB_DIN    (GPIO7) — WS2812 数据输入，网表中有 10k 下拉
 *   RGB 与 OLED 共用 WS4621C 负载开关（GPIO4/OLED_EN）
 *
 *   - 每帧前后各 400us 低电平复位，连续调用也满足锁存时序
 *   - 传输路径互斥锁保护；超时后保留缓冲区至传输完成
 *   - 负载开关由调用者管理
 *
 * DeepSleep 序列：
 *   1. 清除灯带 → 等待 RMT 发送完成
 *   2. GPIO7 输出 LOW + gpio_hold_en()
 */

#ifndef RMT_WS2812_H
#define RMT_WS2812_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 RMT 通道和 WS2812。
 *
 * 配置内容：
 *   - GPIO7 上的 RMT TX 通道
 *   - LED 数量 = 1
 *   - 不 acquire 负载开关；调用者在供电稳定后发送首帧
 */
void rgb_init(void);

/**
 * @brief 将单颗 LED 设置为 8 位 RGB 颜色（应用全局亮度缩放）。
 *
 * 推荐上层模块使用 rgb_led 引擎，而非直接调用此函数。
 *
 * @param r  红色 0..255
 * @param g  绿色 0..255
 * @param b  蓝色 0..255
 */
void rgb_set_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 直接设置颜色（不经过亮度缩放，不管理负载开关）。
 *
 * 仅供实时性要求极高的场景使用（如 morse_tutor 教学闪光）。
 * 调用者需自行管理负载开关的 acquire/release。
 *
 * @param r  红色 0..255
 * @param g  绿色 0..255
 * @param b  蓝色 0..255
 */
void rgb_set_color_direct(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 立即关闭 LED。
 *
 * 调用 rgb_set_color(0,0,0) 发送全黑数据。
 */
void rgb_off(void);

/** Last successfully transmitted color, after brightness scaling. */
void rgb_get_output(uint8_t *r, uint8_t *g, uint8_t *b);

/**
 * @brief 设置全局亮度百分比。
 *
 * 后续所有 rgb_set_color() 调用会自动缩放 RGB 值。
 * 影响所有上层灯效（rgb_led 引擎自动继承）。
 *
 * @param pct  亮度百分比 0-100
 */
void rgb_set_brightness(uint8_t pct);

/**
 * @brief 获取当前全局亮度百分比。
 * @return 亮度百分比 0-100
 */
uint8_t rgb_get_brightness(void);

/**
 * @brief [弃用] 阻塞式闪烁。
 *
 * @deprecated 请使用 rgb_led_trigger() 非阻塞引擎。
 * @param duration_ms  闪烁持续时间（毫秒）。
 */
void rgb_flash(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif /* RMT_WS2812_H */
