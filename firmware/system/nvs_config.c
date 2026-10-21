/**
 * @file    nvs_config.c
 * @brief   基于 NVS 的配置管理 — V5.5 阶段六
 *
 * 实现 RAM 镜像 + 5 秒延迟写入策略。
 * 使用 FreeRTOS 单次定时器进行消抖。
 *
 * NVS 键名：
 *   "cfg_blob"   — 完整 v7 nvs_config_t 二进制块，含加载时清除的历史 _dirty 字段
 *   "cfg_ver"    — uint8_t 版本标记，用于迁移
 */

#include "nvs_config.h"
#include "friend_mgr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_system.h"          /* esp_restart() */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"       /* vTaskDelay() */
#include "freertos/timers.h"
#include <stddef.h>
#include <string.h>

static const char *TAG = "nvs_cfg";

/* ── 全局配置实例 ────────────────────────────────────────── */
nvs_config_t g_config;

/* ── NVS 键名 ─────────────────────────────────────────────── */
#define NVS_KEY_BLOB    "cfg_blob"
#define NVS_KEY_VERSION "cfg_ver"
/* Preserve v7 layout; new services have independent versioned stores. */
#define NVS_VERSION 7
static bool s_config_write_allowed = true;

static TimerHandle_t g_save_timer = NULL;
/* C6 is single-core: a short critical section takes a coherent RAM copy while
 * task-level setters are paused. Never hold it during Flash or timer queue IO.
 * One static writer snapshot avoids putting the v7 blob on the timer stack. */
static portMUX_TYPE s_save_lock = portMUX_INITIALIZER_UNLOCKED;
static nvs_config_t s_save_snapshot;
static uint32_t s_config_generation;
static bool s_save_in_progress;

/* ── 前向声明 ────────────────────────────────────────────── */
static void save_timer_cb(TimerHandle_t timer);
static void schedule_config_save(void)
{
    /* One nonblocking queue command. A completed older save must never stop a
     * newer edit's timer; a redundant callback simply sees a clean config. */
    if (g_save_timer && xTimerReset(g_save_timer, 0) != pdPASS) {
        ESP_LOGW(TAG, "Save timer queue full; configuration remains dirty");
    }
}

/* ── 辅助函数：设置默认值 ────────────────────────────────── */

static void config_set_defaults(void)
{
    memset(&g_config, 0, sizeof(g_config));

    g_config.led_enabled          = DEFAULT_LED_ENABLED;
    g_config.buzzer_enabled       = DEFAULT_BUZZER_ENABLED;
    g_config.vibration_enabled    = DEFAULT_VIBRATION_ENABLED;
    g_config.rgb_brightness       = DEFAULT_RGB_BRIGHTNESS;
    g_config.rgb_color_menu[0]    = DEFAULT_RGB_MENU_R;
    g_config.rgb_color_menu[1]    = DEFAULT_RGB_MENU_G;
    g_config.rgb_color_menu[2]    = DEFAULT_RGB_MENU_B;
    g_config.rgb_color_delivered[0] = DEFAULT_RGB_DELIVERED_R;
    g_config.rgb_color_delivered[1] = DEFAULT_RGB_DELIVERED_G;
    g_config.rgb_color_delivered[2] = DEFAULT_RGB_DELIVERED_B;
    g_config.rgb_color_cached[0]  = DEFAULT_RGB_CACHED_R;
    g_config.rgb_color_cached[1]  = DEFAULT_RGB_CACHED_G;
    g_config.rgb_color_cached[2]  = DEFAULT_RGB_CACHED_B;
    g_config.rgb_color_forward[0] = DEFAULT_RGB_FORWARD_R;
    g_config.rgb_color_forward[1] = DEFAULT_RGB_FORWARD_G;
    g_config.rgb_color_forward[2] = DEFAULT_RGB_FORWARD_B;
    g_config.rgb_color_received[0] = DEFAULT_RGB_RECEIVED_R;
    g_config.rgb_color_received[1] = DEFAULT_RGB_RECEIVED_G;
    g_config.rgb_color_received[2] = DEFAULT_RGB_RECEIVED_B;
    g_config.rgb_flash_duration_ms = DEFAULT_RGB_FLASH_DURATION_MS;
    g_config.rgb_breathing_period_ms = DEFAULT_RGB_BREATHING_PERIOD_MS;
    g_config.rgb_unread_interval_ms = DEFAULT_RGB_UNREAD_INTERVAL_MS;
    g_config.dark_mode            = DEFAULT_DARK_MODE;
    g_config.long_text_scroll_enable = DEFAULT_LONG_TEXT_SCROLL_ENABLE;
    g_config.long_text_scroll_speed  = DEFAULT_LONG_TEXT_SCROLL_SPEED;
    g_config.long_text_threshold_px  = DEFAULT_LONG_TEXT_THRESHOLD_PX;
    g_config.oled_anim_timeout    = DEFAULT_OLED_ANIM_TIMEOUT;
    g_config.oled_sleep_timeout   = DEFAULT_OLED_SLEEP_TIMEOUT;
    g_config.morse_dot_ms         = DEFAULT_MORSE_DOT_MS;
    g_config.morse_letter_gap_ms  = DEFAULT_MORSE_LETTER_GAP_MS;
    g_config.morse_auto_send_ms   = MORSE_AUTO_SEND_MS_DEFAULT;
    g_config.morse_wpm            = DEFAULT_MORSE_WPM;
    g_config.morse_auto_decode    = DEFAULT_MORSE_AUTO_DECODE;
#if LOG_DEV_MODE_ENABLE
    g_config.log_level            = DEFAULT_LOG_LEVEL;
    g_config.usb_log_enabled      = DEFAULT_USB_LOG_ENABLED;
    g_config.mesh_log_enabled     = DEFAULT_MESH_LOG_ENABLED;
    g_config.gatt_log_enabled     = DEFAULT_GATT_LOG_ENABLED;
    g_config.system_log_enabled   = DEFAULT_SYSTEM_LOG_ENABLED;
#endif
    g_config.mesh_default_ttl     = DEFAULT_MESH_TTL;
    g_config.deepsleep_interval_sec = DEFAULT_DEEPSLEEP_INTERVAL_SEC;
    g_config.idle_sleep_timeout_sec = DEFAULT_IDLE_SLEEP_TIMEOUT_SEC;
    g_config.quick_msg_seq       = 0;        /* V6.39: 快捷消息序列号从 1 开始（首次发送时 ++） */

    /* 默认快捷消息 */
    const char *defaults[] = {
        DEFAULT_QUICK_MSG_0, DEFAULT_QUICK_MSG_1, DEFAULT_QUICK_MSG_2,
        DEFAULT_QUICK_MSG_3, DEFAULT_QUICK_MSG_4, DEFAULT_QUICK_MSG_5,
        DEFAULT_QUICK_MSG_6, DEFAULT_QUICK_MSG_7, DEFAULT_QUICK_MSG_8,
        DEFAULT_QUICK_MSG_9,
    };
    for (int i = 0; i < QUICK_MESSAGE_COUNT; i++) {
        strncpy(g_config.quick_msgs[i].text, defaults[i], QUICK_MESSAGE_MAX_LEN - 1);
        g_config.quick_msgs[i].text[QUICK_MESSAGE_MAX_LEN - 1] = '\0';
        g_config.quick_msgs[i].in_use = true;
    }

    g_config.contact_count = 0;
    g_config.my_device_name[0] = '\0';
    g_config._dirty = false;
}

/* ── 初始化 ──────────────────────────────────────────────── */

void nvs_config_init(void)
{
    ESP_LOGI(TAG, "Initializing NVS config");

    /* 0. 初始化 NVS 闪存子系统（必须最先调用）
     * 处理首次使用或 NVS 版本变更导致的初始化错误 */
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS unavailable (%d); preserving flash, using RAM defaults", err);
        s_config_write_allowed = false;
        config_set_defaults();
        return;
    }
    ESP_LOGI(TAG, "NVS flash subsystem initialized");

    /* 创建保存消抖定时器（单次，5 秒） */
    g_save_timer = xTimerCreate(
        "nvs_save",
        pdMS_TO_TICKS(NVS_COMMIT_DELAY_MS),
        pdFALSE,    /* 单次触发 */
        NULL,
        save_timer_cb
    );
    if (g_save_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create save timer!");
    }

    /* 加载配置前运行 NVS 恢复检查 */
    nvs_config_recovery_check();

    /* 从 NVS 加载或设为默认值 */
    nvs_config_load();

    ESP_LOGI(TAG, "Config ready (%u quick msgs, %u contacts)",
             QUICK_MESSAGE_COUNT, g_config.contact_count);
}

/* ── NVS 恢复检查 ────────────────────────────────────────── */

void nvs_config_recovery_check(void)
{
    /* 通过尝试打开命名空间来验证 NVS 分区健康状态。
     * 如果主 NVS 分区损坏，ESP-IDF NVS 将自动
     * 回退到备份扇区。
     *
     * 此处进行只读探测，在尝试加载配置数据前
     * 触发任何必要的恢复操作。 */
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        /* 命名空间存在且可读 — 检查基本完整性 */
        size_t blob_size = 0;
        err = nvs_get_blob(handle, NVS_KEY_BLOB, NULL, &blob_size);
        if (err == ESP_OK && blob_size == sizeof(nvs_config_t)) {
            ESP_LOGI(TAG, "NVS health: OK (blob %u bytes)", (unsigned)blob_size);
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "NVS health: no existing config (new device)");
        } else {
            ESP_LOGW(TAG, "NVS health: blob error (%d), using defaults", err);
        }
        nvs_close(handle);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "NVS health: namespace not found (new device)");
    } else if (err == ESP_ERR_NVS_INVALID_STATE || err == ESP_ERR_NVS_CORRUPT_KEY_PART) {
        ESP_LOGE(TAG, "NVS unreadable; preserve data for recovery");
        s_config_write_allowed = false;
    } else {
        ESP_LOGW(TAG, "NVS health: unknown error %d", err);
    }
}

/* ── 加载配置 ────────────────────────────────────────────── */

void nvs_config_load(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "NVS namespace not found — using defaults");
        config_set_defaults();
        return;
    }
    if (err != ESP_OK) {
        s_config_write_allowed = false;
        ESP_LOGW(TAG, "NVS open failed (%d) — using defaults", err);
        config_set_defaults();
        return;
    }

    /* 检查版本 */
    uint8_t version = 0;
    err = nvs_get_u8(handle, NVS_KEY_VERSION, &version);
    if (err != ESP_OK || version != NVS_VERSION) {
        s_config_write_allowed = false; /* Preserve unknown schema. */
        ESP_LOGW(TAG, "Config version mismatch (%d vs %d) — using defaults",
                 version, NVS_VERSION);
        nvs_close(handle);
        config_set_defaults();
        return;
    }

    /* 读取配置块 */
    size_t blob_size = sizeof(nvs_config_t);
    err = nvs_get_blob(handle, NVS_KEY_BLOB, &g_config, &blob_size);
    if (err == ESP_OK && blob_size == sizeof(nvs_config_t)) {
        ESP_LOGI(TAG, "Config loaded from NVS (%u bytes)", (unsigned)blob_size);
        g_config._dirty = false;

        /* v7 migration: ignore the obsolete compilation-time fingerprint. */
    } else {
        s_config_write_allowed = false;
        ESP_LOGW(TAG, "Config blob read failed (%d) — using defaults", err);
        config_set_defaults();
    }

    nvs_close(handle);
}

/* ── 保存配置 ────────────────────────────────────────────── */

void nvs_config_save(void)
{
    portENTER_CRITICAL(&s_save_lock);
    if (!s_config_write_allowed || !g_config._dirty) {
        portEXIT_CRITICAL(&s_save_lock);
        return;
    }
    if (s_save_in_progress) {
        /* A caller preparing sleep/OTA must observe dirty and try again; do not
         * wait for a timer-task writer or let that older commit clear its guard. */
        ++s_config_generation;
        portEXIT_CRITICAL(&s_save_lock);
        schedule_config_save();
        return;
    }
    s_save_in_progress = true;
    uint32_t generation = s_config_generation;
    memcpy(&s_save_snapshot, &g_config, sizeof(s_save_snapshot));
    portEXIT_CRITICAL(&s_save_lock);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Open NVS for write failed: %d", err);
        goto complete;
    }

    /* 写入版本号 */
    err = nvs_set_u8(handle, NVS_KEY_VERSION, NVS_VERSION);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS write version failed: %d", err);
        nvs_close(handle);
        goto complete;
    }

    /* 写入配置块 */
    err = nvs_set_blob(handle, NVS_KEY_BLOB, &s_save_snapshot, sizeof(nvs_config_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS write blob failed: %d", err);
        nvs_close(handle);
        goto complete;
    }

    err = nvs_commit(handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Config saved to NVS (%u bytes)", (unsigned)sizeof(nvs_config_t));
    } else {
        ESP_LOGE(TAG, "NVS commit failed: %d", err);
    }

    nvs_close(handle);
complete:
    portENTER_CRITICAL(&s_save_lock);
    if (err == ESP_OK && generation == s_config_generation &&
        !memcmp(&g_config, &s_save_snapshot, offsetof(nvs_config_t, _dirty))) {
        g_config._dirty = false;
    }
    bool retry = g_config._dirty;
    s_save_in_progress = false;
    portEXIT_CRITICAL(&s_save_lock);
    if (retry) schedule_config_save();
}

/* ── Mark dirty + debounce timer ────────────────────────────── */

void nvs_config_mark_dirty(void)
{
    portENTER_CRITICAL(&s_save_lock);
    ++s_config_generation;
    g_config._dirty = true;
    portEXIT_CRITICAL(&s_save_lock);
    schedule_config_save();
}

static void save_timer_cb(TimerHandle_t timer)
{
    (void)timer;
    ESP_LOGI(TAG, "Debounce timer expired — saving config");
    nvs_config_save();
}

/* ── Quick messages ─────────────────────────────────────────── */

const char *nvs_config_get_quick_msg(uint8_t index)
{
    if (index >= QUICK_MESSAGE_COUNT) return NULL;
    if (!g_config.quick_msgs[index].in_use) return NULL;
    return g_config.quick_msgs[index].text;
}

bool nvs_config_set_quick_msg(uint8_t index, const char *text)
{
    if (index >= QUICK_MESSAGE_COUNT || text == NULL) return false;

    strncpy(g_config.quick_msgs[index].text, text, QUICK_MESSAGE_MAX_LEN - 1);
    g_config.quick_msgs[index].text[QUICK_MESSAGE_MAX_LEN - 1] = '\0';
    g_config.quick_msgs[index].in_use = true;
    nvs_config_mark_dirty();
    return true;
}

/* ── Contact aliases ────────────────────────────────────────── */

const char *nvs_config_get_alias(uint32_t node_id)
{
    for (uint8_t i = 0; i < g_config.contact_count; i++) {
        if (g_config.contacts[i].in_use &&
            g_config.contacts[i].node_id == node_id) {
            return g_config.contacts[i].alias;
        }
    }
    return NULL;
}

bool nvs_config_set_alias(uint32_t node_id, const char *alias)
{
    if (alias == NULL) return false;

    /* Update existing? */
    for (uint8_t i = 0; i < g_config.contact_count; i++) {
        if (g_config.contacts[i].in_use &&
            g_config.contacts[i].node_id == node_id) {
            strncpy(g_config.contacts[i].alias, alias, 7);
            g_config.contacts[i].alias[7] = '\0';
            nvs_config_mark_dirty();
            return true;
        }
    }

    /* Add new */
    if (g_config.contact_count >= NVS_CONTACT_MAX) {
        ESP_LOGW(TAG, "Contact alias table full (%u entries)", NVS_CONTACT_MAX);
        return false;
    }

    uint8_t idx = g_config.contact_count;
    g_config.contacts[idx].node_id = node_id;
    strncpy(g_config.contacts[idx].alias, alias, 7);
    g_config.contacts[idx].alias[7] = '\0';
    g_config.contacts[idx].device_name[0] = '\0';
    g_config.contacts[idx].sync_state = CONTACT_SYNC_DONE;
    g_config.contacts[idx].in_use = true;
    g_config.contact_count++;
    nvs_config_mark_dirty();
    return true;
}

/* ── Phase 6: Extended contact management ───────────────────── */

bool nvs_config_set_contact_full(uint32_t node_id, const char *device_name,
                                  const char *alias, uint8_t sync_state)
{
    /* Update existing? */
    for (uint8_t i = 0; i < g_config.contact_count; i++) {
        if (g_config.contacts[i].in_use &&
            g_config.contacts[i].node_id == node_id) {
            if (device_name && device_name[0]) {
                strncpy(g_config.contacts[i].device_name, device_name, 7);
                g_config.contacts[i].device_name[7] = '\0';
            }
            if (alias && alias[0]) {
                strncpy(g_config.contacts[i].alias, alias, 7);
                g_config.contacts[i].alias[7] = '\0';
            }
            g_config.contacts[i].sync_state = sync_state;
            nvs_config_mark_dirty();
            return true;
        }
    }

    /* Add new */
    if (g_config.contact_count >= NVS_CONTACT_MAX) {
        ESP_LOGW(TAG, "Contact table full (%u entries)", NVS_CONTACT_MAX);
        return false;
    }

    uint8_t idx = g_config.contact_count;
    g_config.contacts[idx].node_id = node_id;
    if (device_name && device_name[0]) {
        strncpy(g_config.contacts[idx].device_name, device_name, 7);
        g_config.contacts[idx].device_name[7] = '\0';
    } else {
        g_config.contacts[idx].device_name[0] = '\0';
    }
    if (alias && alias[0]) {
        strncpy(g_config.contacts[idx].alias, alias, 7);
        g_config.contacts[idx].alias[7] = '\0';
    } else {
        g_config.contacts[idx].alias[0] = '\0';
    }
    g_config.contacts[idx].sync_state = sync_state;
    g_config.contacts[idx].in_use = true;
    g_config.contacts[idx].pinned = false;   /* V6.32: 新联系人默认不置顶 */
    g_config.contact_count++;
    nvs_config_mark_dirty();
    return true;
}

bool nvs_config_delete_alias(uint32_t node_id)
{
    for (uint8_t i = 0; i < g_config.contact_count; i++) {
        if (g_config.contacts[i].in_use &&
            g_config.contacts[i].node_id == node_id) {
            /* Shift remaining entries down */
            for (uint8_t j = i; j < g_config.contact_count - 1; j++) {
                g_config.contacts[j] = g_config.contacts[j + 1];
            }
            /* Clear the last slot */
            memset(&g_config.contacts[g_config.contact_count - 1], 0,
                   sizeof(contact_alias_t));
            g_config.contact_count--;
            nvs_config_mark_dirty();
            ESP_LOGI(TAG, "Contact 0x%08lX deleted", (unsigned long)node_id);
            return true;
        }
    }
    return false;
}

const char *nvs_config_get_contact_display_name(uint32_t node_id)
{
    for (uint8_t i = 0; i < g_config.contact_count; i++) {
        if (g_config.contacts[i].in_use &&
            g_config.contacts[i].node_id == node_id) {
            /* Prefer alias, fall back to device_name */
            if (g_config.contacts[i].alias[0]) {
                return g_config.contacts[i].alias;
            }
            if (g_config.contacts[i].device_name[0]) {
                return g_config.contacts[i].device_name;
            }
            return NULL;
        }
    }
    return NULL;
}

const contact_alias_t *nvs_config_get_contact_by_index(uint8_t index)
{
    if (index >= g_config.contact_count) return NULL;
    if (!g_config.contacts[index].in_use) return NULL;
    return &g_config.contacts[index];
}

/* ── V6.32: 好友置顶管理 ──────────────────────────────────── */

bool nvs_config_toggle_pin_contact(uint32_t node_id)
{
    for (uint8_t i = 0; i < g_config.contact_count; i++) {
        if (g_config.contacts[i].in_use &&
            g_config.contacts[i].node_id == node_id) {
            g_config.contacts[i].pinned = !g_config.contacts[i].pinned;
            nvs_config_mark_dirty();
            ESP_LOGI(TAG, "Contact 0x%08lX pinned=%d",
                     (unsigned long)node_id,
                     (int)g_config.contacts[i].pinned);
            return true;
        }
    }
    return false;
}

bool nvs_config_is_contact_pinned(uint32_t node_id)
{
    for (uint8_t i = 0; i < g_config.contact_count; i++) {
        if (g_config.contacts[i].in_use &&
            g_config.contacts[i].node_id == node_id) {
            return g_config.contacts[i].pinned;
        }
    }
    return false;
}

/* ── Device name ────────────────────────────────────────────── */

void nvs_config_set_device_name(const char *name)
{
    if (name == NULL) {
        g_config.my_device_name[0] = '\0';
    } else {
        strncpy(g_config.my_device_name, name, 7);
        g_config.my_device_name[7] = '\0';
    }
    nvs_config_mark_dirty();
    ESP_LOGI(TAG, "Device name set to '%s'", g_config.my_device_name);
}

const char *nvs_config_get_device_name(void)
{
    if (g_config.my_device_name[0]) {
        return g_config.my_device_name;
    }
    return "";  /* Empty string if not set */
}

/* ── Sync helpers ───────────────────────────────────────────── */

uint8_t nvs_config_get_pending_sync_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < g_config.contact_count; i++) {
        if (g_config.contacts[i].in_use &&
            g_config.contacts[i].sync_state == CONTACT_SYNC_PENDING) {
            count++;
        }
    }
    return count;
}

void nvs_config_mark_contacts_synced(void)
{
    bool changed = false;
    for (uint8_t i = 0; i < g_config.contact_count; i++) {
        if (g_config.contacts[i].in_use &&
            g_config.contacts[i].sync_state == CONTACT_SYNC_PENDING) {
            g_config.contacts[i].sync_state = CONTACT_SYNC_DONE;
            changed = true;
        }
    }
    if (changed) {
        nvs_config_mark_dirty();
        ESP_LOGI(TAG, "All pending contacts marked synced");
    }
}

/* ══════════════════════════════════════════════════════════════
 *  莫尔斯教学进度持久化（阶段八）
 *
 *  使用独立的 NVS 键 "morse_prog"，与主配置 blob 分离存储，
 *  避免进度数据污染配置版本迁移逻辑。
 * ══════════════════════════════════════════════════════════════ */

#define NVS_KEY_MORSE_PROGRESS  "morse_prog"
#define NVS_KEY_MORSE_VER       "morse_ver"
#define MORSE_PROGRESS_VERSION  1

void nvs_config_load_morse_progress(morse_progress_t *progress)
{
    if (!progress) return;
    memset(progress, 0, sizeof(*progress));

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Morse progress: no NVS namespace (new device)");
        return;
    }

    /* Check version */
    uint8_t ver = 0;
    err = nvs_get_u8(handle, NVS_KEY_MORSE_VER, &ver);
    if (err != ESP_OK || ver != MORSE_PROGRESS_VERSION) {
        ESP_LOGI(TAG, "Morse progress: version mismatch or not found — starting fresh");
        nvs_close(handle);
        return;
    }

    /* Read progress blob */
    size_t blob_size = sizeof(morse_progress_t);
    err = nvs_get_blob(handle, NVS_KEY_MORSE_PROGRESS, progress, &blob_size);
    if (err == ESP_OK && blob_size == sizeof(morse_progress_t)) {
        ESP_LOGI(TAG, "Morse progress loaded: Tier %u, Lesson %u, XP=%u, Streak=%u",
                 progress->current_tier, progress->current_lesson,
                 progress->total_xp, progress->streak);
    } else {
        ESP_LOGW(TAG, "Morse progress blob error (%d) — starting fresh", err);
        memset(progress, 0, sizeof(*progress));
    }

    nvs_close(handle);
}

void nvs_config_save_morse_progress(const morse_progress_t *progress)
{
    if (!progress) return;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Morse progress: open NVS for write failed: %d", err);
        return;
    }

    /* Write version */
    uint8_t ver = MORSE_PROGRESS_VERSION;
    err = nvs_set_u8(handle, NVS_KEY_MORSE_VER, ver);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Morse progress: write version failed: %d", err);
        nvs_close(handle);
        return;
    }

    /* Write blob */
    err = nvs_set_blob(handle, NVS_KEY_MORSE_PROGRESS, progress, sizeof(morse_progress_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Morse progress: write blob failed: %d", err);
        nvs_close(handle);
        return;
    }

    err = nvs_commit(handle);
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Morse progress saved (Tier %u, Lesson %u, XP=%u)",
                 progress->current_tier, progress->current_lesson, progress->total_xp);
    } else {
        ESP_LOGE(TAG, "Morse progress: commit failed: %d", err);
    }

    nvs_close(handle);
}

/* ══════════════════════════════════════════════════════════════
 *  恢复出厂设置 (V6.19)
 * ══════════════════════════════════════════════════════════════ */

void nvs_config_factory_reset(void)
{
    ESP_LOGW(TAG, "=== Factory reset: erasing all NVS data ===");

    /* 1. 擦除 NVS 闪存分区（所有用户数据: 配置/联系人/消息/进度/密钥） */
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS erase failed: %d", err);
        return;
    }
    ESP_LOGI(TAG, "NVS erased, restarting...");

    /* 2. 短暂延迟确保日志输出 */
    vTaskDelay(pdMS_TO_TICKS(200));

    /* 3. 软件复位 — 等效于重新上电 */
    esp_restart();
}

/* ���� V9.0: MAC-based friend check �������������������������������������������������������� */
bool nvs_config_is_friend(const uint8_t *ble_mac)
{
    return ble_mac && friend_mgr_is_friend(ble_mac);
}
