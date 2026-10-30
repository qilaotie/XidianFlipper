/**
 * @file wououi.c
 * @brief WouoUI-128_32 UI 核心引擎
 *
 * 包含:
 *   - 动画系统 (animation)
 *   - 渐变效果 (fade)
 *   - 磁贴 UI (tile_show)
 *   - 列表 UI (list_show) + 尾部内容绘制
 *   - 弹窗 UI (window_show)
 *   - 电压页面 (volt_show, 仅框架)
 *   - 状态机 (ui_proc)
 *   - 初始化 & 辅助函数
 *
 * 移植自: WouoUI-128_32.ino (Arduino U8g2lib -> u8g2 C API)
 */

#include "wououi.h"
#include "ui_zh.h"
#include "ui_style.h"
#include "local_pages.h"
#include "tile_marquee.h"
#include "esp_timer.h"
#include "tetris_page.h"
#include "snake_page.h"
#include "pet_page.h"
#include "message_service.h"
#include "menu_content.h"
#include "ec06.h"
#include "storage_nvs.h"
#include "oled_power.h"
#include "nvs_config.h"
#include "rgb_led.h"            /* V9.4: rgb_led_stop() 用于弹窗关闭时停止呼吸灯 */
#include "haptic_feedback.h"   /* V8.1: 所有 UI 交互声效反馈 */
#include "dev_mode.h"
#include "log_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>

static bool zh_scope(void){
    if(ui.index>=M_FUN_PET && ui.index<=M_PET_GIFTS)return true;
    return (ui.index>=M_PARTY_HUB && ui.index<=M_PARTY_LEAVE) || (ui.index>=M_TOOLS_REMOTE && ui.index<=M_SETTINGS_SYSTEM) || (ui.index>=M_NETWORK_STATUS && ui.index<=M_ENSEMBLE) || ui.index==M_DRAFT_SLOT || ui.index==M_DRAFT_CONFIRM || ui.index==M_MAIN || ui.index==M_SETTING || ui.index==M_TOOLS || ui.index==M_FUN_HUB || ui.index==M_GAME_HELP ||
        ui.index==M_SLEEP_CONFIRM || ui.index==M_WIFI_CONNECT || ui.index==M_WIFI_FORGET ||
        ui.index==M_LEARN_HUB || ui.index==M_MESSAGES_HUB || ui.index==M_CHANNEL_HUB || ui.index==M_FRIENDS ||
        ui.index==M_FRIEND_DETAIL || ui.index==M_RECIPIENT_PICK || ui.index==M_ABOUT ||
        (ui.index==M_WINDOW && win.index==M_SETTING) || (ui.index==M_WINDOW && win.index>=M_SETTINGS_DISPLAY && win.index<=M_SETTINGS_SYSTEM);
}
/* Restrict localization to system UI, never user messages or text editors. */
#define u8g2_DrawStr(d,x,y,s) (zh_scope()?ui_zh_draw(d,x,y,s):(void)u8g2_DrawStr(d,x,y,s))
#define u8g2_GetStrWidth(d,s) (zh_scope()?ui_zh_width(d,s):u8g2_GetStrWidth(d,s))
#include <string.h>
#include <math.h>

/*===========================================================================
 * 全局变量定义
 *===========================================================================*/
u8g2_t       u8g2;
UI_STATE_T   ui;
TILE_STATE_T tile;
LIST_STATE_T list;
VOLT_STATE_T volt;
CHECK_BOX_STATE_T check_box;
WIN_STATE_T  win;
SPOT_STATE_T spot;
BTN_STATE_T  btn;
KNOB_STATE_T knob;
EEPROM_STATE_T eeprom;
MSG_RX_STATE_T msg_rx;
bool g_morse_key_active = false;  /* V6.9: 摩斯键模式（KEY=莫斯键） */

uint8_t  *buf_ptr = NULL;
uint16_t  buf_len = 0;

/*===========================================================================
 * EC11 事件 -> btn 适配 (每帧调用一次)
 *
 * 按键映射：
 *   ENC_BTN 短按 → BTN_ID_SP（确认键）
 *   ENC_BTN 长按 → BTN_ID_LP
 *   KEY 按下   → BTN_ID_BACK（取消/返回键）
 *   KEY 释放   → 不产生 btn 事件（PRESS 已处理）
 *===========================================================================*/
void ec06_to_btn(void)
{
    ec06_event_t ev = ec06_pop_event();
    if (ev.type == EC06_EVENT_NONE) return;
    btn.pressed = true;
    switch (ev.type) {
        case EC06_EVENT_CW:          btn.id = BTN_ID_CW; break;
        case EC06_EVENT_CCW:         btn.id = BTN_ID_CC; break;
        case EC06_EVENT_SHORT_PRESS: btn.id = BTN_ID_SP; break;
        case EC06_EVENT_LONG_PRESS:  btn.id = BTN_ID_LP; break;
        case EC06_EVENT_DOUBLE_CLICK:btn.id = BTN_ID_SP; break;  /* 双击 = 确认 */
        case EC06_EVENT_KEY_DOWN:    btn.id = BTN_ID_BACK; break;
        case EC06_EVENT_KEY_UP:      /* KEY 释放不产生额外 btn 事件 */
        case EC06_EVENT_ENC_DOWN:    /* ENC_BTN 按下: 莫斯键模式专用, 菜单忽略 */
        case EC06_EVENT_ENC_UP:      /* ENC_BTN 释放: 莫斯键模式专用, 菜单忽略 */
        default: btn.pressed = false; break;
    }
}

/*===========================================================================
 * 缓冲初始化
 *===========================================================================*/
void oled_buffer_init(void)
{
    buf_ptr = u8g2_GetBufferPtr(&u8g2);
    buf_len = 8 * u8g2_GetBufferTileHeight(&u8g2) * u8g2_GetBufferTileWidth(&u8g2);
}

/*===========================================================================
 * 动画引擎
 *===========================================================================*/

/**
 * @brief 非线性平滑缓动动画
 *
 * 原版算法:
 *   if (fabs(*a - *a_trg) < 0.15f)  *a = *a_trg;
 *   else  *a += (*a_trg - *a) / (ui.param[n] / 10.0f);
 *
 * @param a     当前值指针
 * @param a_trg 目标值指针
 * @param n     UI 参数索引 (动画速度)
 */
void animation(float *a, float *a_trg, uint8_t n)
{
    if (*a != *a_trg) {
        if (fabsf(*a - *a_trg) < 0.15f) {
            *a = *a_trg;
        } else {
            *a += (*a_trg - *a) / (ui.param[n] / 10.0f);
        }
    }
}

/*===========================================================================
 * 渐变效果 (页面转场)
 *===========================================================================*/

/**
 * @brief 4 阶段渐变转场
 *
 * 通过直接操作屏幕缓冲实现条纹状淡入淡出。
 * 暗模式: AND 操作 (黑色渐变)
 * 亮模式: OR 操作 (白色渐变)
 *
 * Stage 1-2: 奇数字节
 * Stage 3-4: 偶数字节
 */
void fade(void)
{
    /* Transition stages follow the UI clock; never sleep in a renderer. */

    if (ui.param[DARK_MODE]) {
        switch (ui.fade) {
            case 1:
                for (uint16_t i = 0; i < buf_len; ++i)
                    if (i % 2 != 0) buf_ptr[i] = buf_ptr[i] & 0xAA;
                break;
            case 2:
                for (uint16_t i = 0; i < buf_len; ++i)
                    if (i % 2 != 0) buf_ptr[i] = buf_ptr[i] & 0x00;
                break;
            case 3:
                for (uint16_t i = 0; i < buf_len; ++i)
                    if (i % 2 == 0) buf_ptr[i] = buf_ptr[i] & 0x55;
                break;
            case 4:
                for (uint16_t i = 0; i < buf_len; ++i)
                    if (i % 2 == 0) buf_ptr[i] = buf_ptr[i] & 0x00;
                break;
            default:
                ui.state = S_NONE;
                ui.fade = 0;
                break;
        }
    } else {
        switch (ui.fade) {
            case 1:
                for (uint16_t i = 0; i < buf_len; ++i)
                    if (i % 2 != 0) buf_ptr[i] = buf_ptr[i] | 0xAA;
                break;
            case 2:
                for (uint16_t i = 0; i < buf_len; ++i)
                    if (i % 2 != 0) buf_ptr[i] = buf_ptr[i] | 0x00;
                break;
            case 3:
                for (uint16_t i = 0; i < buf_len; ++i)
                    if (i % 2 == 0) buf_ptr[i] = buf_ptr[i] | 0x55;
                break;
            case 4:
                for (uint16_t i = 0; i < buf_len; ++i)
                    if (i % 2 == 0) buf_ptr[i] = buf_ptr[i] | 0x00;
                break;
            default:
                ui.state = S_NONE;
                ui.fade = 0;
                break;
        }
    }

    ui.fade++;
}

/*===========================================================================
 * 选择框辅助函数
 *===========================================================================*/

void check_box_v_init(uint8_t *param)
{
    check_box.v = param;
}

void check_box_m_init(uint8_t *param)
{
    check_box.m = param;
}

void check_box_s_init(uint8_t *param, uint8_t *param_p)
{
    check_box.s = param;
    check_box.s_p = param_p;
}

void check_box_m_select(uint8_t param)
{
    check_box.m[param] = !check_box.m[param];
    eeprom.change = true;
}

void check_box_s_select(uint8_t val, uint8_t pos)
{
    *check_box.s = val;
    *check_box.s_p = pos;
    eeprom.change = true;
}

/*===========================================================================
 * 弹窗初始化
 *===========================================================================*/

static void _win_list_update_anim_targets(void);

void window_value_init(char title[], uint8_t select, uint8_t *value,
                       uint8_t max, uint8_t min, uint8_t step,
                       M_SELECT *bg, uint8_t index)
{
    strncpy(win.title, title, sizeof(win.title) - 1);
    win.title[sizeof(win.title) - 1] = '\0';
    win.select = select;
    win.value = value;
    win.original_value = *value;
    win.value_confirmed = false;
    win.max = max;
    win.min = min;
    win.step = step;
    win.bg = bg;
    win.index = index;
    win.is_list = false;
    ui.index = M_WINDOW;
    ui.state = S_WINDOW;
}

/**
 * @brief 列表选择弹窗初始化 (V6.13)
 *
 * 弹窗内 CW/CCW 切换选项，SP 确认写入 *selection 并关闭弹窗。
 * KEY 取消（不修改 selection）。
 *
 * @param title     弹窗标题
 * @param items     选项数组 (M_SELECT[])
 * @param count     选项数量
 * @param selection 选中索引指针 (读写: 初始值→最终选择)
 * @param bg        背景菜单 (用于绘制底层)
 * @param index     背景页索引
 */
void window_list_init(char title[], M_SELECT items[], uint8_t count,
                      uint8_t *selection, M_SELECT *bg, uint8_t index)
{
    LOG_UI_DIAG("ui", "WIN-LIST: title='%s' count=%d cur=%d", title, count, *selection);
    strncpy(win.title, title, sizeof(win.title) - 1);
    win.title[sizeof(win.title) - 1] = '\0';
    win.is_list = true;
    win.list_items = items;
    win.list_count = count;
    win.list_sel = selection;
    win.list_cur = *selection;
    if (win.list_cur >= count) win.list_cur = 0;
    win.bg = bg;
    win.index = index;
    win.select = 0;
    win.list_scroll = 0;
    win.list_box_w = 0;
    win.list_box_w_trg = 0;
    win.list_box_row = 0;
    win.list_box_row_trg = 0;
    /* V6.38: 初始化光标动画目标, 弹窗出现时立即显示选择框 */
    _win_list_update_anim_targets();
    ui.index = M_WINDOW;
    ui.state = S_WINDOW;
}

/* ══════════════════════════════════════════════════════════════
 *  V6.50: 消息展示弹窗 (复用 window_list 渲染，无选择框)
 * ══════════════════════════════════════════════════════════════ */

void window_msg_init(char title[], const char *text,
                     M_SELECT *bg, uint8_t index, bool is_sent)
{
    LOG_UI_DIAG("ui", "WIN-MSG: title='%s' text='%.16s' is_sent=%d",
                title, text ? text : "", is_sent ? 1 : 0);
    static M_SELECT msg_items[1];
    static char     msg_buf[65];
    static uint8_t  msg_sel = 0;

    if (text) {
        strncpy(msg_buf, text, sizeof(msg_buf) - 1);
        msg_buf[sizeof(msg_buf) - 1] = '\0';
    } else {
        msg_buf[0] = '\0';
    }
    msg_items[0].m_select = msg_buf;

    strncpy(win.title, title, sizeof(win.title) - 1);
    win.title[sizeof(win.title) - 1] = '\0';
    win.is_list  = true;
    win.is_msg   = true;
    win.is_sent  = is_sent;                                  /* V9.4 */
    win.msg_ticks   = 0;
    win.msg_timeout = is_sent ? 60 : 0;  /* V9.4: 发送确认 2s 自动消失；接收消息需按键关闭 */
    win.list_items  = msg_items;
    win.list_count  = 1;
    win.list_sel    = &msg_sel;
    win.list_cur    = 0;
    win.bg          = bg;
    win.index       = index;
    win.select      = 0;
    win.list_scroll = 0;
    win.list_box_w     = 0;
    win.list_box_w_trg = 0;
    win.list_box_row     = 0;
    win.list_box_row_trg = 0;

    ui.index = M_WINDOW;
    ui.state = S_WINDOW;
}

/*===========================================================================
 * 磁贴 (Tile) UI — 主菜单图标网格
 *===========================================================================*/

static void tile_text(int x,int baseline,const char *text,uint32_t elapsed,bool scrolling)
{
    int left=x+37,right=x+127;
    if(left<0)left=0;
    if(right>DISP_W)right=DISP_W;
    if(left>=right)return;
    unsigned width=(unsigned)u8g2_GetStrWidth(&u8g2,text);
    int offset=scrolling?(int)tile_marquee_offset(width,90,elapsed):0;
    u8g2_SetClipWindow(&u8g2,left,baseline-10,right,baseline+2);
    u8g2_DrawStr(&u8g2,x+37-offset,baseline,text);
    u8g2_SetMaxClipWindow(&u8g2);
}
static void simple_icon(int x,unsigned icon){
    /* Native line art: no image buffers or font changes to games. */
    u8g2_DrawFrame(&u8g2,x+3,3,27,22);
    if(icon==0){u8g2_DrawLine(&u8g2,x+4,4,x+16,14);u8g2_DrawLine(&u8g2,x+29,4,x+16,14);}
    else if(icon==1){u8g2_DrawVLine(&u8g2,x+16,7,14);u8g2_DrawHLine(&u8g2,x+10,11,13);}
    else if(icon==2){u8g2_DrawVLine(&u8g2,x+16,4,20);u8g2_DrawHLine(&u8g2,x+7,10,6);u8g2_DrawHLine(&u8g2,x+20,10,6);}
    else if(icon==3){u8g2_DrawBox(&u8g2,x+9,9,3,3);u8g2_DrawBox(&u8g2,x+21,9,3,3);u8g2_DrawHLine(&u8g2,x+10,19,13);}
    else if(icon==4){u8g2_DrawBox(&u8g2,x+8,7,3,3);u8g2_DrawBox(&u8g2,x+15,13,3,3);u8g2_DrawBox(&u8g2,x+22,19,3,3);}
    else if(icon==5){u8g2_DrawHLine(&u8g2,x+7,9,19);u8g2_DrawHLine(&u8g2,x+7,18,19);u8g2_DrawBox(&u8g2,x+11,7,3,5);u8g2_DrawBox(&u8g2,x+20,16,3,5);}
    else if(icon==6){u8g2_DrawBox(&u8g2,x+15,7,3,3);u8g2_DrawVLine(&u8g2,x+16,13,8);}
    else {u8g2_DrawLine(&u8g2,x+9,8,x+23,21);u8g2_DrawLine(&u8g2,x+22,7,x+9,21);}
}
void tile_show(const uint8_t icon_pic[][16 * 7])
{
    static const char *names[] = {"MESSAGES", "MORSE LIVE", "LEARN", "FRIENDS", "FUN", "TOOLS", "SETTINGS", "ABOUT"};
    static const char *details[] = {"Quick phrases", "Key & listen", "Practice + quiz", "Touch to add", "Pet, arcade & party games", "Remotes, Wi-Fi & scanners", "Make it yours", "BLUE LONG"};
    static const unsigned icons[] = {0,1,2,3,4,7,5,6};
    static int previous=-1;
    static uint32_t selected_at;
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    if(!ui.init || previous!=ui.select[ui.layer]){previous=ui.select[ui.layer];selected_at=now;}
    if (!ui.init) {
        ui.init = true;
        tile.icon_x_trg = -(float)((int)ui.select[ui.layer] * TILE_ICON_S);
        tile.icon_x = tile.icon_x_trg + TILE_ICON_S * 0.25f;
    }
    animation(&tile.icon_x, &tile.icon_x_trg, TILE_ANI);
    u8g2_SetDrawColor(&u8g2, 1);
    for (unsigned i = 0; i < ui.num[ui.index] && i < 8; ++i) {
        int x = (int)((tile.icon_x / TILE_ICON_S + i) * DISP_W);
        if (x <= -DISP_W || x >= DISP_W) continue;
        unsigned skin=ui_style_skin();
        if(skin==UI_SKIN_PIXEL)u8g2_DrawXBMP(&u8g2, x + 2, 0, TILE_ICON_W, TILE_ICON_H, icon_pic[icons[i]]);
        else if(skin==UI_SKIN_MINIMAL)simple_icon(x,icons[i]);
        else {
            static const char *const codes[]={"MSG","CW","LRN","FRD","FUN","TLS","SET","INF"};
            u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);u8g2_DrawFrame(&u8g2,x+2,1,30,26);
            u8g2_DrawStr(&u8g2,x+6,11,codes[i]);u8g2_DrawStr(&u8g2,x+6,23,">_");
        }
        u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
        bool scroll=i==ui.select[ui.layer] && g_config.long_text_scroll_enable;
        tile_text(x,11,names[i],now-selected_at,scroll);
        u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
        char detail[24];
        if (i == 0 && message_service_unread_count()) {
            snprintf(detail, sizeof(detail), "%u NEW MESSAGES", message_service_unread_count());
            tile_text(x,23,detail,now-selected_at,scroll);
        } else {
            const char *description=details[i];
            if(!g_config.long_text_scroll_enable){
                if(i==4)description="Pet & party games";
                if(i==5)description="Remote / Wi-Fi";
            }
            tile_text(x,23,description,now-selected_at,scroll);
        }
    }
    for (unsigned i = 0; i < 8; ++i) {
        if (i == ui.select[ui.layer]) u8g2_DrawBox(&u8g2, 22 + i * 11, 29, 7, 2);
        else u8g2_DrawHLine(&u8g2, 24 + i * 11, 30, 3);
    }
    if (!ui.param[DARK_MODE]) {
        u8g2_SetDrawColor(&u8g2, 2);
        u8g2_DrawBox(&u8g2, 0, 0, DISP_W, DISP_H);
    }
}

/*===========================================================================
 * 列表尾部内容绘制
 *===========================================================================*/

/** 显示数值 (开头 ~) */
void list_draw_value(int n)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", check_box.v[n - 1]);
    u8g2_DrawStr(&u8g2, CHECK_BOX_L_S, list.temp + LIST_TEXT_H + LIST_TEXT_S, buf);
}

/** 绘制选择框外框 — V6.15: 圆角半径 3 */
void list_draw_check_box_frame(void)
{
    u8g2_DrawRFrame(&u8g2, CHECK_BOX_L_S, list.temp + CHECK_BOX_U_S,
                    CHECK_BOX_F_W, CHECK_BOX_F_H, 3);
}

/** 绘制选择框内点 — V6.15: 圆形渐变 (6帧从半径1→4) */
void list_draw_check_box_dot(void)
{
    static int s_cb_frame = 0;   /* 动画帧计数 0-5 */
    static int s_cb_last_val = 0; /* 上次选中值 (0/1) */

    /* 检测从未选→选中过渡，启动动画 */
    if (s_cb_last_val == 0) {
        s_cb_frame = 0;
    }
    s_cb_last_val = 1;

    /* 6 帧动画: 半径从 1 渐大到 4 (像素) */
    int cx = CHECK_BOX_L_S + CHECK_BOX_F_W / 2;
    int cy = list.temp + CHECK_BOX_U_S + CHECK_BOX_F_H / 2;
    int r;
    if (s_cb_frame >= 5) {
        r = 4;
    } else {
        r = s_cb_frame + 1;
        if (r > 4) r = 4;
    }
    if (s_cb_frame < 5) s_cb_frame++;

    u8g2_DrawDisc(&u8g2, cx, cy, r, U8G2_DRAW_ALL);
}

/** 旋钮功能显示 (开头 #) */
void list_draw_krf(int n)
{
    switch (check_box.v[n - 1]) {
        case 0: u8g2_DrawStr(&u8g2, CHECK_BOX_L_S, list.temp + LIST_TEXT_H + LIST_TEXT_S, "OFF"); break;
        case 1: u8g2_DrawStr(&u8g2, CHECK_BOX_L_S, list.temp + LIST_TEXT_H + LIST_TEXT_S, "VOL"); break;
        case 2: u8g2_DrawStr(&u8g2, CHECK_BOX_L_S, list.temp + LIST_TEXT_H + LIST_TEXT_S, "BRI"); break;
    }
}

/** 按键键值显示 (开头 $) */
void list_draw_kpf(int n)
{
    if (check_box.v[n - 1] == 0) {
        u8g2_DrawStr(&u8g2, CHECK_BOX_L_S, list.temp + LIST_TEXT_H + LIST_TEXT_S, "OFF");
    } else if (check_box.v[n - 1] <= 90) {
        char str[2] = { (char)check_box.v[n - 1], '\0' };
        u8g2_DrawStr(&u8g2, CHECK_BOX_L_S, list.temp + LIST_TEXT_H + LIST_TEXT_S, str);
    } else {
        u8g2_DrawStr(&u8g2, CHECK_BOX_L_S, list.temp + LIST_TEXT_H + LIST_TEXT_S, "?");
    }
}

/* ── 长文本滚动状态 (V6.14) ── */
static int16_t  s_scroll_offset = 0;
static uint8_t  s_scroll_tick = 0;
static uint8_t  s_scroll_dir = 1;   /* 1=右滚, 0=左滚 */
static uint8_t  s_scroll_pause = 0;  /* 两端暂停帧计数 */

/** 根据列表每行首字符判断尾部绘制内容 */
void list_draw_text_and_check_box(M_SELECT arr[], int i)
{
    /* V6.14: 长文本滚动 — 仅对当前选中行生效 */
    if (g_config.long_text_scroll_enable && i == (int)ui.select[ui.layer]) {
        const char *text = arr[i].m_select;
        u8g2_SetFont(&u8g2, LIST_FONT);
        int text_w = u8g2_GetStrWidth(&u8g2, text);
        /* V6.15: 按前缀计算可用宽度, 与复选框/数值不重叠 */
        int avail_w;
        char pf = text[0];
        if (pf == '+') {
            avail_w = CHECK_BOX_L_S - LIST_TEXT_S - 4;       /* ~87px, 避让复选框 */
        } else if (pf == '~') {
            avail_w = CHECK_BOX_L_S - LIST_TEXT_S - 20;      /* ~71px, 避让 "255" */
        } else {
            avail_w = DISP_W - LIST_TEXT_S - 4;               /* 全宽 */
        }
        if (avail_w < 40) avail_w = 40;

        if (text_w > avail_w) {
            int max_offset = text_w - avail_w;
            if (max_offset < 0) max_offset = 0;

            /* 滚动速度控制: 每 (12 - speed) 帧移动 1 像素 */
            uint8_t speed = g_config.long_text_scroll_speed;
            if (speed < 1) speed = 1;
            if (speed > 10) speed = 10;
            uint8_t interval = 12 - speed;
            if (interval < 1) interval = 1;

            s_scroll_tick++;
            if (s_scroll_tick >= interval) {
                s_scroll_tick = 0;

                if (s_scroll_pause > 0) {
                    s_scroll_pause--;
                } else {
                    if (s_scroll_dir) {
                        s_scroll_offset++;
                        if (s_scroll_offset >= max_offset) {
                            s_scroll_offset = max_offset;
                            s_scroll_pause = 30;  /* 右端暂停 30 帧 (~1s) */
                            s_scroll_dir = 0;
                        }
                    } else {
                        s_scroll_offset--;
                        if (s_scroll_offset <= 0) {
                            s_scroll_offset = 0;
                            s_scroll_pause = 20;  /* 左端暂停 20 帧 */
                            s_scroll_dir = 1;
                        }
                    }
                }
            }

            /* 用裁剪窗口限制绘制区域 */
            u8g2_SetClipWindow(&u8g2, LIST_TEXT_S, 0, avail_w, DISP_H);
            u8g2_DrawStr(&u8g2, LIST_TEXT_S - s_scroll_offset,
                         list.temp + LIST_TEXT_H + LIST_TEXT_S, text);
            /* 恢复全屏裁剪 */
            u8g2_SetMaxClipWindow(&u8g2);
        } else {
            /* 文字不超宽, 正常绘制 */
            u8g2_DrawStr(&u8g2, LIST_TEXT_S, list.temp + LIST_TEXT_H + LIST_TEXT_S, text);
        }
    } else {
        u8g2_DrawStr(&u8g2, LIST_TEXT_S, list.temp + LIST_TEXT_H + LIST_TEXT_S,
                     arr[i].m_select);
    }

    // 设置光标位置用于数值打印
    char first_char = arr[i].m_select[0];
    switch (first_char) {
        case '~': list_draw_value(i); break;
        case '+':
            list_draw_check_box_frame();
            if (check_box.m[i - 1] == 1) list_draw_check_box_dot();
            break;
        case '=':
            list_draw_check_box_frame();
            if (*check_box.s_p == i) list_draw_check_box_dot();
            break;
        case '#': list_draw_krf(i); break;
        case '$': list_draw_kpf(i); break;
        default: break;
    }
}

/*===========================================================================
 * 列表 (List) UI
 *===========================================================================*/

void list_show(M_SELECT arr[], uint8_t ui_index)
{
    // 更新动画目标值（仅选择框宽度）
    u8g2_SetFont(&u8g2, LIST_FONT);
    list.box_x_trg = (float)(u8g2_GetStrWidth(&u8g2, arr[ui.select[ui.layer]].m_select)
                    + LIST_TEXT_S * 2);

    /* V6.15: 选择框避让右侧复选框/数值 */
    char first_char = arr[ui.select[ui.layer]].m_select[0];
    if (first_char == '+') {
        /* + 项: 复选框在 x=95, 选择框不覆盖 */
        float max_x = (float)(CHECK_BOX_L_S - 4);
        if (list.box_x_trg > max_x) list.box_x_trg = max_x;
    } else if (first_char == '~') {
        /* ~ 项: 数值在 x=95, 留出 ~18px 给 3 位数字 "255" */
        float max_x = (float)(CHECK_BOX_L_S - 20);
        if (list.box_x_trg > max_x) list.box_x_trg = max_x;
    }

    // 计算滚动条光标目标位置
    if (ui.num[ui_index] > 1) {
        list.bar_y_trg = ceilf((float)(ui.select[ui.layer]) *
                               ((float)DISP_H / (float)(ui.num[ui_index] - 1)));
    } else {
        list.bar_y_trg = 0.0f;
    }

    // 计算动画过渡值
    animation(&list.y, &list.y_trg, LIST_ANI);
    animation(&list.box_x, &list.box_x_trg, LIST_ANI);
    animation(&list.box_y, &list.box_y_trg[ui.layer], LIST_ANI);
    animation(&list.bar_y, &list.bar_y_trg, LIST_ANI);

    // 检查循环动画是否结束
    if (list.loop && list.box_y == list.box_y_trg[ui.layer]) {
        list.loop = false;
    }

    // 绘制滚动条（右侧竖线 + 光标位置指示器）
    u8g2_SetDrawColor(&u8g2, 1);
    {
        int bar_left = DISP_W - LIST_BAR_W;
        int bar_mid  = DISP_W - (int)ceilf((float)LIST_BAR_W / 2.0f);
        int bar_h    = (int)list.bar_y;
        if (bar_h < 0) bar_h = 0;
        if (bar_h > DISP_H) bar_h = DISP_H;
        u8g2_DrawHLine(&u8g2, bar_left, 0, LIST_BAR_W);            // 顶部横线帽
        u8g2_DrawHLine(&u8g2, bar_left, DISP_H - 1, LIST_BAR_W);   // 底部横线帽
        u8g2_DrawVLine(&u8g2, bar_mid, 0, DISP_H);                  // 中间竖脊线
        u8g2_DrawBox(&u8g2, bar_left, 0, LIST_BAR_W, bar_h);       // 当前位置光标条
    }

    // 绘制列表文字
    if (!ui.init) {
        for (int i = 0; i < (int)ui.num[ui_index]; ++i) {
            if (ui.param[LIST_UFD]) {
                list.temp = (int)(i * list.y
                    - LIST_LINE_H * (int)ui.select[ui.layer]
                    + list.box_y_trg[ui.layer]);
            } else {
                list.temp = (int)((i - (int)ui.select[ui.layer]) * list.y
                    + list.box_y_trg[ui.layer]);
            }
            list_draw_text_and_check_box(arr, i);
        }
        if (list.y == list.y_trg) {
            ui.init = true;
            list_update_scroll_targets();
            list.y = list.y_trg;
        }
    } else {
        // 仅渲染屏幕可见的 2-3 行（而非全部 N 行，大幅提升帧率）
        int first = -(int)list.y / LIST_LINE_H;
        if (first < 0) first = 0;
        int last = (DISP_H - (int)list.y - 1) / LIST_LINE_H + 1;
        if (last >= (int)ui.num[ui_index]) last = (int)ui.num[ui_index] - 1;

        for (int i = first; i <= last; ++i) {
            list.temp = LIST_LINE_H * i + (int)list.y;
            list_draw_text_and_check_box(arr, i);
        }
    }

    // 绘制文字选择框 (反色)
    u8g2_SetDrawColor(&u8g2, 2);
    if(ui_style_skin()==UI_SKIN_MINIMAL)u8g2_DrawRFrame(&u8g2,0,(int)list.box_y,(int)list.box_x,LIST_LINE_H,1);
    else if(ui_style_skin()==UI_SKIN_TERMINAL){
        u8g2_DrawBox(&u8g2,0,(int)list.box_y,2,LIST_LINE_H);
        u8g2_DrawHLine(&u8g2,0,(int)list.box_y+LIST_LINE_H-1,(int)list.box_x);
    }else u8g2_DrawRBox(&u8g2,0,(int)list.box_y,(int)list.box_x,LIST_LINE_H,(int)LIST_BOX_R);

    // 白天模式遮罩
    if (!ui.param[DARK_MODE]) {
        u8g2_DrawBox(&u8g2, 0, 0, DISP_W, DISP_H);
        if (ui.index == M_WINDOW) {
            u8g2_DrawBox(&u8g2, 0, 0, DISP_W, DISP_H);
        }
    }
}

/*===========================================================================
 * 弹窗 (Window) UI
 *===========================================================================*/

void window_show(void)
{
    // 绘制背景列表
    list_show(win.bg, win.index);

    // 背景虚化 (可选)
    if (ui.param[WIN_BOK]) {
        for (uint16_t i = 0; i < buf_len; ++i) {
            buf_ptr[i] = buf_ptr[i] & (i % 2 == 0 ? 0x55 : 0xAA);
        }
    }

    // 更新动画目标值
    u8g2_SetFont(&u8g2, WIN_FONT);
    win.bar_trg = (float)(*win.value - win.min)
        / (float)(win.max - win.min) * (float)(WIN_BAR_W - 4);

    // 计算动画过渡值
    animation(&win.bar, &win.bar_trg, WIN_ANI);
    animation(&win.y, &win.y_trg, WIN_ANI);

    // 绘制窗口
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawRBox(&u8g2, win.l, (int16_t)win.y, WIN_W, WIN_H, 2);   // 外框背景
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_DrawRFrame(&u8g2, win.l, (int16_t)win.y, WIN_W, WIN_H, 2); // 外框描边
    u8g2_DrawRFrame(&u8g2, win.l + 5, (int16_t)win.y + 20,
                    WIN_BAR_W, WIN_BAR_H, 1);                         // 进度条外框
    u8g2_DrawBox(&u8g2, win.l + 7, (int16_t)win.y + 22,
                 (int)win.bar, WIN_BAR_H - 4);                        // 进度条

    // 绘制标题
    u8g2_DrawStr(&u8g2, win.l + 5, (int16_t)win.y + 14, win.title);

    // 绘制当前值
    char val_buf[8];
    snprintf(val_buf, sizeof(val_buf), "%d", *win.value);
    u8g2_DrawStr(&u8g2, win.l + 78, (int16_t)win.y + 14, val_buf);

    // 需要窗口修改参数时立即生效
    if (strcmp(win.title, "Disp Bri") == 0) {
        u8g2_SetContrast(&u8g2, ui.param[DISP_BRI]);
    }

    // 白天模式遮罩
    u8g2_SetDrawColor(&u8g2, 2);
    if (!ui.param[DARK_MODE]) {
        u8g2_DrawBox(&u8g2, 0, 0, DISP_W, DISP_H);
    }
}

/*===========================================================================
 * 初始化函数
 *===========================================================================*/

/** 默认参数初始化 */
void ui_param_init(void)
{
    ui.param[DISP_BRI]  = 255;   // 屏幕亮度
    ui.param[TILE_ANI]  = 30;    // 磁贴动画速度
    ui.param[LIST_ANI]  = 60;    // 列表动画速度
    ui.param[WIN_ANI]   = 25;    // 弹窗动画速度
    ui.param[SPOT_ANI]  = 50;    // 聚光动画速度
    ui.param[TAG_ANI]   = 60;    // 标签动画速度
    ui.param[FADE_ANI]  = 30;    // 消失动画速度
    ui.param[BTN_SPT]   = 25;    // 按键短按时长
    ui.param[BTN_LPT]   = 150;   // 按键长按时长
    ui.param[TILE_UFD]  = 1;     // 磁贴图标从头展开
    ui.param[LIST_UFD]  = 1;     // 菜单列表从头展开
    ui.param[TILE_LOOP] = 0;     // 磁贴循环模式
    ui.param[LIST_LOOP] = 0;     // 列表循环模式
    ui.param[WIN_BOK]   = 0;     // 弹窗背景虚化
    ui.param[KNOB_DIR]  = 0;     // 旋钮方向
    ui.param[DARK_MODE] = 1;     // 黑暗模式
}

/** 页面行数/图标数初始化 */
void ui_init(void)
{
    /* ── V10.6: 主菜单 6 磁贴 ── */
    ui.num[M_MAIN]    = 8;   /* Seven original tiles plus wireless tools. */
    ui.num[M_HOME]    = 1;
    /* ── V8.0: 设置精简为 9 项 ── */
    ui.num[M_SETTING] = 12;
    ui.num[M_ABOUT]   = 7;
    /* ── 二级功能页 (V6.13) ── */
    ui.num[M_QUICK_MSG]       = 10;
    ui.num[M_MORSE_INPUT]     = 0;   // 自定义交互模式
    ui.num[M_MORSE_LEARN]     = 6;   // 5 Tier + Free Practice
    ui.num[M_FRIENDS]         = 1;   // 动态刷新
    ui.num[M_MESH_STATUS]     = 8;   // list_show 8行
    ui.num[M_MSG_RX]          = 0;
    ui.num[M_DEV_MODE]        = 0;
    ui.num[M_NAME_SETUP]      = 0;   // 自定义交互模式
    ui.num[M_MORSE]           = 1;   // V10.5: CQ入口已移除，仅剩 Practice
    /* ── 小游戏页 (V10.6): 自定义交互模式 ── */
    ui.num[M_ANSWER_BOOK]     = 0;
    ui.num[M_DICE]            = 0;
    ui.num[M_CRAPS]           = 0;

    // 列表可见行数
    list.line_n = DISP_H / LIST_LINE_H;  // 32/16 = 2
}

/** 磁贴页面初始化 */
void tile_param_init(void)
{
    ui.init = false;
    tile.icon_x = 0;
    tile.icon_x_trg = TILE_ICON_S;
    tile.icon_y = -TILE_ICON_H;
    tile.icon_y_trg = 0;
    tile.indi_x = 0;
    tile.indi_x_trg = TILE_INDI_W;
}

/*===========================================================================
 * 磁贴选择切换
 *===========================================================================*/

void tile_rotate_switch(void)
{
    uint8_t old_sel = ui.select[ui.layer];
    switch (btn.id) {
        case BTN_ID_CW:
            if (ui.select[ui.layer] < ui.num[ui.index] - 1) {
                ui.select[ui.layer]++;
            } else if (ui.param[TILE_LOOP]) {
                ui.select[ui.layer] = 0;
            }
            break;
        case BTN_ID_CC:
            if (ui.select[ui.layer] > 0) {
                ui.select[ui.layer]--;
            } else if (ui.param[TILE_LOOP]) {
                ui.select[ui.layer] = ui.num[ui.index] - 1;
            }
            break;
        default: break;
    }
    if (ui.select[ui.layer] != old_sel) {
        haptic_play(HAPTIC_TICK);  /* 磁贴旋转反馈音，与列表页一致 */
    } else if (btn.id == BTN_ID_CW || btn.id == BTN_ID_CC) {
        haptic_play(HAPTIC_BUMP);  /* 已到首/尾 */
    }
    tile.indi_x_trg = TILE_INDI_W;
    tile.icon_x_trg = -(float)((int)ui.select[ui.layer] * TILE_ICON_S);
}

/*===========================================================================
 * 列表滚动目标计算 (V6.38)
 *
 * 当列表恰好 2 项（屏幕显示 2 行）时：
 *   - 不滚动列表 (y_trg = 0)，两项始终可见
 *   - 移动选择框 (box_y_trg) 在两项之间切换
 * 当列表 ≥3 项时：
 *   - 选择框固定在顶部 (box_y_trg = 0)
 *   - 滚动列表 (y_trg) 来定位选中项
 *===========================================================================*/
void list_update_scroll_targets(void)
{
    if (ui.num[ui.index] == 2) {
        /* 2 项固定显示: 列表不滚动, 光标在上下行间移动 */
        list.y_trg = 0.0f;
        list.box_y_trg[ui.layer] = (float)(LIST_LINE_H * (int)ui.select[ui.layer]);
    } else {
        /* 3+ 项: 选择框置顶, 列表滚动 */
        list.box_y_trg[ui.layer] = 0.0f;
        list.y_trg = -(float)(LIST_LINE_H * (int)ui.select[ui.layer]);
    }
}

/*===========================================================================
 * 列表选择切换
 *===========================================================================*/

void list_rotate_switch(void)
{
    /* V6.14: 选择变化时重置滚动偏移 */
    s_scroll_offset = 0;
    s_scroll_tick = 0;
    s_scroll_dir = 1;
    s_scroll_pause = 0;

    switch (btn.id) {
        case BTN_ID_CW:
            if (ui.select[ui.layer] < ui.num[ui.index] - 1) {
                ui.select[ui.layer]++;
            } else if (ui.param[LIST_LOOP]) {
                ui.select[ui.layer] = 0;
                list.loop = true;
            }
            break;
        case BTN_ID_CC:
            if (ui.select[ui.layer] > 0) {
                ui.select[ui.layer]--;
            } else if (ui.param[LIST_LOOP]) {
                ui.select[ui.layer] = ui.num[ui.index] - 1;
                list.loop = true;
            }
            break;
        default: break;
    }
    list_update_scroll_targets();
    haptic_play(HAPTIC_TICK);
}

/*===========================================================================
 * 弹窗数值处理
 *===========================================================================*/

void window_value_proc(void)
{
    switch (btn.id) {
        case BTN_ID_CW:
            if (*win.value < win.max) {
                unsigned next = (unsigned)*win.value + win.step;
                *win.value = next > win.max ? win.max : (uint8_t)next;
            }
            break;
        case BTN_ID_CC:
            if (*win.value > win.min) {
                int next = (int)*win.value - win.step;
                *win.value = next < win.min ? win.min : (uint8_t)next;
            }
            break;
        case BTN_ID_SP:
            ui.index = win.index;
            ui.state = S_NONE;  /* V6.14: S_NONE 保持列表滚动位置 */
            win.value_confirmed = true;
            eeprom.change = true;
            nvs_write_all_data();  /* 立即落盘 ui.param[]（亮度/按键阈值等），修复改后不保存 */
            break;
        case BTN_ID_BACK:
            /* V6.14: KEY 关闭弹窗（放弃修改） */
            *win.value = win.original_value;
            ui.index = win.index;
            ui.state = S_NONE;
            break;
        default: break;
    }
    if (strcmp(win.title, "Disp Bri") == 0) {
        u8g2_SetContrast(&u8g2, ui.param[DISP_BRI]);
    }
}

/**
 * @brief 列表选择弹窗渲染 — V6.32 重写：WouoUI 原生弹窗内嵌 2 行 list
 *
 * 弹窗外框（RBox+RFrame，与 window_show 一致）内嵌 WouoUI 风格 list。
 * 无标题，2 行可见（WIN_H 32 / LIST_LINE_H 16）。
 * 选择框使用与 list_show 完全相同的 XOR 反色 + 平滑动画（LIST_ANI）。
 */
void window_list_show(void)
{
    /* ── V6.50: 消息弹窗超时自动关闭（msg_timeout=0 时不超时）── */
    if (win.is_msg) {
        if (win.msg_timeout > 0) {
            win.msg_ticks++;
            if (win.msg_ticks >= win.msg_timeout) {
                /* V9.4: 超时关闭时同样清除状态 + 停止呼吸灯 */
                win.is_msg = false;
                win.is_sent = false;
                rgb_led_stop();
                ui.index = win.index;
                ui.state = S_NONE;
                return;
            }
        }
    }

    /* 绘制背景列表 */
    if (win.bg != NULL) {
        list_show(win.bg, win.index);
    }

    /* 背景虚化 (可选) */
    if (ui.param[WIN_BOK]) {
        for (uint16_t i = 0; i < buf_len; ++i) {
            buf_ptr[i] = buf_ptr[i] & (i % 2 == 0 ? 0x55 : 0xAA);
        }
    }

    /* ── 动画 ── */
    animation(&win.y, &win.y_trg, WIN_ANI);
    animation(&win.list_box_w, &win.list_box_w_trg, LIST_ANI);
    animation(&win.list_box_row, &win.list_box_row_trg, LIST_ANI);

    /* ══ 弹窗外框（与 window_show 完全一致）══ */
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawRBox(&u8g2, win.l, (int16_t)win.y, WIN_W, WIN_H, 2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_DrawRFrame(&u8g2, win.l, (int16_t)win.y, WIN_W, WIN_H, 2);

    /* ══ V6.50: 消息弹窗 — 标题行 + 消息内容 ══ */
    if (win.is_msg) {
        /* 标题行：发送人名称 */
        if (win.title[0] != '\0') {
            u8g2_SetFont(&u8g2, WIN_FONT);
            u8g2_SetDrawColor(&u8g2, 1);
            u8g2_DrawStr(&u8g2, win.l + 5, (int16_t)win.y + 14, win.title);
        }
        /* 消息内容行（标题下方） */
        if (win.list_count > 0 && win.list_items[0].m_select[0] != '\0') {
            const char *text = win.list_items[0].m_select;
            int text_y = (int16_t)win.y + 14 + LIST_TEXT_H + LIST_TEXT_S;
            u8g2_SetFont(&u8g2, LIST_FONT);
            u8g2_SetDrawColor(&u8g2, 1);
            if (g_config.long_text_scroll_enable) {
                int text_w = u8g2_GetStrWidth(&u8g2, text);
                int avail_w = WIN_W - 10;
                if (text_w > avail_w && avail_w > 30) {
                    int max_offset = text_w - avail_w;
                    if (max_offset < 0) max_offset = 0;
                    uint8_t speed = g_config.long_text_scroll_speed;
                    if (speed < 1) speed = 1;
                    if (speed > 10) speed = 10;
                    uint8_t interval = 12 - speed;
                    if (interval < 1) interval = 1;
                    static int16_t smx = 0;
                    static uint8_t smt = 0, smd = 1, smp = 0;
                    smt++;
                    if (smt >= interval) {
                        smt = 0;
                        if (smp > 0) {
                            smp--;
                        } else {
                            if (smd) {
                                smx++;
                                if (smx >= max_offset) {
                                    smx = max_offset;
                                    smp = 30;
                                    smd = 0;
                                }
                            } else {
                                smx--;
                                if (smx <= 0) {
                                    smx = 0;
                                    smp = 20;
                                    smd = 1;
                                }
                            }
                        }
                    }
                    u8g2_SetClipWindow(&u8g2, (uint16_t)(win.l + 5),
                                       (uint16_t)(text_y - LIST_TEXT_H),
                                       (uint16_t)avail_w, (uint16_t)LIST_LINE_H);
                    u8g2_DrawStr(&u8g2, win.l + 5 - smx, text_y, text);
                    u8g2_SetMaxClipWindow(&u8g2);
                    goto draw_dark_mode;
                }
            }
            u8g2_DrawStr(&u8g2, win.l + 5, text_y, text);
        }
        goto draw_dark_mode;
    }

    /* ══ 标准选择弹窗：内嵌 list（无标题）══ */
    u8g2_SetFont(&u8g2, LIST_FONT);

    int visible = WIN_H / LIST_LINE_H;
    if (visible > (int)win.list_count)
        visible = (int)win.list_count;

    /* ── V6.50: 长文本滚动状态（移植自 list_draw_text_and_check_box）── */
    static int16_t  s_win_scroll_offset = 0;
    static uint8_t  s_win_scroll_tick = 0;
    static uint8_t  s_win_scroll_dir = 1;
    static uint8_t  s_win_scroll_pause = 0;

    for (int i = 0; i < visible; i++) {
        int idx = win.list_scroll + i;
        if (idx >= (int)win.list_count) break;

        int line_y = (int16_t)win.y + LIST_TEXT_H + LIST_TEXT_S + i * LIST_LINE_H;
        const char *text = win.list_items[idx].m_select;

        /* V6.50: 长文本水平滚动（受全局 config 控制） */
        if (g_config.long_text_scroll_enable && text && text[0]) {
            u8g2_SetFont(&u8g2, LIST_FONT);
            int text_w = u8g2_GetStrWidth(&u8g2, text);
            int avail_w = WIN_W - LIST_TEXT_S - 8;  /* 窗口内可用宽度 */

            if (text_w > avail_w && avail_w > 30) {
                int max_offset = text_w - avail_w;
                if (max_offset < 0) max_offset = 0;

                uint8_t speed = g_config.long_text_scroll_speed;
                if (speed < 1) speed = 1;
                if (speed > 10) speed = 10;
                uint8_t interval = 12 - speed;
                if (interval < 1) interval = 1;

                s_win_scroll_tick++;
                if (s_win_scroll_tick >= interval) {
                    s_win_scroll_tick = 0;
                    if (s_win_scroll_pause > 0) {
                        s_win_scroll_pause--;
                    } else {
                        if (s_win_scroll_dir) {
                            s_win_scroll_offset++;
                            if (s_win_scroll_offset >= max_offset) {
                                s_win_scroll_offset = max_offset;
                                s_win_scroll_pause = 30;
                                s_win_scroll_dir = 0;
                            }
                        } else {
                            s_win_scroll_offset--;
                            if (s_win_scroll_offset <= 0) {
                                s_win_scroll_offset = 0;
                                s_win_scroll_pause = 20;
                                s_win_scroll_dir = 1;
                            }
                        }
                    }
                }

                u8g2_SetDrawColor(&u8g2, 1);
                u8g2_SetClipWindow(&u8g2, (uint16_t)(win.l + LIST_TEXT_S),
                                   (uint16_t)(line_y - LIST_TEXT_H),
                                   (uint16_t)avail_w, (uint16_t)LIST_LINE_H);
                u8g2_DrawStr(&u8g2, win.l + LIST_TEXT_S - s_win_scroll_offset,
                             line_y, text);
                u8g2_SetMaxClipWindow(&u8g2);
                continue;
            }
        }

        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawStr(&u8g2, win.l + LIST_TEXT_S, line_y, text);
    }

    /* ══ 选择框 — XOR 反色，与 list_show 的 DrawRBox color=2 完全一致 ══ */
    u8g2_SetDrawColor(&u8g2, 2);
    int box_x = win.l;
    int box_y = (int16_t)win.y + (int)win.list_box_row;
    u8g2_DrawRBox(&u8g2, box_x, box_y, (int)win.list_box_w, LIST_LINE_H, (int)LIST_BOX_R);

draw_dark_mode:
    /* 白天模式遮罩 */
    u8g2_SetDrawColor(&u8g2, 2);
    if (!ui.param[DARK_MODE]) {
        u8g2_DrawBox(&u8g2, 0, 0, DISP_W, DISP_H);
    }
}

static void _win_list_update_anim_targets(void);

/**
 * @brief 列表选择弹窗输入处理 — V6.50: 支持消息弹窗模式
 */
void window_list_proc(void)
{
    /* ── V6.50: 消息弹窗 — 仅 SP/BACK 关闭，不自动超时 ── */
    if (win.is_msg) {
        if (btn.id == BTN_ID_SP || btn.id == BTN_ID_BACK) {
            /* V9.4: 关闭弹窗时清除消息状态 + 停止呼吸灯 */
            win.is_msg = false;
            win.is_sent = false;
            rgb_led_stop();
            ui.index = win.index;
            ui.state = S_NONE;
            return;
        }
        return;
    }

    /* ── 列表选择弹窗 — 标准 WouoUI 行为 ── */
    int visible = WIN_H / LIST_LINE_H;

    switch (btn.id) {
        case BTN_ID_CW:
            if (win.list_cur + 1 < win.list_count) {
                win.list_cur++;
                if (win.list_cur >= win.list_scroll + visible)
                    win.list_scroll = win.list_cur - visible + 1;
                _win_list_update_anim_targets();
            }
            break;
        case BTN_ID_CC:
            if (win.list_cur > 0) {
                win.list_cur--;
                if (win.list_cur < win.list_scroll)
                    win.list_scroll = win.list_cur;
                _win_list_update_anim_targets();
            }
            break;
        case BTN_ID_SP:
            if (win.list_sel != NULL)
                *win.list_sel = win.list_cur;
            LOG_UI_DIAG("ui", "WIN-LIST SEL: cur=%d saved=%d", win.list_cur,
                     win.list_sel ? *win.list_sel : -1);
            ui.index = win.index;
            ui.state = S_NONE;
            break;
        case BTN_ID_BACK:
            LOG_UI_DIAG("ui", "WIN-LIST CANCEL → back to index=%d", win.index);
            ui.index = win.index;
            ui.state = S_NONE;
            break;
        default: break;
    }
}

/**
 * @brief 更新弹窗内嵌 list 选择框动画目标值（V6.32）
 *
 * 与 list_show 的 box_x_trg 计算逻辑完全一致。
 */
static void _win_list_update_anim_targets(void)
{
    if (win.list_cur >= win.list_count) return;

    const char *text = win.list_items[win.list_cur].m_select;
    u8g2_SetFont(&u8g2, LIST_FONT);
    win.list_box_w_trg = (float)(u8g2_GetStrWidth(&u8g2, text) + LIST_TEXT_S * 2);

    int row_in_view = (int)win.list_cur - (int)win.list_scroll;
    if (row_in_view < 0) row_in_view = 0;
    if (row_in_view >= WIN_H / LIST_LINE_H) row_in_view = WIN_H / LIST_LINE_H - 1;
    win.list_box_row_trg = (float)(row_in_view * LIST_LINE_H);
}

/*===========================================================================
 * 层级管理
 *===========================================================================*/

/** 导航至子页面（保存当前页为下一层的父页，供 KEY 返回） */
void ui_navigate_to(uint8_t target_page)
{
    if (ui.layer + 1 >= UI_DEPTH) {
        ESP_LOGW("ui", "NAV: layer overflow, ignore %d -> %d", ui.index, target_page);
        return;
    }
    LOG_UI_DIAG("ui", "NAV: %d -> %d (layer=%d)", ui.index, target_page, ui.layer);
    ui.parent_index[ui.layer + 1] = ui.index;
    ui.index = target_page;
    ui.state = S_LAYER_IN;
    haptic_play(HAPTIC_CONFIRM);
}

void layer_init_in(void)
{
    ui.layer++;
    ui.select[ui.layer] = 0;
    ui.init = false;
    ui.state = S_NONE;
    if (ui.index == M_MAIN) {
        tile_param_init();
    }
}

void layer_init_out(void)
{
    if (ui.layer > 0) {
        ui.layer--;
    }
    ui.init = false;
    ui.state = S_NONE;
    if (ui.index == M_MAIN) {
        tile_param_init();
    }
}

/*===========================================================================
 * 状态机 (ui_proc)
 *===========================================================================*/

void ui_proc(void)
{
    /* V6.15: 每次 ui 循环确保按键处理可用 (防止 free practice 退出残留) */
    g_morse_key_active = false;

    switch (ui.state) {
        case S_FADE:
            fade();
            break;
        case S_WINDOW:
            /* 弹窗初始化: 设置位置动画参数 */
            win.y = WIN_Y;
            win.y_trg = 0;
            win.bar = 0;
            ui.state = S_NONE;
            break;
        case S_LAYER_IN:
            layer_init_in();
            break;
        case S_LAYER_OUT:
            layer_init_out();
            break;
        case S_NONE:
        default:
            break;
    }

    /* 根据页面索引调用处理函数 */
    local_pages_sync_page();
    switch (ui.index) {
        case M_HOME:    home_proc();    break;
        case M_MAIN:    main_proc();    break;
        case M_SETTING: case M_SETTINGS_DISPLAY: case M_SETTINGS_FEEDBACK: case M_SETTINGS_INPUT: case M_SETTINGS_SYSTEM: settings_proc(); break;
        case M_ABOUT:   about_new_proc(); break;
        /* ── 二级功能页 (V6.13) ── */
        case M_QUICK_MSG:       local_pages_proc();   break;
        case M_MORSE_INPUT:     local_pages_proc();      break;
        case M_MORSE_LEARN:     local_pages_proc(); break;
        case M_MORSE:           morse_submenu_proc(); break;  /* V7.7 */
        /* case M_FRIENDS: V9.1 废弃 — 好友系统暂缓 */
        case M_MESH_STATUS:     mesh_status_proc();  break;
#if LOG_DEV_MODE_ENABLE
        case M_DEV_MODE:        dev_mode_page_proc();  break;
#endif
        case M_MSG_RX:          msg_rx_proc();      break;
        case M_QUICK_MSG_SENDING: quick_msg_sending_proc(); break;
        case M_NAME_SETUP:      name_setup_proc();  break;
        /* ── 小游戏页 (V10.6) ── */
        case M_ANSWER_BOOK:     answer_book_proc(); break;
        case M_DICE:            dice_proc();        break;
        case M_CRAPS:           craps_proc();        break;
        case M_TETRIS:          tetris_page_proc();  break;
        case M_SNAKE:           snake_page_proc();   break;
        case M_PET:             pet_page_proc();     break;
        case M_GAME_HELP: {extern void game_help_page_proc(void);game_help_page_proc();break;}
        case M_RPS:             rps_proc();          break;
        case M_WINDOW:
            if (win.title[0] != '\0') {
#if 0  /* V6.53: 每 tick 刷屏噪音，调试 UI 状态机时改为 1 */
                LOG_UI_DIAG("ui", "WIN-DISPATCH: is_list=%d is_msg=%d btn_pressed=%d btn_id=%d",
                         win.is_list ? 1 : 0, win.is_msg ? 1 : 0,
                         btn.pressed ? 1 : 0, btn.id);
#endif
                if (win.is_list) {
                    /* 列表选择弹窗 */
                    window_list_show();
                    if (btn.pressed) {
                        window_list_proc();
                        btn.pressed = false;
                    }
                } else {
                    /* 数值修改弹窗 */
                    window_show();
                    if (btn.pressed) {
                        window_value_proc();
                        btn.pressed = false;
                    }
                }
            }
            break;
        default: local_pages_proc(); break;
    }

    /* ── 回退 BACK 处理器（页面未消费的 BTN_ID_BACK 走默认返回上一级）── */
    if (btn.pressed && btn.id == BTN_ID_BACK) {
        btn.pressed = false;
        haptic_play(HAPTIC_BACK);

        LOG_UI_DIAG("ui", "BACK-HANDLER: index=%d layer=%d state=%d",
                 ui.index, ui.layer, ui.state);

        if (ui.index == M_WINDOW) {
            /* 弹窗中：关闭弹窗，返回弹窗背景页面 */
            if (win.index < UI_MNUMB) {
                ui.index = win.index;
                ui.state = S_LAYER_OUT;
            }
        } else if (ui.index == M_HOME) {
            /* 主页：KEY 无操作 */
#if LOG_DEV_MODE_ENABLE
        } else if (ui.index == M_DEV_MODE) {
            /* V6.15: Dev Mode — 子页→主菜单, 主菜单→M_MAIN */
            if (dev_mode_is_in_sub_page()) {
                dev_mode_back_to_main();
            } else if (ui.layer > 0) {
                ui.index = ui.parent_index[ui.layer];
                ui.state = S_LAYER_OUT;
            }
#endif
        } else if (ui.layer > 0) {
            /* 子页面（含主菜单）：通过 parent_index 返回上一级 */
            ui.index = ui.parent_index[ui.layer];
            ui.state = S_LAYER_OUT;
        }
    }
}
