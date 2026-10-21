/**
 * @file storage_nvs.h
 * @brief NVS (Non-Volatile Storage) 封装 — 替代 Arduino EEPROM
 *
 * 存储 ui.param[16] + knob.param[4], 11 字节校验头, 允许 1 位误码。
 * 仅在参数变更后进入睡眠时写入, 避免频繁擦写 NVS。
 *
 * 原封不动从 u8g2_espc6 移植, 合入 BLUE_LONG_MESH 的 nvs_config 体系中。
 */

#ifndef STORAGE_NVS_H
#define STORAGE_NVS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 NVS 并读取存储的参数
 *
 * 如果 NVS 分区不存在或校验失败 (>1 位误码), 使用默认参数。
 * 否则从 NVS 加载 ui.param[] 和 knob.param[]。
 */
void nvs_init(void);

/**
 * @brief 将所有参数写入 NVS (仅在参数有变更时调用)
 *
 * 写入顺序: 11 字节校验头 + 16 字节 ui.param + 4 字节 knob.param
 */
void nvs_write_all_data(void);

/**
 * @brief 从 NVS 读取所有参数
 */
void nvs_read_all_data(void);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_NVS_H */
