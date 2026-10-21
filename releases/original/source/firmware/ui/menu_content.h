/**
 * @file menu_content.h
 * @brief WouoUI-128_32 菜单文字内容 & 主菜单图标位图 — extern 声明
 *
 * 移植自 https://github.com/RQNG/WouoUI/tree/main/WouoUI-128_32
 * 已更新 About 页面为 BLUE_LONG_MESH (ESP32-C6) 信息
 * 实际定义在 menu_content.c 中。
 */

#ifndef MENU_CONTENT_H
#define MENU_CONTENT_H

#include "wououi.h"
#include "assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 菜单文字数组 (extern 声明)
 *===========================================================================*/

/* ── 系统菜单 (V6.13) ── */
extern M_SELECT home_menu[];
extern M_SELECT quick_msg_menu[];
extern M_SELECT settings_menu[];       /* V6.13: 合并 12 项 */
extern M_SELECT about_new_menu[];
extern M_SELECT mesh_status_menu[];    /* V6.13: list_show 8行 */

/* ── V7.7: 统一摩斯子菜单 ── */
extern M_SELECT morse_submenu[];       /* CQ Broadcast / Practice 2项 */

/* ── 摩斯学习统一页 (V6.13) ── */
extern M_SELECT morse_learn_menu[];    /* 5 Tier + Free Practice */
extern M_SELECT morse_learn_lesson_menu[]; /* 动态课程列表 */
extern uint8_t morse_learn_lesson_count;
extern uint8_t g_morse_learn_tier;

/* ── 好友动态菜单 (V6.13) ── */
extern M_SELECT friends_dynamic_menu[];
extern uint8_t friends_menu_count;
extern uint8_t friends_contact_map[];  /* V6.32: 菜单索引 → NVS contact 索引映射 */

/* ── 动态菜单刷新 ── */
void quick_msg_menu_refresh(void);
void friends_menu_refresh(void);

/* ── 摩斯学习菜单刷新 (V6.13) ── */
void morse_learn_lesson_refresh(uint8_t tier);

/* ── Mesh 状态菜单刷新 (V6.13) ── */
void mesh_status_menu_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* MENU_CONTENT_H */
