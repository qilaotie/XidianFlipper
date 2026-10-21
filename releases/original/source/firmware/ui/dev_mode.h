/**
 * @file    dev_mode.h
 * @brief   开发者模式 OLED 菜单 — V0.10.0
 *
 * 提供阶段 0 硬件交互测试 + 阶段 1-6 参数诊断 + 系统级诊断的 OLED 渲染。
 * 全部代码受 LOG_DEV_MODE_ENABLE 编译开关控制。
 */

#ifndef DEV_MODE_H
#define DEV_MODE_H

#include <stdint.h>
#include <stdbool.h>
#include "log_config.h"

#if LOG_DEV_MODE_ENABLE

#include "wououi.h"
#include "ec06.h"     /* ec06_event_t（统一输入驱动） */

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════
 *  硬件测试状态机（阶段 0）
 * ══════════════════════════════════════════════════════════════ */

typedef enum {
    HW_TEST_MENU = 0,       /**< 测试菜单列表 */
    HW_TEST_OLED,           /**< OLED 显示模式测试 */
    HW_TEST_RGB,            /**< WS2812 RGB LED 测试 */
    HW_TEST_BUZZER,         /**< 蜂鸣器测试 */
    HW_TEST_MOTOR,          /**< 震动马达测试 */
    HW_TEST_ENCODER,        /**< 编码器实时监测 */
    HW_TEST_KEY,            /**< KEY 按钮实时监测 */
    HW_TEST_ENC_BTN,        /**< ENC_BTN 实时监测 */
    HW_TEST_CHARGE,         /**< 充电检测实时监测 */
    HW_TEST_DONE
} hw_test_state_t;

/* 子测试步进索引（用于 OLED/RGB/Buzzer/Motor 步进模式） */
typedef enum {
    HW_STEP_OLED_NONE = 0,
    HW_STEP_OLED_ALL_ON,
    HW_STEP_OLED_ALL_OFF,
    HW_STEP_OLED_CHECKER,
    HW_STEP_OLED_VLINE,
    HW_STEP_OLED_HLINE,
    HW_STEP_OLED_NORMAL,
    HW_STEP_OLED_MAX
} hw_oled_step_t;

typedef enum {
    HW_STEP_RGB_NONE = 0,
    HW_STEP_RGB_RED,
    HW_STEP_RGB_GREEN,
    HW_STEP_RGB_BLUE,
    HW_STEP_RGB_WHITE,
    HW_STEP_RGB_YELLOW,
    HW_STEP_RGB_CYAN,
    HW_STEP_RGB_MAGENTA,
    HW_STEP_RGB_OFF,
    HW_STEP_RGB_MAX
} hw_rgb_step_t;

typedef enum {
    HW_STEP_BUZZER_NONE = 0,
    HW_STEP_BUZZER_SHORT,
    HW_STEP_BUZZER_LONG,
    HW_STEP_BUZZER_DOUBLE,
    HW_STEP_BUZZER_TRIPLE,
    HW_STEP_BUZZER_MAX
} hw_buzzer_step_t;

typedef enum {
    HW_STEP_MOTOR_NONE = 0,
    HW_STEP_MOTOR_LIGHT,
    HW_STEP_MOTOR_MEDIUM,
    HW_STEP_MOTOR_STRONG,
    HW_STEP_MOTOR_LONG,
    HW_STEP_MOTOR_MAX
} hw_motor_step_t;

/* ══════════════════════════════════════════════════════════════
 *  Dev Mode 诊断页面类型
 * ══════════════════════════════════════════════════════════════ */

typedef enum {
    DEV_PAGE_NONE = 0,
    DEV_PAGE_MAIN,          /**< Dev Mode 主菜单 */
    DEV_PAGE_HW_TEST,       /**< 阶段 0: 硬件测试 */
    DEV_PAGE_PHASE1,        /**< Phase 1: GPIO 诊断 */
    DEV_PAGE_PHASE2,        /**< Phase 2: Mesh 诊断 */
    DEV_PAGE_PHASE3,        /**< Phase 3: UI 诊断 */
    DEV_PAGE_PHASE4,        /**< Phase 4: GATT 诊断 (V9.0.1: GATT 已移除) */
    DEV_PAGE_PHASE5,        /**< Phase 5: OTA 诊断 (V9.0.1: OTA 已移除) */
    DEV_PAGE_PHASE6,        /**< Phase 6: Friends 诊断 */
    DEV_PAGE_SYS_DIAG,      /**< 系统级诊断 */
} dev_page_t;

/* ══════════════════════════════════════════════════════════════
 *  API
 * ══════════════════════════════════════════════════════════════ */

/**
 * @brief 初始化开发者模式模块。
 */
void dev_mode_init(void);

/**
 * @brief 渲染开发者模式页面。
 * @param page  当前 dev 页面类型
 */
void dev_mode_render(dev_page_t page);

/**
 * @brief 处理开发者模式输入事件。
 * @param page  当前 dev 页面类型
 * @param evt   输入事件指针
 * @return      输入是否被处理
 */
bool dev_mode_handle_input(dev_page_t page, const ec06_event_t *evt);

/**
 * @brief 获取当前硬件测试状态。
 */
hw_test_state_t dev_mode_get_hw_test_state(void);

/**
 * @brief 获取 Dev Mode 是否活跃。
 */
bool dev_mode_is_active(void);

/**
 * @brief 进入硬件测试菜单（HW_TEST_MENU 状态）。
 *
 * 从 dev_mode_page_proc() 调用以初始化硬件测试子页面。
 */
void dev_mode_enter_hw_test(void);

/**
 * @brief 清理硬件测试资源（RGB/Buzzer/Motor 关闭）。
 *
 * 从 dev_mode_page_proc() 调用以在返回主菜单时释放硬件。
 */
void dev_mode_hw_cleanup(void);

/**
 * @brief 查询当前是否在 Dev Mode 子页面 (非主菜单)。
 * @return true=子页面, false=主菜单
 */
bool dev_mode_is_in_sub_page(void);

/**
 * @brief 从子页面返回 Dev Mode 主菜单。
 */
void dev_mode_back_to_main(void);

/**
 * @brief 设置当前 Dev Mode 页面 (供 dev_mode_page_proc 同步状态)。
 */
void dev_mode_set_page(dev_page_t page);

/**
 * @brief 获取当前 Dev Mode 页面 (供 dev_mode_page_proc 读取状态)。
 * @return 当前 dev_page_t 枚举值
 */
dev_page_t dev_mode_get_page(void);

#if LOG_HW_TEST_ENABLE
/**
 * @brief 通知编码器旋转事件（用于硬件测试中的编码器测试）。
 * @param cw  true=顺时针, false=逆时针
 */
void dev_mode_notify_encoder(bool cw);

/**
 * @brief 通知按键事件（用于硬件测试中的 KEY/ENC_BTN 测试）。
 * @param pressed      true=按下, false=释放
 * @param duration_ms  按压时长（毫秒）
 */
void dev_mode_notify_key(bool pressed, uint32_t duration_ms);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LOG_DEV_MODE_ENABLE */
#endif /* DEV_MODE_H */
