/**
 * @file    log_manager.c
 * @brief   日志管理器运行时实现 + 统计收集 + 系统诊断 dump — V0.10.0
 */

#include "log_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "log_mgr";

/* ══════════════════════════════════════════════════════════════
 *  内部状态
 * ══════════════════════════════════════════════════════════════ */

static log_diag_stats_t s_stats;
static uint32_t s_channel_mask = 0xFF;      /**< 默认所有通道开启 */
static uint8_t  s_runtime_level = LOG_LVL_INFO;
static uint32_t s_init_timestamp_ms = 0;
static uint32_t s_last_rate_calc_ms = 0;
static uint32_t s_rate_line_count = 0;

/* RTC 持久化字段（在 DeepSleep 期间保留） */
static RTC_DATA_ATTR uint32_t s_rtc_boot_count = 0;
static RTC_DATA_ATTR uint32_t s_rtc_deepsleep_enter_count = 0;
static RTC_DATA_ATTR uint64_t s_rtc_deepsleep_total_ms = 0;
static RTC_DATA_ATTR uint32_t s_rtc_reset_reason = 0;
static RTC_DATA_ATTR uint32_t s_rtc_log_lines_dropped = 0;

/* ══════════════════════════════════════════════════════════════
 *  内部辅助
 * ══════════════════════════════════════════════════════════════ */

static const char *reset_reason_to_str(uint32_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:   return "Power-on";
    case ESP_RST_EXT:       return "External pin";
    case ESP_RST_SW:        return "Software reset";
    case ESP_RST_PANIC:     return "Exception/Panic";
    case ESP_RST_INT_WDT:   return "Interrupt WDT";
    case ESP_RST_TASK_WDT:  return "Task WDT";
    case ESP_RST_WDT:       return "Other WDT";
    case ESP_RST_DEEPSLEEP: return "DeepSleep wake";
    case ESP_RST_BROWNOUT:  return "Brownout";
    case ESP_RST_SDIO:      return "SDIO";
    case ESP_RST_USB:       return "USB UART";
    case ESP_RST_JTAG:      return "JTAG";
    default:                return "Unknown";
    }
}

/* ══════════════════════════════════════════════════════════════
 *  公开 API
 * ══════════════════════════════════════════════════════════════ */

void log_manager_init(void)
{
    s_init_timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);

    /* 恢复 RTC 持久化统计 */
    s_stats.boot_count             = ++s_rtc_boot_count;
    s_stats.deepsleep_enter_count  = s_rtc_deepsleep_enter_count;
    s_stats.deepsleep_total_ms     = (uint32_t)(s_rtc_deepsleep_total_ms / 1000);
    s_stats.reset_reason           = s_rtc_reset_reason;
    s_stats.log_lines_dropped      = s_rtc_log_lines_dropped;
    s_stats.last_diag_timestamp_ms = 0;

    /* 非持久化字段清零 */
    s_stats.log_bytes_written  = 0;
    s_stats.log_rate_per_sec   = 0;
    s_stats.mesh_pkt_rx        = 0;
    s_stats.mesh_pkt_tx        = 0;
    s_stats.gatt_connect_count = 0;
    s_stats.gatt_disconnect_count = 0;
    s_stats.task_watermark_radio   = 0;
    s_stats.task_watermark_ui      = 0;
    s_stats.task_watermark_battery = 0;
    s_stats.task_watermark_anim    = 0;

    /* 记录本次复位原因（供下次启动使用） */
    s_rtc_reset_reason = (uint32_t)esp_reset_reason();

    /* Dump 编译期开关矩阵 */
    log_manager_dump_build_config();

    ESP_LOGI(TAG, "Log manager initialized (boot#%lu, reset=%s, level=%u)",
             (unsigned long)s_stats.boot_count,
             reset_reason_to_str(s_stats.reset_reason),
             (unsigned)s_runtime_level);
}

void log_manager_set_level(uint8_t level)
{
    if (level > LOG_LVL_VERBOSE) level = LOG_LVL_VERBOSE;
    s_runtime_level = level;

    /* 同步到 ESP-IDF 底层 */
    esp_log_level_t esp_level = ESP_LOG_NONE;
    switch (level) {
    case LOG_LVL_ERROR:   esp_level = ESP_LOG_ERROR;   break;
    case LOG_LVL_WARN:    esp_level = ESP_LOG_WARN;    break;
    case LOG_LVL_INFO:    esp_level = ESP_LOG_INFO;    break;
    case LOG_LVL_DEBUG:   esp_level = ESP_LOG_DEBUG;   break;
    case LOG_LVL_VERBOSE: esp_level = ESP_LOG_VERBOSE; break;
    default:              esp_level = ESP_LOG_NONE;     break;
    }
    esp_log_level_set("*", esp_level);

    ESP_LOGI(TAG, "Log level set to %u (%s)", (unsigned)level,
             level == 0 ? "NONE" :
             level == 1 ? "ERROR" :
             level == 2 ? "WARN" :
             level == 3 ? "INFO" :
             level == 4 ? "DEBUG" : "VERBOSE");
}

uint8_t log_manager_get_level(void)
{
    return s_runtime_level;
}

void log_manager_set_channel(const char *channel, bool enabled)
{
    log_channel_t ch = 0;
    if (strcmp(channel, "usb") == 0)       ch = LOG_CH_USB;
    else if (strcmp(channel, "mesh") == 0)  ch = LOG_CH_MESH;
    else if (strcmp(channel, "gatt") == 0)   ch = LOG_CH_GATT;
    else if (strcmp(channel, "ui") == 0)     ch = LOG_CH_UI;
    else if (strcmp(channel, "power") == 0)  ch = LOG_CH_POWER;
    else if (strcmp(channel, "system") == 0) ch = LOG_CH_SYSTEM;
    else if (strcmp(channel, "input") == 0)  ch = LOG_CH_INPUT;
    else return;

    if (enabled) {
        s_channel_mask |= (uint32_t)ch;
    } else {
        s_channel_mask &= ~(uint32_t)ch;
    }

    ESP_LOGI(TAG, "Channel '%s' %s", channel, enabled ? "ENABLED" : "DISABLED");
}

bool log_manager_get_channel(const char *channel)
{
    log_channel_t ch = 0;
    if (strcmp(channel, "usb") == 0)       ch = LOG_CH_USB;
    else if (strcmp(channel, "mesh") == 0)  ch = LOG_CH_MESH;
    else if (strcmp(channel, "gatt") == 0)   ch = LOG_CH_GATT;
    else if (strcmp(channel, "ui") == 0)     ch = LOG_CH_UI;
    else if (strcmp(channel, "power") == 0)  ch = LOG_CH_POWER;
    else if (strcmp(channel, "system") == 0) ch = LOG_CH_SYSTEM;
    else if (strcmp(channel, "input") == 0)  ch = LOG_CH_INPUT;
    else return false;

    return (s_channel_mask & (uint32_t)ch) != 0;
}

const log_diag_stats_t *log_manager_get_stats(void)
{
    return &s_stats;
}

void log_manager_record_event(const char *event_type, uint32_t value)
{
    if (!event_type) return;

    if (strcmp(event_type, "mesh_rx") == 0) {
        s_stats.mesh_pkt_rx += value;
    } else if (strcmp(event_type, "mesh_tx") == 0) {
        s_stats.mesh_pkt_tx += value;
    } else if (strcmp(event_type, "gatt_connect") == 0) {
        s_stats.gatt_connect_count += value;
    } else if (strcmp(event_type, "gatt_disconnect") == 0) {
        s_stats.gatt_disconnect_count += value;
    } else if (strcmp(event_type, "deepsleep_enter") == 0) {
        s_stats.deepsleep_enter_count++;
        s_rtc_deepsleep_enter_count++;
    } else if (strcmp(event_type, "deepsleep_ms") == 0) {
        s_stats.deepsleep_total_ms += value;
        s_rtc_deepsleep_total_ms += value;
    } else if (strcmp(event_type, "log_dropped") == 0) {
        s_stats.log_lines_dropped += value;
        s_rtc_log_lines_dropped += value;
    }
    /* 可扩展更多事件类型 */
}

void log_manager_system_diag(void)
{
    if (!(s_channel_mask & LOG_CH_SYSTEM)) return;

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    uint32_t uptime_s = (now_ms > s_init_timestamp_ms)
                        ? (now_ms - s_init_timestamp_ms) / 1000 : 0;

    /* 更新 FreeRTOS 水位线快照 */
    /* Note: The task handles are currently not exported globally.
     * When the actual task creation code is available, add:
     *   extern TaskHandle_t g_radio_task_handle;
     *   extern TaskHandle_t g_ui_task_handle;
     * For now, show "N/A" values. */
    s_stats.task_watermark_radio   = 0;
    s_stats.task_watermark_ui      = 0;
    s_stats.task_watermark_battery = 0;
    s_stats.task_watermark_anim    = 0;

    /* 计算日志速率 */
    if (s_last_rate_calc_ms > 0 && now_ms > s_last_rate_calc_ms) {
        uint32_t elapsed_s = (now_ms - s_last_rate_calc_ms) / 1000;
        if (elapsed_s > 0) {
            s_stats.log_rate_per_sec = s_rate_line_count / elapsed_s;
        }
    }
    s_last_rate_calc_ms = now_ms;
    s_rate_line_count = 0;

    s_stats.last_diag_timestamp_ms = now_ms;

    ESP_LOGD(TAG, "══════ SYSTEM DIAGNOSTICS ══════");
    ESP_LOGD(TAG, "  Uptime:       %lu s", (unsigned long)uptime_s);
    ESP_LOGD(TAG, "  Boot count:   %lu", (unsigned long)s_stats.boot_count);
    ESP_LOGD(TAG, "  Reset reason: %s",
             reset_reason_to_str(s_stats.reset_reason));
    ESP_LOGD(TAG, "  Free heap:    %lu bytes (min: %lu)",
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGD(TAG, "  RTC Fast Mem: %lu / %lu bytes",
             (unsigned long)(s_rtc_boot_count * 16 + s_rtc_deepsleep_enter_count * 8),
             (unsigned long)8192);
    ESP_LOGD(TAG, "  Radio task WM: %lu bytes",
             (unsigned long)s_stats.task_watermark_radio);
    ESP_LOGD(TAG, "  UI task WM:    %lu bytes",
             (unsigned long)s_stats.task_watermark_ui);
    ESP_LOGD(TAG, "  DeepSleep:     %lu entries, %lu ms total",
             (unsigned long)s_stats.deepsleep_enter_count,
             (unsigned long)s_stats.deepsleep_total_ms);
    ESP_LOGD(TAG, "  Log dropped:   %lu lines, rate: %lu/s",
             (unsigned long)s_stats.log_lines_dropped,
             (unsigned long)s_stats.log_rate_per_sec);
    ESP_LOGD(TAG, "  Mesh: RX=%lu TX=%lu | GATT: conn=%lu disc=%lu",
             (unsigned long)s_stats.mesh_pkt_rx,
             (unsigned long)s_stats.mesh_pkt_tx,
             (unsigned long)s_stats.gatt_connect_count,
             (unsigned long)s_stats.gatt_disconnect_count);
    ESP_LOGD(TAG, "════════════════════════════════");
}

void log_manager_dump_build_config(void)
{
    ESP_LOGD(TAG, "═══ Build Configuration ═══");
    ESP_LOGD(TAG, "  LOG_MASTER_ENABLE:    %d", LOG_MASTER_ENABLE);
    ESP_LOGD(TAG, "  LOG_LEVEL:            %d", LOG_LEVEL);
    ESP_LOGD(TAG, "  Phase switches: P0=%d P1=%d P2=%d P3=%d P4=%d P5=%d P6=%d",
             LOG_PHASE0_ENABLE, LOG_PHASE1_ENABLE, LOG_PHASE2_ENABLE,
             LOG_PHASE3_ENABLE, LOG_PHASE4_ENABLE, LOG_PHASE5_ENABLE,
             LOG_PHASE6_ENABLE);
    ESP_LOGD(TAG, "  Module switches: MESH=%d GATT=%d UI=%d PWR=%d MORSE=%d FRD=%d HAPT=%d SYS=%d DIAG=%d IN=%d",
             LOG_MESH_ENABLE, LOG_GATT_ENABLE, LOG_UI_ENABLE,
             LOG_POWER_ENABLE, LOG_MORSE_ENABLE, LOG_FRIEND_ENABLE,
             LOG_HAPTIC_ENABLE, LOG_SYSTEM_ENABLE, LOG_DIAG_ENABLE,
             LOG_INPUT_ENABLE);
    ESP_LOGD(TAG, "  Dev Mode:            %d", LOG_DEV_MODE_ENABLE);
    ESP_LOGD(TAG, "  HW Test:             %d", LOG_HW_TEST_ENABLE);
    ESP_LOGD(TAG, "══════════════════════════════");
}
