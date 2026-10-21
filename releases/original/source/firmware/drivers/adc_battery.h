/**
 * @file    adc_battery.h
 * @brief   通过 CHG_STAT(GPIO0) 检测 MCP73831 充电状态
 *
 * 硬件电路：
 *   MCP73831 STAT — R4(1MΩ) —+— GPIO0 (ESP32-C6)
 *                              |
 *                           R5(2MΩ)
 *                              |
 *                             GND
 *   理想浮空输入分压 V_GPIO = V_STAT × R5/(R4+R5) = V_STAT × 2/3
 *
 * MCP73831 STAT 为三态输出：
 *   - 充电中：    强 LOW   (~0V,  V_GPIO ≈ 0V)
 *   - 充电完成：  HIGH     (~5V,  V_GPIO ≈ 3.3V)
 *   - 关机：      High-Z   (浮空，由 R5 拉至 0V)
 *
 * 当前检测算法（电路、1M 串联导致的采样限制见 hardware/hardware.md，待实测）：
 *   1. 短暂使能 GPIO0 内部上拉（约 1 ms）
 *   2. 读取：HIGH → High-Z（仅电池）；LOW → 强下拉（充电中）
 *   3. 禁用上拉，再次读取：HIGH → 充电完成
 *   4. 恢复 GPIO0 为输入浮空（无上下拉，无 Hold）
 *
 * CHG_STAT 保持输入，不作输出或 Hold LOW，避免改变外部采样网络。
 */

#ifndef ADC_BATTERY_H
#define ADC_BATTERY_H

#include <stdint.h>

/** 充电状态枚举 */
typedef enum {
    CHARGE_OFF       = 0,  /**< 未充电 / 关机（STAT 高阻态） */
    CHARGE_CHARGING  = 1,  /**< 充电进行中（STAT LOW） */
    CHARGE_DONE      = 2,  /**< 充电完成（STAT HIGH） */
    CHARGE_ERROR     = 3,  /**< 检测错误 */
} charge_state_t;

/**
 * @brief 初始化 GPIO0 用于充电检测。
 *
 * 将 GPIO0 配置为输入模式，无内部上下拉。
 * 必须在 gpio_init_all() 之后调用一次。
 */
void charge_status_init(void);

/**
 * @brief 读取当前充电状态。
 *
 * 此函数短暂（约 1.5 ms）使能 GPIO0 内部上拉以探测
 * MCP73831 STAT 引脚状态，然后恢复 GPIO0 为输入浮空。
 * 可从任意任务安全调用。
 *
 * @return charge_state_t  当前充电状态。
 */
charge_state_t charge_status_read(void);

#endif /* ADC_BATTERY_H */
