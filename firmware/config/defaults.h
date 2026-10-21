/**
 * @file    defaults.h
 * @brief   用户可配置的默认值 — V5.4
 *
 * 这些默认值持久化存储于 NVS 中。
 * V9.0.1: 移除 GATT 手机配置通道。
 */

#ifndef DEFAULTS_H
#define DEFAULTS_H

#include <stdbool.h>
#include <stdint.h>

/* ── LED / 指示灯默认值 ─────────────────────────────────── */
#define DEFAULT_LED_ENABLED             true
#define DEFAULT_BUZZER_ENABLED          true
#define DEFAULT_VIBRATION_ENABLED       true

/* ── RGB LED 灯效默认值 (V7.0) ────────────────────────────── */
#define DEFAULT_RGB_BRIGHTNESS          40
#define DEFAULT_RGB_FLASH_DURATION_MS   80
#define DEFAULT_RGB_BREATHING_PERIOD_MS 2000
#define DEFAULT_RGB_UNREAD_INTERVAL_MS  5000
/* 菜单进入 — 橙色 */
#define DEFAULT_RGB_MENU_R              255
#define DEFAULT_RGB_MENU_G              165
#define DEFAULT_RGB_MENU_B              0
/* 消息已送达 — 绿色 */
#define DEFAULT_RGB_DELIVERED_R         0
#define DEFAULT_RGB_DELIVERED_G         255
#define DEFAULT_RGB_DELIVERED_B         0
/* 消息未送达已缓存 — 紫色 */
#define DEFAULT_RGB_CACHED_R            255
#define DEFAULT_RGB_CACHED_G            0
#define DEFAULT_RGB_CACHED_B            255
/* 转发消息 — 黄色 */
#define DEFAULT_RGB_FORWARD_R           255
#define DEFAULT_RGB_FORWARD_G           255
#define DEFAULT_RGB_FORWARD_B           0
/* 收到消息 / 未读呼吸 — 蓝色 */
#define DEFAULT_RGB_RECEIVED_R          0
#define DEFAULT_RGB_RECEIVED_G          0
#define DEFAULT_RGB_RECEIVED_B          255
/* V8.0: 好友添加成功 — 绿色 */
#define DEFAULT_RGB_FRIEND_ADDED_R      0
#define DEFAULT_RGB_FRIEND_ADDED_G      255
#define DEFAULT_RGB_FRIEND_ADDED_B      0
/* V8.0: 好友添加超时 — 红色 */
#define DEFAULT_RGB_FRIEND_TIMEOUT_R    255
#define DEFAULT_RGB_FRIEND_TIMEOUT_G    0
#define DEFAULT_RGB_FRIEND_TIMEOUT_B    0

/* ── 显示默认值 (V6.13) ─────────────────────────────────── */
#define DEFAULT_DARK_MODE               true    /* 默认暗底白字 */
#define DEFAULT_LONG_TEXT_SCROLL_ENABLE true
#define DEFAULT_LONG_TEXT_SCROLL_SPEED  5       /* 像素/帧 */
#define DEFAULT_LONG_TEXT_THRESHOLD_PX  100     /* 超过 100px 触发滚动 */

/* ── OLED 超时默认值（单位：秒）─────────────────────────── */
#define DEFAULT_OLED_ANIM_TIMEOUT       30
#define DEFAULT_OLED_SLEEP_TIMEOUT      60

/* ── 莫尔斯电码默认值 ───────────────────────────────────── */
#define DEFAULT_MORSE_DOT_MS            200     /* V7.7: 默认点持续时长（ms） */
#define DEFAULT_MORSE_LETTER_GAP_MS     750     /* 字母间隔（毫秒） */
#define DEFAULT_MORSE_WPM               12      /* 码速，词/分钟（教学用，V7.7由dot_ms反算） */
#define DEFAULT_MORSE_AUTO_DECODE       true    /* 自动译码（接收端）默认开启 */

/* ── 电源默认值 ─────────────────────────────────────────── */
#define DEFAULT_DEEPSLEEP_INTERVAL_SEC  5       /* Wake Slot 周期（秒） */
#define DEFAULT_IDLE_SLEEP_TIMEOUT_SEC  600     /* Legacy cfg_blob default; never triggers sleep. */

/* ── BLE Mesh 默认值 ─────────────────────────────────────── */
#define DEFAULT_MESH_TTL                3       /* 默认 TTL 值 */
#define DEFAULT_EMERGENCY_PHY_S2        true    /* 紧急消息使用 S=2 编码 PHY */

/* ── 日志系统默认值（V0.10.0）──────────────────────────── */
#define DEFAULT_LOG_LEVEL               3       /* INFO */
#define DEFAULT_USB_LOG_ENABLED         true
#define DEFAULT_MESH_LOG_ENABLED        true
#define DEFAULT_GATT_LOG_ENABLED        false  /* V9.0.1: GATT removed */
#define DEFAULT_SYSTEM_LOG_ENABLED      false

/* ── 默认快捷消息 ────────────────────────────────────────── */
#define DEFAULT_QUICK_MSG_0  "Signal received."
#define DEFAULT_QUICK_MSG_1  "666"
#define DEFAULT_QUICK_MSG_2  "All good here."
#define DEFAULT_QUICK_MSG_3  "Mission completed."
#define DEFAULT_QUICK_MSG_4  "On my way."
#define DEFAULT_QUICK_MSG_5  "Arrived safely."
#define DEFAULT_QUICK_MSG_6  "Still connected."
#define DEFAULT_QUICK_MSG_7  "Keep in touch."
#define DEFAULT_QUICK_MSG_8  "See you in the future."
#define DEFAULT_QUICK_MSG_9  "XDU forever"

#endif /* DEFAULTS_H */
