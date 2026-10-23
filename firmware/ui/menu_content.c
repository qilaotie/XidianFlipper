/**
 * @file menu_content.c
 * @brief WouoUI-128_32 菜单文字内容 & 主菜单图标位图 — 实际定义
 *
 * About 页面已适配 BLUE_LONG_MESH (ESP32-C6) 设备信息。
 */

#include "menu_content.h"
#include "quick_msg.h"
#include "nvs_config.h"
/* V9.0: morse_tutor 已移除，摩斯学习课程菜单相关函数保留空桩 */
#include "ble_radio.h"
#include "config_mode.h"
#include <stdio.h>
#include <string.h>

/*===========================================================================
 * 系统菜单文字数组 — V6.1
 *===========================================================================*/

M_SELECT home_menu[] = {
    {"This is home"},
};

/* 快捷消息文字缓冲区 (V6.8: 从 NVS 动态加载) */
static char qm_text[QUICK_MESSAGE_COUNT][QUICK_MESSAGE_MAX_LEN];

M_SELECT quick_msg_menu[] = {
    {qm_text[0]},
    {qm_text[1]},
    {qm_text[2]},
    {qm_text[3]},
    {qm_text[4]},
    {qm_text[5]},
    {qm_text[6]},
    {qm_text[7]},
    {qm_text[8]},
    {qm_text[9]},
};

/**
 * @brief 从 NVS 刷新快捷消息菜单文字 (V6.8)
 *
 * 应在 nvs_config_init() 之后调用。
 * 若 NVS 消息为空，使用 defaults.h 默认值。
 */
void quick_msg_menu_refresh(void)
{
    for (int i = 0; i < QUICK_MESSAGE_COUNT; i++) {
        const char *msg = quick_msg_get(i);
        if (msg && msg[0] != '\0') {
            strncpy(qm_text[i], msg, QUICK_MESSAGE_MAX_LEN - 1);
        } else {
            /* 空槽位用占位填充 */
            snprintf(qm_text[i], QUICK_MESSAGE_MAX_LEN, "[Msg %d]", i + 1);
        }
    }
}

/* ══════════════════════════════════════════════════════════════
 *  V6.13: 快捷消息发送改用弹窗列表选择，移除 QM_SEND 菜单
 *  收件人列表在 quick_msg_proc() 中动态构建后传入 window_list_init()
 * ══════════════════════════════════════════════════════════════ */

/* ══════════════════════════════════════════════════════════════
 *  V10.5: CQ Broadcast 子菜单已移除 — 磁贴1 直达 M_MORSE_LEARN
 *  保留空数组以维持编译兼容（morse_submenu_proc 引用）
 * ══════════════════════════════════════════════════════════════ */

M_SELECT morse_submenu[] = {
    {"- Practice"},
};

/* ══════════════════════════════════════════════════════════════
 *  V6.13: 摩斯学习统一页面 — 菜单数组
 *  合并 Tier 列表 + 课程列表 + 自由练习 为同页状态机
 * ══════════════════════════════════════════════════════════════ */

M_SELECT morse_learn_menu[] = {
    {"- Tier 0: First Steps"},
    {"- Tier 1: Building"},
    {"- Tier 2: Full Alphabet"},
    {"- Tier 3: Numbers"},
    {"- Tier 4: Pro Signals"},
    {"- [ Free Practice ]"},
};

/* ── 课程列表动态缓冲区 ── */
#define MORSE_LEARN_LESSON_MAX  16
static char morse_learn_lesson_text[MORSE_LEARN_LESSON_MAX][24];
uint8_t morse_learn_lesson_count = 0;
uint8_t g_morse_learn_tier = 0;

M_SELECT morse_learn_lesson_menu[MORSE_LEARN_LESSON_MAX] = {
    {morse_learn_lesson_text[0]},
};

void morse_learn_lesson_refresh(uint8_t tier)
{
    /* V9.0: morse_tutor 已移除，此函数为空桩 */
}

/* ══════════════════════════════════════════════════════════════
 *  V9.1: 好友系统暂缓 — 好友动态菜单已移除
 *  保留 friends_menu_count 和 friends_dynamic_menu 空桩防编译错误
 * ══════════════════════════════════════════════════════════════ */

uint8_t friends_menu_count = 0;
uint8_t friends_contact_map[1] = {0};
M_SELECT friends_dynamic_menu[1] = {
    {""},
};

void friends_menu_refresh(void)
{
    /* V9.1: 好友系统暂缓，空桩 */
}

/* ══════════════════════════════════════════════════════════════
 *  设置页 — 9 项统一列表（无自动关屏设置）
 *  ~ = 数值弹窗, + = 二值开关, - = 纯文本/链接
 * ══════════════════════════════════════════════════════════════ */

M_SELECT settings_menu[] = {
    {"~ Disp Bri"},
    {"~ Btn SPT"},
    {"~ Btn LPT"},
    {"+ LED Enable"},
    {"+ Buzzer Enable"},
    {"~ Morse WPM"},
    {"+ Text Scroll"},
    {"- Factory Reset"},
    {"- Screen / power"},
    {"+ Chinese UI"},
    {"- Skin: PIXEL"},
    {"- Home: MESSAGES"},
};

/* ══════════════════════════════════════════════════════════════
 *  V6.13: Mesh 状态页 — list_show 8行
 * ══════════════════════════════════════════════════════════════ */

static char mesh_status_text[8][32];

M_SELECT mesh_status_menu[8] = {
    {mesh_status_text[0]}, {mesh_status_text[1]},
    {mesh_status_text[2]}, {mesh_status_text[3]},
    {mesh_status_text[4]}, {mesh_status_text[5]},
    {mesh_status_text[6]}, {mesh_status_text[7]},
};

/**
 * @brief 刷新 Mesh 状态文字 (每帧由 mesh_status_proc 调用)
 */
void mesh_status_menu_refresh(void)
{
    ble_radio_state_t bst = ble_radio_get_state();
    const char *ble_str = "OFF";
    switch (bst) {
        case BLE_RADIO_IDLE:        ble_str = "Idle"; break;
        case BLE_RADIO_ADVERTISING: ble_str = "Adv"; break;
        case BLE_RADIO_SCANNING:    ble_str = "Scan"; break;
        case BLE_RADIO_EMERGENCY:   ble_str = "EMG"; break;
        default: break;
    }
    snprintf(mesh_status_text[0], 32, "BLE: %s", ble_str);
    snprintf(mesh_status_text[1], 32, "Mode: %s", config_mode_get_name());

    /* V9.0: No mesh sync, always show N/A */
    snprintf(mesh_status_text[2], 32, "Sync: N/A");
    snprintf(mesh_status_text[3], 32, "Duty: %us", DEFAULT_DEEPSLEEP_SEC);

    /* V9.0: No mesh stats, show 0 */
    snprintf(mesh_status_text[4], 32, "RX: 0");
    snprintf(mesh_status_text[5], 32, "TX: 0");
    snprintf(mesh_status_text[6], 32, "Relay: N/A");
    snprintf(mesh_status_text[7], 32, "FW: V9.0");
}

/* ══════════════════════════════════════════════════════════════
 *  关于本机 (保留)
 * ══════════════════════════════════════════════════════════════ */

M_SELECT about_new_menu[] = {
    {"- Last modification time of firmware"},
    {"- 2026/6/18 10:00 "},
    {"- UI: THANK WouoUI"},
    {"- The software and hardware are all written by the students here"},
    {"- Based on ESP32-C6, using BLE Mesh and NimBLE"},
    {"- BLE Mesh v7.4"},
    {"- (c) 2026"},
};
