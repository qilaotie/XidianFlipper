/**
 * @file    OLED_config.h
 * @brief   WouoUI-128_32 全部外观参数 — V5.12 U8G2 直连架构
 *
 * 原项目：https://github.com/RQNG/WouoUI/tree/main/WouoUI-128_32
 *
 * 本文件集中管理所有可调的外观/布局/动画参数。
 * 所有宏均带 #ifndef 守卫，允许编译期（-D 或 sdkconfig）覆盖。
 *
 * 硬件平台：ESP32-C6 + SSD1306 128×32 I2C
 * 图形库：U8g2 (olikraus/u8g2 + mkfrey/u8g2-hal-esp-idf)
 * I2C 频率：由 u8g2-hal-esp-idf 的 I2C_MASTER_FREQ_HZ 控制（600kHz，SSD1306 快速模式）
 */

#ifndef OLED_CONFIG_H
#define OLED_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "u8g2.h"

/* ═══════════════════════════════════════════════════════════
 *  显示硬件参数
 * ═══════════════════════════════════════════════════════════ */

#ifndef DISP_W
#define DISP_W                  128     /**< 屏幕宽度（像素） */
#endif

#ifndef DISP_H
#define DISP_H                  32      /**< 屏幕高度（像素） */
#endif

#ifndef OLED_I2C_ADDR
#define OLED_I2C_ADDR           0x3C    /**< SSD1306 I2C 地址（7 位） */
#endif

#ifndef OLED_I2C_FREQ_HZ
#define OLED_I2C_FREQ_HZ        700000  /**< I2C 时钟频率（Hz）— SSD1306 标准快速模式 400kHz */
#endif

/** U8g2 构造函数 — SSD1306 128×32 全帧缓冲 + I2C 回调 */
#define U8G2_SETUP_FN           u8g2_Setup_ssd1306_128x32_univision_f

/* ═══════════════════════════════════════════════════════════
 *  列表布局（WouoUI-128_32 原版参数，原封不动）
 * ═══════════════════════════════════════════════════════════ */

#ifndef LIST_FONT
#define LIST_FONT               u8g2_font_HelvetiPixel_tr   /**< 列表字体 */
#endif

#ifndef LIST_LINE_H
#define LIST_LINE_H             16      /**< 列表单行高度（像素） */
#endif

#ifndef LIST_TEXT_H
#define LIST_TEXT_H             8       /**< 列表字体高度（像素，来自 U8g2 字体度量） */
#endif

#ifndef LIST_TEXT_S
#define LIST_TEXT_S             4       /**< 列表文字上/左/右边距（像素） */
#endif

#ifndef LIST_BAR_W
#define LIST_BAR_W              5       /**< 列表滚动条宽度（需为奇数） */
#endif

#ifndef LIST_BOX_R
#define LIST_BOX_R              0.5f    /**< 列表选中框圆角半径 */
#endif

/* ── 备选：超窄行高测试参数（默认关闭）────────────────── */
#ifdef LIST_ULTRA_NARROW
#undef  LIST_FONT
#define LIST_FONT               u8g2_font_4x6_tr
#undef  LIST_LINE_H
#define LIST_LINE_H             7
#undef  LIST_TEXT_H
#define LIST_TEXT_H             5
#undef  LIST_TEXT_S
#define LIST_TEXT_S             1
#undef  LIST_BAR_W
#define LIST_BAR_W              7
#endif

/* ═══════════════════════════════════════════════════════════
 *  磁贴布局
 * ═══════════════════════════════════════════════════════════ */

#ifndef TILE_ICON_W
#define TILE_ICON_W             28      /**< 磁贴图标宽度 */
#endif

#ifndef TILE_ICON_H
#define TILE_ICON_H             28      /**< 磁贴图标高度 */
#endif

#ifndef TILE_ICON_S
#define TILE_ICON_S             32      /**< 磁贴图标间距 */
#endif

#ifndef TILE_INDI_H
#define TILE_INDI_H             2       /**< 磁贴指示器高度 */
#endif

#ifndef TILE_INDI_W
#define TILE_INDI_W             14      /**< 磁贴指示器宽度 */
#endif

/* ═══════════════════════════════════════════════════════════
 *  复选框/单选框布局
 * ═══════════════════════════════════════════════════════════ */

#ifndef CHECK_BOX_L_S
#define CHECK_BOX_L_S           95      /**< 复选框左边距 */
#endif

#ifndef CHECK_BOX_U_S
#define CHECK_BOX_U_S           2       /**< 复选框上边距 */
#endif

#ifndef CHECK_BOX_F_W
#define CHECK_BOX_F_W           12      /**< 复选框外框宽度 */
#endif

#ifndef CHECK_BOX_F_H
#define CHECK_BOX_F_H           12      /**< 复选框外框高度 */
#endif

#ifndef CHECK_BOX_D_S
#define CHECK_BOX_D_S           2       /**< 复选框内点距离外框的边距 */
#endif

/* ═══════════════════════════════════════════════════════════
 *  弹窗布局
 * ═══════════════════════════════════════════════════════════ */

#ifndef WIN_FONT
#define WIN_FONT                u8g2_font_HelvetiPixel_tr   /**< 弹窗字体 */
#endif

#ifndef WIN_W
#define WIN_W                   102     /**< 弹窗宽度 */
#endif

#ifndef WIN_H
#define WIN_H                   32      /**< 弹窗高度 */
#endif

#ifndef WIN_BAR_W
#define WIN_BAR_W               92      /**< 弹窗进度条宽度 */
#endif

#ifndef WIN_BAR_H
#define WIN_BAR_H               7       /**< 弹窗进度条高度 */
#endif

#ifndef WIN_Y_OFF
#define WIN_Y_OFF               (-WIN_H - 2)  /**< 弹窗离场/入场起始 Y 偏移 */
#endif

/* 默认动画速度（由 ui_core.c 的 ui_param_init() 使用） */
#ifndef OLED_DEFAULT_TILE_ANI
#define OLED_DEFAULT_TILE_ANI   30
#endif
#ifndef OLED_DEFAULT_LIST_ANI
#define OLED_DEFAULT_LIST_ANI   60
#endif
#ifndef OLED_DEFAULT_WIN_ANI
#define OLED_DEFAULT_WIN_ANI    25
#endif
#ifndef OLED_DEFAULT_SPOT_ANI
#define OLED_DEFAULT_SPOT_ANI   50
#endif
#ifndef OLED_DEFAULT_TAG_ANI
#define OLED_DEFAULT_TAG_ANI    60
#endif
#ifndef OLED_DEFAULT_FADE_ANI
#define OLED_DEFAULT_FADE_ANI   30
#endif
#ifndef OLED_DEFAULT_BTN_SPT
#define OLED_DEFAULT_BTN_SPT    25
#endif
#ifndef OLED_DEFAULT_BTN_LPT
#define OLED_DEFAULT_BTN_LPT    150
#endif

/* ═══════════════════════════════════════════════════════════
 *  系统常量
 * ═══════════════════════════════════════════════════════════ */

#ifndef UI_DEPTH_MAX
#define UI_DEPTH_MAX            20      /**< 最大菜单嵌套深度 */
#endif

#ifndef UI_PARAM_COUNT
#define UI_PARAM_COUNT          16      /**< UI 参数数量（对应 ui_param 枚举） */
#endif

/* ── UI 参数索）（对应 ui.param[] 数组）────────────────── */
enum {
    UI_PARAM_DISP_BRI = 0,     /**< 屏幕亮度 */
    UI_PARAM_TILE_ANI,         /**< 磁贴动画速度 */
    UI_PARAM_LIST_ANI,         /**< 列表动画速度 */
    UI_PARAM_WIN_ANI,          /**< 弹窗动画速度 */
    UI_PARAM_SPOT_ANI,         /**< 聚光动画速度 */
    UI_PARAM_TAG_ANI,          /**< 标签动画速度 */
    UI_PARAM_FADE_ANI,         /**< 淡入淡出速度 */
    UI_PARAM_BTN_SPT,          /**< 按键短按时长 */
    UI_PARAM_BTN_LPT,          /**< 按键长按时长 */
    UI_PARAM_TILE_UFD,         /**< 磁贴从头展开 */
    UI_PARAM_LIST_UFD,         /**< 列表从头展开 */
    UI_PARAM_TILE_LOOP,        /**< 磁贴循环模式 */
    UI_PARAM_LIST_LOOP,        /**< 列表循环模式 */
    UI_PARAM_WIN_BOK,          /**< 弹窗背景虚化 */
    UI_PARAM_KNOB_DIR,         /**< 旋钮方向 */
    UI_PARAM_DARK_MODE,        /**< 黑暗模式 */
};

#ifdef __cplusplus
}
#endif

/* ═══════════════════════════════════════════════════════════
 *  UI_ 前缀别名 — ui_core.c 使用这些名称以区分「UI 层宏」
 *  与底层驱动宏。所有别名直接映射到上述原始宏。
 * ═══════════════════════════════════════════════════════════ */

/* 显示尺寸 */
#define UI_DISP_W               DISP_W
#define UI_DISP_H               DISP_H

/* 列表布局 */
#define UI_LIST_LINE_H          LIST_LINE_H
#define UI_LIST_TEXT_H          LIST_TEXT_H
#define UI_LIST_TEXT_S          LIST_TEXT_S
#define UI_LIST_BAR_W           LIST_BAR_W
#define UI_LIST_BOX_R           LIST_BOX_R

/* 磁贴布局 */
#define UI_TILE_ICON_W          TILE_ICON_W
#define UI_TILE_ICON_H          TILE_ICON_H
#define UI_TILE_ICON_S          TILE_ICON_S
#define UI_TILE_INDI_H          TILE_INDI_H
#define UI_TILE_INDI_W          TILE_INDI_W

/* 复选框 */
#define UI_CHECK_BOX_L_S        CHECK_BOX_L_S
#define UI_CHECK_BOX_U_S        CHECK_BOX_U_S
#define UI_CHECK_BOX_F_W        CHECK_BOX_F_W
#define UI_CHECK_BOX_F_H        CHECK_BOX_F_H
#define UI_CHECK_BOX_D_S        CHECK_BOX_D_S

/* 弹窗 */
#define UI_WIN_W                WIN_W
#define UI_WIN_H                WIN_H
#define UI_WIN_BAR_W            WIN_BAR_W
#define UI_WIN_BAR_H            WIN_BAR_H
#define UI_WIN_R                2       /**< 弹窗圆角半径 */

/* 动画速度（硬编码默认值，后续可改为从 NVS 读取） */
#define UI_ANIM_LIST            OLED_DEFAULT_LIST_ANI     /**< 列表动画 60 */
#define UI_ANIM_WIN             OLED_DEFAULT_WIN_ANI      /**< 弹窗动画 25 */
#define UI_ANIM_FADE            OLED_DEFAULT_FADE_ANI     /**< 淡入淡出 30 */
#define UI_ANIM_TILE            OLED_DEFAULT_TILE_ANI     /**< 磁贴动画 30 */
#define UI_ANIM_SPOT            OLED_DEFAULT_SPOT_ANI     /**< 聚光动画 50 */
#define UI_ANIM_TAG             OLED_DEFAULT_TAG_ANI      /**< 标签动画 60 */

#endif /* OLED_CONFIG_H */
