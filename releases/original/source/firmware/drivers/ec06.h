/**
 * @file    ec06.h
 * @brief   EC06 统一输入驱动 — EC11 编码器 + GPIO19 独立按键
 *
 * 合并自 input_ec11 (WouoUI 移植) 和 input_encoder (自研)。
 * 架构: 四路 GPIO ANYEDGE ISR → 环形缓冲 → ec06_task → FreeRTOS 事件队列
 *
 * 引脚: ENC_A=GPIO2, ENC_B=GPIO3, ENC_BTN=GPIO18, KEY=GPIO19 (外部 100k 上拉)
 *
 * 按键映射:
 *   ENC_BTN 按下 → EC06_EVENT_ENC_DOWN      (莫斯键按下, V6.13)
 *   ENC_BTN 释放 → EC06_EVENT_ENC_UP        (莫斯键释放, 携带按压时长, V6.13)
 *   ENC_BTN 短按 → EC06_EVENT_SHORT_PRESS   (确认键)
 *   ENC_BTN 长按 → EC06_EVENT_LONG_PRESS    (≥300ms)
 *   ENC_BTN 双击 → EC06_EVENT_DOUBLE_CLICK  (≤400ms 间隔)
 *   KEY 按下     → EC06_EVENT_KEY_DOWN      (取消/返回)
 *   KEY 释放     → EC06_EVENT_KEY_UP        (携带按压时长)
 *
 * 诊断日志受 LOG_MASTER_ENABLE + LOG_LEVEL + LOG_INPUT_ENABLE 三层控制。
 */

#ifndef EC06_H
#define EC06_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════
 *  事件类型
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    EC06_EVENT_NONE         = 0,
    EC06_EVENT_CW           = 1,    /**< 编码器顺时针旋转 */
    EC06_EVENT_CCW          = 2,    /**< 编码器逆时针旋转 */
    EC06_EVENT_SHORT_PRESS  = 3,    /**< ENC_BTN 短按（确认） */
    EC06_EVENT_LONG_PRESS   = 4,    /**< ENC_BTN 长按（≥ long_press_ms） */
    EC06_EVENT_DOUBLE_CLICK = 5,    /**< ENC_BTN 双击（≤ double_click_ms 间隔） */
    EC06_EVENT_KEY_DOWN     = 6,    /**< KEY (GPIO19) 按下（取消/返回） */
    EC06_EVENT_KEY_UP       = 7,    /**< KEY (GPIO19) 释放 */
    EC06_EVENT_ENC_DOWN     = 8,    /**< ENC_BTN 按下（莫斯键, V6.13） */
    EC06_EVENT_ENC_UP       = 9,    /**< ENC_BTN 释放（莫斯键, 携带按压时长, V6.13） */
} ec06_event_type_t;

/* ═══════════════════════════════════════════════════════════
 *  事件数据
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    ec06_event_type_t type;         /**< 事件类型 */
    uint32_t          duration_ms;  /**< 按压时长 (KEY_UP / SHORT_PRESS / LONG_PRESS / DOUBLE_CLICK 时有效) */
    uint64_t          timestamp_us; /**< 原始 ESP timer 时间，不是 UI 消费时间 */
} ec06_event_t;

/* ═══════════════════════════════════════════════════════════
 *  配置
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t  gpio_a;            /**< 编码器 A 相引脚 (默认 GPIO2) */
    uint8_t  gpio_b;            /**< 编码器 B 相引脚 (默认 GPIO3) */
    uint8_t  gpio_btn;          /**< 编码器按键引脚 (默认 GPIO18) */
    uint8_t  gpio_key;          /**< 独立按键引脚 (默认 GPIO19) */
    bool     invert_dir;        /**< 反转编码器方向 */
    uint32_t debounce_ms;       /**< 按键消抖时间 (默认 5ms) */
    uint32_t short_press_ms;    /**< 短按最小持续时间 (默认 50ms, 低于此值视为噪声) */
    uint32_t long_press_ms;     /**< 长按触发阈值 (默认 300ms) */
    uint32_t double_click_ms;   /**< 双击最大间隔 (默认 400ms) */
    uint32_t key_debounce_ms;   /**< KEY 消抖时间 (默认 20ms) */
} ec06_config_t;

/** 默认配置（匹配硬件：外部 100k 上拉，内部上下拉禁用） */
#define EC06_CONFIG_DEFAULT {                          \
    .gpio_a = 2, .gpio_b = 3, .gpio_btn = 18, .gpio_key = 19, \
    .invert_dir = false,                               \
    .debounce_ms = 5, .short_press_ms = 50,             \
    .long_press_ms = 300, .double_click_ms = 400,       \
    .key_debounce_ms = 20 }

/* ═══════════════════════════════════════════════════════════
 *  API
 * ═══════════════════════════════════════════════════════════ */

/**
 * @brief 初始化 EC06 输入驱动。
 *
 * 配置四路 GPIO 为 ANYEDGE 中断，安装 ISR 服务，
 * 创建 ec06_task（优先级 5，栈 3584 字节），
 * 创建事件队列（容量 16）。
 *
 * @param cfg  配置指针，NULL 则使用 EC06_CONFIG_DEFAULT。
 */
void ec06_init(const ec06_config_t *cfg);

/**
 * @brief 获取下一个输入事件（非阻塞）。
 *
 * @return ec06_event_t  事件数据。若无事件，type == EC06_EVENT_NONE。
 */
ec06_event_t ec06_pop_event(void);

/**
 * @brief 获取并清零编码器累计步数。
 *
 * @return int32_t  上次调用以来的净旋转步数（+CW / -CCW）。
 */
int32_t ec06_get_count(void);

/**
 * @brief 获取 KEY (GPIO19) 最后一次按压时长（毫秒）。
 *
 * 若 KEY 当前按住，返回实时已按住时长。
 */
uint32_t ec06_get_key_duration_ms(void);

/**
 * @brief 检查 KEY (GPIO19) 当前是否按住。
 */
bool ec06_is_key_held(void);

/**
 * @brief 输出诊断统计到串口（事件计数、队列溢出等）。
 */
void ec06_dump_stats(void);

/**
 * @brief 运行时更新短按/长按阈值（设置菜单 Btn SPT/LPT 即时生效用）。
 *
 * @param short_press_ms  短按最小持续时间（低于此值视为噪声）
 * @param long_press_ms   长按触发阈值
 */
void ec06_set_press_thresholds(uint32_t short_press_ms, uint32_t long_press_ms);

/** 合并实体/远程来源后的原始边沿；在 EC06 任务调用，回调只能非阻塞入队。 */
typedef void (*ec06_raw_encoder_observer_t)(bool pressed, int64_t timestamp_us, void *ctx);
void ec06_set_raw_encoder_observer(ec06_raw_encoder_observer_t observer, void *ctx);
bool ec06_encoder_is_held(void);
typedef enum {
    EC06_REMOTE_ROTATE = 1,
    EC06_REMOTE_ENCODER_DOWN = 2,
    EC06_REMOTE_ENCODER_UP = 3,
    EC06_REMOTE_BACK_DOWN = 4,
    EC06_REMOTE_BACK_UP = 5,
    EC06_REMOTE_RELEASE_ALL = 6,
} ec06_remote_action_t;
/** true 仅表示入队；token 关联 USB 回执。 */
bool ec06_remote_input(ec06_remote_action_t action, int16_t value, uint32_t token);
typedef void (*ec06_remote_result_t)(uint32_t token, bool applied, void *ctx);
void ec06_set_remote_result_observer(ec06_remote_result_t observer, void *ctx);
/** 使尚未执行的远程输入失效并释放远程源，实体按住不受影响。 */
void ec06_remote_release_all(void);
/** Renew a 1.5 s remote-input lease, enforced by the EC06 task even if UI stalls. */
void ec06_remote_keepalive(void);

#ifdef __cplusplus
}
#endif
#endif /* EC06_H */
