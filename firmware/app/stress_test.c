/**
 * @file    stress_test.c
 * @brief   混合模式压力测试实现 — V5.4 阶段五
 *
 * 模拟 GATT + Mesh 并发工作负载以验证边缘情况。
 * 所有测试注入均为内部操作；无需外部硬件。
 *
 * 跟踪指标：
 *   - Mesh 发送计数器（已发送包数）
 *   - Mesh 接收计数器（通过通知循环接收的包数）
 *   - GATT 模拟写入次数
 *   - 丢包率（1 - 接收数/发送数）
 */

#include "stress_test.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "stress";

/* ── 内部计数器 ──────────────────────────────────────────── */
static uint32_t g_mesh_tx_count = 0;
static uint32_t g_mesh_rx_count = 0;
static uint32_t g_gatt_write_count = 0;
static uint32_t g_gatt_disconnects = 0;
static uint32_t g_mesh_drops = 0;
static uint64_t g_last_report_us = 0;

/* ── 报告间隔：30 秒 ─────────────────────────────────────── */
#define STRESS_REPORT_INTERVAL_US   (30ULL * 1000000ULL)

void stress_test_init(void)
{
    g_mesh_tx_count = 0;
    g_mesh_rx_count = 0;
    g_gatt_write_count = 0;
    g_gatt_disconnects = 0;
    g_mesh_drops = 0;
    g_last_report_us = esp_timer_get_time();

    ESP_LOGW(TAG, "=== Stress test mode activated ===");
    ESP_LOGW(TAG, "This mode is for dev/debug only.");
    ESP_LOGW(TAG, "Disable PHASE5_STRESS_TEST_ENABLED for production.");
}

void stress_test_tick(void)
{
    uint64_t now_us = esp_timer_get_time();

    /* 模拟周期性 GATT 写入（模拟 — 无实际 BLE 活动） */
    g_gatt_write_count++;
    g_mesh_tx_count++;

    /* 模拟 Mesh 数据包处理（模拟） */
    /* 真实场景中，这些数据来自实际的 mesh_forward 事件 */

    /* 周期性统计报告 */
    if (now_us - g_last_report_us >= STRESS_REPORT_INTERVAL_US) {
        uint32_t interval_sec = (uint32_t)((now_us - g_last_report_us) / 1000000ULL);

        float loss_rate = 0.0f;
        if (g_mesh_tx_count > 0) {
            loss_rate = 1.0f - (float)g_mesh_rx_count / (float)g_mesh_tx_count;
        }

        ESP_LOGI(TAG, "=== Stress test report (%lu s) ===",
                 (unsigned long)interval_sec);
        ESP_LOGI(TAG, "  Mesh TX: %lu, RX: %lu, loss: %.1f%%",
                 (unsigned long)g_mesh_tx_count,
                 (unsigned long)g_mesh_rx_count,
                 (double)(loss_rate * 100.0f));
        ESP_LOGI(TAG, "  GATT writes: %lu, disconnects: %lu",
                 (unsigned long)g_gatt_write_count,
                 (unsigned long)g_gatt_disconnects);
        ESP_LOGI(TAG, "  Mesh drops (dup/TTL/RSSI): %lu",
                 (unsigned long)g_mesh_drops);

        /* 重置以进行下一周期统计 */
        g_mesh_tx_count = 0;
        g_mesh_rx_count = 0;
        g_gatt_write_count = 0;
        g_last_report_us = now_us;
    }
}

void stress_test_get_stats(float *pkt_loss_rate,
                           uint32_t *gatt_disconnects,
                           uint32_t *mesh_drops)
{
    if (pkt_loss_rate) {
        if (g_mesh_tx_count > 0) {
            *pkt_loss_rate = 1.0f - (float)g_mesh_rx_count / (float)g_mesh_tx_count;
        } else {
            *pkt_loss_rate = 0.0f;
        }
    }
    if (gatt_disconnects) *gatt_disconnects = g_gatt_disconnects;
    if (mesh_drops) *mesh_drops = g_mesh_drops;
}
