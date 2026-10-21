/**
 * @file    rgb_led.h
 * @brief   RGB LED 异步灯效引擎
 *
 * 基于 FreeRTOS 自动重载定时器和帧截止时间实现异步灯效。
 * 上层只需调用 rgb_led_trigger(event)，引擎自动查表获取颜色/效果/时长。
 *
 * API 在任务上下文调用，串行保护状态；定时器回调不等待引擎锁。
 *
 * 使用示例：
 *   rgb_led_init();                          // 初始化
 *   rgb_led_trigger(RGB_EVENT_MENU_ENTER);   // 进入菜单 → 橙色快闪
 *   rgb_led_unread_tick();                   // ui_task 主循环每秒调用
 *
 * 优先级（仿 haptic）：
 *   LOW  — UNREAD_BREATHING：可被任何模式覆盖
 *   MED  — 业务事件（MENU/MSG_*）：仅被 HIGH 覆盖
 *   HIGH — 预留 EMERGENCY
 */

#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════
 *  业务事件枚举（Phase 3 调用入口）
 * ══════════════════════════════════════════════════════════════ */

typedef enum {
    RGB_EVENT_MENU_ENTER = 0,       /**< 进入顶级菜单 */
    RGB_EVENT_MSG_DELIVERED,        /**< 消息已送达 */
    RGB_EVENT_MSG_CACHED,           /**< 消息未送达已缓存 */
    RGB_EVENT_MSG_FORWARD,          /**< 转发别人消息 */
    RGB_EVENT_MSG_RECEIVED,         /**< 收到新消息 */
    RGB_EVENT_UNREAD_BREATHING,     /**< 未读消息呼吸（内部 tick 驱动） */
    RGB_EVENT_FRIEND_ADDED,         /**< V8.0: 好友添加成功 */
    RGB_EVENT_FRIEND_TIMEOUT,       /**< V8.0: 好友添加超时 */
    RGB_LED_EVENT_GAME_ROLL,        /**< 小游戏: 滚动/洗牌中 — 彩虹流动(循环, 需 stop 或被覆盖) */
    RGB_LED_EVENT_GAME_RESULT,      /**< 小游戏: 定格揭晓 — 白色短促双爆闪 (HIGH) */

    RGB_EVENT_COUNT                 /**< 事件总数 */
} rgb_event_t;

/* ══════════════════════════════════════════════════════════════
 *  公共 API（Phase 3 唯一入口）
 * ══════════════════════════════════════════════════════════════ */

/**
 * @brief 初始化 RGB LED 灯效引擎。
 *
 * - 创建 FreeRTOS 自动重载定时器，仅在灯效活动期间运行
 * - 从 g_config 读取 RGB 颜色/时长配置填充内部配置表
 * - 调用 rgb_set_brightness() 应用全局亮度
 *
 * 在 firmware_init_all() 中 rgb_init() 之后调用。
 */
void rgb_led_init(void);

/**
 * @brief 触发业务灯效（异步播放）。
 *
 * 根据 event 查内部配置表获取颜色+效果+时长，启动定时器异步播放。
 * 入口自动 acquire 负载开关，效果结束时自动 release。
 *
 * @param event  业务事件类型
 */
void rgb_led_trigger(rgb_event_t event);

/**
 * @brief 触发业务灯效并动态覆盖颜色（异步播放）。
 *
 * 用于需要根据运行时状态决定颜色的场景（如 friend_mgr 碰一碰）。
 * 其他参数（效果、时长）仍从配置表读取。
 *
 * @param event  业务事件类型
 * @param r      红色 0..255
 * @param g      绿色 0..255
 * @param b      蓝色 0..255
 */
void rgb_led_trigger_color(rgb_event_t event, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 强制停止所有灯效并熄灭 LED。
 */
void rgb_led_stop(void);

/**
 * @brief 未读消息呼吸灯 tick 驱动。
 *
 * 由 ui_task 主循环每秒调用一次。
 * 内部判断：
 *   - 是否有未消耗的消息弹窗
 *   - 距上次呼吸是否已过 RGB_UNREAD_INTERVAL_MS
 *   - 当前无更高优先级灯效时触发一次呼吸周期
 *
 * 调用者只需每秒调用一次，无需关心内部逻辑。
 */
void rgb_led_unread_tick(void);

/**
 * @brief 动态修改全局亮度（运行时即时生效）。
 * @param pct  亮度百分比 0-100
 */
void rgb_led_set_brightness(uint8_t pct);

#ifdef __cplusplus
}
#endif

#endif /* RGB_LED_H */
