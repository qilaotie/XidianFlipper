/**
 * @file    firmware.h
 * @brief   固件组件主包含头文件 — V5.4
 *
 * 引入此单一头文件即可包含所有固件模块。
 */

#ifndef FIRMWARE_H
#define FIRMWARE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── 配置层 ──────────────────────────────────────────────── */
#include "pins.h"
#include "constants.h"
#include "defaults.h"
#include "log_config.h"

/* ── 驱动层 ──────────────────────────────────────────────── */
#include "gpio_config.h"
#include "oled_power.h"
#include "mcpwm_buzzer.h"
#include "rmt_ws2812.h"
#include "adc_battery.h"
#include "ec06.h"

/* ── 系统层 ──────────────────────────────────────────────── */
#include "power_manager.h"
#include "power_mgmt.h"
#include "event_dispatcher.h"
#include "nvs_config.h"
#include "storage_nvs.h"

/* ── 协议层 ──────────────────────────────────────────────── */
#include "mesh_packet.h"
#include "mesh_crypto.h"

/* ── 无线层 ──────────────────────────────────────────────── */
#include "ble_radio.h"

/* ── 应用层（V6.0：WouoUI、莫尔斯、快捷消息、通知）─────────── */
#include "wououi.h"
#include "ui_task.h"
#include "morse_engine.h"
#include "morse_input.h"
#include "morse_buffer.h"
#include "morse_tx.h"
#include "morse_rx_queue.h"
#include "morse_ui.h"
#include "morse_tree.h"
#include "morse_terminal.h"
#include "quick_msg.h"
#include "mesh_notify.h"

/* ── 阶段五：压力测试 ───────────────────────────────────── */
#include "stress_test.h"

/* ── V9.1: 好友管理暂缓（friend_mgr.h 保留空桩，mesh_notify 内部引用） ── */
#include "friend_mgr.h"

/* ── 阶段七：声学与震动交互反馈 ───────────────────────── */
#include "haptic_feedback.h"

/* ── 配置模式 ──────────────────────────────────────────── */
#include "config_mode.h"

/* ── 日志系统 ──────────────────────────────────────────── */
#include "log_manager.h"

/* ── 开发者模式（条件编译）──────────────────────────────── */
#if LOG_DEV_MODE_ENABLE
#include "dev_mode.h"
#endif

/* ── 全局初始化 — 由 app_main() 调用一次 ───────────────── */
/**
 * @brief 初始化所有固件子系统以进行正常运行。
 *
 * 在冷启动或从 DeepSleep 唤醒后需要完整 UI 时调用。
 * 不触发 DeepSleep — 由调用者决定。
 */
void firmware_init_all(void);

#ifdef __cplusplus
}
#endif

#endif /* FIRMWARE_H */
