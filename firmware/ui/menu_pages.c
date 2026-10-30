/**
 * @file menu_pages.c
 * @brief WouoUI-128_32 ��ҳ�洦���??
 *
 * ??ui_proc() ״̬���б�����??
 * ÿ�� _proc() ���������ҳ�����ʾ + ���봦��??
 */

#include "wououi.h"
#include "ui_language.h"
#include "ui_style.h"
#include "home_widgets.h"
#include "ui_zh.h"
#include "local_pages.h"
#include "morse_playback.h"
#include "message_service.h"
#include "rps_game.h"
#include "menu_content.h"
#include "ec06.h"
#include "quick_msg.h"
#include "nvs_config.h"
#include "mesh_notify.h"
#include "rgb_led.h"
#include "haptic_feedback.h"
#include "morse_engine.h"
#include "morse_ui.h"           /* V7.0: Morse CQ �㲥 UI */
#include "morse_terminal.h"
#include "morse_input.h"        /* V7.7: morse_input_set_dot_ms() */
/* V9.1: friend_mgr.h 移除 — 好友系统暂缓 */
#include "ble_radio.h"
#include "config_mode.h"
#include "log_config.h"
#include "dev_mode.h"
#include "rmt_ws2812.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <stdio.h>

static bool localized_game_text(void){return ui.index==M_ANSWER_BOOK || ui.index==M_DICE || ui.index==M_CRAPS;}
#define u8g2_DrawStr(d,x,y,s) (localized_game_text()?ui_zh_draw(d,x,y,s):(void)u8g2_DrawStr(d,x,y,s))
#define u8g2_GetStrWidth(d,s) (localized_game_text()?ui_zh_width(d,s):u8g2_GetStrWidth(d,s))

/* V9.0: 移除 V6.39 发送状态页变量（fire-and-forget 无需等待ACK） */
static uint32_t s_sending_start_ms = 0;      /* 进入发送页的时刻 */

/*===========================================================================
 * ����??(����)
 *===========================================================================*/

void main_proc(void)
{
    tile_show(main_icon_pic);

    if (btn.pressed) {
        LOG_UI_DIAG("ui", "MAIN: btn id=%d select=%d", btn.id, ui.select[ui.layer]);
        switch (btn.id) {
            case BTN_ID_CW:
            case BTN_ID_CC:
                btn.pressed = false;
                tile_rotate_switch();
                break;

            case BTN_ID_SP:
                btn.pressed = false;
                ESP_LOGI("ui", "main_proc SP on tile %d -> page %d", ui.select[ui.layer], ui.select[ui.layer]);
                /* V10.6: 6 tiles: QuickMsg / Morse / Settings / About / AnswerBook / Dice */
                switch (ui.select[ui.layer]) {
                    case 0: ui_navigate_to(M_MESSAGES_HUB); break;
                    case 1: local_pages_open_channel(); break;
                    case 2: ui_navigate_to(M_LEARN_HUB); break;
                    case 3: ui_navigate_to(M_FRIENDS); break;
                    case 4: ui_navigate_to(M_FUN_HUB); break;
                    case 5: ui_navigate_to(M_TOOLS); break;
                    case 6: ui_navigate_to(M_SETTING); break;
                    case 7: ui_navigate_to(M_ABOUT); break;
                }
                break;

            case BTN_ID_BACK:
                /* Pass through to ui_proc fallback ??return to M_HOME */
                break;

            default:
                btn.pressed = false;
                break;
        }
        if (!tile.select_flag && ui.init) {
            tile.indi_x = 0;
        }
    }
}

/*===========================================================================
 * ϵͳ�˵�ҳ�洦����� ??V6.1
 *===========================================================================*/

/* ���� ��ҳ������㣩V6.12: ����??������������������������������������������������ */

static uint32_t s_game_entry_epoch;
void menu_pages_on_enter(uint8_t page)
{
    if (page != M_RPS) rps_game_leave();
    if (page == M_ANSWER_BOOK || page == M_DICE || page == M_CRAPS) ++s_game_entry_epoch;
}

static void pager_home_draw(void)
{
    const unsigned unread = message_service_unread_count();
    char line[28];
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
    /* Pager status rail: radio readiness, room, and the real sound setting. */
    u8g2_DrawVLine(&u8g2, 2, 3, 5);
    u8g2_DrawLine(&u8g2, 0, 1, 2, 3);
    u8g2_DrawLine(&u8g2, 4, 1, 2, 3);
    if (ble_radio_is_ready()) {
        u8g2_DrawBox(&u8g2, 6, 6, 2, 2);
        u8g2_DrawBox(&u8g2, 9, 4, 2, 4);
        u8g2_DrawBox(&u8g2, 12, 2, 2, 6);
    }
    snprintf(line, sizeof(line), "ROOM %02u", message_service_get_channel());
    u8g2_DrawStr(&u8g2, 20, 7, line);
    u8g2_DrawStr(&u8g2, 96, 7, g_config.buzzer_enabled ? "SOUND" : "MUTE");
    if(ui_style_skin()==UI_SKIN_MINIMAL){for(int x=0;x<128;x+=8)u8g2_DrawHLine(&u8g2,x,10,4);}
    else {u8g2_DrawHLine(&u8g2,0,10,128);if(ui_style_skin()==UI_SKIN_TERMINAL)u8g2_DrawHLine(&u8g2,0,12,128);}
    if(home_widget_draw(unread))return;
    /* Large envelope and unread badge occupy the old button-help space. */
    const int bounce = unread && (esp_timer_get_time() / 500000) % 2 ? -1 : 0;
    u8g2_DrawRFrame(&u8g2, 3, 16 + bounce, 23, 14, 2);
    u8g2_DrawLine(&u8g2, 4, 17 + bounce, 14, 24 + bounce);
    u8g2_DrawLine(&u8g2, 24, 17 + bounce, 14, 24 + bounce);
    if (unread) {
        u8g2_DrawDisc(&u8g2, 25, 16 + bounce, 3, U8G2_DRAW_ALL);
        u8g2_SetDrawColor(&u8g2, 0);
        u8g2_DrawVLine(&u8g2, 25, 14 + bounce, 3);
        u8g2_SetDrawColor(&u8g2, 1);
    }
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    snprintf(line, sizeof(line), "%.14s", g_config.my_device_name[0] ? g_config.my_device_name : "BLUE LONG");
    u8g2_DrawStr(&u8g2, 34, 21, line);
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
    if (unread) snprintf(line, sizeof(line), "%02u NEW MESSAGES", unread);
    else snprintf(line, sizeof(line), "%02u SAVED  %s", message_service_inbox_count(),
                  ble_radio_is_ready() ? "ONLINE" : "STARTING");
    u8g2_DrawStr(&u8g2, 34, 31, line);
}

void home_proc(void)
{
    pager_home_draw();

    if (btn.pressed) {
        LOG_UI_DIAG("ui", "HOME: btn id=%d", btn.id);
        switch (btn.id) {
            case BTN_ID_SP:
                btn.pressed = false;
                rgb_led_trigger(RGB_EVENT_MENU_ENTER);  /* V7.0: ��ɫ����һ??*/
                // haptic removed
                ui_navigate_to(M_MAIN);
                break;
            case BTN_ID_BACK:
                /* Pass through to ui_proc fallback ??stay at home */
                break;
            default:
                ESP_LOGW("ui", "[UI-DIAG] HOME: UNHANDLED btn id=%d (pressed cleared)", btn.id);
                btn.pressed = false;
                break;
        }
    }

}

/* V9.1: 快捷消息直接广播 — 移除联系人选择弹窗 */

void quick_msg_proc(void)
{
    local_pages_proc();
}

/* V9.1: friends_proc 移除 — 好友系统暂缓。menu_set_blocked_popup 保留空桩兼容 */
void menu_set_blocked_popup(uint32_t blocked_by_id)
{
    (void)blocked_by_id;
}

void friends_proc(void)
{
    local_pages_proc();
}

/* �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
 *  V6.13: ����????�ϲ� 12 ??
 *  ~ = ��ֵ��?? + = ��ֵ��??SP�л�), - = ����
 * �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T */

/* Ten settings rows; checkbox storage uses the renderer's row-minus-one index. */
static uint8_t s_settings_toggles[9];  /* [2]=LED, [3]=Buzzer, [5]=Scroll, [8]=Chinese */
static uint8_t s_settings_values[9];

/* V6.14: �������ؼ�????�ӳٱ���༭??*/
static bool    s_popup_pending_dotms = false;
static uint8_t s_dot_ms_val = 6;
static uint8_t s_applied_spt = 0xFF;      /* 已应用到 ec06 的 BTN_SPT（0xFF=未应用） */
static uint8_t s_applied_lpt = 0xFF;

/* V6.19: �ָ��������õ��� */
static bool    s_reset_popup_pending = false;
static uint8_t s_reset_sel_sentinel = 0xFF;

void settings_proc(void)
{
    extern nvs_config_t g_config;
    if(ui.index==M_SETTING){
        static M_SELECT categories[]={{"- Network and connections"},{"- Display and language"},{"- Sound and lights"},{"- Buttons and Morse"},{"- Power and system"}};
        static const uint8_t pages[]={M_SETTINGS_NETWORK,M_SETTINGS_DISPLAY,M_SETTINGS_FEEDBACK,M_SETTINGS_INPUT,M_SETTINGS_SYSTEM};
        ui.num[M_SETTING]=5;list_show(categories,M_SETTING);
        if(btn.pressed && (btn.id==BTN_ID_CW || btn.id==BTN_ID_CC)){btn.pressed=false;list_rotate_switch();}
        else if(btn.pressed && btn.id==BTN_ID_SP){btn.pressed=false;ui_navigate_to(pages[ui.select[ui.layer]%5]);}
        return;
    }
    static const uint8_t display[]={0,6,9,10,11},feedback[]={3,4},input[]={1,2,5},system[]={8,7};
    const uint8_t *map=display;unsigned count=sizeof(display);
    if(ui.index==M_SETTINGS_FEEDBACK){map=feedback;count=sizeof(feedback);}
    if(ui.index==M_SETTINGS_INPUT){map=input;count=sizeof(input);}
    if(ui.index==M_SETTINGS_SYSTEM){map=system;count=sizeof(system);}
    static M_SELECT rows[5];
    static uint8_t toggles[6],values[6];
    uint8_t page=ui.index;

    /* V6.19: �ӻָ���������ȷ�ϵ�����??*/
    if (s_reset_popup_pending && ui.index == page && ui.state == S_NONE) {
        s_reset_popup_pending = false;
        if (s_reset_sel_sentinel == 1) {
            /* �û�ѡ��??"Confirm Reset" */
            nvs_config_factory_reset();
            /* ���ᵽ������ ??esp_restart() �������� */
        }
        s_reset_sel_sentinel = 0xFF;
    }

    /* V6.14: ����ֵ�������غ󣬽��༭ֵд??g_config */
    if (ui.index == page && ui.state == S_NONE) {
        if (s_popup_pending_dotms && win.value_confirmed) {
            g_config.morse_wpm = s_dot_ms_val;
            g_config.morse_dot_ms = (uint16_t)(1200u / s_dot_ms_val);
            nvs_config_mark_dirty();
            /* 即时应用到引擎 */
            morse_input_set_dot_ms(g_config.morse_dot_ms);
            /* V9.9.1: 防止 g_config.morse_dot_ms==0 时除零崩溃 */
            if (g_config.morse_dot_ms > 0) {
                morse_engine_set_wpm((uint8_t)(1200 / g_config.morse_dot_ms));
            }
            s_popup_pending_dotms = false;
        }
        s_popup_pending_dotms = false;
        /* Btn SPT/LPT 改动即时应用到 ec06 按键驱动（弹窗直接改 ui.param） */
        if (ui.param[BTN_SPT] != s_applied_spt || ui.param[BTN_LPT] != s_applied_lpt) {
            s_applied_spt = ui.param[BTN_SPT];
            s_applied_lpt = ui.param[BTN_LPT];
            ec06_set_press_thresholds((uint32_t)ui.param[BTN_SPT] * BTN_PARAM_TIMES,
                                      (uint32_t)ui.param[BTN_LPT] * BTN_PARAM_TIMES);
        }
    }

    /* ═══════════════════════════════════════════════════════════
     * V9.9.1: check_box 索引映射（关键！勿改！）
     *
     * 渲染函数 list_draw_text_and_check_box(i) 的访问约定:
     *    + 开关项 → check_box.m[i - 1]
     *    ~ 数值项 → check_box.v[i - 1]
     *
     * check_box.m = s_settings_toggles (无偏移)
     *   → s_toggles[2] → 菜单项3 (LED)    的复选框
     *   → s_toggles[3] → 菜单项4 (Buzzer) 的复选框
     *   → s_toggles[5] → 菜单项6 (Scroll) 的复选框
     *
     * check_box.v = s_settings_values + 1 (偏移+1)
     *   → v[-1]=s[0], v[0]=s[1], v[1]=s[2], v[4]=s[5]
     * ═══════════════════════════════════════════════════════════ */
    memset(s_settings_toggles, 0, sizeof(s_settings_toggles));
    s_settings_toggles[2]  = g_config.led_enabled ? 1 : 0;            /* item 3 → check_box.m[2] */
    s_settings_toggles[3]  = g_config.buzzer_enabled ? 1 : 0;         /* item 4 → check_box.m[3] */
    s_settings_toggles[5]  = g_config.long_text_scroll_enable ? 1 : 0; /* item 6 → check_box.m[5] */
    s_settings_toggles[8] = ui_language_chinese()?1:0; /* item 9 -> row-minus-one */

    check_box.m = s_settings_toggles;
    s_settings_values[0] = ui.param[DISP_BRI];                         /* item 0: ~ Disp Bri */
    s_settings_values[1] = ui.param[BTN_SPT];                           /* item 1: ~ Btn SPT */
    s_settings_values[2] = ui.param[BTN_LPT];                           /* item 2: ~ Btn LPT */
    s_settings_values[5] = (uint8_t)(1200u / (g_config.morse_dot_ms ? g_config.morse_dot_ms : 200));     /* item 5: ~ Morse Speed (*10ms) */
    check_box.v = s_settings_values + 1;
    settings_menu[10].m_select=(char *)ui_style_skin_label();
    settings_menu[11].m_select=(char *)ui_style_widget_label();
    for(unsigned i=0;i<count;i++){
        rows[i]=settings_menu[map[i]];
        toggles[i]=map[i]?s_settings_toggles[map[i]-1]:0;
        values[i]=s_settings_values[map[i]];
    }
    /* A leading dummy cell permits the renderer's row-minus-one convention. */
    check_box.m=toggles+1;check_box.v=values+1;
    ui.num[page]=count;list_show(rows,page);

    if (btn.pressed) {
        switch (btn.id) {
            case BTN_ID_CW:
            case BTN_ID_CC:
                btn.pressed = false;
                list_rotate_switch();
                break;

            case BTN_ID_SP: {
                btn.pressed = false;
                uint8_t sel = ui.select[ui.layer];
                if(sel>=count)break;
                switch (map[sel]) {
                    /* ~ ��ֵ��??*/
                    case 0: /* Disp Bri */
                        window_value_init("Disp Bri", sel,
                            &ui.param[DISP_BRI], 255, 0, 5, rows, page);
                        break;
                    case 1: /* Btn SPT */
                        window_value_init("Btn SPT", sel,
                            &ui.param[BTN_SPT], 255, 0, 5, rows, page);
                        break;
                    case 2: /* Btn LPT */
                        window_value_init("Btn LPT", sel,
                            &ui.param[BTN_LPT], 255, 0, 5, rows, page);
                        break;
                    /* + 开关项 (直接映射到 s_settings_toggles[]) */
                    case 3: /* LED Enable */
                        g_config.led_enabled = !g_config.led_enabled;
                        if (!g_config.led_enabled) rgb_led_stop();
                        nvs_config_mark_dirty();
                        break;
                    case 4: /* Buzzer Enable */
                        g_config.buzzer_enabled = !g_config.buzzer_enabled;
                        if (!g_config.buzzer_enabled) { morse_playback_stop(); haptic_stop_all(); }
                        nvs_config_mark_dirty();
                        break;
                    case 5: /* Morse Speed (×10ms) */
                        s_dot_ms_val = (uint8_t)(1200u / (g_config.morse_dot_ms ? g_config.morse_dot_ms : 200));
                        window_value_init("Speed (WPM)", sel,
                            &s_dot_ms_val, 30, 4, 1, rows, page);
                        s_popup_pending_dotms = true;   /* V6.14: 延迟保存，等弹窗关闭 */
                        break;
                    case 6: /* Text Scroll */
                        g_config.long_text_scroll_enable = !g_config.long_text_scroll_enable;
                        nvs_config_mark_dirty();
                        break;
                    case 7: /* Factory Reset (V6.19) */
                        {
                            static M_SELECT reset_items[2];
                            static char reset_text[2][16];

                            strcpy(reset_text[0], "Cancel");
                            strcpy(reset_text[1], "Confirm Reset");
                            reset_items[0].m_select = reset_text[0];
                            reset_items[1].m_select = reset_text[1];
                            s_reset_sel_sentinel = 0xFF;
                            s_reset_popup_pending = true;
                            window_list_init("Factory Reset?", reset_items, 2,
                                            &s_reset_sel_sentinel, rows, page);
                        }
                        break;
                    case 8:
                        ui_navigate_to(M_SLEEP_CONFIRM);
                        break;
                    case 9:
                        settings_menu[9].m_select=ui_language_set_chinese(!ui_language_chinese())?"+ Chinese UI":"+ Language SAVE ERROR";
                        break;
                    case 10:
                        ui_style_set((ui_style_skin()+1)%UI_SKINS,ui_style_widget());
                        break;
                    case 11:
                        ui_style_set(ui_style_skin(),(ui_style_widget()+1)%UI_WIDGETS);
                        break;
                    default: break;
                }
                break;
            }

            case BTN_ID_BACK:
                /* Pass through to ui_proc fallback ??return to M_MAIN */
                break;

            default:
                btn.pressed = false;
                break;
        }
    }
}

/* �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
 *  V7.4: Ī˹�ն� ??�����������滻ԭĦ˹ѧϰ����??
 *
 *  ����Ī��˹������/����/��ʾϵͳ�������ع���??
 *  ���� OLED ���֣��ٶ�??+ Ī˹�����ӻ� + ��Ϣ��??
 *  ����: ENC_BTN=Ī˹?? ��ť=��?? KEY=��������??
 * �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T */

void morse_terminal_proc(void)
{
    local_pages_proc();
}

/* ���� ����ҳ�� (?? ���������������������������������������������������������������������������������� */

void about_new_proc(void)
{
    list_show(about_new_menu, M_ABOUT);

    if (btn.pressed) {
        switch (btn.id) {
            case BTN_ID_CW:
            case BTN_ID_CC:
                btn.pressed = false;
                list_rotate_switch();
                break;

            case BTN_ID_BACK:
                /* Pass through to ui_proc fallback ??return to M_MAIN */
                break;

            default:
                btn.pressed = false;
                break;
        }
    }
}

/* �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
 *  ��Ϣ���յ��� (V6.50: ���� WouoUI M_WINDOW ��Ⱦ)
 *
 *  msg_rx_trigger() �ڲ����� window_msg_init() ??����??
 *  WouoUI ���??window_list_show/window_list_proc ͳһ����??
 *  ����������Ϊ�������??M_MSG_RX ö�ٵ���??
 * �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T */

void msg_rx_proc(void)
{
    /* V6.50: msg_rx_trigger �Ѹ�??M_WINDOW ����??
     * �����쳣����˴�����ȫ������һҳ??*/
    if (ui.layer > 0) {
        ui.index = ui.parent_index[ui.layer];
        ui.state = S_LAYER_OUT;
    } else {
        ui.index = M_MAIN;
        ui.state = S_NONE;
    }
}

/**
 * @brief ������Ϣ���� (??ui_task ����)
 *
 * V6.39: ������ն�ȥ????8 ���������ڣ�??(src_id, msg_id) ȥ��??
 */
void msg_rx_trigger(const char *text, uint32_t src_id, bool is_emergency)
{
    LOG_UI_DIAG("ui", "MSG-RX: text='%.16s' src=0x%08lX is_urg=%d ui_index=%d",
             text ? text : "(null)", (unsigned long)src_id, is_emergency ? 1 : 0, ui.index);

    /* V9.6: 已有弹窗时跳过（防止覆盖未读消息），但 "Sent!" 确认弹窗不阻挡 */
    if (ui.index == M_WINDOW && !win.is_sent) {
        LOG_UI_DIAG("ui", "MSG-RX SKIP: already showing received msg");
        return;
    }

    /* V9.6: 基于时间戳的去重（5s 窗口），替代永久去重导致的第二次不弹窗 bug */
    {
        static uint32_t s_rx_dedup_hashes[4] = {0};
        static uint32_t s_rx_dedup_times[4] = {0};
        static uint8_t  s_rx_dedup_idx = 0;
        uint32_t now_sec = (uint32_t)(esp_timer_get_time() / 1000000);

        uint32_t hash = src_id;
        if (text) {
            for (int i = 0; i < 4 && text[i]; i++) {
                hash ^= ((uint32_t)(uint8_t)text[i]) << (i * 8);
            }
        }

        for (int i = 0; i < 4; i++) {
            if (s_rx_dedup_hashes[i] == hash &&
                (now_sec - s_rx_dedup_times[i]) < 5) {
                LOG_UI_DIAG("ui", "MSG-RX DEDUP: hash=0x%08lX (within 5s)", (unsigned long)hash);
                return;
            }
        }
        s_rx_dedup_hashes[s_rx_dedup_idx] = hash;
        s_rx_dedup_times[s_rx_dedup_idx] = now_sec;
        s_rx_dedup_idx = (s_rx_dedup_idx + 1) % 4;
    }

    /* ���� V6.50: ʹ�� WouoUI ԭ����Ϣ���� ���� */
    char title[20];
    if (is_emergency) {
        snprintf(title, sizeof(title), "!! EMERGENCY !!");
    } else {
        const char *sender = nvs_config_get_contact_display_name(src_id);
        if (sender && sender[0]) {
            snprintf(title, sizeof(title), "From %s", sender);
        } else {
            snprintf(title, sizeof(title), "Message");
        }
    }

    window_msg_init(title, text, NULL, ui.index, false);  /* V9.4: is_sent=false → 触发未读呼吸灯 */
    LOG_UI_DIAG("ui", "MSG-RX POPUP: title='%s' text='%.16s'", title, text ? text : "");
    haptic_play(is_emergency ? HAPTIC_EMERGENCY : HAPTIC_MSG_RX);
    rgb_led_trigger(RGB_EVENT_MSG_RECEIVED);  /* V7.0: ���ƿ���һ??*/
}

/* �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
 *  V6.44: �����Ϣ����״????���ú��ѵȴ�������ʽ
 *
 *  ??FRIEND_MGR_WAIT_USER_CONFIRM ����ͳһ??
 *    - RBox + RFrame ����
 *    - "Sending... Xs" ���� + ����??
 *    - �ײ���������5�볬ʱ��
 *    - KEY_BACK �ر� + �Զ�����㲥����
 * �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T */

#define QMSG_SEND_TIMEOUT_MS  5000

void quick_msg_sending_proc(void)
{
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    uint32_t elapsed = (now_ms > s_sending_start_ms)
                       ? (now_ms - s_sending_start_ms) : 0;

    /* V9.0: simplified - show "Sent!" for 1.5s then auto-return */
    int win_x = win.l;
    int win_y = (int16_t)win.y + 2;
    int win_w = WIN_W;
    int win_h = WIN_H - 4;

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawRBox(&u8g2, win_x, win_y, win_w, win_h, 2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_DrawRFrame(&u8g2, win_x, win_y, win_w, win_h, 2);

    u8g2_SetFont(&u8g2, WIN_FONT);
    u8g2_DrawStr(&u8g2, win_x + 5, win_y + 14, "Sent!");

    if (elapsed >= 1500) {
        if (ui.layer > 0) {
            ui.index = ui.parent_index[ui.layer];
            ui.state = S_LAYER_OUT;
        }
    }

    if (btn.pressed && btn.id == BTN_ID_BACK) {
        btn.pressed = false;
        if (ui.layer > 0) {
            ui.index = ui.parent_index[ui.layer];
            ui.state = S_LAYER_OUT;
        }
    }
}

/* �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
 *  Ħ˹�ֶ�����??(V6.9)
 *
 *  KEY (GPIO19) = Ī˹������ס����/�����ɿ�����??
 *  CW/CCW       = ���� WPM (5-40)
 *  ENC_BTN      = �˳���??
 * �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T */

/* �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
 *  V7.0: Ħ˹ CQ �㲥ϵͳ ??����״̬��
 *  ��������Զ�����??00% ռ�ձ�ɨ�裩��ENC_BTN=��������KEY=����
 * �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T */

void morse_input_proc(void)
{
    local_pages_proc();
}

/* �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
 *  [V6.13 ����] Ħ˹��ѧҳ�棨V7.4: ԭ��??morse_learn_unified_proc ���Ƴ���
 *   ��ѧ������ʱ������������뱸��������??
 * �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T */

void morse_tutor_proc(void)
{
    /* V9.0: 教学系统已移除 */
}

/* �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
 *  V6.19: �豸������ҳ ??��ťѡ��??����, �״ο�����??
 *
 *  �ַ�?? A-Z(26) + 0-9(10) + �ո� + �հ�??= 38 ��ѡ��
 *  ����: CW/CCW ѭ���ַ�, SP ȷ�ϡ���һ?? KEY ������һ??
 *  ��??7 ?? ĩλ SP = ���沢��??
 * �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T */

#define NAME_MAX_LEN     7
#define NAME_CHAR_COUNT  38   /* 26 letters + 10 digits + space + blank */

static const char g_name_chars[NAME_CHAR_COUNT] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _";

/* V9.0: Mesh 状态页桩（移除自组网后仅显示 BLE 连接概要） */
void mesh_status_proc(void)
{
    list_show(mesh_status_menu, M_MESH_STATUS);

    if (btn.pressed) {
        switch (btn.id) {
            case BTN_ID_BACK:
                /* Pass through to ui_proc fallback — 返回父页 */
                break;
            default:
                btn.pressed = false;
                break;
        }
    }
}

void name_setup_proc(void)
{
    static char   s_name[NAME_MAX_LEN + 1] = "";
    static int8_t s_cursor = 0;       /* ��ǰ�༭??(0~6) */
    static uint8_t s_char_idx[NAME_MAX_LEN] = {0}; /* ÿλ�ַ�??g_name_chars �е����� */
    static bool   s_initialised = false;
    static bool   s_confirmed = false;
    static uint32_t s_blink_ms = 0;   /* �����˸��ʱ */
    static uint32_t s_confirm_ms = 0; /* ȷ��ʱ�� (����1s�Զ�����) */

    /* �״ν���: ??NVS ������������ */
    if (!s_initialised) {
        s_initialised = true;
        s_confirmed = false;
        s_cursor = 0;
        s_confirm_ms = 0;
        memset(s_name, 0, sizeof(s_name));
        memset(s_char_idx, 0, sizeof(s_char_idx));

        const char *existing = nvs_config_get_device_name();
        if (existing && existing[0]) {
            /* ��������: ��λ�������ַ������� */
            size_t len = strlen(existing);
            if (len > NAME_MAX_LEN) len = NAME_MAX_LEN;
            for (size_t i = 0; i < len; i++) {
                s_name[i] = existing[i];
                /* ���ַ����в�??*/
                bool found = false;
                for (uint8_t j = 0; j < NAME_CHAR_COUNT; j++) {
                    if (g_name_chars[j] == existing[i]) {
                        s_char_idx[i] = j;
                        found = true;
                        break;
                    }
                }
                if (!found) s_char_idx[i] = 0;
            }
            s_name[len] = '\0';
            /* ���ŵ�����ĩβ��������༭ */
            s_cursor = (int8_t)len;
            if (s_cursor >= NAME_MAX_LEN) s_cursor = NAME_MAX_LEN - 1;
        }
    }

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

    /* ���� ���봦�� ������������������������������������������������������������������������ */
    if (btn.pressed && !s_confirmed) {
        switch (btn.id) {
            case BTN_ID_CW:
                btn.pressed = false;
                /* ѭ����һ����??*/
                if (s_char_idx[s_cursor] < NAME_CHAR_COUNT - 1) {
                    s_char_idx[s_cursor]++;
                } else {
                    s_char_idx[s_cursor] = 0;
                }
                s_name[s_cursor] = g_name_chars[s_char_idx[s_cursor]];
                s_name[s_cursor + 1] = '\0';
                break;

            case BTN_ID_CC:
                btn.pressed = false;
                /* ѭ����һ����??*/
                if (s_char_idx[s_cursor] > 0) {
                    s_char_idx[s_cursor]--;
                } else {
                    s_char_idx[s_cursor] = NAME_CHAR_COUNT - 1;
                }
                s_name[s_cursor] = g_name_chars[s_char_idx[s_cursor]];
                s_name[s_cursor + 1] = '\0';
                break;

            case BTN_ID_SP:
                btn.pressed = false;
                /* 首字符是空格时不响应确认（命名允许空格，但首字母不能是空格） */
                if (s_cursor == 0 && s_name[0] == ' ') {
                    haptic_play(HAPTIC_ERROR);
                    break;
                }
                if (s_cursor < NAME_MAX_LEN - 1) {
                    /* �Ƶ���һ??*/
                    s_cursor++;
                } else {
                    /* �������һ?? ȷ�ϱ��� */
                    s_confirmed = true;
                    s_confirm_ms = now_ms;  /* ��¼ȷ��ʱ�� */
                    /* ȥ��β���ո�/�հ�??*/
                    int last = NAME_MAX_LEN - 1;
                    while (last >= 0) {
                        char c = s_name[last];
                        if (c == ' ' || c == '_' || c == '\0') {
                            s_name[last] = '\0';
                            last--;
                        } else {
                            break;
                        }
                    }
                    if (s_name[0] == '\0') {
                        /* ����ȫ��: ��Ĭ���� */
                        strncpy(s_name, "MESH", NAME_MAX_LEN);
                    }
                    nvs_config_set_device_name(s_name);
                    haptic_play(HAPTIC_CONFIRM);
                }
                break;

            case BTN_ID_BACK:
                btn.pressed = false;
                if (s_cursor > 0) {
                    /* ���˵���һ??*/
                    s_cursor--;
                }
                /* �������???? ����??(������?? ������������) */
                break;

            default:
                btn.pressed = false;
                break;
        }
    }

    /* ���� ȷ��??1 ���Զ�������??�������������������������������������� */
    if (s_confirmed) {
        if (now_ms - s_confirm_ms > 1000) {
            /* ���þ�̬״?? ������ҳ (ֱ����ת, ���ı��?? */
            s_initialised = false;
            s_cursor = 0;
            s_confirm_ms = 0;
            strcpy(s_name, nvs_config_get_device_name());
            ui.index = M_HOME;
            ui.state = S_NONE;
            ui.layer = 0;
            memset(ui.select, 0, sizeof(ui.select));
            return;
        }
    }

    /* ���� �����˸ (���༭��) ������������������������������������������������ */
    if (!s_confirmed) {
        if (now_ms - s_blink_ms > 400) {
            s_blink_ms = now_ms;
        }
    }
    bool cursor_visible = ((now_ms - s_blink_ms) < 200);

    /* ���� ��Ⱦ �������������������������������������������������������������������������������� */
    u8g2_SetFont(&u8g2, LIST_FONT);
    u8g2_SetDrawColor(&u8g2, 1);

    if (s_confirmed) {
        /* ȷ��?? ���⸲��??"Saved!" + �ʺ��� */
        u8g2_DrawStr(&u8g2, 2, 8, "Saved!");
        char confirm_text[32];
        snprintf(confirm_text, sizeof(confirm_text), "Hi, %s!", s_name);
        u8g2_DrawStr(&u8g2, 2, 24, confirm_text);
    } else {
        /* �༭?? ���� + ���� + ��� + �ײ���ʾ */
        u8g2_DrawStr(&u8g2, 2, 8, "Name Setup");

        /* ������ʾ??(y=20, 7�ַ� �� 10px���, �ײ���ռ����ʾ) */
        int name_x = 28;
        int name_y = 20;
        for (int i = 0; i < NAME_MAX_LEN; i++) {
            char ch = s_name[i];
            if (ch == '\0') ch = '_';  /* 空位显示下划线占位，空格显示为真实空隙 */
            char buf[2] = {ch, '\0'};
            u8g2_DrawStr(&u8g2, name_x + i * 10, name_y, buf);

            /* ��ǰ���?? ��˸�»�??*/
            if (i == s_cursor && cursor_visible) {
                u8g2_DrawHLine(&u8g2, name_x + i * 10, name_y + 2, 8);
            }
        }

        /* �ײ���ʾ (y=31, ��Ļ�ײ�) */
        if (s_cursor < NAME_MAX_LEN - 1) {
            u8g2_DrawStr(&u8g2, 2, 31, "YOUR PAGER NAME");
        } else {
            u8g2_DrawStr(&u8g2, 2, 31, "SAVE THIS NAME");
        }
    }

    /* �ڰ�ģʽ���� */
}

/* �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
 *  [V6.13 ����׮] ���º����Ѻϲ���ϳ�, �����׮
 *  ʵ�ʹ�����Ǩ����:
 *    quick_msg_send ??window_list_init()
 *    morse_proc ??�ѷ�??
 *    morse_learn_proc ??�ѷϳ���V7.4: �滻??morse_terminal_proc??
 *    morse_decode_proc/morse_lesson_list_proc ??���յ�ͳһ??
 *    friend_add_proc ??friends_proc ��������
 *    settings_led/morse/power ??settings_proc �ϲ�
 * �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T */

void quick_msg_send_proc(void) {}

/* ── V10.5: 子菜单已废弃 — 磁贴1直达 M_MORSE_LEARN ─────────────── */
void morse_submenu_proc(void)
{
    /* V10.5: CQ Broadcast 入口已移除，此函数保留仅用于编译兼容。
     * 若意外进入则直接回退到 Learn 页面。 */
    ui_navigate_to(M_MORSE_LEARN);
}

void morse_proc(void) {}
void morse_learn_proc(void) {}
void morse_decode_proc(void) {}
void morse_lesson_list_proc(void) {}
void friend_add_proc(void) {}
void settings_led_proc(void) {}
void settings_morse_proc(void) {}
void settings_power_proc(void) {}

/* ============================================================================
 *  Mini games (V10.6): Answer Book + Dice
 *
 *  Shared non-blocking state machine (proc runs once per UI frame):
 *    IDLE    : show prompt / last result; SP starts a roll
 *    ROLLING : esp_timer ms-timing drives rapid random flips (TICK haptic
 *              cadence + RGB_LED_EVENT_GAME_ROLL loop), no vTaskDelay
 *    roll timer expires -> freeze on pre-drawn result, HAPTIC_REVEAL
 *              + RGB_LED_EVENT_GAME_RESULT (HIGH prio, replaces ROLL)
 *  BACK is always consumed: rgb_led_stop() (ROLL may still be looping),
 *  then explicit pop to parent page.
 * ============================================================================ */

#define GAME_PHASE_IDLE     0
#define GAME_PHASE_ROLLING  1

/* ---- Answer Book -------------------------------------------------------- */

#define ANSWER_ROLL_MS   1500   /* total flip animation time */
#define ANSWER_FLIP_MS   70     /* per-answer display slice (~21 flips) */
#define ANSWER_FONT      u8g2_font_helvB12_tr

/* 40 classic answer-book phrases, all <= 12 ASCII chars (128px width safe) */
static const char *const g_answers[] = {
    "YES",          "NO",           "MAYBE",        "ASK AGAIN",
    "GO FOR IT",    "JUST DO IT",   "WAIT A BIT",   "NOT NOW",
    "BE PATIENT",   "LET IT GO",    "DON'T RUSH",   "THINK TWICE",
    "FOLLOW HEART", "ASK A FRIEND", "SLEEP ON IT",  "TAKE A BREAK",
    "CARPE DIEM",   "WHY NOT",      "SURE THING",   "ABSOLUTELY",
    "NEVER MIND",   "FORGET IT",    "TRY LATER",    "GIVE IT TIME",
    "STAY CALM",    "KEEP GOING",   "HOLD ON",      "TRUST FATE",
    "ROLL WITH IT", "BIG YES",      "BIG NO",       "100% YES",
    "RISKY MOVE",   "PLAY IT SAFE", "CHANGE PLANS", "FOCUS FIRST",
    "STEP BY STEP", "DO IT TODAY",  "TOMORROW",     "LUCK IS NEAR",
};
#define ANSWER_COUNT  (sizeof(g_answers) / sizeof(g_answers[0]))

void answer_book_proc(void)
{
    static uint32_t s_entry_epoch = UINT32_MAX;   /* entry-detect: page seen last frame */
    static uint8_t  s_phase = GAME_PHASE_IDLE;
    static uint8_t  s_shown = 0;           /* answer index on screen */
    static uint8_t  s_final = 0;           /* pre-drawn final answer index */
    static bool     s_has_answer = false;  /* a roll has completed */
    static uint32_t s_roll_start = 0;
    static uint32_t s_next_flip = 0;

    /* first frame after entering this page: reset game state.
     * rgb_led_stop() covers the case where a msg popup (M_WINDOW) stole the
     * page mid-roll while the ROLL loop was still running. */
    if (s_entry_epoch != s_game_entry_epoch) {
        s_entry_epoch = s_game_entry_epoch;
        s_phase      = GAME_PHASE_IDLE;
        s_has_answer = false;
        s_shown      = 0;
        s_final      = 0;
        rgb_led_stop();
    }

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

    if (btn.pressed) {
        switch (btn.id) {
            case BTN_ID_SP:
                btn.pressed = false;
                if (s_phase == GAME_PHASE_IDLE) {
                    s_phase      = GAME_PHASE_ROLLING;
                    s_final      = (uint8_t)(esp_random() % ANSWER_COUNT);
                    s_roll_start = now_ms;
                    s_next_flip  = now_ms;
                    rgb_led_trigger(RGB_LED_EVENT_GAME_ROLL);
                }
                break;

            case BTN_ID_BACK:
                /* consumed: stop possibly-looping ROLL effect, then pop */
                btn.pressed = false;
                rgb_led_stop();
                haptic_play(HAPTIC_BACK);
                ui.index = ui.parent_index[ui.layer];
                ui.state = S_LAYER_OUT;
                return;

            default:
                btn.pressed = false;
                break;
        }
    }

    /* rolling animation driver */
    if (s_phase == GAME_PHASE_ROLLING) {
        if (now_ms - s_roll_start >= ANSWER_ROLL_MS) {
            s_phase      = GAME_PHASE_IDLE;
            s_shown      = s_final;
            s_has_answer = true;
            haptic_play(HAPTIC_REVEAL);
            rgb_led_trigger(RGB_LED_EVENT_GAME_RESULT);
        } else if ((int32_t)(now_ms - s_next_flip) >= 0) {
            s_next_flip = now_ms + ANSWER_FLIP_MS;
            s_shown     = (uint8_t)(esp_random() % ANSWER_COUNT);
            haptic_play(HAPTIC_TICK);
        }
    }

    /* render */
    u8g2_SetDrawColor(&u8g2, 1);

    if (s_phase == GAME_PHASE_IDLE && !s_has_answer) {
        /* entry prompt */
        u8g2_SetFont(&u8g2, LIST_FONT);
        const char *l1 = "The Book of Answers";
        const char *l2 = "Hold a question...";
        u8g2_DrawStr(&u8g2, (DISP_W - u8g2_GetStrWidth(&u8g2, l1)) / 2, 12, l1);
        u8g2_DrawStr(&u8g2, (DISP_W - u8g2_GetStrWidth(&u8g2, l2)) / 2, 26, l2);
    } else {
        const char *txt = g_answers[s_shown];
        if(ui_language_chinese()){
            ui_zh_draw(&u8g2,2,11,"YOUR ANSWER");
            ui_zh_draw_fit(&u8g2,(DISP_W-ui_zh_width(&u8g2,txt))/2,27,txt,124);return;
        }
        u8g2_SetFont(&u8g2, ANSWER_FONT);
        int16_t w = u8g2_GetStrWidth(&u8g2, txt);
        if (w > DISP_W - 8) {           /* safety fallback for long strings */
            u8g2_SetFont(&u8g2, LIST_FONT);
            w = u8g2_GetStrWidth(&u8g2, txt);
        }
        u8g2_DrawStr(&u8g2, (DISP_W - w) / 2, 21, txt);
        if (s_phase == GAME_PHASE_IDLE) {
            /* settled: hint for another flip */
            u8g2_SetFont(&u8g2, LIST_FONT);
            u8g2_DrawStr(&u8g2, 2, 31, "YOUR ANSWER");
        }
    }
}

/* ---- Dice --------------------------------------------------------------- */

#define DICE_ROLL_MS    1000    /* total tumble animation time */
#define DICE_FLIP_MS    80      /* per-face display slice (~12 tumbles) */
#define DICE_BOX_W      28
#define DICE_BOX_X      ((DISP_W - DICE_BOX_W) / 2)
#define DICE_BOX_Y      2
#define DICE_PIP_R      3

/* pip offsets inside the 28x28 box (center offset 13 ~= 13.5) */
#define DICE_OFF_EDGE   7
#define DICE_OFF_MID    13
#define DICE_OFF_FAR    20

/** draw one die face (1-6) centered on screen; 0 = unrolled "?" placeholder */
static void dice_draw_face(uint8_t face)
{
    const int x0 = DICE_BOX_X;
    const int y0 = DICE_BOX_Y;

    u8g2_DrawRFrame(&u8g2, x0, y0, DICE_BOX_W, DICE_BOX_W, 3);

    if (face == 0) {
        u8g2_SetFont(&u8g2, ANSWER_FONT);
        u8g2_DrawStr(&u8g2, x0 + 10, y0 + 20, "?");
        return;
    }

    const int tl = x0 + DICE_OFF_EDGE, tm = y0 + DICE_OFF_EDGE;
    const int cm = x0 + DICE_OFF_MID,  mm = y0 + DICE_OFF_MID;
    const int br = x0 + DICE_OFF_FAR,  bm = y0 + DICE_OFF_FAR;

    /* pip layout: bit0 TL, bit1 TR, bit2 ML, bit3 MR, bit4 BL, bit5 BR, bit6 C */
    static const uint8_t face_pips[7] = {
        0x00,                                  /* 0: unused */
        0x40,                                  /* 1: C */
        0x21,                                  /* 2: TL+BR */
        0x61,                                  /* 3: TL+BR+C */
        0x33,                                  /* 4: TL+TR+BL+BR */
        0x73,                                  /* 5: 4 corners + C */
        0x3F,                                  /* 6: TL+TR+ML+MR+BL+BR */
    };
    uint8_t p = face_pips[face];

    if (p & 0x01) u8g2_DrawDisc(&u8g2, tl, tm, DICE_PIP_R, U8G2_DRAW_ALL);
    if (p & 0x02) u8g2_DrawDisc(&u8g2, br, tm, DICE_PIP_R, U8G2_DRAW_ALL);
    if (p & 0x04) u8g2_DrawDisc(&u8g2, tl, mm, DICE_PIP_R, U8G2_DRAW_ALL);
    if (p & 0x08) u8g2_DrawDisc(&u8g2, br, mm, DICE_PIP_R, U8G2_DRAW_ALL);
    if (p & 0x10) u8g2_DrawDisc(&u8g2, tl, bm, DICE_PIP_R, U8G2_DRAW_ALL);
    if (p & 0x20) u8g2_DrawDisc(&u8g2, br, bm, DICE_PIP_R, U8G2_DRAW_ALL);
    if (p & 0x40) u8g2_DrawDisc(&u8g2, cm, mm, DICE_PIP_R, U8G2_DRAW_ALL);
}

void dice_proc(void)
{
    static uint32_t s_entry_epoch = UINT32_MAX;
    static uint8_t  s_phase = GAME_PHASE_IDLE;
    static uint8_t  s_shown = 0;           /* face on screen (0 = "?") */
    static uint8_t  s_final = 0;           /* pre-drawn final face 1-6 */
    static bool     s_has_result = false;
    static uint32_t s_roll_start = 0;
    static uint32_t s_next_flip = 0;

    if (s_entry_epoch != s_game_entry_epoch) {
        s_entry_epoch = s_game_entry_epoch;
        s_phase      = GAME_PHASE_IDLE;
        s_has_result = false;
        s_shown      = 0;
        s_final      = 0;
        rgb_led_stop();   /* cover popup-interrupted roll (see answer_book) */
    }

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

    if (btn.pressed) {
        switch (btn.id) {
            case BTN_ID_SP:
                btn.pressed = false;
                if (s_phase == GAME_PHASE_IDLE) {
                    s_phase      = GAME_PHASE_ROLLING;
                    s_final      = (uint8_t)(esp_random() % 6) + 1;
                    s_roll_start = now_ms;
                    s_next_flip  = now_ms;
                    rgb_led_trigger(RGB_LED_EVENT_GAME_ROLL);
                }
                break;

            case BTN_ID_BACK:
                btn.pressed = false;
                rgb_led_stop();
                haptic_play(HAPTIC_BACK);
                ui.index = ui.parent_index[ui.layer];
                ui.state = S_LAYER_OUT;
                return;

            default:
                btn.pressed = false;
                break;
        }
    }

    if (s_phase == GAME_PHASE_ROLLING) {
        if (now_ms - s_roll_start >= DICE_ROLL_MS) {
            s_phase      = GAME_PHASE_IDLE;
            s_shown      = s_final;
            s_has_result = true;
            haptic_play(HAPTIC_REVEAL);
            rgb_led_trigger(RGB_LED_EVENT_GAME_RESULT);
        } else if ((int32_t)(now_ms - s_next_flip) >= 0) {
            s_next_flip = now_ms + DICE_FLIP_MS;
            s_shown     = (uint8_t)(esp_random() % 6) + 1;
            haptic_play(HAPTIC_TICK);
        }
    }

    /* render */
    u8g2_SetDrawColor(&u8g2, 1);
    dice_draw_face(s_shown);

    if (s_phase == GAME_PHASE_IDLE) {
        if (!s_has_result) {
            /* entry prompt, left of the die */
            u8g2_SetFont(&u8g2, LIST_FONT);
            u8g2_DrawStr(&u8g2, 4, 13, "LUCK");
            u8g2_DrawStr(&u8g2, 4, 25, "DICE");
        } else {
            /* big result digit on the left, re-roll hint on the right */
            char num[2] = { (char)('0' + s_shown), '\0' };
            u8g2_SetFont(&u8g2, VOLT_FONT);
            int16_t w = u8g2_GetStrWidth(&u8g2, num);
            u8g2_DrawStr(&u8g2, (DICE_BOX_X - w) / 2, 26, num);
            u8g2_SetFont(&u8g2, LIST_FONT);
            u8g2_DrawStr(&u8g2, DICE_BOX_X + DICE_BOX_W + 6, 13, "LUCK");
            u8g2_DrawStr(&u8g2, DICE_BOX_X + DICE_BOX_W + 6, 25, "1 / 6");
        }
    }
}

/* ---- Craps (Pass Line, no wagering) ------------------------------------ */

#define CRAPS_DIE_W       24
#define CRAPS_DIE_Y       3
#define CRAPS_DIE1_X      2
#define CRAPS_DIE2_X      30

enum {
    CRAPS_READY = 0,
    CRAPS_POINT,
    CRAPS_WIN,
    CRAPS_LOSE,
};

static void craps_draw_die(int x0, uint8_t face)
{
    static const uint8_t face_pips[7] = {
        0x00, 0x40, 0x21, 0x61, 0x33, 0x73, 0x3F,
    };
    const int a = 6, m = 12, b = 18;

    u8g2_DrawRFrame(&u8g2, x0, CRAPS_DIE_Y, CRAPS_DIE_W, CRAPS_DIE_W, 3);
    if (face < 1 || face > 6) return;

    uint8_t p = face_pips[face];
    if (p & 0x01) u8g2_DrawDisc(&u8g2, x0 + a, CRAPS_DIE_Y + a, 2, U8G2_DRAW_ALL);
    if (p & 0x02) u8g2_DrawDisc(&u8g2, x0 + b, CRAPS_DIE_Y + a, 2, U8G2_DRAW_ALL);
    if (p & 0x04) u8g2_DrawDisc(&u8g2, x0 + a, CRAPS_DIE_Y + m, 2, U8G2_DRAW_ALL);
    if (p & 0x08) u8g2_DrawDisc(&u8g2, x0 + b, CRAPS_DIE_Y + m, 2, U8G2_DRAW_ALL);
    if (p & 0x10) u8g2_DrawDisc(&u8g2, x0 + a, CRAPS_DIE_Y + b, 2, U8G2_DRAW_ALL);
    if (p & 0x20) u8g2_DrawDisc(&u8g2, x0 + b, CRAPS_DIE_Y + b, 2, U8G2_DRAW_ALL);
    if (p & 0x40) u8g2_DrawDisc(&u8g2, x0 + m, CRAPS_DIE_Y + m, 2, U8G2_DRAW_ALL);
}

void craps_proc(void)
{
    static uint32_t s_entry_epoch = UINT32_MAX;
    static uint8_t s_phase = GAME_PHASE_IDLE;
    static uint8_t s_die_a = 1, s_die_b = 1;
    static uint8_t s_final_a = 1, s_final_b = 1;
    static uint8_t s_point = 0;
    static uint8_t s_status = CRAPS_READY;
    static uint32_t s_roll_start = 0;
    static uint32_t s_next_flip = 0;

    if (s_entry_epoch != s_game_entry_epoch) {
        s_entry_epoch = s_game_entry_epoch;
        s_phase = GAME_PHASE_IDLE;
        s_die_a = s_die_b = 1;
        s_point = 0;
        s_status = CRAPS_READY;
        rgb_led_stop();
    }

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

    if (btn.pressed) {
        switch (btn.id) {
            case BTN_ID_SP:
                btn.pressed = false;
                if (s_phase == GAME_PHASE_IDLE) {
                    if (s_status == CRAPS_WIN || s_status == CRAPS_LOSE) {
                        s_point = 0;
                        s_status = CRAPS_READY;
                    }
                    s_phase = GAME_PHASE_ROLLING;
                    s_final_a = (uint8_t)(esp_random() % 6) + 1;
                    s_final_b = (uint8_t)(esp_random() % 6) + 1;
                    s_roll_start = now_ms;
                    s_next_flip = now_ms;
                    rgb_led_trigger(RGB_LED_EVENT_GAME_ROLL);
                }
                break;

            case BTN_ID_BACK:
                btn.pressed = false;
                rgb_led_stop();
                haptic_play(HAPTIC_BACK);
                ui.index = ui.parent_index[ui.layer];
                ui.state = S_LAYER_OUT;
                return;

            default:
                btn.pressed = false;
                break;
        }
    }

    if (s_phase == GAME_PHASE_ROLLING) {
        if (now_ms - s_roll_start >= DICE_ROLL_MS) {
            s_phase = GAME_PHASE_IDLE;
            s_die_a = s_final_a;
            s_die_b = s_final_b;
            uint8_t sum = s_die_a + s_die_b;

            if (s_point == 0) {
                if (sum == 7 || sum == 11) s_status = CRAPS_WIN;
                else if (sum == 2 || sum == 3 || sum == 12) s_status = CRAPS_LOSE;
                else {
                    s_point = sum;
                    s_status = CRAPS_POINT;
                }
            } else if (sum == s_point) {
                s_status = CRAPS_WIN;
            } else if (sum == 7) {
                s_status = CRAPS_LOSE;
            } else {
                s_status = CRAPS_POINT;
            }

            haptic_play(HAPTIC_REVEAL);
            rgb_led_trigger(RGB_LED_EVENT_GAME_RESULT);
        } else if ((int32_t)(now_ms - s_next_flip) >= 0) {
            s_next_flip = now_ms + DICE_FLIP_MS;
            s_die_a = (uint8_t)(esp_random() % 6) + 1;
            s_die_b = (uint8_t)(esp_random() % 6) + 1;
            haptic_play(HAPTIC_TICK);
        }
    }

    u8g2_SetDrawColor(&u8g2, 1);
    craps_draw_die(CRAPS_DIE1_X, s_die_a);
    craps_draw_die(CRAPS_DIE2_X, s_die_b);

    u8g2_SetFont(&u8g2, LIST_FONT);
    if (s_phase == GAME_PHASE_ROLLING) {
        u8g2_DrawStr(&u8g2, 61, 12, "ROLLING");
        u8g2_DrawStr(&u8g2, 61, 25, s_point ? "HIT POINT" : "COME OUT");
    } else if (s_status == CRAPS_WIN) {
        u8g2_DrawStr(&u8g2, 61, 12, "YOU WIN");
        u8g2_DrawStr(&u8g2, 61, 25, "SP: AGAIN");
    } else if (s_status == CRAPS_LOSE) {
        u8g2_DrawStr(&u8g2, 61, 12, "YOU LOSE");
        u8g2_DrawStr(&u8g2, 61, 25, "SP: AGAIN");
    } else if (s_status == CRAPS_POINT) {
        char point_text[12];
        snprintf(point_text, sizeof(point_text), ui_zh_lookup("POINT: %u"), s_point);
        u8g2_DrawStr(&u8g2, 61, 12, point_text);
        u8g2_DrawStr(&u8g2, 61, 25, "SP: ROLL");
    } else {
        u8g2_DrawStr(&u8g2, 61, 12, "CRAPS");
        u8g2_DrawStr(&u8g2, 61, 25, "SP: START");
    }
}

/* �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
 *  ������ģʽҳ??(V6.12, ��������)
 * �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T */

#if LOG_DEV_MODE_ENABLE

void dev_mode_page_proc(void)
{
    static uint8_t    s_dev_sub_sel = 0;

    /* ���� Dev Mode ����??(WouoUI list_show) ���������������� */
    static M_SELECT dev_main_menu[] = {
        {"- HW Test"},
        {"- P1: GPIO/Power"},
        {"- P2: Mesh Stats"},
        {"- P3: UI Diag"},
        {"- P4: GATT Diag"},
        {"- P5: OTA"},
        {"- P6: Friends"},
        {"- Sys Diag"},
    };
    ui.num[M_DEV_MODE] = 8;

    if (dev_mode_get_page() == DEV_PAGE_MAIN) {
        list_show(dev_main_menu, M_DEV_MODE);

        if (btn.pressed) {
            switch (btn.id) {
                case BTN_ID_CW:
                case BTN_ID_CC:
                    btn.pressed = false;
                    list_rotate_switch();
                    break;

                case BTN_ID_SP:
                    btn.pressed = false;
                    s_dev_sub_sel = ui.select[ui.layer];
                    switch (s_dev_sub_sel) {
                        case 0: dev_mode_set_page(DEV_PAGE_HW_TEST);
                                dev_mode_enter_hw_test();
                                break;
                        case 1: dev_mode_set_page(DEV_PAGE_PHASE1);   break;
                        case 2: dev_mode_set_page(DEV_PAGE_PHASE2);   break;
                        case 3: dev_mode_set_page(DEV_PAGE_PHASE3);   break;
                        case 4: dev_mode_set_page(DEV_PAGE_PHASE4);   break;
                        case 5: dev_mode_set_page(DEV_PAGE_PHASE5);   break;
                        case 6: dev_mode_set_page(DEV_PAGE_PHASE6);   break;
                        case 7: dev_mode_set_page(DEV_PAGE_SYS_DIAG); break;
                        default: break;
                    }
                    break;

                case BTN_ID_BACK:
                    /* ��??Dev Mode ??���� M_MAIN */
                    btn.pressed = false;
                    dev_mode_set_page(DEV_PAGE_MAIN);
                    ui.index = ui.parent_index[ui.layer];
                    ui.state = S_LAYER_OUT;
                    return;

                default:
                    btn.pressed = false;
                    break;
            }
        }
    } else {
        /* ���� Dev Mode ��ҳ?? ʹ�������Ի���Ⱦ ���������������� */
        dev_mode_render(dev_mode_get_page());

        /* ������ҳ����??(ͨ�� ec06 ԭʼ�¼�, SP ��������) */
        ec06_event_t ev;
        while ((ev = ec06_pop_event()).type != EC06_EVENT_NONE) {
            dev_mode_handle_input(dev_mode_get_page(), &ev);`r`n        }`r`n#endif

        /* V6.15: BACK ??ui_proc ���˴�����ͳһ�ӹ�, �˴�����??*/
    }


/* ══════════════════════════════════════════════════════════════
 *  [V6.13 废弃桩] 以下函数已合并或废除, 保留空桩
 * ══════════════════════════════════════════════════════════════ */

void quick_msg_send_proc(void) {}
void morse_proc(void) {}
void morse_learn_proc(void) {}
void morse_decode_proc(void) {}
void morse_lesson_list_proc(void) {}
void friend_add_proc(void) {}
void settings_led_proc(void) {}
void settings_morse_proc(void) {}
void settings_power_proc(void) {}
#endif /* LOG_DEV_MODE_ENABLE */
