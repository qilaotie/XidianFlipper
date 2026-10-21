/**
 * @file    config_mode.h
 * @brief   设备模式管理 — V9.0.1 简化
 *
 * V9.0.1: 移除 MODE_CONFIG / MODE_OTA（GATT 已移除），仅保留 MODE_NORMAL。
 *   所有 API 保留以维持调用方兼容，但始终返回 NORMAL / true。
 */

#ifndef CONFIG_MODE_H
#define CONFIG_MODE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 设备模式枚举（保留旧枚举值以兼容） ──────────────────── */
typedef enum {
    MODE_NORMAL  = 0,   /**< 唯一有效模式：正常 BLE 通信 + DeepSleep */
    MODE_CONFIG  = 1,   /**< @deprecated V9.0.1: GATT 已移除 */
    MODE_OTA     = 2,   /**< @deprecated V9.0.1: OTA-over-GATT 已移除 */
} device_mode_t;

/* ── 公开 API（V9.0.1: 全部为桩实现，始终返回默认值）─────── */

void config_mode_init(void);
device_mode_t config_mode_get(void);
bool config_mode_request(device_mode_t new_mode);
bool config_mode_deepsleep_allowed(void);
bool config_mode_radio_allowed(void);
const char *config_mode_get_name(void);
bool config_mode_is_ota(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_MODE_H */
