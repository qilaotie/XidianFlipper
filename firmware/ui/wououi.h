/**
 * @file wououi.h
 * @brief WouoUI-128_32 移植 — 共享类型定义、枚举、结构体、宏
 *
 * 移植自 https://github.com/RQNG/WouoUI/tree/main/WouoUI-128_32
 * 平台: ESP32-C6 + ESP-IDF v5.5.4 + u8g2 C API
 */

#ifndef WOUOUI_H
#define WOUOUI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 显示参数
 *===========================================================================*/
#define DISP_H              32      // 屏幕高度 (像素)
#define DISP_W              128     // 屏幕宽度 (像素)

/*===========================================================================
 * UI 结构参数
 *===========================================================================*/
#define UI_DEPTH            20      // 最深嵌套层级
#define UI_MNUMB            100     // 最大菜单/页面数量
#define UI_PARAM            16      // 参数总数 (索引 0-15)

/*===========================================================================
 * 页面索引枚举
 *===========================================================================*/
enum PAGE_INDEX {
    M_WINDOW,       // 弹窗 (顶层遮罩)
    M_HOME,         // 主页 (最外层, KEY 返回到此)
    M_MAIN,         // 主菜单 (磁贴, V10.6: 6 磁贴)
    M_SETTING,      // 设置 (V6.13: 合并所有子设置, list_show)
    M_ABOUT,        // 关于本机 (列表)
    /* ── 二级功能页 (V6.13: 全部从 M_MAIN 直达) ── */
    M_QUICK_MSG,    // 快捷消息列表 (SP→弹窗选收件人)
    M_MORSE_INPUT,  // 摩斯手动输入 (独立磁贴)
    M_MORSE_LEARN,  // 摩斯学习中心 (合并: Tier+课程+自由练习)
    M_MORSE_TUTOR,  // 摩斯教学页面 (全屏交互模式)
    M_FRIENDS,      // [V9.1 废弃] 好友列表 — 好友系统暂缓，保留枚举值防编译错误
    M_MESH_STATUS,  // Mesh 网络状态 (list_show 8行)
    M_MSG_RX,       // 消息接收弹窗
    M_QUICK_MSG_SENDING,  // V6.39: 快捷消息发送状态页（已送达/等待中）
    M_DEV_MODE,     // 开发者模式 (条件编译, 独立磁贴)
    M_NAME_SETUP,   // 设备名设置 (旋钮选字母/数字, 首次开机引导)
    M_MORSE,        // V7.7: 统一摩斯入口（子菜单: CQ广播/Practice终端）
    /* ── 小游戏页 (V10.6: 主菜单磁贴 4/5) ── */
    M_ANSWER_BOOK,  // 答案之书 (SP 翻牌, 随机答案短句)
    M_DICE,         // 骰子 (SP 摇骰, 1-6 点面)
    M_CRAPS,        // 花旗骰 (Pass Line 简化玩法)
    M_MESSAGES_HUB,
    M_CHANNEL_HUB,
    M_LEARN_HUB,
    M_TRAINING,
    M_FUN_HUB,
    M_INBOX,
    M_MESSAGE_VIEW,
    M_CHANNEL_SELECT,
    M_FRIEND_PAIR,
    M_FRIEND_DETAIL,
    M_RECIPIENT_PICK,
    M_TX_STATUS,
    M_SLEEP_CONFIRM,
    /* ── [V6.13 废弃, 保留枚举值以防编译错误] ── */
    M_QUICK_MSG_SEND_DEPRECATED,
    M_MORSE_DEPRECATED,
    M_MORSE_LESSON_LIST_DEPRECATED,
    M_MORSE_DECODE_DEPRECATED,
    M_FRIEND_ADD_DEPRECATED,
    M_SETTING_LED_DEPRECATED,
    M_SETTING_MORSE_DEPRECATED,
    M_SETTING_POWER_DEPRECATED,
    M_RPS,
    M_TOOLS,
    M_BLE_SCAN,
    M_HID_SLIDES,
    M_HID_MEDIA,
    M_WIFI_SCAN,
    M_TOOL_MEMORY,
    M_MESSAGE_COMPOSE,
    M_TREASURE,
    M_REACTION,
    M_MESSAGE_EMOTES,
    M_NUMBER_BOMB,
    M_TIME_GUESS,
    M_HID_CAMERA,
    M_WIFI_CONNECT,
    M_WIFI_NETWORKS,
    M_WIFI_PASSWORD,
    M_WIFI_STATUS,
    M_WIFI_FORGET,
    M_TETRIS,
    M_SNAKE,
    M_PET,
    M_CLOUD_SYNC,
    M_GAME_HELP,
    M_DRAFT_BOX,
    M_DRAFT_SLOT,
    M_DRAFT_CONFIRM,
    M_MESSAGE_EMOTE_LIST,
};

/*===========================================================================
 * UI 状态枚举
 *===========================================================================*/
enum UI_STATE {
    S_FADE,         // 转场动画 (渐变)
    S_WINDOW,       // 弹窗初始化
    S_LAYER_IN,     // 进入更深层级
    S_LAYER_OUT,    // 返回更浅层级
    S_NONE,         // 直接渲染页面 (正常模式)
};

/*===========================================================================
 * UI 参数索引枚举 (对应 ui.param[])
 *===========================================================================*/
enum UI_PARAM_INDEX {
    DISP_BRI,       // 0: 屏幕亮度 (0-255)
    TILE_ANI,       // 1: 磁贴动画速度 (10-100)
    LIST_ANI,       // 2: 列表动画速度 (10-100)
    WIN_ANI,        // 3: 弹窗动画速度 (10-100)
    SPOT_ANI,       // 4: 聚光动画速度 (10-100)
    TAG_ANI,        // 5: 标签动画速度 (10-100)
    FADE_ANI,       // 6: 消失动画速度 (0-255)
    BTN_SPT,        // 7: 按键短按时长阈值 (0-255)
    BTN_LPT,        // 8: 按键长按时长阈值 (0-255)
    TILE_UFD,       // 9: 磁贴图标从头展开 (0/1)
    LIST_UFD,       // 10: 列表从头展开 (0/1)
    TILE_LOOP,      // 11: 磁贴循环模式 (0/1)
    LIST_LOOP,      // 12: 列表循环模式 (0/1)
    WIN_BOK,        // 13: 弹窗背景虚化 (0/1)
    KNOB_DIR,       // 14: 旋钮方向反转 (0/1)
    DARK_MODE,      // 15: 黑暗模式 (1=暗, 0=亮)
};

/*===========================================================================
 * 旋钮参数
 *===========================================================================*/
#define KNOB_PARAM          4
#define KNOB_DISABLE        0
#define KNOB_ROT_VOL        1
#define KNOB_ROT_BRI        2

enum KNOB_PARAM_INDEX {
    KNOB_ROT,       // 睡眠下旋转旋钮: 0=禁用, 1=音量, 2=亮度
    KNOB_COD,       // 睡眠下短按键码: 0=禁用, 否则为字符码
    KNOB_ROT_P,     // 旋转功能在单选列表中的位置
    KNOB_COD_P,     // 键码在单选列表中的位置
};

/*===========================================================================
 * 按键 ID
 *===========================================================================*/
#define BTN_ID_CC           0   // 逆时针旋转
#define BTN_ID_CW           1   // 顺时针旋转
#define BTN_ID_SP           2   // 短按（确认键）
#define BTN_ID_LP           3   // 长按
#define BTN_ID_BACK         4   // KEY (GPIO19) 取消/返回键

#define BTN_PARAM_TIMES     2   // 参数值到毫秒的倍数 (uint8_t 不够大)

/*===========================================================================
 * EEPROM/NVS 校验
 *===========================================================================*/
#define EEPROM_CHECK        11  // 校验字节数 (允许 1 位误码)

/*===========================================================================
 * 磁贴 (Tile) 参数
 *===========================================================================*/
#define TILE_ICON_H         28  // 磁贴图标高度
#define TILE_ICON_W         28  // 磁贴图标宽度
#define TILE_ICON_S         32  // 磁贴图标间距
#define TILE_INDI_H         2   // 指示器高度
#define TILE_INDI_W         14  // 指示器宽度

/*===========================================================================
 * 列表 (List) 参数
 *===========================================================================*/
#define LIST_FONT           u8g2_font_HelvetiPixel_tr
#define LIST_TEXT_H         8   // 文字高度
#define LIST_LINE_H         16  // 单行高度
#define LIST_TEXT_S         4   // 文字边距
#define LIST_BAR_W          5   // 滚动条宽度 (需奇数)
#define LIST_BOX_R          0.5f // 选择框圆角半径

/*===========================================================================
 * 电压测量页面参数 (保留 UI 框架)
 *===========================================================================*/
#define WAVE_SAMPLE         20
#define WAVE_W              32
#define WAVE_L              24
#define WAVE_U              0
#define WAVE_MAX            27
#define WAVE_MIN            4
#define WAVE_BOX_H          32
#define WAVE_BOX_W          32
#define WAVE_BOX_L_S        24
#define VOLT_FONT           u8g2_font_helvB18_tr
#define VOLT_TEXT_BG_L_S    59
#define VOLT_TEXT_BG_W      59
#define VOLT_TEXT_BG_H      32

/*===========================================================================
 * 选择框 (CheckBox) 参数
 *===========================================================================*/
#define CHECK_BOX_L_S       95
#define CHECK_BOX_U_S       2
#define CHECK_BOX_F_W       12
#define CHECK_BOX_F_H       12
#define CHECK_BOX_D_S       2

/*===========================================================================
 * 弹窗 (Window) 参数
 *===========================================================================*/
#define WIN_FONT            u8g2_font_HelvetiPixel_tr
#define WIN_H               32
#define WIN_W               102
#define WIN_BAR_W           92
#define WIN_BAR_H           7
#define WIN_Y               (-WIN_H - 2)
#define WIN_Y_TRG           (-WIN_H - 2)

/*===========================================================================
 * 结构体定义
 *===========================================================================*/

/** 菜单项 */
typedef struct MENU {
    char *m_select;             // 菜单文字
} M_SELECT;

/** UI 全局状态 */
typedef struct {
    bool      init;                     // 初始化标志
    uint8_t   num[UI_MNUMB];            // 各页面行数/图标数
    uint8_t   select[UI_DEPTH];         // 每层选中项
    uint8_t   layer;                    // 当前层级
    uint8_t   index;                    // 当前页面索引
    uint8_t   state;                    // 当前状态
    bool      sleep;                    // 睡眠模式标志
    uint8_t   fade;                     // 渐变阶段 (1-4)
    uint8_t   param[UI_PARAM];          // UI 参数
    uint8_t   parent_index[UI_DEPTH];   // 每层的父页面索引 (KEY 返回用)
} UI_STATE_T;

/** 磁贴页面 */
typedef struct {
    int16_t temp;
    bool    select_flag;
    float   icon_x;
    float   icon_x_trg;
    float   icon_y;
    float   icon_y_trg;
    float   indi_x;
    float   indi_x_trg;
    float   title_y;
    float   title_y_trg;
} TILE_STATE_T;

/** 列表页面 */
typedef struct {
    uint8_t line_n;                     // 屏幕可见行数 = DISP_H / LIST_LINE_H
    int16_t temp;
    bool    loop;
    float   y;
    float   y_trg;
    float   box_x;
    float   box_x_trg;
    float   box_y;
    float   box_y_trg[UI_DEPTH];
    float   bar_y;
    float   bar_y_trg;
} LIST_STATE_T;

/** 电压测量页面 */
typedef struct {
    int     val;                        // 当前 ADC 值 (占位: 0)
    float   text_bg_r;
    float   text_bg_r_trg;
} VOLT_STATE_T;

/** 选择框跟踪 */
typedef struct {
    uint8_t *v;     // 显示数值参数指针
    uint8_t *m;     // 多选参数数组指针
    uint8_t *s;     // 单选参数指针
    uint8_t *s_p;   // 单选位置指针
} CHECK_BOX_STATE_T;

/** 弹窗 */
typedef struct {
    uint8_t   *value;
    uint8_t   original_value;
    bool      value_confirmed;
    uint8_t   max;
    uint8_t   min;
    uint8_t   step;
    M_SELECT  *bg;
    uint8_t   index;
    char      title[20];
    uint8_t   select;
    uint8_t   l;
    uint8_t   u;
    float     bar;
    float     bar_trg;
    float     y;
    float     y_trg;
    /* ── V6.13: 列表选择弹窗模式 ── */
    bool      is_list;        /* true = 列表选择弹窗, false = 数值弹窗 */
    M_SELECT  *list_items;    /* 列表选项数组 */
    uint8_t   list_count;     /* 选项数量 */
    uint8_t   *list_sel;      /* 选中索引指针 (读写) */
    uint8_t   list_cur;       /* 弹窗内当前选中索引 */
    uint8_t   list_scroll;       /* V6.32: 弹窗内 list 滚动偏移（首个可见项索引） */
    /* ── V6.32: 弹窗内嵌 list 动画（与 list_show 的 box_x/box_y 动画一致）── */
    float     list_box_w;        /* 选择框宽度（动画当前值） */
    float     list_box_w_trg;    /* 选择框宽度目标值 */
    float     list_box_row;      /* 选择框行偏移（动画当前值，相对 win.y, px） */
    float     list_box_row_trg;  /* 选择框行偏移目标值 */
    /* ── V6.50: 消息弹窗模式 ── */
    bool      is_msg;         /* true = 消息展示弹窗 (非选择) */
    bool      is_sent;        /* V9.4: true = 发送确认弹窗 (不触发未读呼吸灯) */
    uint8_t   msg_ticks;      /* 已显示帧数 (30Hz) */
    uint8_t   msg_timeout;    /* 超时帧数 (90 = 3s) */
} WIN_STATE_T;

/** 聚光灯 (保留，未使用) */
typedef struct {
    float   l;      float   l_trg;
    float   r;      float   r_trg;
    float   u;      float   u_trg;
    float   d;      float   d_trg;
} SPOT_STATE_T;

/** 按键/旋钮 (仅 id + pressed, ec11 驱动写入) */
typedef struct {
    uint8_t   id;
    bool      pressed;
} BTN_STATE_T;

/** 旋钮功能参数 */
typedef struct {
    uint8_t   param[KNOB_PARAM];
} KNOB_STATE_T;

/** EEPROM/NVS 状态 */
typedef struct {
    bool      init;
    bool      change;
    int       address;
    uint8_t   check;
    uint8_t   check_param[EEPROM_CHECK];
} EEPROM_STATE_T;

/*===========================================================================
 * 全局变量声明 (extern)
 *===========================================================================*/
extern u8g2_t       u8g2;           // U8G2 显示句柄
extern UI_STATE_T   ui;             // UI 全局状态
extern TILE_STATE_T tile;           // 磁贴动画状态
extern LIST_STATE_T list;           // 列表动画状态
extern VOLT_STATE_T volt;           // 电压页面状态
extern CHECK_BOX_STATE_T check_box; // 选择框跟踪
extern WIN_STATE_T  win;            // 弹窗状态
extern SPOT_STATE_T spot;           // 聚光灯状态
extern BTN_STATE_T  btn;            // 按键状态
extern KNOB_STATE_T knob;           // 旋钮参数
extern EEPROM_STATE_T eeprom;       // NVS 状态

/* ── 消息弹窗全局状态 (V6.8) ── */
typedef struct {
    bool     active;
    char     text[65];
    uint32_t src_id;
    bool     is_emergency;
    uint32_t show_ticks;    /* 已显示帧数 */
    uint8_t  saved_index;   /* 弹窗前页面索引 */
} MSG_RX_STATE_T;

extern MSG_RX_STATE_T msg_rx;

extern uint8_t  *buf_ptr;
extern uint16_t  buf_len;

/*===========================================================================
 * 函数声明
 *===========================================================================*/

/* 初始化 */
void ui_param_init(void);
void ui_init(void);
void tile_param_init(void);
void oled_buffer_init(void);

/* 动画 */
void animation(float *a, float *a_trg, uint8_t n);
void fade(void);

/* 磁贴 */
void tile_show(const uint8_t icon_pic[][16 * 7]);
void tile_rotate_switch(void);

/* 列表 */
void list_show(M_SELECT arr[], uint8_t ui_index);
void list_rotate_switch(void);
void list_update_scroll_targets(void);

/* 弹窗 */
void window_value_init(char title[], uint8_t select, uint8_t *value,
                       uint8_t max, uint8_t min, uint8_t step,
                       M_SELECT *bg, uint8_t index);
void window_list_init(char title[], M_SELECT items[], uint8_t count,
                      uint8_t *selection, M_SELECT *bg, uint8_t index);
void window_msg_init(char title[], const char *text,
                     M_SELECT *bg, uint8_t index, bool is_sent);
void window_show(void);
void window_list_show(void);
void window_value_proc(void);
void window_list_proc(void);

/* 选择框 */
void check_box_v_init(uint8_t *param);
void check_box_m_init(uint8_t *param);
void check_box_s_init(uint8_t *param, uint8_t *param_p);
void check_box_m_select(uint8_t param);
void check_box_s_select(uint8_t val, uint8_t pos);

/* 页面处理 (在 menu_pages.c 中定义) */
void home_proc(void);
void menu_pages_on_enter(uint8_t page);
void main_proc(void);
/* ── 二级功能页 (V6.13) ── */
void quick_msg_proc(void);
void morse_input_proc(void);
void morse_terminal_proc(void);    /* V7.4: 莫斯终端（替换原摩斯学习中心） */
void morse_submenu_proc(void);     /* V7.7: 统一摩斯子菜单 */
void friends_proc(void);
void quick_msg_sending_proc(void);  /* V6.44: 进度条式等待界面 */
void settings_proc(void);
void about_new_proc(void);
void mesh_status_proc(void);

/* ── 小游戏页 (V10.6) ── */
void answer_book_proc(void);
void dice_proc(void);
void craps_proc(void);
void rps_proc(void);

/* ── V6.8 消息弹窗 ── */
void msg_rx_proc(void);
void msg_rx_trigger(const char *text, uint32_t src_id, bool is_emergency);

/* ── [V6.13 废弃桩] ── */
void quick_msg_send_proc(void);
void morse_proc(void);
void morse_learn_proc(void);
void morse_decode_proc(void);
void morse_lesson_list_proc(void);
void friend_add_proc(void);
void settings_led_proc(void);
void settings_morse_proc(void);
void settings_power_proc(void);
void dev_mode_page_proc(void);
void name_setup_proc(void);

/* ── V6.9 摩斯键模式标志 ── */
extern bool g_morse_key_active;

/* 层级管理 */
void ui_navigate_to(uint8_t target_page);
void layer_init_in(void);
void layer_init_out(void);
void ui_proc(void);

/* EC11 → btn 适配 */
void ec06_to_btn(void);

#ifdef __cplusplus
}
#endif

#endif /* WOUOUI_H */
