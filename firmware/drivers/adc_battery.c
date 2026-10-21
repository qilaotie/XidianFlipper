/**
 * @file    adc_battery.c
 * @brief   通过 CHG_STAT(GPIO0) 检测 MCP73831 充电状态
 *
 * 实现 MCP73831 STAT 引脚的三态检测方法，
 * 电路与采样限制见 hardware/hardware.md；本板三态判定仍待电压实测。
 *
 * 检测逻辑：
 *   ┌─────────────┬──────────────┬─────────────────────┐
 *   │ STAT 状态   │ 上拉开启读取  │ 上拉关闭读取        │
 *   ├─────────────┼──────────────┼─────────────────────┤
 *   │ 关机        │ HIGH（浮空）  │ LOW（R5 下拉）      │
 *   │ 充电中      │ LOW（强下拉） │ LOW（强下拉）       │
 *   │ 充电完成    │ HIGH（3.3V）  │ HIGH（3.3V 驱动）   │
 *   └─────────────┴──────────────┴─────────────────────┘
 *
 * 检测算法：
 *   1. 使能内部上拉，等待 1 ms 稳定
 *   2. 读取电平
 *   3. 若为 LOW  → 充电中（STAT 强下拉对抗上拉）
 *   4. 若为 HIGH → 禁用上拉，等待 0.5 ms，再次读取
 *      - 仍为 HIGH → 充电完成（STAT 驱动 HIGH）
 *      - 变为 LOW  → 关机（STAT 高阻态，仅 R5 拉低）
 *   5. 恢复输入浮空（无上下拉）
 */

#include "adc_battery.h"
#include "pins.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "charge";

/* ── 稳定时间（微秒）─────────────────────────────────────── */
#define PU_SETTLE_US    1000    /* 上拉给寄生电容充电需 1 ms */
#define FLOAT_SETTLE_US 500     /* 释放上拉后放电需 0.5 ms */

void charge_status_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << CHG_STAT),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    ESP_LOGI(TAG, "CHG_STAT (GPIO%d) configured as input floating", CHG_STAT);
}

charge_state_t charge_status_read(void)
{
    int level_pu_on = -1;
    int level_pu_off = -1;

    /* 步骤 1：使能内部上拉，等待稳定 */
    gpio_set_pull_mode(CHG_STAT, GPIO_PULLUP_ONLY);
    esp_rom_delay_us(PU_SETTLE_US);

    /* 步骤 2：上拉开启时读取电平 */
    level_pu_on = gpio_get_level(CHG_STAT);

    if (level_pu_on == 0) {
        /* STAT 强拉 LOW → 充电进行中 */
        gpio_set_pull_mode(CHG_STAT, GPIO_FLOATING);
        ESP_LOGD(TAG, "Charge state: charging");
        return CHARGE_CHARGING;
    }

    /* 步骤 3：上拉为 HIGH — 禁用上拉，等待，再次读取 */
    gpio_set_pull_mode(CHG_STAT, GPIO_FLOATING);
    esp_rom_delay_us(FLOAT_SETTLE_US);

    level_pu_off = gpio_get_level(CHG_STAT);

    /* 步骤 4：最终恢复为输入浮空（双重保险） */
    gpio_set_pull_mode(CHG_STAT, GPIO_FLOATING);

    if (level_pu_off == 1) {
        /* 无上拉仍为 HIGH → STAT 驱动 HIGH → 充电完成 */
        ESP_LOGD(TAG, "Charge state: done");
        return CHARGE_DONE;
    } else {
        /* 跌落为 LOW → STAT 高阻态，R5 拉至 GND → 关机/未充电 */
        ESP_LOGD(TAG, "Charge state: not charging (no charger)");
        return CHARGE_OFF;
    }
}
