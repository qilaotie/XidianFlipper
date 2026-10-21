/**
 * @file    event_dispatcher.h
 * @brief   轻量级事件系统 — V5.4
 *
 * 架构文档 §10.3 规定：
 *   - 不使用系统级 EventBus（避免 RAM 浪费、队列阻塞）
 *   - radio → mesh：直接回调
 *   - UI：队列
 *   - 动画：队列
 *   - 系统：EventGroup
 *
 * 阶段一只需要：
 *   - 最小化 EventGroup 用于唤醒/休眠协调
 *   - 简单的事件类型枚举供后续扩展
 */

#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdint.h>

/* ── 事件位定义（EventGroup）─────────────────────────────── */
#define EVT_WAKEUP_BIT          BIT0    /**< 设备从 DeepSleep 唤醒 */
#define EVT_BUTTON_PRESS_BIT    BIT1    /**< 任意按键按下 */
#define EVT_CHARGE_STATE_BIT    BIT2    /**< 充电状态变化 */
#define EVT_DEEP_SLEEP_REQ_BIT  BIT3    /**< 请求立即进入 DeepSleep */
#define EVT_EMERGENCY_RX_BIT    BIT4    /**< 收到紧急消息 */
#define EVT_UI_REFRESH_BIT      BIT5    /**< UI 需要刷新 */

/* ── 事件类型（用于队列事件，预留扩展）─────────────────── */
typedef enum {
    EVENT_NONE = 0,
    EVENT_BUTTON_PRESS,
    EVENT_BUTTON_LONG_PRESS,
    EVENT_ENCODER_CW,
    EVENT_ENCODER_CCW,
    EVENT_MESH_MSG_RX,
    EVENT_MESH_MSG_TX_DONE,
    EVENT_CHARGE_CHANGED,
    EVENT_TIMEOUT_OLED_ANIM,
    EVENT_TIMEOUT_OLED_SLEEP,
} event_type_t;

/* ── 事件数据结构体（用于队列事件）──────────────────────── */
typedef struct {
    event_type_t    type;
    uint32_t        arg1;
    uint32_t        arg2;
    void           *ptr;
} event_t;

/* ── 全局 EventGroup 句柄（仅初始化一次）────────────────── */
extern EventGroupHandle_t g_evt_group;

/**
 * @brief 初始化事件系统。
 *
 * 创建全局 EventGroup。必须在启动时、任何任务使用 event_*() 函数前调用一次。
 */
void event_init(void);

/**
 * @brief 设置一个或多个事件位。
 * @param bits  要设置的 EVT_*_BIT 位掩码。
 * @return 设置位之前的 EventGroup 值。
 */
static inline EventBits_t event_set(EventBits_t bits) {
    return xEventGroupSetBits(g_evt_group, bits);
}

/**
 * @brief 等待一个或多个事件位。
 * @param bits        要等待的 EVT_*_BIT 位掩码。
 * @param clear_on_ex 若为 true，退出时清除位。
 * @param wait_all    若为 true，等待全部位；否则等待任意位。
 * @param timeout_ms  超时毫秒数（portMAX_DELAY 表示永久等待）。
 * @return 满足等待条件的 EventGroup 值，超时返回 0。
 */
static inline EventBits_t event_wait(EventBits_t bits,
                                      bool clear_on_ex,
                                      bool wait_all,
                                      uint32_t timeout_ms) {
    return xEventGroupWaitBits(g_evt_group, bits,
                                clear_on_ex ? pdTRUE : pdFALSE,
                                wait_all ? pdTRUE : pdFALSE,
                                pdMS_TO_TICKS(timeout_ms));
}

/**
 * @brief 清除特定事件位。
 * @param bits  要清除的 EVT_*_BIT 位掩码。
 * @return 清除位之前的 EventGroup 值。
 */
static inline EventBits_t event_clear(EventBits_t bits) {
    return xEventGroupClearBits(g_evt_group, bits);
}

#endif /* EVENT_DISPATCHER_H */
