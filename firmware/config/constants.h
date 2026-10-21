/**
 * @file    constants.h
 * @brief   系统级常量定义 — V5.4
 *
 * 当前协议与职责见 docs/local-architecture.md；历史参数见 docs/archive/pre-v12/项目架构.md。
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

/* ── DeepSleep / 扫描周期 (V9.0 简化) ────────────────── */
#define DEFAULT_DEEPSLEEP_SEC       5       /* 默认 DeepSleep 周期（秒） */
#define DEEPSLEEP_GRACE_PERIOD_SEC  15      /* 冷启动后 DeepSleep 禁止窗口（秒），确保 USB 枚举完成 */
#define SCAN_PERIOD_MS              5000    /* V10.8: 扫描周期 5s（功耗优化: 2% 占空比） */
#define SCAN_WINDOW_MS              100     /* V10.8: 扫描窗口 100ms（功耗优化） */
#define COVER_ADV_MS                6000    /* V10.8: 覆盖广播 6s ≥ 扫描周期 5s */

/* Display standby and deep sleep are manual actions; no idle timeout applies. */

/* ── 莫尔斯电码 ─────────────────────────────────────────── */
#define OLED_WIDTH                  128
#define OLED_HEIGHT                 32
#define OLED_PAGES                  (OLED_HEIGHT / 8)

/**
 * OLED 全局强制电源开关（调试用）
 *
 *   0 = 正常模式（默认）— 跟随电源管理逻辑，DeepSleep 时关闭
 *   1 = 强制常开 — 启动后永不关闭 OLED，DeepSleep 也不 Hold LOW
 *       用于：屏幕不亮时定位是电源管理还是 I2C/初始化问题
 *   2 = 强制常关 — 永远不开启 OLED，所有 oled_power_on() 调用被忽略
 *       用于：隔离 OLED 功耗、验证其他功能不受 OLED 影响
 *
 *   调试完毕请恢复为 0。
 */
#define OLED_FORCE_POWER            0   /* 0=正常, 1=强制常开, 2=强制常关 */

/**
 * DeepSleep 全局强制禁止开关（调试用）
 *
 *   0 = 正常模式（默认）— 允许 DeepSleep
 *   1 = 强制禁止休眠 — enter_deep_sleep() 直接 return，设备永不入睡
 *       用于：调试 USB 枚举、OLED 显示、BLE 连接等场景
 *
 *   调试完毕请恢复为 0。
 */
#define DEEPSLEEP_FORCE_DISABLE     0   /* 0=正常, 1=强制禁止休眠 */

/* ── 莫尔斯电码 ─────────────────────────────────────────── */
/* 莫尔斯电码时序阈值现已由 morse_engine_set_wpm() 动态计算（V5.7 阶段八）。
 * 参见 morse_engine.h 中的 API 说明。
 * 保留消抖和容差常量： */
#define MORSE_DEBOUNCE_MS           20      /* 消抖阈值（小于此值 = 毛刺） */
#define MORSE_TOLERANCE_PCT         0.30f   /* 教学模式容差 ±30% */
#define MORSE_LETTER_GAP_MIN_MS     600     /* 字母间隔最小值（毫秒） */
#define MORSE_LETTER_GAP_MAX_MS     900     /* 字母间隔最大值（毫秒，可配置） */
#define MORSE_AUTO_SEND_MS_DEFAULT  1500    /* 空闲超时自动发送默认值（毫秒） */
#define MORSE_TX_AUTO_RX_MS         5000    /* V10.5: TX 无活动自动回 RX 超时（毫秒） */
#define RX_REVEAL_INTERVAL_MS       120     /* V10.5: 打字机逐字揭示间隔（毫秒） */

/* ── 快捷消息 ───────────────────────────────────────────── */
#define QUICK_MESSAGE_MAX_LEN       48
#define QUICK_MESSAGE_COUNT         10

/* ── BLE 广播 ──────────────────────────────────────────── */
#define BLE_ADV_INTERVAL_MIN_MS     100     /* 广播间隔最小值（毫秒） */
#define BLE_ADV_INTERVAL_MAX_MS     300     /* 广播间隔最大值（毫秒） */

/* ── NVS 非易失存储 ─────────────────────────────────────── */
#define NVS_NAMESPACE               "mesh_cfg"
#define NVS_COMMIT_DELAY_MS         5000    /* NVS 写入消抖延迟（毫秒） */

/* ── 定时器（可通过手机配置）────────────────────────────── */
#define DEFAULT_OLED_ANIM_TIMEOUT_SEC   30
#define DEFAULT_OLED_SLEEP_TIMEOUT_SEC  60

#endif /* CONSTANTS_H */
