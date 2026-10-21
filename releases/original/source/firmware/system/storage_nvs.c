/**
 * @file storage_nvs.c
 * @brief NVS (Non-Volatile Storage) 封装实现 — 替代 Arduino EEPROM
 *
 * 存储 ui.param[16] + knob.param[4], 11 字节校验头, 允许 1 位误码。
 * 仅在参数变更后进入睡眠时写入, 避免频繁擦写 NVS。
 *
 * 原封不动从 u8g2_espc6 移植, 合入 BLUE_LONG_MESH 的 nvs_config 体系中。
 */

#include "storage_nvs.h"
#include "wououi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "storage_nvs";
#define NVS_NAMESPACE   "wououi"
#define NVS_KEY         "params"

void nvs_init(void)
{
    /* 初始化默认参数 */
    ui_param_init();

    /* 初始化 knob 默认参数 */
    knob.param[KNOB_ROT] = KNOB_DISABLE;
    knob.param[KNOB_COD] = 0;
    knob.param[KNOB_ROT_P] = 2;
    knob.param[KNOB_COD_P] = 2;

    /* 尝试从 NVS 读取 */
    nvs_read_all_data();

    /* 初始化选择框数据指针 */
    check_box_m_init(ui.param + TILE_UFD);  // 多选从 TILE_UFD 开始
    check_box_v_init(ui.param + DISP_BRI);  // 数值显示

    eeprom.init = true;
}

void nvs_write_all_data(void)
{
    if (!eeprom.init || !eeprom.change) return;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %d", err);
        return;
    }

    /* 构造 31 字节 blob: 11 字节校验头 + 16 字节 ui.param + 4 字节 knob.param */
    uint8_t blob[EEPROM_CHECK + UI_PARAM + KNOB_PARAM];
    for (int i = 0; i < EEPROM_CHECK; ++i) {
        blob[i] = (uint8_t)('a' + i);
    }
    memcpy(blob + EEPROM_CHECK, ui.param, UI_PARAM);
    memcpy(blob + EEPROM_CHECK + UI_PARAM, knob.param, KNOB_PARAM);

    err = nvs_set_blob(handle, NVS_KEY, blob, sizeof(blob));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    if (err == ESP_OK) {
        eeprom.change = false;
        ESP_LOGI(TAG, "Params written to NVS");
    } else {
        ESP_LOGW(TAG, "nvs write/commit failed: %d", err);
    }

    nvs_close(handle);
}

void nvs_read_all_data(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "NVS not found, using defaults");
        return;
    }

    uint8_t blob[EEPROM_CHECK + UI_PARAM + KNOB_PARAM];
    size_t len = sizeof(blob);
    err = nvs_get_blob(handle, NVS_KEY, blob, &len);
    nvs_close(handle);

    if (err != ESP_OK || len != sizeof(blob)) {
        ESP_LOGI(TAG, "NVS blob not found or wrong size, using defaults");
        return;
    }

    /* 校验: 逐字节比较, 允许最多 1 字节错误 */
    int errors = 0;
    for (int i = 0; i < EEPROM_CHECK; ++i) {
        if (blob[i] != (uint8_t)('a' + i)) {
            errors++;
        }
    }
    if (errors > 1) {
        ESP_LOGW(TAG, "NVS blob corrupted (%d errors), using defaults", errors);
        return;
    }

    /* 加载参数 */
    memcpy(ui.param, blob + EEPROM_CHECK, UI_PARAM);
    memcpy(knob.param, blob + EEPROM_CHECK + UI_PARAM, KNOB_PARAM);
    ESP_LOGI(TAG, "Params loaded from NVS (errors=%d)", errors);
}
