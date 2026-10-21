/**
 * @file    oled_power.c
 * @brief   OLED 电源门控实现 — V7.0
 *
 * 委托给 load_switch 模块集中管理 GPIO4（WS4621C 负载开关）。
 * OLED 与 WS2812 共用同一负载开关，load_switch 负责协调。
 *
 * 关屏时序（重要）：
 *   1. 发送 SSD1306 Display OFF 命令（I2C，通过 U8G2）
 *   2. 等待 100ms（确保命令完成 + 电荷泵放电）
 *   3. 释放负载开关（若 RGB 也未使用则关断 GPIO4）
 */

#include "oled_power.h"
#include "load_switch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "u8g2.h"
#include "wououi.h"     /* ui.param[DISP_BRI] 用户亮度 */

static const char *TAG = "oled_pwr";

void oled_power_on(void)
{
    /* 委托给 load_switch — 自动处理 GPIO4 上电 + 150ms 稳定 */
    load_switch_acquire(LOAD_SW_OLED);
    ESP_LOGD(TAG, "OLED power ON (delegated to load_switch)");
}

void oled_power_off(void)
{
    /*
     * 关屏时序（统一入口）：
     *   1. 发送 Display OFF 命令 → SSD1306 进入 sleep mode
     *   2. 等待 100ms → 确保 I2C 命令完成 + 内部电荷泵放电
     *   3. 释放负载开关 → 若 RGB 也未使用则 OLED_3V3 断电
     */
    u8g2_SetPowerSave(&u8g2, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    load_switch_release(LOAD_SW_OLED);
    ESP_LOGI(TAG, "OLED power OFF (display cmd sent + 100ms wait + load_switch release)");
}

void oled_power_hold_for_sleep(void)
{
    /*
     * DeepSleep 关屏时序：
     *   1. 发送 Display OFF 命令
     *   2. 等待 100ms
     *   3. 强制关断负载开关 + GPIO Hold（忽略引用计数）
     */
    u8g2_SetPowerSave(&u8g2, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    load_switch_hold_for_sleep();
    ESP_LOGI(TAG, "OLED_EN held LOW for DeepSleep (delegated to load_switch)");
}

void oled_display_restore(void)
{
    /*
     * 关屏待机恢复时序：
     *   1. 负载开关重新上电（含 150ms 稳定等待）
     *   2. 重发 SSD1306 初始化序列 — OLED_3V3 被切断后
     *      控制器寄存器全部丢失，必须完整 InitDisplay
     *   3. 退出 Display OFF + 恢复用户亮度
     */
    load_switch_acquire(LOAD_SW_OLED);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetContrast(&u8g2, ui.param[DISP_BRI]);
    ESP_LOGI(TAG, "OLED display restored (re-init + power save off)");
}
