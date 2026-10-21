/**
 * @file    oled_power.h
 * @brief   OLED 电源门控 — GPIO4 控制 WS4621C 负载开关 — V6.0
 *
 * OLED_3V3 由 GPIO4 通过 WS4621C 负载开关独立控制。
 * 上电时序：GPIO4 HIGH → 等 150ms（RC 稳定 + 电荷泵启动）→ U8g2 init
 * 下电时序：U8g2 Display OFF → 等 100ms → GPIO4 LOW
 *
 * DeepSleep 期间 GPIO4 必须 HOLD LOW，防止 OLED 漏电。
 */

#ifndef OLED_POWER_H
#define OLED_POWER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 开启 OLED 电源。
 *
 * 执行序列：
 *   1. 释放 GPIO4 的 Hold（DeepSleep 残留）
 *   2. GPIO4 输出 HIGH → WS4621C 导通 → OLED_3V3 上电
 *   3. 等待 150ms（RC 复位 τ=22ms, 5τ=110ms + 余量）
 *
 * @note 此函数仅管理电源，不初始化 I2C 或 SSD1306。
 *       I2C 和 SSD1306 初始化由 U8g2 HAL 回调处理。
 */
void oled_power_on(void);

/**
 * @brief 关闭 OLED（统一关屏入口）。
 *
 * 执行序列：
 *   1. U8g2 发送 SSD1306 Display OFF 命令（I2C sleep mode）
 *   2. 等待 100ms（命令完成 + 电荷泵放电）
 *   3. GPIO4 输出 LOW → WS4621C 关断 → OLED_3V3 断电
 *
 * @note 此函数为所有关屏场景的统一入口。
 *       若需进入 DeepSleep，请改用 oled_power_hold_for_sleep()。
 */
void oled_power_off(void);

/**
 * @brief 为 DeepSleep 关闭 OLED 并 Hold GPIO4。
 *
 * 执行序列同 oled_power_off()，最后额外调用 gpio_hold_en(OLED_EN)。
 * 外部 100k 下拉 + HOLD LOW → DeepSleep 期间 WS4621C 完全关断。
 * 必须在 esp_deep_sleep_start() 之前调用。
 */
void oled_power_hold_for_sleep(void);

/**
 * @brief 关屏待机后恢复显示（重新上电 + 完整重新初始化）。
 *
 * 执行序列：
 *   1. load_switch_acquire(LOAD_SW_OLED) → GPIO4 HIGH + 150ms 稳定
 *   2. u8g2_InitDisplay() → 重发 SSD1306 初始化序列
 *      （OLED_3V3 曾被切断，控制器寄存器状态已丢失，仅
 *        SetPowerSave(0) 不足以恢复）
 *   3. u8g2_SetPowerSave(0) → 退出 Display OFF
 *   4. u8g2_SetContrast(ui.param[DISP_BRI]) → 恢复用户亮度
 *
 * 调用后由 UI 任务的下一帧全量重绘（ClearBuffer+SendBuffer）。
 * 与 oled_power_on() 的区别：本函数包含控制器重新初始化，
 * 专用于"芯片未复位、但 OLED 被断过电"的关屏待机恢复场景；
 * oled_power_on() 仅管理上电，用于开机时尚未 u8g2_Setup 的路径。
 */
void oled_display_restore(void);

#ifdef __cplusplus
}
#endif

#endif /* OLED_POWER_H */
