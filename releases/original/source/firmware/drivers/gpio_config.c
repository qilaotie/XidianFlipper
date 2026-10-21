/**
 * @file    gpio_config.c
 * @brief   GPIO 初始化与 DeepSleep Hold 策略 — V5.4
 *
 * 引脚与供电事实见 hardware/hardware.md；Hold 经验见 docs/lessons-learned.md。
 *
 * Hold 原则：
 *   - 外部下拉  → 输出 LOW + gpio_hold_en()
 *   - 外部上拉  → 输入（高阻）+ 不 Hold
 *   - 外部分压+下拉 → 输入 + 不 Hold（CHG_STAT — 安全关键）
 *   - 无外部偏置 → 输出 LOW（AUDIO_P/N）
 *
 * 外部上拉输入避免 Hold LOW，以免产生额外静态电流。
 * CHG_STAT 保持输入，不用输出 Hold 改变外部采样网络。
 */

#include "gpio_config.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "gpio_cfg";

/* ── 辅助函数：配置单个输出引脚并 Hold ───────────────────── */
static void __attribute__((unused)) gpio_out_hold_low(gpio_num_t pin)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(pin, 0);
    gpio_hold_en(pin);
}

/* ── 辅助函数：配置单个输出引脚，不 Hold ─────────────────── */
static void gpio_out_low_no_hold(gpio_num_t pin)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(pin, 0);
}

/* ── 辅助函数：外部上拉输入（不 Hold），可选内部上拉 ────── */
static void gpio_in_pu_no_hold(gpio_num_t pin, bool internal_pullup)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = internal_pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
}

/* ── 辅助函数：输入浮空，无上下拉，不 Hold（安全关键）────── */
static void gpio_in_float_no_hold(gpio_num_t pin)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,   /* 依赖外部分压 */
        .pull_down_en = GPIO_PULLDOWN_DISABLE, /* 依赖外部 R5=2MΩ */
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
}

/* ══════════════════════════════════════════════════════════════
 *  公开 API
 * ══════════════════════════════════════════════════════════════ */

void gpio_init_all(void)
{
    ESP_LOGI(TAG, "Init all GPIOs (V5.4 Hold strategy)");

#if TEMP_PULL_INIT_ENABLE
    /* ── 初始化阶段临时上下拉：匹配外部电阻，确保引脚在配置前的安全状态 ── */
    ESP_LOGI(TAG, "Applying temporary pull-up/down matching external resistors...");

    /* 临时上拉 — 匹配外部上拉电阻 (100k/4.7k) */
    gpio_set_pull_mode(ENC_A, GPIO_PULLUP_ONLY);      /* GPIO2  — 外部 100k 上拉 */
    gpio_set_pull_mode(ENC_B, GPIO_PULLUP_ONLY);      /* GPIO3  — 外部 100k 上拉 */
    gpio_set_pull_mode(ENC_BTN, GPIO_PULLUP_ONLY);    /* GPIO18 — 外部 100k 上拉 */
    gpio_set_pull_mode(KEY, GPIO_PULLUP_ONLY);        /* GPIO19 — 外部 100k 上拉 */
    gpio_set_pull_mode(I2C_SDA, GPIO_PULLUP_ONLY);    /* GPIO5  — R7=4.7k 上拉至 OLED_3V3 */
    gpio_set_pull_mode(I2C_SCL, GPIO_PULLUP_ONLY);    /* GPIO6  — R8=4.7k 上拉至 OLED_3V3 */

    /* 临时下拉 — 匹配外部下拉电阻 (100k/10k) */
    gpio_set_pull_mode(OLED_EN, GPIO_PULLDOWN_ONLY);  /* GPIO4  — 外部 100k 下拉 */
    gpio_set_pull_mode(RGB_DIN, GPIO_PULLDOWN_ONLY);  /* GPIO7  — 外部 10k 下拉 */
    gpio_set_pull_mode(AUDIO_P, GPIO_PULLDOWN_ONLY);  /* GPIO20 — 外部 100k 下拉 */
    gpio_set_pull_mode(AUDIO_N, GPIO_PULLDOWN_ONLY);  /* GPIO21 — 外部 100k 下拉 */

    /* 短暂稳定时间，让内部弱上下拉建立（~50µs 足够 RC 充电）*/
    esp_rom_delay_us(50);

    /* 注意：后续 gpio_config() 调用会覆盖此处的临时上下拉设置：
     *   - 输入引脚 → gpio_in_pu_no_hold() 将按 INPUT_PULLUP_ENABLE 重配上下拉
     *   - I2C 引脚 → gpio_in_pu_no_hold(..., false) 将关闭上拉（依赖外部 4.7k）
     *   - 输出引脚 → gpio_out_low_no_hold() 将关闭所有内部上下拉 */
#endif

    /* ── 外设：输出 LOW，不 Hold（外部下拉）───────────────
     * 重要：Hold 仅在 gpio_prepare_deep_sleep() 中应用。
     * 若在初始化时 Hold，后续 oled_power_on / rgb_init / motor_init
     * 将无法改变引脚电平（gpio_set_level 被 Hold 静默阻止）。 */
    gpio_out_low_no_hold(OLED_EN);      /* GPIO4  — OLED 电源，外部 100k 下拉 */
    gpio_out_low_no_hold(RGB_DIN);      /* GPIO7 — WS2812 数据，外部 10k 下拉 */
    /* gpio_out_low_no_hold(MOTOR_PWM);    GPIO22 — 振动马达，外部 100k 下拉（已禁用）*/

    /* ── 蜂鸣器：输出 LOW，不 Hold（外部 100k 下拉）─────── */
    gpio_out_low_no_hold(AUDIO_P);      /* GPIO20 */
    gpio_out_low_no_hold(AUDIO_N);      /* GPIO21 */

    /* ── 输入：外部上拉，不 Hold ─────────────────────────── */
    gpio_in_pu_no_hold(ENC_A,   INPUT_PULLUP_ENABLE);   /* GPIO2  — 外部 100k 上拉 + 内部上拉 */
    gpio_in_pu_no_hold(ENC_B,   INPUT_PULLUP_ENABLE);   /* GPIO3  — 外部 100k 上拉 + 内部上拉 */
    gpio_in_pu_no_hold(ENC_BTN, INPUT_PULLUP_ENABLE);   /* GPIO18 — 外部 100k 上拉 + 内部上拉 */
    gpio_in_pu_no_hold(KEY,     INPUT_PULLUP_ENABLE);   /* GPIO19 — 外部 100k 上拉 + 内部上拉 */

    /* ── CHG_STAT：输入，无上下拉，不 Hold（关键安全！）─── */
    gpio_in_float_no_hold(CHG_STAT);    /* GPIO0 — 网表 R4/R5=1M/2M 分压 */

    /* ── I2C 引脚：保持输入直到 I2C 驱动接管 ────────────── */
    gpio_in_pu_no_hold(I2C_SDA, false); /* GPIO5  — R7=4.7k 上拉至 OLED_3V3，无需内部上拉 */
    gpio_in_pu_no_hold(I2C_SCL, false); /* GPIO6  — R8=4.7k 上拉至 OLED_3V3，无需内部上拉 */

    /* ── 调试 / USB：保持默认（启动引导/硬件配置）───────── */
    /* USB GPIO12、GPIO13 — 保持默认；GPIO0 已作为 CHG_STAT 配置 */

    ESP_LOGI(TAG, "GPIO init done");
}

void gpio_prepare_deep_sleep(void)
{
    ESP_LOGI(TAG, "Preparing GPIO for DeepSleep");

    /*
     * 确保所有外设关闭并 Hold。
     * 这些引脚已在 _init_all 中配置；我们重新确认电平
     * 并应用 Hold（某些 Hold 可能已设置，这在 ESP32-C6 上
     * 是安全的 — gpio_hold_en 是幂等的）。
     */

    /* OLED：关闭 + Hold */
#if OLED_FORCE_POWER == 1
    /* 全局强制开屏 — 保持 OLED_EN 高电平，不 Hold */
    ESP_LOGI(TAG, "OLED forced ON — keeping OLED_EN HIGH, skip hold");
    gpio_set_level(OLED_EN, 1);
    gpio_set_direction(OLED_EN, GPIO_MODE_OUTPUT);
    /* 明确不 Hold：调试期间让 GPIO4 保持输出高 */
#else
    gpio_set_level(OLED_EN, 0);
    gpio_set_direction(OLED_EN, GPIO_MODE_OUTPUT);
    gpio_hold_en(OLED_EN);
#endif

    /* RGB 数据：低电平 + Hold */
    gpio_set_level(RGB_DIN, 0);
    gpio_set_direction(RGB_DIN, GPIO_MODE_OUTPUT);
    gpio_hold_en(RGB_DIN);

    /* 马达：低电平 + Hold（已禁用）
    gpio_set_level(MOTOR_PWM, 0);
    gpio_set_direction(MOTOR_PWM, GPIO_MODE_OUTPUT);
    gpio_hold_en(MOTOR_PWM); */

    /* 蜂鸣器：低电平，不 Hold */
    gpio_set_level(AUDIO_P, 0);
    gpio_set_level(AUDIO_N, 0);
    gpio_set_direction(AUDIO_P, GPIO_MODE_OUTPUT);
    gpio_set_direction(AUDIO_N, GPIO_MODE_OUTPUT);
    /* AUDIO_P/N 明确不 Hold */

    /*
     * CHG_STAT（GPIO0）：输入模式，不 Hold — 安全关键
     * 切勿设为输出或 Hold 低电平。如果 MCP73831 STAT 变为 HIGH
     * （充电完成）而 GPIO0 被 Hold LOW，导致的电平冲突
     * 可能永久损坏 GPIO。
     */
    gpio_set_direction(CHG_STAT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CHG_STAT, GPIO_FLOATING);
    /* 禁止 gpio_hold_en(CHG_STAT) — 绝对禁止 */

    /* 交互输入：已是输入模式，无需 Hold */
    /* ENC_A、ENC_B、ENC_BTN、KEY — 保持输入模式 */

    /*
     * ESP32-C6 支持 DeepSleep 期间逐引脚 Hold
     *（SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP）。
     * 上述单独的 gpio_hold_en() 调用已足够 —
     * 此目标上无需全局 gpio_deep_sleep_hold_en()。
     */

    ESP_LOGI(TAG, "DeepSleep GPIO Hold applied");
}

void gpio_release_deep_sleep_hold(void)
{
    /* 释放之前 Hold 的引脚 */
    gpio_hold_dis(OLED_EN);
    gpio_hold_dis(RGB_DIN);
    /* gpio_hold_dis(MOTOR_PWM);  马达已禁用 */
    ESP_LOGI(TAG, "GPIO Hold released");
}
