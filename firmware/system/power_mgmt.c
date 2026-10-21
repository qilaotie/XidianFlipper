/* UI-owned manual display standby and explicit manual deep sleep. */
#include "power_mgmt.h"
#include "power_manager.h"
#include "load_switch.h"
#include "gpio_config.h"
#include "pins.h"
#include "constants.h"
#include "nvs_config.h"
#include "storage_nvs.h"
#include "ec06.h"
#include "mcpwm_buzzer.h"
#include "rmt_ws2812.h"
#include "rgb_led.h"
#include "oled_power.h"
#include "haptic_feedback.h"
#include "message_service.h"
#include "pet_service.h"
#include "morse_playback.h"
#include "morse_terminal.h"
#include "morse_training_ui.h"
#include "wououi.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "pwrmgmt";
volatile bool g_power_off_in_progress;
RTC_NOINIT_ATTR bool g_idle_deep_sleep_active;
wake_reason_t g_wake_reason = WAKE_REASON_COLD_BOOT;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_last_activity_us;
static bool s_inited;
static volatile bool s_screen_off;
static bool s_wake_requested;
static bool s_sleep_requested;
static bool s_standby_requested;
static int64_t s_input_guard_until;

void power_mgmt_init(void)
{
    s_last_activity_us = esp_timer_get_time();
    s_inited = true;
    s_screen_off = false;
    g_power_off_in_progress = false;
    g_idle_deep_sleep_active = false;
    if (g_wake_reason == WAKE_REASON_IDLE_KEY)
        s_input_guard_until = esp_timer_get_time() + 500000;
}

/* Input task only publishes intent; it never touches the I2C display. */
void power_mgmt_notify_activity(void)
{
    portENTER_CRITICAL(&s_lock);
    s_last_activity_us = esp_timer_get_time();
    if (s_screen_off) s_wake_requested = true;
    portEXIT_CRITICAL(&s_lock);
}

uint32_t power_mgmt_get_idle_sec(void)
{
    if (!s_inited) return 0;
    portENTER_CRITICAL(&s_lock);
    int64_t last = s_last_activity_us;
    portEXIT_CRITICAL(&s_lock);
    return (uint32_t)((esp_timer_get_time() - last) / 1000000);
}

bool power_mgmt_is_screen_off(void) { return s_screen_off; }
/* Legacy callers cannot re-enable automatic standby. */
bool power_mgmt_auto_standby_enabled(void) { return false; }
void power_mgmt_set_auto_standby(bool enabled)
{
    (void)enabled;
}
void power_mgmt_request_screen_off(void) { s_standby_requested = true; }
void power_mgmt_guard_input(void) { s_input_guard_until = esp_timer_get_time() + 500000; }

bool power_mgmt_input_guard(void)
{
    if (!s_input_guard_until) return false;
    int64_t now = esp_timer_get_time();
    if (ec06_encoder_is_held() || ec06_is_key_held())
        s_input_guard_until = now + 500000;
    if (now < s_input_guard_until) return true;
    s_input_guard_until = 0;
    return false;
}

bool power_mgmt_request_sleep(void)
{
#if DEEPSLEEP_FORCE_DISABLE
    return false;
#endif
    if (!s_inited || power_is_grace_period()) return false;
    portENTER_CRITICAL(&s_lock);
    s_sleep_requested = true;
    portEXIT_CRITICAL(&s_lock);
    return true;
}

static void manual_sleep(void)
{
    /* The radio owner must quiesce and persist before cutting power. */
    if (!message_service_prepare_sleep()) {
        ESP_LOGW(TAG, "Sleep postponed: message/relationship storage is busy");
        haptic_play(HAPTIC_ERROR);
        return;
    }
    nvs_write_all_data();
    if(!pet_service_flush()){
        message_service_resume();
        ESP_LOGW(TAG,"Sleep cancelled: pet state could not be saved");
        haptic_play(HAPTIC_ERROR);
        return;
    }
    nvs_config_save();
    if (g_config._dirty || eeprom.change) {
        message_service_resume();
        ESP_LOGW(TAG, "Sleep cancelled: settings could not be saved");
        haptic_play(HAPTIC_ERROR);
        return;
    }
    g_power_off_in_progress = true;
    morse_playback_stop();
    haptic_stop_all();
    buzzer_stop();
    rgb_led_stop();
    rgb_off();
    if (!s_screen_off) oled_power_off();
    s_screen_off = true;
    gpio_prepare_deep_sleep();
    load_switch_hold_for_sleep();
    gpio_set_direction(CHG_STAT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CHG_STAT, GPIO_FLOATING);
    esp_err_t err = esp_sleep_enable_timer_wakeup(POWER_MGMT_IDLE_POLL_US);
    if (err != ESP_OK) {
        /* Restart cleanly if the controller cannot arm the wake source. */
        ESP_LOGE(TAG, "Wake timer unavailable: %d; restarting", err);
        esp_restart();
    }
    g_idle_deep_sleep_active = true;
    esp_deep_sleep_start();
}

/* Called exclusively by ui_task, including when its display is off. */
void power_mgmt_tick(void)
{
    if (!s_inited) return;
    bool wake, sleep, standby;
    portENTER_CRITICAL(&s_lock);
    wake = s_wake_requested;
    sleep = s_sleep_requested;
    standby = s_standby_requested;
    s_wake_requested = false;
    s_sleep_requested = false;
    s_standby_requested = false;
    portEXIT_CRITICAL(&s_lock);
    if (sleep) { manual_sleep(); return; }
    if (s_screen_off && wake) {
        oled_display_restore();
        s_screen_off = false;
        haptic_play(HAPTIC_WINDOW_OPEN);
        s_input_guard_until = esp_timer_get_time() + 500000;
        return;
    }
#if OLED_FORCE_POWER == 0
    if (!s_screen_off && standby) {
        /* UI is the sole display owner; no delays waiting for another UI frame. */
        s_screen_off = true;
        if (morse_training_ui_is_active()) morse_training_ui_suspend_input();
        else if (ui.index == M_MORSE_INPUT || ui.index == M_MORSE_LEARN)
            morse_terminal_clear_input();
        haptic_play(HAPTIC_SLEEP);
        oled_power_off(); /* Releases only the OLED share, never the RGB share. */
    }
#endif
}

bool power_mgmt_poll_and_wake(void)
{
    if (!g_idle_deep_sleep_active) {
        return false;
    }

    /* 等待电平稳定：gpio_init_all() 刚执行完，外部 100k 上拉
     * 需要时间对引脚电容充电（τ≈0.5µs）。等待 3ms 确保万无一失。 */
    esp_rom_delay_us(3000);

    /* ── 三轮消抖读取 ──────────────────────────────────
     * 每轮间隔 5ms，三轮全部 LOW 才判定为真实按键。
     * 任意一轮为 HIGH 即取消判定，避免 GPIO 毛刺误唤醒。 */
    int btn[3], key[3];
    btn[0] = gpio_get_level(ENC_BTN);
    key[0] = gpio_get_level(KEY);

    if (btn[0] == 1 && key[0] == 1) {
        /* 第一轮均为 HIGH — 快速路径：直接回睡 */
        goto back_to_sleep;
    }

    esp_rom_delay_us(5000);
    btn[1] = gpio_get_level(ENC_BTN);
    key[1] = gpio_get_level(KEY);

    if (btn[1] == 1 && key[1] == 1) {
        ESP_LOGW(TAG, "Poll glitch: BTN %d→%d KEY %d→%d (R1 low, R2 high) — back to sleep",
                 btn[0], btn[1], key[0], key[1]);
        goto back_to_sleep;
    }

    esp_rom_delay_us(5000);
    btn[2] = gpio_get_level(ENC_BTN);
    key[2] = gpio_get_level(KEY);

    /* 三轮一致且均为 LOW → 确认真实按键 */
    if (btn[0] == 0 && btn[1] == 0 && btn[2] == 0 &&
        key[0] == 1 && key[1] == 1 && key[2] == 1) {
        ESP_LOGI(TAG, "ENC_BTN pressed (3-read confirm: %d,%d,%d) — waking up fully",
                 btn[0], btn[1], btn[2]);
        g_idle_deep_sleep_active = false;
        g_power_off_in_progress = false;
        g_wake_reason = WAKE_REASON_IDLE_KEY;
        return true;
    }
    if (key[0] == 0 && key[1] == 0 && key[2] == 0 &&
        btn[0] == 1 && btn[1] == 1 && btn[2] == 1) {
        ESP_LOGI(TAG, "KEY pressed (3-read confirm: %d,%d,%d) — waking up fully",
                 key[0], key[1], key[2]);
        g_idle_deep_sleep_active = false;
        g_power_off_in_progress = false;
        g_wake_reason = WAKE_REASON_IDLE_KEY;
        return true;
    }
    if (btn[0] == 0 && btn[1] == 0 && btn[2] == 0 &&
        key[0] == 0 && key[1] == 0 && key[2] == 0) {
        ESP_LOGI(TAG, "Both keys pressed (3-read confirm) — waking up fully");
        g_idle_deep_sleep_active = false;
        g_power_off_in_progress = false;
        g_wake_reason = WAKE_REASON_IDLE_KEY;
        return true;
    }

    /* 三轮读数不一致 — 噪声/毛刺，回睡 */
    ESP_LOGW(TAG, "Poll unstable: BTN %d,%d,%d KEY %d,%d,%d — back to sleep",
             btn[0], btn[1], btn[2], key[0], key[1], key[2]);

back_to_sleep:

    /* 未按下 — 快速回睡（不重新初始化外设，不设 Hold，
     * 直接复用上次的 DeepSleep 配置 + 重设 RTC 定时器）
     *
     * 注意：app_main 顶部已调用 gpio_release_deep_sleep_hold()
     * 和 gpio_init_all()，因此需要在此处重新应用关键引脚 Hold，
     * 确保 DeepSleep 期间 OLED_EN/RGB_DIN 保持低电平不浮空。 */

    /* 重新应用 GPIO4 OLED_EN Hold */
    gpio_set_level(OLED_EN, 0);
    gpio_set_direction(OLED_EN, GPIO_MODE_OUTPUT);
    gpio_hold_en(OLED_EN);

    /* 重新应用 GPIO7 RGB_DIN Hold */
    gpio_set_level(RGB_DIN, 0);
    gpio_set_direction(RGB_DIN, GPIO_MODE_OUTPUT);
    gpio_hold_en(RGB_DIN);

    /* CHG_STAT 已是输入浮空（gpio_init_all 配置），无需额外操作 */

    esp_err_t err = esp_sleep_enable_timer_wakeup(POWER_MGMT_IDLE_POLL_US);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RTC timer config failed: 0x%x — aborting sleep, full wake", err);
        gpio_release_deep_sleep_hold();
        g_idle_deep_sleep_active = false;
        g_power_off_in_progress = false;
        return true;  /* 安全回退：完整唤醒 */
    }
    esp_deep_sleep_start();
    /* NOTREACHED */
    return false;
}
