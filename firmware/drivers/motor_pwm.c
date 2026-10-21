/**
 * @file    motor_pwm.c
 * @brief   振动马达 PWM 驱动（LEDC）— V5.4 阶段三 【已禁用：2026-06-16】
 *
 * 马达功能已禁用。所有实现已移入 motor_pwm.h 空桩。
 * 恢复时删除 #if 0 包裹即可。
 */
#if 0  /* ── 马达已禁用 ── */

/**
 * @file    motor_pwm.c
 * @brief   振动马达 PWM 驱动（LEDC）— V5.4 阶段三
 *
 * Uses LEDC (LED PWM Controller) on GPIO22.
 * MCPWM is already occupied by the buzzer on GPIO20/21.
 *
 * PWM frequency: ~5 kHz (suitable for small vibration motors).
 * Resolution: 10-bit (0-1023 duty steps).
 */

#include "motor_pwm.h"
#include "pins.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "motor";

/* ── LEDC configuration constants ───────────────────────────── */
#define MOTOR_LEDC_TIMER        LEDC_TIMER_0
#define MOTOR_LEDC_MODE         LEDC_LOW_SPEED_MODE
#define MOTOR_LEDC_CHANNEL      LEDC_CHANNEL_0
#define MOTOR_LEDC_DUTY_RES     LEDC_TIMER_10_BIT
#define MOTOR_LEDC_FREQ_HZ      5000        /* 5 kHz PWM */
#define MOTOR_LEDC_DUTY_MAX     ((1 << LEDC_TIMER_10_BIT) - 1)  /* 1023 */

/* ── Run state ──────────────────────────────────────────────── */
static bool motor_running = false;
static uint8_t motor_duty_pct = 50;  /* Default 50% duty */

/* ── Initialisation ─────────────────────────────────────────── */

void motor_init(void)
{
    ESP_LOGD(TAG, "Initialising LEDC motor PWM on GPIO%d, %lu Hz",
             MOTOR_PWM, (unsigned long)MOTOR_LEDC_FREQ_HZ);

    /* 0. 防御性释放 Hold — gpio_init_all 或 DeepSleep 唤醒残留
     * 可能将 GPIO22 锁定在 LOW，LEDC 输出将无法驱动马达 */
    gpio_hold_dis(MOTOR_PWM);

    /* LEDC timer configuration */
    ledc_timer_config_t timer_cfg = {
        .speed_mode       = MOTOR_LEDC_MODE,
        .duty_resolution  = MOTOR_LEDC_DUTY_RES,
        .timer_num        = MOTOR_LEDC_TIMER,
        .freq_hz          = MOTOR_LEDC_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK,
        .deconfigure      = false,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    /* LEDC channel configuration — start with duty = 0 (stopped) */
    ledc_channel_config_t ch_cfg = {
        .gpio_num       = MOTOR_PWM,
        .speed_mode     = MOTOR_LEDC_MODE,
        .channel        = MOTOR_LEDC_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = MOTOR_LEDC_TIMER,
        .duty           = 0,
        .hpoint         = 0,
        .flags          = { .output_invert = 0 },
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));

    /* Ensure output is LOW */
    ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, 0);
    ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL);

    motor_running = false;
    ESP_LOGD(TAG, "Motor PWM ready (stopped)");
}

/* ── Start / Stop ───────────────────────────────────────────── */

void motor_start(void)
{
    if (motor_running) {
        ESP_LOGW(TAG, "Motor already running");
        return;
    }

    uint32_t duty_raw = (uint32_t)motor_duty_pct * MOTOR_LEDC_DUTY_MAX / 100;
    ESP_LOGD(TAG, "Motor START — duty %u%% (raw %lu)", motor_duty_pct, (unsigned long)duty_raw);

    ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, duty_raw);
    ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL);
    motor_running = true;
}

void motor_stop(void)
{
    if (!motor_running) {
        /* Still ensure output is LOW (belt-and-suspenders) */
        ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, 0);
        ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL);
        return;
    }

    ESP_LOGD(TAG, "Motor STOP");
    ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, 0);
    ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL);
    motor_running = false;
}

/* ── Duty control ───────────────────────────────────────────── */

void motor_set_duty(uint8_t duty_pct)
{
    if (duty_pct > 100) duty_pct = 100;
    motor_duty_pct = duty_pct;

    if (motor_running) {
        uint32_t duty_raw = (uint32_t)duty_pct * MOTOR_LEDC_DUTY_MAX / 100;
        ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, duty_raw);
        ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL);
    }
}

bool motor_is_running(void)
{
    return motor_running;
}

#endif /* ── 马达已禁用 ── */
