/**
 * @file    main.c
 * @brief   BLUE_LONG_MESH �?超低功�?BLE Mesh 手持终端
 *
 * ESP32-C6-MINI-1-N4  |  BLE 混合 Mesh  |  DeepSleep 周期唤醒
 * V5.5 架构 �?阶段六：好友碰一碰添�?+ 双向同步
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"       /* V6.52: 扫描窗口随机抖动 */
#include "firmware.h"
#include "morse_engine.h"      /* V5.8: morse_lookup for auto-decode */
#include "quick_msg.h"         /* V6.48: quick_msg_is_waiting() */

static const char *TAG = "main";

/* ══════════════════════════════════════════════════════════════
 *  V7.0: Morse 模式全局标志
 *  由 morse_input_proc 设置，主循环据此调整扫描占空比
 * ══════════════════════════════════════════════════════════════ */
bool g_morse_mode_active = false;

/* ══════════════════════════════════════════════════════════════
 *  阶段测试开�?(1 = 启用, 0 = 关闭)
 *
 *  重要：每个阶段的测试代码保留，通过宏控制。
 *  新阶段追加宏定义，旧阶段设为 0 即可跳过。
 * ══════════════════════════════════════════════════════════════ */
#define PHASE1_OLED_SPLASH_ENABLED  1   /* OLED 启动画面 — U8g2 验证 */
#define PHASE1_DEEPSLEEP_DEMO       0   /* 3秒后自动入眠演示 */
#define PHASE2_MESH_TEST_ENABLED    0   /* 阶段二：Mesh 收发测试 */
#define PHASE3_UI_TEST_ENABLED      1   /* 阶段三：UI 菜单测试 (�?Wake Slot) */
#define PHASE4_GATT_TEST_ENABLED    0   /* V9.0.1: GATT 已移除 */
#define PHASE5_OTA_TEST_ENABLED     0   /* V9.0.1: OTA-over-GATT 已移除 */
#define PHASE5_STRESS_TEST_ENABLED  0   /* 阶段五：混合模式压力测试 (仅在调试时启用) */
#define PHASE6_FRIEND_TEST_ENABLED  1   /* 阶段六：碰一碰加好友测试 */

/* ── 唤醒原因辅助函数 ────────────────────────────────────── */
static void print_wakeup_cause(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
    case ESP_SLEEP_WAKEUP_EXT1:
        ESP_LOGI(TAG, "Wake: EXT1 (key), GPIO mask 0x%llx",
                 esp_sleep_get_ext1_wakeup_status());
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        /* V10.6.2: 空闲轮询时定时器唤醒非常频繁（每 500ms），
         * 降为 DEBUG 级别避免串口噪音。ROM bootloader 输出无法抑制。 */
        ESP_LOGD(TAG, "Wake: RTC timer");
        break;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
        ESP_LOGI(TAG, "Wake: Cold boot / reset");
        break;
    default:
        ESP_LOGI(TAG, "Wake: other (cause=%d)", (int)cause);
        break;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  V6.53: main_loop_tick_lite — 阻塞期轻量基础服务
 *
 *  供阻塞式操作（RTS 握手/FAST_ACK）内部调用。
 *  仅执行硬实时必需任务，不操作 OLED、不处理 UI 状态机。
 *  调用间隔建议 ≤ 50ms。
 * ══════════════════════════════════════════════════════════════ */
void main_loop_tick_lite(void)
{
    /* 看门狗喂食 — vTaskDelay 已隐式让出 CPU 给 idle task，无需显式操作 */
    /* 电池 ADC 采样 — 可延迟，跳过 */
    /* 按键 GPIO 扫描 — 可延迟，跳过 */
    /* 检查系统复位标志 — 无需操作 */
    /* 关键：vTaskDelay(1) 让出 CPU 给 NimBLE 任务和 idle task */
    vTaskDelay(1);
}

/* ── app_main 主函数 ─────────────────────────────────────── */
void app_main(void)
{
    g_wake_reason = WAKE_REASON_COLD_BOOT;
    gpio_release_deep_sleep_hold();
    gpio_init_all();
    gpio_set_level(AUDIO_P, 0);
    gpio_set_level(AUDIO_N, 0);
    gpio_set_direction(AUDIO_P, GPIO_MODE_OUTPUT);
    gpio_set_direction(AUDIO_N, GPIO_MODE_OUTPUT);
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    print_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_TIMER && g_idle_deep_sleep_active) {
        power_mgmt_poll_and_wake(); /* Returns only for a confirmed wake key. */
    } else {
        g_idle_deep_sleep_active = false;
        if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) power_set_cold_boot_time();
        else g_wake_reason = WAKE_REASON_IDLE_KEY;
    }
    firmware_init_all();
    power_mgmt_init();
    if (g_wake_reason == WAKE_REASON_COLD_BOOT) haptic_play(HAPTIC_BOOT);
    ui_task_create();
    /* Radio, input, playback and UI each have an owner. Never schedule scans here. */
    while (1) {
#if LOG_SYSTEM_ENABLE
        log_manager_system_diag();
#endif
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
