#include "message_service.h"
#include "morse_training_ui.h"
#include "morse_playback.h"
/**
 * @file    rgb_led.c
 * @brief   RGB LED 异步灯效引擎
 *
 * 架构：
 *   - 底层: rmt_ws2812.c — 串行传输、显式复位间隔和亮度缩放
 *   - 中层: 活动期间自动重载定时器 + 单调时钟帧截止时间
 *   - 上层: 业务事件触发 — rgb_led_trigger(event)
 *
 * 调度约束：
 *   - API 串行化状态；回调只尝试拿锁，忙时下个 tick 重试
 *   - 定时器命令不等待队列；启动失败立即灭灯，停止失败清空状态后重试
 *   - 帧截止时间保留每步时长，过期回调不会提前推进新效果
 *
 * 负载开关管理：
 *   - trigger() 入口 → acquire LOAD_SW_RGB
 *   - 效果完成后 → release
 *   - OLED 与 RGB 共享 GPIO4(WS4621C)，双方都释放时关断
 */

#include "rgb_led.h"
#include "rmt_ws2812.h"
#include "load_switch.h"
#include "nvs_config.h"
#include "defaults.h"          /* V8.0: DEFAULT_RGB_FRIEND_* */
#include "wououi.h"           /* 读取 win.is_msg / ui.index 判断未读状态 */
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "rgb_led";

/* ══════════════════════════════════════════════════════════════
 *  内部效果枚举（私有，不暴露给 Phase 3）
 * ══════════════════════════════════════════════════════════════ */

typedef enum {
    EFFECT_FLASH_ONCE = 0,      /**< 快闪一次 */
    EFFECT_FLASH_TWICE,         /**< 快闪两次 */
    EFFECT_BREATHING,           /**< 单色呼吸（正弦亮度） */
    EFFECT_RAINBOW_BREATHING,   /**< 彩虹呼吸（HSV 色相循环） */
    EFFECT_CYCLE_SLOW,          /**< 慢闪循环 */
    EFFECT_CYCLE_FAST,          /**< 快闪循环 */
    EFFECT_SOLID,               /**< 常亮 */
    EFFECT_OFF,                 /**< 关闭 */

    EFFECT_COUNT
} rgb_effect_t;

/* ══════════════════════════════════════════════════════════════
 *  效果步骤定义
 * ══════════════════════════════════════════════════════════════ */

/** 单个灯效步骤 */
typedef struct {
    uint16_t duration_ms;       /**< 本步骤持续时间，0=序列结束 */
    uint8_t  r;                 /**< 红色 0..255 */
    uint8_t  g;                 /**< 绿色 0..255 */
    uint8_t  b;                 /**< 蓝色 0..255 */
} rgb_step_t;

/** 完整效果定义 */
typedef struct {
    rgb_effect_t  effect;       /**< 效果枚举 */
    const rgb_step_t *steps;    /**< 步骤数组（duration_ms=0 结束） */
    uint8_t       step_count;   /**< 步骤数 */
} rgb_effect_def_t;

/* ── EFFECT_FLASH_ONCE 模板 ──────────────────────────────── */
/* 使用时动态填充颜色和时长 */

/* ── EFFECT_FLASH_TWICE 模板 ──────────────────────────────── */

/* ── EFFECT_BREATHING 步骤（20 步 × 50ms = 1000ms 半周期）── */
#define BREATHING_STEPS  20
#define BREATHING_STEP_MS 50

/* ── EFFECT_RAINBOW_BREATHING 色相步数 ────────────────────── */
#define RAINBOW_STEPS     36    /* 每步 10° */

/* ══════════════════════════════════════════════════════════════
 *  事件→效果配置表
 * ══════════════════════════════════════════════════════════════ */

typedef struct {
    rgb_effect_t  effect;           /**< 效果模式 */
    uint8_t       color_r;          /**< 默认红色 */
    uint8_t       color_g;          /**< 默认绿色 */
    uint8_t       color_b;          /**< 默认蓝色 */
    uint16_t      duration_ms;      /**< 快闪持续 / 呼吸周期 */
    uint16_t      interval_ms;      /**< 间隔（仅 UNREAD 使用） */
    uint8_t       priority;         /**< 0=LOW, 1=MED, 2=HIGH */
} rgb_trigger_cfg_t;

static rgb_trigger_cfg_t g_cfg_table[RGB_EVENT_COUNT];

/* ══════════════════════════════════════════════════════════════
 *  运行时状态
 * ══════════════════════════════════════════════════════════════ */

/* Serialize effect state and power ownership. Timer callbacks only try the
 * mutex because a task may hold it while waiting for power-up or RMT. */
#define RGB_TIMER_POLL_MS 10
static SemaphoreHandle_t s_engine_mutex;
static int64_t s_next_step_us;
static uint32_t s_frame_ms;

static TimerHandle_t      s_timer       = NULL;   /**< 效果步骤定时器 */
static rgb_event_t        s_active_event = RGB_EVENT_COUNT; /**< 当前活跃事件 */
static uint8_t            s_active_priority = 0;   /**< 当前活跃优先级 */
static const rgb_step_t  *s_active_steps = NULL;   /**< 当前步骤数组 */
static uint8_t            s_step_index   = 0;      /**< 当前步骤索引 */
static uint8_t            s_step_count   = 0;      /**< 总步骤数 */

/* V7.0.1: 静态步骤缓冲区 — 替代栈指针，防止 use-after-free 导致定时器崩溃 */
#define RGB_MAX_STEPS 8
static rgb_step_t         s_steps_buf[RGB_MAX_STEPS];
/* ── 呼吸效果运行时状态 ──────────────────────────────────── */
static bool               s_breathing_active = false;
static uint8_t            s_breathing_step   = 0;     /**< 呼吸当前步 0..BREATHING_STEPS*2 */
static uint8_t            s_breathing_r      = 0;     /**< 呼吸目标颜色 R */
static uint8_t            s_breathing_g      = 0;     /**< 呼吸目标颜色 G */
static uint8_t            s_breathing_b      = 0;     /**< 呼吸目标颜色 B */

/* ── 彩虹呼吸运行时状态 ──────────────────────────────────── */
static bool               s_rainbow_active    = false;
static uint8_t            s_rainbow_step      = 0;    /**< 色相步 0..RAINBOW_STEPS */
static uint8_t            s_rainbow_brightness = 255; /**< 当前亮度（与呼吸叠加） */

/* ── UNREAD 状态 ──────────────────────────────────────────── */
static uint32_t           s_unread_last_breath_ms = 0;

/* ══════════════════════════════════════════════════════════════
 *  前向声明
 * ══════════════════════════════════════════════════════════════ */

static void timer_callback(TimerHandle_t timer);
static void start_effect(const rgb_step_t *steps, uint8_t count,
                         rgb_event_t event, uint8_t priority);
static void build_flash_once_steps(rgb_step_t *out, uint8_t r, uint8_t g, uint8_t b,
                                   uint16_t duration_ms);
static void build_flash_twice_steps(rgb_step_t *out, uint8_t r, uint8_t g, uint8_t b,
                                    uint16_t duration_ms);
static void start_breathing(uint8_t r, uint8_t g, uint8_t b, uint16_t period_ms,
                            rgb_event_t event, uint8_t priority);
static void start_rainbow_breathing(uint16_t period_ms, rgb_event_t event, uint8_t priority);
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v,
                       uint8_t *r, uint8_t *g, uint8_t *b);
static void effect_cleanup(void);
static void load_cfg_table(void);
static void trigger_locked(rgb_event_t event, bool custom_color,
                           uint8_t r, uint8_t g, uint8_t b);

static bool engine_lock(void)
{
    return s_engine_mutex && s_timer &&
           xSemaphoreTake(s_engine_mutex, portMAX_DELAY) == pdTRUE;
}

static void set_frame_deadline(uint32_t duration_ms)
{
    s_next_step_us = esp_timer_get_time() + (int64_t)(duration_ms ? duration_ms : 1) * 1000;
}

static void arm_effect_timer(uint32_t first_frame_ms)
{
    set_frame_deadline(first_frame_ms);
    /* Reset also starts an inactive auto-reload timer. Never wait for timer
     * command space while holding the engine mutex. */
    if (xTimerReset(s_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Cannot schedule RGB effect; switching LED off");
        effect_cleanup();
    }
}

/* ══════════════════════════════════════════════════════════════
 *  HSV → RGB 转换
 * ══════════════════════════════════════════════════════════════ */

static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v,
                       uint8_t *r, uint8_t *g, uint8_t *b)
{
    /* h: 0-360, s: 0-255, v: 0-255 */
    if (s == 0) {
        *r = *g = *b = v;
        return;
    }

    uint16_t region = h / 60;
    uint16_t remainder = (h - (region * 60)) * 255 / 60;
    uint8_t p = (uint8_t)((uint16_t)v * (255 - s) / 255);
    uint8_t q = (uint8_t)((uint16_t)v * (255 - ((uint16_t)s * remainder) / 255) / 255);
    uint8_t t = (uint8_t)((uint16_t)v * (255 - ((uint16_t)s * (255 - remainder)) / 255) / 255);

    switch (region) {
        case 0:  *r = v; *g = t; *b = p; break;
        case 1:  *r = q; *g = v; *b = p; break;
        case 2:  *r = p; *g = v; *b = t; break;
        case 3:  *r = p; *g = q; *b = v; break;
        case 4:  *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  效果构建器
 * ══════════════════════════════════════════════════════════════ */

static void build_flash_once_steps(rgb_step_t *out, uint8_t r, uint8_t g, uint8_t b,
                                   uint16_t duration_ms)
{
    out[0].duration_ms = duration_ms;
    out[0].r = r; out[0].g = g; out[0].b = b;
    out[1].duration_ms = 0;  /* 序列结束 → 触发关灯 + cleanup */
    out[1].r = 0; out[1].g = 0; out[1].b = 0;
}

static void build_flash_twice_steps(rgb_step_t *out, uint8_t r, uint8_t g, uint8_t b,
                                    uint16_t duration_ms)
{
    uint16_t gap = duration_ms / 2;
    if (gap < 30) gap = 30;
    out[0].duration_ms = duration_ms;
    out[0].r = r; out[0].g = g; out[0].b = b;
    out[1].duration_ms = gap;
    out[1].r = 0; out[1].g = 0; out[1].b = 0;
    out[2].duration_ms = duration_ms;
    out[2].r = r; out[2].g = g; out[2].b = b;
    out[3].duration_ms = 0;
    out[3].r = 0; out[3].g = 0; out[3].b = 0;
}

/* ══════════════════════════════════════════════════════════════
 *  效果启动
 * ══════════════════════════════════════════════════════════════ */

static void start_effect(const rgb_step_t *steps, uint8_t count,
                         rgb_event_t event, uint8_t priority)
{
    if (!steps || count == 0 || count > RGB_MAX_STEPS) {
        effect_cleanup();
        return;
    }
    s_active_event = event;
    s_active_priority = priority;
    memcpy(s_steps_buf, steps, count * sizeof(rgb_step_t));
    s_active_steps = s_steps_buf;
    s_step_index = 0;
    s_step_count = count;
    s_breathing_active = false;
    s_rainbow_active = false;
    rgb_set_color(steps[0].r, steps[0].g, steps[0].b);
    arm_effect_timer(steps[0].duration_ms);
}

static void start_breathing(uint8_t r, uint8_t g, uint8_t b, uint16_t period_ms,
                            rgb_event_t event, uint8_t priority)
{
    s_active_event = event;
    s_active_priority = priority;
    s_active_steps = NULL;
    s_step_count = 0;
    s_breathing_active = true;
    s_rainbow_active = false;
    s_breathing_step = 0;
    s_breathing_r = r;
    s_breathing_g = g;
    s_breathing_b = b;
    s_frame_ms = period_ms / (BREATHING_STEPS * 2);
    if (s_frame_ms < 10) s_frame_ms = 10;
    rgb_set_color(0, 0, 0);
    arm_effect_timer(s_frame_ms);
}

static void start_rainbow_breathing(uint16_t period_ms, rgb_event_t event, uint8_t priority)
{
    s_active_event = event;
    s_active_priority = priority;
    s_active_steps = NULL;
    s_step_count = 0;
    s_breathing_active = false;
    s_rainbow_active = true;
    s_rainbow_step = 0;
    s_rainbow_brightness = 255;
    s_frame_ms = period_ms / RAINBOW_STEPS;
    if (s_frame_ms < 10) s_frame_ms = 10;
    rgb_set_color(255, 0, 0);
    arm_effect_timer(s_frame_ms);
}

/* ══════════════════════════════════════════════════════════════
 *  效果清理
 * ══════════════════════════════════════════════════════════════ */

static void effect_cleanup(void)
{
    /* Clear state before stop: a stale callback or failed stop must never
     * turn the LED back on or corrupt a replacement effect. */
    s_next_step_us = 0;
    s_active_event = RGB_EVENT_COUNT;
    s_active_priority = 0;
    s_active_steps = NULL;
    s_step_count = 0;
    s_step_index = 0;
    memset(s_steps_buf, 0, sizeof(s_steps_buf));
    s_breathing_active = false;
    s_rainbow_active = false;
    if (s_timer && xTimerStop(s_timer, 0) != pdPASS) {
        ESP_LOGW(TAG, "RGB timer stop queue full; state already cleared");
    }
    rgb_off();
    load_switch_release(LOAD_SW_RGB);
}

/* ══════════════════════════════════════════════════════════════
 *  定时器回调 — 效果步骤链推进
 * ══════════════════════════════════════════════════════════════ */

static void advance_effect(void)
{
    if (s_breathing_active) {
        s_breathing_step++;
        uint8_t total_steps = BREATHING_STEPS * 2;
        if (s_breathing_step >= total_steps) {
            if (s_active_event == RGB_EVENT_UNREAD_BREATHING) {
                s_unread_last_breath_ms = (uint32_t)(esp_timer_get_time() / 1000);
            }
            effect_cleanup();
            return;
        }
        float phase = (float)s_breathing_step / (float)total_steps * (float)M_PI;
        uint8_t brightness = (uint8_t)(sinf(phase) * 255.0f);
        rgb_set_color((uint16_t)s_breathing_r * brightness / 255,
                      (uint16_t)s_breathing_g * brightness / 255,
                      (uint16_t)s_breathing_b * brightness / 255);
        set_frame_deadline(s_frame_ms);
        return;
    }
    if (s_rainbow_active) {
        s_rainbow_step = (s_rainbow_step + 1) % RAINBOW_STEPS;
        uint8_t r, g, b;
        hsv_to_rgb((uint16_t)s_rainbow_step * 360 / RAINBOW_STEPS,
                   255, s_rainbow_brightness, &r, &g, &b);
        rgb_set_color(r, g, b);
        set_frame_deadline(s_frame_ms);
        return;
    }
    if (!s_active_steps || ++s_step_index >= s_step_count ||
        s_active_steps[s_step_index].duration_ms == 0) {
        effect_cleanup();
        return;
    }
    const rgb_step_t *step = &s_active_steps[s_step_index];
    /* Override colors are baked into lit steps. Dark gaps stay dark. */
    rgb_set_color(step->r, step->g, step->b);
    set_frame_deadline(step->duration_ms);
}

static void timer_callback(TimerHandle_t timer)
{
    (void)timer;
    /* Never wait for a task from the timer daemon. Auto-reload guarantees a
     * later callback even when power-up or another caller owns the mutex. */
    if (xSemaphoreTake(s_engine_mutex, 0) != pdTRUE) return;
    if (s_active_event == RGB_EVENT_COUNT) {
        /* Retry cleanup stop after a full timer command queue. */
        if (xTimerStop(s_timer, 0) != pdPASS) {
            ESP_LOGW(TAG, "RGB idle timer stop still pending");
        }
    } else if (!g_config.led_enabled) {
        effect_cleanup();
    } else if (s_next_step_us && esp_timer_get_time() >= s_next_step_us) {
        advance_effect();
    }
    xSemaphoreGive(s_engine_mutex);
}

/* ══════════════════════════════════════════════════════════════
 *  配置表加载
 * ══════════════════════════════════════════════════════════════ */

static void load_cfg_table(void)
{
    /* ── MENU_ENTER ── */
    g_cfg_table[RGB_EVENT_MENU_ENTER] = (rgb_trigger_cfg_t){
        .effect       = EFFECT_FLASH_ONCE,
        .color_r      = g_config.rgb_color_menu[0],
        .color_g      = g_config.rgb_color_menu[1],
        .color_b      = g_config.rgb_color_menu[2],
        .duration_ms  = g_config.rgb_flash_duration_ms,
        .interval_ms  = 0,
        .priority     = 1,  /* MED */
    };

    /* ── MSG_DELIVERED ── */
    g_cfg_table[RGB_EVENT_MSG_DELIVERED] = (rgb_trigger_cfg_t){
        .effect       = EFFECT_FLASH_ONCE,
        .color_r      = g_config.rgb_color_delivered[0],
        .color_g      = g_config.rgb_color_delivered[1],
        .color_b      = g_config.rgb_color_delivered[2],
        .duration_ms  = g_config.rgb_flash_duration_ms,
        .interval_ms  = 0,
        .priority     = 1,
    };

    /* ── MSG_CACHED ── */
    g_cfg_table[RGB_EVENT_MSG_CACHED] = (rgb_trigger_cfg_t){
        .effect       = EFFECT_FLASH_ONCE,
        .color_r      = g_config.rgb_color_cached[0],
        .color_g      = g_config.rgb_color_cached[1],
        .color_b      = g_config.rgb_color_cached[2],
        .duration_ms  = g_config.rgb_flash_duration_ms,
        .interval_ms  = 0,
        .priority     = 1,
    };

    /* ── MSG_FORWARD ── */
    g_cfg_table[RGB_EVENT_MSG_FORWARD] = (rgb_trigger_cfg_t){
        .effect       = EFFECT_FLASH_ONCE,
        .color_r      = g_config.rgb_color_forward[0],
        .color_g      = g_config.rgb_color_forward[1],
        .color_b      = g_config.rgb_color_forward[2],
        .duration_ms  = g_config.rgb_flash_duration_ms,
        .interval_ms  = 0,
        .priority     = 1,
    };

    /* ── MSG_RECEIVED ── */
    g_cfg_table[RGB_EVENT_MSG_RECEIVED] = (rgb_trigger_cfg_t){
        .effect       = EFFECT_FLASH_ONCE,
        .color_r      = g_config.rgb_color_received[0],
        .color_g      = g_config.rgb_color_received[1],
        .color_b      = g_config.rgb_color_received[2],
        .duration_ms  = g_config.rgb_flash_duration_ms,
        .interval_ms  = 0,
        .priority     = 1,
    };

    /* ── UNREAD_BREATHING ── */
    g_cfg_table[RGB_EVENT_UNREAD_BREATHING] = (rgb_trigger_cfg_t){
        .effect       = EFFECT_BREATHING,
        .color_r      = g_config.rgb_color_received[0],
        .color_g      = g_config.rgb_color_received[1],
        .color_b      = g_config.rgb_color_received[2],
        .duration_ms  = g_config.rgb_breathing_period_ms,
        .interval_ms  = g_config.rgb_unread_interval_ms,
        .priority     = 0,  /* LOW — 可被任何事件打断 */
    };

    /* ── V8.0: FRIEND_ADDED ── */
    g_cfg_table[RGB_EVENT_FRIEND_ADDED] = (rgb_trigger_cfg_t){
        .effect       = EFFECT_FLASH_ONCE,
        .color_r      = DEFAULT_RGB_FRIEND_ADDED_R,
        .color_g      = DEFAULT_RGB_FRIEND_ADDED_G,
        .color_b      = DEFAULT_RGB_FRIEND_ADDED_B,
        .duration_ms  = g_config.rgb_flash_duration_ms,
        .interval_ms  = 0,
        .priority     = 1,  /* MED */
    };

    /* ── V8.0: FRIEND_TIMEOUT ── */
    g_cfg_table[RGB_EVENT_FRIEND_TIMEOUT] = (rgb_trigger_cfg_t){
        .effect       = EFFECT_FLASH_ONCE,
        .color_r      = DEFAULT_RGB_FRIEND_TIMEOUT_R,
        .color_g      = DEFAULT_RGB_FRIEND_TIMEOUT_G,
        .color_b      = DEFAULT_RGB_FRIEND_TIMEOUT_B,
        .duration_ms  = g_config.rgb_flash_duration_ms,
        .interval_ms  = 0,
        .priority     = 1,  /* MED */
    };

    /* ── GAME_ROLL — 小游戏滚动/洗牌中: 彩虹流动闪烁 ──
     * 循环播放: 由调用方 rgb_led_stop() 结束，或被 GAME_RESULT (HIGH) 覆盖 */
    g_cfg_table[RGB_LED_EVENT_GAME_ROLL] = (rgb_trigger_cfg_t){
        .effect       = EFFECT_RAINBOW_BREATHING,
        .color_r      = 0,      /* 彩虹效果不使用固定颜色 */
        .color_g      = 0,
        .color_b      = 0,
        .duration_ms  = 900,    /* 完整色相循环周期 (36 步 × 25ms) */
        .interval_ms  = 0,
        .priority     = 1,  /* MED */
    };

    /* ── GAME_RESULT — 定格揭晓: 白色短促双爆闪 ──
     * 必须 HIGH: 才能覆盖正在播放的 GAME_ROLL (MED) */
    g_cfg_table[RGB_LED_EVENT_GAME_RESULT] = (rgb_trigger_cfg_t){
        .effect       = EFFECT_FLASH_TWICE,
        .color_r      = 255,
        .color_g      = 255,
        .color_b      = 255,
        .duration_ms  = 80,     /* 80ms 亮 / 40ms 灭 / 80ms 亮 */
        .interval_ms  = 0,
        .priority     = 2,  /* HIGH */
    };
}

/* ══════════════════════════════════════════════════════════════
 *  公共 API 实现
 * ══════════════════════════════════════════════════════════════ */

void rgb_led_init(void)
{
    /* Called during firmware initialization, before event producers start. */
    if (s_timer) return;
    if (!s_engine_mutex) s_engine_mutex = xSemaphoreCreateMutex();
    if (!s_engine_mutex) {
        ESP_LOGE(TAG, "Failed to create RGB engine mutex");
        return;
    }
    TickType_t ticks = pdMS_TO_TICKS(RGB_TIMER_POLL_MS);
    if (!ticks) ticks = 1;
    s_timer = xTimerCreate("rgb_led", ticks, pdTRUE, NULL, timer_callback);
    if (!s_timer) {
        ESP_LOGE(TAG, "Failed to create RGB timer");
        return;
    }
    load_cfg_table();
    rgb_set_brightness(g_config.rgb_brightness);
    s_unread_last_breath_ms = 0;
    ESP_LOGI(TAG, "Engine ready (brightness=%u%%)", g_config.rgb_brightness);
}

/* The caller owns s_engine_mutex; never recurse through the public API. */
static void trigger_locked(rgb_event_t event, bool custom_color,
                           uint8_t r, uint8_t g, uint8_t b)
{
    if (!g_config.led_enabled) return;
    const rgb_trigger_cfg_t *cfg = &g_cfg_table[event];
    if (s_active_event != RGB_EVENT_COUNT && s_active_priority >= cfg->priority) return;
    if (!custom_color) {
        r = cfg->color_r;
        g = cfg->color_g;
        b = cfg->color_b;
    }
    load_switch_acquire(LOAD_SW_RGB);
    switch (cfg->effect) {
        case EFFECT_FLASH_ONCE: {
            rgb_step_t steps[2];
            build_flash_once_steps(steps, r, g, b, cfg->duration_ms);
            start_effect(steps, 2, event, cfg->priority);
            break;
        }
        case EFFECT_FLASH_TWICE: {
            rgb_step_t steps[4];
            build_flash_twice_steps(steps, r, g, b, cfg->duration_ms);
            start_effect(steps, 4, event, cfg->priority);
            break;
        }
        case EFFECT_BREATHING:
            start_breathing(r, g, b, cfg->duration_ms, event, cfg->priority);
            break;
        case EFFECT_RAINBOW_BREATHING:
            start_rainbow_breathing(cfg->duration_ms, event, cfg->priority);
            break;
        case EFFECT_SOLID:
            s_active_event = event;
            s_active_priority = cfg->priority;
            s_breathing_active = false;
            s_rainbow_active = false;
            s_active_steps = NULL;
            s_step_count = 0;
            s_next_step_us = 0;
            if (xTimerStop(s_timer, 0) != pdPASS) {
                ESP_LOGW(TAG, "RGB solid timer stop queue full");
            }
            rgb_set_color(r, g, b);
            break;
        default:
            effect_cleanup();
            break;
    }
}

void rgb_led_trigger(rgb_event_t event)
{
    if ((unsigned)event >= RGB_EVENT_COUNT || !engine_lock()) return;
    trigger_locked(event, false, 0, 0, 0);
    xSemaphoreGive(s_engine_mutex);
}

void rgb_led_trigger_color(rgb_event_t event, uint8_t r, uint8_t g, uint8_t b)
{
    if ((unsigned)event >= RGB_EVENT_COUNT || !engine_lock()) return;
    trigger_locked(event, true, r, g, b);
    xSemaphoreGive(s_engine_mutex);
}

void rgb_led_stop(void)
{
    if (!engine_lock()) return;
    effect_cleanup();
    xSemaphoreGive(s_engine_mutex);
}

void rgb_led_unread_tick(void)
{
    /* 仅在 LED 使能时工作 */
    if (!g_config.led_enabled) return;

    /* 检查是否有未读消息（弹窗未消耗）
     * V9.4: 发送确认弹窗(is_sent=true)不触发未读呼吸灯 */
    if (!message_service_unread_count() || morse_training_ui_is_active() ||
        morse_playback_is_active() || g_morse_key_active) return;

    if (!engine_lock()) return;
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    uint16_t interval = g_config.rgb_unread_interval_ms;
    if (s_unread_last_breath_ms == 0 ||
        (now_ms - s_unread_last_breath_ms) >= interval) {
        trigger_locked(RGB_EVENT_UNREAD_BREATHING, false, 0, 0, 0);
    }
    xSemaphoreGive(s_engine_mutex);
}

void rgb_led_set_brightness(uint8_t pct)
{
    rgb_set_brightness(pct);
}
