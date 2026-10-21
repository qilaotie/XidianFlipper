/**
 * @file    event_dispatcher.c
 * @brief   轻量级事件系统实现 — V5.4
 *
 * 创建单个全局 EventGroup 用于跨任务协调。
 * 阶段一仅使用 EventGroup；后续阶段将为 UI 和
 * 动画任务添加队列。
 */

#include "event_dispatcher.h"
#include "esp_log.h"

static const char *TAG = "event";

EventGroupHandle_t g_evt_group = NULL;

void event_init(void)
{
    g_evt_group = xEventGroupCreate();
    if (g_evt_group == NULL) {
        ESP_LOGE(TAG, "Failed to create EventGroup!");
        abort();
    }
    ESP_LOGI(TAG, "Event system init done");
}
