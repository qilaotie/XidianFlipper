/**
 * @file    log_config.h
 * @brief   编译期日志开关矩阵 — V0.10.0
 *
 * 所有日志和开发者模式功能的编译期开关集中管理。
 * 当 LOG_MASTER_ENABLE=0 时，所有 LOG_* 宏展开为空，固件体积最小。
 */

#ifndef LOG_CONFIG_H
#define LOG_CONFIG_H

/* ══════════════════════════════════════════════════════════════
 *  总开关
 * ══════════════════════════════════════════════════════════════ */

/** 日志总开关：0=全部静默，1=允许日志输出（受下级开关进一步过滤） */
#define LOG_MASTER_ENABLE           1

/** 编译期日志最低级别：
 *  0=NONE  1=ERROR  2=WARN  3=INFO  4=DEBUG  5=VERBOSE
 *  低于此级别的 LOG_TAG 调用在编译期展开为空 */
#define LOG_LEVEL                   3   /* INFO — V10.8 功耗排查 */

/* ══════════════════════════════════════════════════════════════
 *  阶段独立开关
 * ══════════════════════════════════════════════════════════════ */

#define LOG_PHASE0_ENABLE           0   /* 阶段 0: 硬件测试 */
#define LOG_PHASE1_ENABLE           0   /* 阶段 1: GPIO/电源 */
#define LOG_PHASE2_ENABLE           0   /* 阶段 2: Mesh 协议栈 */
#define LOG_PHASE3_ENABLE           0   /* V6.51 关闭减少噪音 */
#define LOG_PHASE4_ENABLE           0   /* V9.0.1: GATT removed */
#define LOG_PHASE5_ENABLE           0   /* 阶段 5: OTA + 压力测试 */
#define LOG_PHASE6_ENABLE           0   /* V6.51 关闭减少噪音 */

/* ══════════════════════════════════════════════════════════════
 *  模块级独立开关
 * ══════════════════════════════════════════════════════════════ */

#define LOG_MESH_ENABLE             0   /* Mesh 转发 — 噪音大，关掉 */
#define LOG_GATT_ENABLE             0   /* V9.0.1: GATT removed */
#define LOG_UI_ENABLE               1   /* V10.8 功耗排查：观察 OLED 刷新状态 */
#define LOG_POWER_ENABLE            1   /* 电源管理 + 充电检测 — V10.8 功耗排查 */
#define LOG_MORSE_ENABLE            0   /* 摩斯引擎 + 教学系统 */
#define LOG_FRIEND_ENABLE           1   /* V8.0: 好友功能排障 */
#define LOG_HAPTIC_ENABLE           0   /* 触觉反馈 */
#define LOG_SYSTEM_ENABLE           1   /* 系统级诊断（Heap/Task/NVS/Boot） */
#define LOG_DIAG_ENABLE             1   /* 诊断专用宏（LOG_DIAG） */
#define LOG_INIT_TIMING_ENABLE      1   /* 模块初始化分段计时 */
#define LOG_INPUT_ENABLE            0   /* 输入事件诊断 */
#define LOG_QMSG_ENABLE             1   /* 快捷消息诊断 */
#define LOG_DIAG_SCAN_ENABLE        0   /* V6.51 关闭(raw hex噪音极大) */
#define LOG_DIAG_UNPACK_ENABLE      0   /* V6.51 关闭 */

/* ── QMSG 诊断宏 ──────────────────────────────────────────── */
#if LOG_QMSG_ENABLE
  #define LOG_QMSG(tag, fmt, ...)   ESP_LOGI(tag, "[QMSG] " fmt, ##__VA_ARGS__)
#else
  #define LOG_QMSG(tag, fmt, ...)   ((void)0)
#endif

/* ── 扫描 DIAG 宏（受 LOG_DIAG_SCAN_ENABLE 控制）─────────── */
#if LOG_DIAG_SCAN_ENABLE
  #define LOG_DIAG_SCAN(tag, fmt, ...) ESP_LOGI(tag, "[DIAG] " fmt, ##__VA_ARGS__)
#else
  #define LOG_DIAG_SCAN(tag, fmt, ...) ((void)0)
#endif

/* ── 解包失败 DIAG 宏（受 LOG_DIAG_UNPACK_ENABLE 控制）───── */
#if LOG_DIAG_UNPACK_ENABLE
  #define LOG_DIAG_UNPACK(tag, fmt, ...) ESP_LOGI(tag, "[DIAG] " fmt, ##__VA_ARGS__)
#else
  #define LOG_DIAG_UNPACK(tag, fmt, ...) ((void)0)
#endif

/* ══════════════════════════════════════════════════════════════
 *  V6.53: 排障阶段日志配置（噪音清理 + 关键缺口补全）
 *  - LOG_UI_DIAG_ENABLE=1（WIN-DISPATCH 已用 #if 0 局部关闭）
 *  - LOG_ACK_DIAG_ENABLE=1（核心：ONLINE-RX/CACHE FLUSH/RX DUP/RX NEW/RX SELF）
 *  - LOG_SCHED_ENABLE=1（保留 tick 心跳 + 状态变化 CYCLE 日志）
 *  - LOG_ANTI_DIAG_ENABLE=1（MSG_ANTI 全链路诊断）
 *  - 缓存日志复用 LOG_ACK_DIAG_ENABLE，不新增独立宏
 * ══════════════════════════════════════════════════════════════ */

/* ══════════════════════════════════════════════════════════════
 *  V6.51: ACK / 调度 / UI 诊断专用开关
 * ══════════════════════════════════════════════════════════════ */

/** ACK 全链路诊断（mesh_forward + mesh_notify + quick_msg） */
#define LOG_ACK_DIAG_ENABLE         1

/** 主循环调度日志（Wake Slot / 交替收发） */
#define LOG_SCHED_ENABLE            1

/** UI 状态机诊断（页面导航 / 窗口 / 按钮消费）
 *  V6.53: WIN-DISPATCH 每 tick 刷屏已用 #if 0 局部关闭 */
#define LOG_UI_DIAG_ENABLE          1

/* ── ACK 诊断宏 ──────────────────────────────────────────── */
#if LOG_ACK_DIAG_ENABLE
  #define LOG_ACK_DIAG(tag, fmt, ...) ESP_LOGI(tag, "[ACK-DIAG] " fmt, ##__VA_ARGS__)
#else
  #define LOG_ACK_DIAG(tag, fmt, ...) ((void)0)
#endif

/* ══════════════════════════════════════════════════════════════
 *  V6.52: MSG_ANTI 反消息专用诊断开关
 * ══════════════════════════════════════════════════════════════ */

/** MSG_ANTI 全链路诊断（调度→发送→去重→接收→缓存清除→中继） */
#define LOG_ANTI_DIAG_ENABLE        1

#if LOG_ANTI_DIAG_ENABLE
  #define LOG_ANTI_DIAG(tag, fmt, ...) ESP_LOGI(tag, "[ANTI-DIAG] " fmt, ##__VA_ARGS__)
#else
  #define LOG_ANTI_DIAG(tag, fmt, ...) ((void)0)
#endif

/* ── 调度诊断宏 ──────────────────────────────────────────── */
#if LOG_SCHED_ENABLE
  #define LOG_SCHED(tag, fmt, ...)   ESP_LOGI(tag, "[SCHED] " fmt, ##__VA_ARGS__)
#else
  #define LOG_SCHED(tag, fmt, ...)   ((void)0)
#endif

/* ── UI 状态机诊断宏 ─────────────────────────────────────── */
#if LOG_UI_DIAG_ENABLE
  #define LOG_UI_DIAG(tag, fmt, ...) ESP_LOGI(tag, "[UI-DIAG] " fmt, ##__VA_ARGS__)
#else
  #define LOG_UI_DIAG(tag, fmt, ...) ((void)0)
#endif

/* ══════════════════════════════════════════════════════════════
 *  开发者模式 OLED 菜单编译开关
 * ══════════════════════════════════════════════════════════════ */

/** 开发者模式 OLED 菜单：0=不编译 Dev Mode 代码（零开销），1=编译 */
#define LOG_DEV_MODE_ENABLE         0

/* Root CMake BLUE_LONG_ENABLE_MIRROR (default ON) defines this for all targets.
 * Standalone builds without that project option keep the feature disabled.
 * Use idf.py -DBLUE_LONG_ENABLE_MIRROR=OFF build to disable it consistently.
 * Always test #if ... && ENABLE_OLED_MIRROR, never just #ifdef. */
#ifndef ENABLE_OLED_MIRROR
#define ENABLE_OLED_MIRROR          0 /* Standalone fallback; root CMake is authoritative. */
#endif

/** 阶段 0 硬件交互测试：0=不编译硬件测试代码，1=编译 */
#define LOG_HW_TEST_ENABLE          1

#endif /* LOG_CONFIG_H */
