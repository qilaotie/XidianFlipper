/**
 * @file    config_mode.c
 * @brief   设备模式管理实现 — V9.0.1 简化（GATT 已移除，仅 NORMAL）
 */

#include "config_mode.h"
#include "esp_log.h"

static const char *TAG = "cfg_mode";

void config_mode_init(void)
{
    ESP_LOGI(TAG, "V9.0.1: single mode NORMAL (GATT removed)");
}

device_mode_t config_mode_get(void)
{
    return MODE_NORMAL;
}

bool config_mode_request(device_mode_t new_mode)
{
    (void)new_mode;
    return false;  /* V9.0.1: only MODE_NORMAL is valid, all transitions denied */
}

bool config_mode_deepsleep_allowed(void)
{
    return true;   /* V9.0.1: always allowed */
}

bool config_mode_radio_allowed(void)
{
    return true;   /* V9.0.1: always allowed */
}

const char *config_mode_get_name(void)
{
    return "NORMAL";
}

bool config_mode_is_ota(void)
{
    return false;  /* V9.0.1: OTA removed */
}
