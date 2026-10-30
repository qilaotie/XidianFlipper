/* Local product pages. Input, transport and playback retain separate owners. */
#include "local_pages.h"
#include "wououi.h"
#include "menu_layout.h"
#include "wireless_tools.h"
#include "message_draft.h"
#include "message_emotes.h"
#include "draft_store.h"
#include "nearby_games.h"
#include "tetris_page.h"
#include "snake_page.h"
#include "pet_page.h"
#include "pet_service.h"
#include "gift_runtime.h"
#include "party_pages.h"
#include "ec06.h"
#include "message_service.h"
#include "friend_mgr.h"
#include "morse_terminal.h"
#include "morse_input.h"
#include "morse_playback.h"
#include "morse_training_ui.h"
#include "morse_codec.h"
#include "quick_msg.h"
#include "nvs_config.h"
#include "power_mgmt.h"
#include "haptic_feedback.h"
#include "mesh_notify.h"
#include "rgb_led.h"
#include "mcpwm_buzzer.h"
#include "esp_timer.h"
#include "ui_zh.h"
#include "ui_language.h"
#include "esp_random.h"
#include "pet_service.h"
#include <stdio.h>
#include <string.h>

#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static uint8_t s_last_page = 255;
static char s_practice[MESSAGE_TEXT_MAX+1];
static uint8_t s_destination[6] = MESH_DST_BROADCAST_MAC;
static bool s_private;
static char s_recipient[FRIEND_NAME_MAX+1];
static uint32_t s_tx_id;
static message_inbox_entry_t s_message;
static friend_record_t s_friend;
static char s_notice[96];
static int64_t s_notice_until;
static uint8_t s_channel_value = 1;
static bool s_channel_pending;
static uint32_t s_channel_errors;
static int64_t s_channel_deadline;
static message_kind_t s_send_kind = MESSAGE_KIND_MORSE;
static char s_send_text[MESSAGE_TEXT_MAX+1];
static char s_draft[MESSAGE_TEXT_MAX+1];
/* Form fields never share persistent message text or a send action. */
static char s_form_text[MESSAGE_TEXT_MAX+1],s_field_edit[MESSAGE_TEXT_MAX+1];
static char *s_field_target;
static size_t s_field_capacity;
static draft_selector_t s_field_selector;
static draft_selector_t s_draft_selector={.selecting_group=true};
static bool s_delete_pending;
static bool s_draft_loaded,s_box_saving,s_draft_list_enter;
static unsigned s_draft_slot;
static unsigned s_emote_group;
static unsigned s_network_page;
static void extras_pages_enter(unsigned page);
static unsigned s_topology_mode,s_topology_pick;
enum {NET_GROUP,NET_ANNOUNCEMENT,NET_OFFLINE,NET_CUSTODY};
static unsigned s_net_mode,s_outbox_slot;
static uint8_t s_offline_dst[6];
static unsigned s_message_filter,s_team_options=2,s_team_action,s_team_task;
static uint8_t s_filter_peer[6],s_reply_peer[6],s_reply_channel;
static bool s_reply;
static char s_team_text[49];
static unsigned s_crowd_kind,s_crowd_action,s_activity_index;
static activity_record_t s_activity_detail;
static char s_crowd_title[33];
static unsigned s_social_section,s_social_action,s_relay_slot,s_card_peer=8,s_stamp_slot;
static unsigned s_music_role,s_music_tune,s_music_bpm=120;
static bool s_stamp_detail;
static uint32_t s_music_sequence,s_music_token;
static unsigned s_music_last_beat;
enum {DRAFT_CONFIRM_OVERWRITE,DRAFT_CONFIRM_DELETE,DRAFT_CONFIRM_NEW,DRAFT_CONFIRM_LOAD};
static unsigned s_draft_confirm;

static void load_draft(void){
    if(!s_draft_loaded){snprintf(s_draft,sizeof(s_draft),"%s",draft_store_work());s_draft_loaded=true;}
}
static void open_draft_editor(void){
    /* Reuse an existing composer ancestor instead of growing navigation depth. */
    for(unsigned layer=ui.layer;layer>0;layer--)if(ui.parent_index[layer]==M_MESSAGE_COMPOSE){
        ui.layer=layer;ui.index=M_MESSAGE_COMPOSE;ui.state=S_LAYER_OUT;return;
    }
    ui_navigate_to(M_MESSAGE_COMPOSE);
}
static void open_content_editor(char *target,size_t capacity){
    s_field_target=target;s_field_capacity=capacity;
    snprintf(s_field_edit,capacity,"%s",target);
    s_field_selector=(draft_selector_t){.selecting_group=true};
    ui_navigate_to(M_CONTENT_EDIT);
}

static void back(void)
{
    btn.pressed = false;
    if (ui.layer) {
        ui.index = ui.parent_index[ui.layer];
        ui.state = S_LAYER_OUT;
        haptic_play(HAPTIC_BACK);
    }
}

static void notice(const char *text)
{
    snprintf(s_notice,sizeof(s_notice),"%s",text);
    s_notice_until=esp_timer_get_time()+1600000;
    haptic_play(HAPTIC_BUMP);
}

static void text_line(int y,const char *text)
{
    u8g2_SetDrawColor(&u8g2,1);
    u8g2_SetFont(&u8g2,u8g2_font_6x10_tf);
    if(ui_language_chinese() && ui.index!=M_MESSAGE_VIEW && ui.index!=M_MESSAGE_COMPOSE && ui.index!=M_CONTENT_EDIT){
        ui_zh_draw_fit(&u8g2,2,y<=12?11:27,text,124);
    }else u8g2_DrawStr(&u8g2,2,y,text);
}

static void footer(const char *text)
{
    if(ui_language_chinese() && ui.index!=M_MESSAGE_VIEW && ui.index!=M_MESSAGE_COMPOSE && ui.index!=M_CONTENT_EDIT){
        if((esp_timer_get_time()/3000000)%2){
            u8g2_SetDrawColor(&u8g2,0);u8g2_DrawBox(&u8g2,0,15,128,17);u8g2_SetDrawColor(&u8g2,1);
            ui_zh_draw_fit(&u8g2,2,27,text,124);
        }
        return;
    }
    u8g2_SetDrawColor(&u8g2,0);
    u8g2_DrawBox(&u8g2,0,24,128,8);
    u8g2_SetDrawColor(&u8g2,1);
    u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    u8g2_DrawStr(&u8g2,2,31,text);
}

/* Returns an activated row, or -1. BACK stays available to the global fallback. */
static int menu(M_SELECT *items,unsigned count)
{
    if (!count) return -1;
    ui.num[ui.index]=(uint8_t)count;
    if (ui.select[ui.layer]>=count) ui.select[ui.layer]=(uint8_t)(count-1);
    if(s_draft_list_enter){
        s_draft_list_enter=false;list_update_scroll_targets();list.y=list.y_trg;
        list.box_y=list.box_y_trg[ui.layer];list.loop=false;ui.init=true;
    }
    list_show(items,ui.index);
    if (!btn.pressed) return -1;
    if (btn.id==BTN_ID_CW || btn.id==BTN_ID_CC) {
        list_rotate_switch();
        btn.pressed=false;
    } else if (btn.id==BTN_ID_SP) {
        btn.pressed=false;
        return ui.select[ui.layer];
    } else if (btn.id!=BTN_ID_BACK) btn.pressed=false;
    return -1;
}

bool local_pages_uses_raw_input(uint8_t page)
{
    return page==M_MORSE_LEARN || page==M_MORSE_INPUT || page==M_TRAINING || page==M_REACTION ||
        page==M_NUMBER_BOMB || page==M_TIME_GUESS;
}

void local_pages_render_waking(void)
{
    text_line(20,"Ready - release key");
}

static bool notifications_allowed(void)
{
    return !local_pages_uses_raw_input(ui.index) && !morse_playback_is_active();
}

static void on_message(const message_inbox_entry_t *entry)
{
    (void)entry;
    /* The durable inbox is the source of truth; an arrival never steals input. */
    rgb_led_trigger(RGB_EVENT_MSG_RECEIVED);
    haptic_play(HAPTIC_MSG_RX);
}

void local_pages_init(void)
{
    s_last_page=255;
    s_practice[0]='\0';
    ui.num[M_MAIN]=8;
    mesh_notify_set_handlers(notifications_allowed,on_message);
}

void local_pages_open_channel(void)
{
    memset(s_destination,0xff,sizeof(s_destination));
    s_private=false;
    s_recipient[0]='\0';
    ui_navigate_to(M_CHANNEL_HUB);
}

void local_pages_sync_page(void)
{
    uint8_t page=ui.index;
    if (page==s_last_page) return;
    if (local_pages_uses_raw_input(s_last_page)) power_mgmt_guard_input();
    if (s_last_page==M_MORSE_INPUT || s_last_page==M_MORSE_LEARN) {
        if (s_last_page==M_MORSE_LEARN)
            snprintf(s_practice,sizeof(s_practice),"%s",morse_terminal_get_message());
        morse_terminal_exit();
    }
    if (s_last_page==M_TRAINING) morse_training_ui_exit();
    if (s_last_page==M_MESSAGE_VIEW || s_last_page==M_CHANNEL_HUB) morse_playback_stop();
    if (s_last_page==M_FRIEND_PAIR) friend_mgr_exit_add_mode();
    if(s_last_page==M_ENSEMBLE){message_service_ensemble_arm(false,s_music_role);buzzer_sequence_cancel(s_music_token);}
    if(page==M_ENSEMBLE)message_service_ensemble_arm(true,s_music_role);
    if(page==M_PASSPORT)s_stamp_detail=false;
    if((page==M_POLL_CREATE || page==M_TASK_CREATE || page==M_CROWD_CREATE) && s_last_page!=M_CONTENT_EDIT && s_last_page!=M_TEAM_CONFIRM && s_last_page!=M_CROWD_CONFIRM)s_form_text[0]=0;
    s_last_page=page;
    if(page==M_NETWORK_STATUS)s_network_page=0;
    extras_pages_enter(page);
    if(page==M_NETWORK_TOPOLOGY)s_topology_mode=s_topology_pick=0;
    s_draft_list_enter=page==M_DRAFT_BOX || page==M_DRAFT_SLOT || page==M_DRAFT_CONFIRM || page==M_MESSAGE_EMOTES || page==M_MESSAGE_EMOTE_LIST ||
        page==M_NET_SEND || page==M_OFFLINE_RECIPIENT || page==M_NET_CONFIRM || page==M_OUTBOX || page==M_OUTBOX_ITEM || page==M_OUTBOX_DELETE ||
        (page>=M_MESSAGE_FILTERS && page<=M_ENSEMBLE);
    menu_pages_on_enter(page);
    { extern void game_help_page_enter(unsigned); game_help_page_enter(page); }
    tetris_page_enter(page);
    snake_page_enter(page);
    pet_page_enter(page);
    wireless_tools_on_enter(page);
    party_pages_enter(page);
    nearby_games_enter(page);
    if (page==M_MORSE_INPUT || page==M_MORSE_LEARN) {
        morse_playback_stop();
        morse_terminal_init();
        morse_terminal_set_practice_mode(page==M_MORSE_LEARN);
        if (page==M_MORSE_INPUT) morse_terminal_set_live_mode(true);
        else morse_terminal_set_message(s_practice);
    }
    if (page==M_CHANNEL_SELECT) {
        s_channel_value=message_service_get_channel();
        s_channel_pending=false;
    }
    if (page==M_FRIEND_DETAIL) s_delete_pending=false;
}

static void terminal_page(void)
{
    g_morse_key_active=true;
    ec06_event_t ev;
    while ((ev=ec06_pop_event()).type!=EC06_EVENT_NONE) {
        if (ev.type==EC06_EVENT_KEY_DOWN) { back(); return; }
        if (ev.type==EC06_EVENT_CW) morse_terminal_feed_rotate(1);
        if (ev.type==EC06_EVENT_CCW) morse_terminal_feed_rotate(-1);
    }
    btn.pressed=false;
    morse_terminal_tick();
    morse_terminal_render();
}

static void training_page(void)
{
    g_morse_key_active=true;
    ec06_event_t ev;
    while ((ev=ec06_pop_event()).type!=EC06_EVENT_NONE) {
        if (morse_training_ui_handle_event(&ev)) { back(); return; }
    }
    btn.pressed=false;
    morse_training_ui_tick();
    morse_training_ui_render();
}

static void learn_hub(void)
{
    static M_SELECT rows[]={ {"- Free practice"},{"- Listen & choose"},
        {"- Follow a letter"},{"- Write a letter"} };
    int selected=menu(rows,COUNT(rows));
    if (selected==0) ui_navigate_to(M_MORSE_LEARN);
    else if (selected>0) {
        morse_training_ui_enter((training_mode_t)(selected-1));
        ui_navigate_to(M_TRAINING);
    }
}

static void messages_hub(void)
{
    static char inbox_label[64];
    static M_SELECT rows[]={ {"- Quick phrases"},{inbox_label},{"- Last send / retry"},{"- Nearby room"},{"- Write message"},{"- ASCII faces"},{"- Draft box"},
        {"- Mesh group chat"},{"- Network announcement"},{"- Offline message"},{"- Offline outbox"},
        {"- Message management"},{"- Multiplayer poll"},{"- Team task board"},
        {"- Group check-in"},{"- Quiz buzzer"},{"- Activity history"},{"- Network diagnostics"},{"- Teams and social"} };
    snprintf(inbox_label,sizeof(inbox_label),ui_zh_lookup("- Inbox (%u new)"),message_service_unread_count());
    int selected=menu(rows,COUNT(rows));
    if (selected==0) ui_navigate_to(M_QUICK_MSG);
    if (selected==1) ui_navigate_to(M_INBOX);
    if (selected==3) ui_navigate_to(M_CHANNEL_SELECT);
    if (selected==4) {s_reply=false;ui_navigate_to(M_MESSAGE_COMPOSE);}
    if (selected==5) ui_navigate_to(M_MESSAGE_EMOTES);
    if (selected==6) {s_box_saving=false;ui_navigate_to(M_DRAFT_BOX);}
    if(selected>=7 && selected<=9){s_net_mode=(unsigned)selected-7;ui_navigate_to(M_NET_SEND);}
    if(selected==10)ui_navigate_to(M_OUTBOX);
    if(selected==11)ui_navigate_to(M_MESSAGE_FILTERS);
    if(selected==12)ui_navigate_to(M_POLL);
    if(selected==13)ui_navigate_to(M_TASK_BOARD);
    if(selected==14){s_crowd_kind=CROWD_CHECKIN;ui_navigate_to(M_CHECKIN);}
    if(selected==15){s_crowd_kind=CROWD_QUIZ;ui_navigate_to(M_QUIZ_BUZZER);}
    if(selected==16)ui_navigate_to(M_ACTIVITY_HISTORY);
    if(selected==17)ui_navigate_to(M_NETWORK_DIAGNOSTICS);
    if(selected==18){s_social_section=0;ui_navigate_to(M_SOCIAL_HUB);}
    if (selected==2) {
        if (s_tx_id) ui_navigate_to(M_TX_STATUS);
        else notice("No previous send");
    }
}

static void send_text(const char *text,message_kind_t kind)
{
    uint16_t length=(uint16_t)strlen(text);
    if (!length) { notice("Draft is empty"); return; }
    if (!message_service_send_text(kind,s_destination,
            s_private?0:message_service_get_channel(),text,length,s_private,&s_tx_id)) {
        notice("Busy / send rejected");
        return;
    }
    /* Retain the draft, including while a private receipt is outstanding. */
    ui_navigate_to(M_TX_STATUS);
}

static void compose_page(void)
{
    load_draft();
    if(btn.pressed && btn.id==BTN_ID_BACK){
        if(message_draft_group_back(&s_draft_selector))btn.pressed=false;
        else if(!draft_store_save_work(s_draft)){btn.pressed=false;notice("SAVE ERROR");}
        else s_reply=false;
    }
    if(btn.pressed && btn.id!=BTN_ID_BACK) {
        uint8_t key=btn.id;
        btn.pressed=false;
        if(key==BTN_ID_CW || key==BTN_ID_CC)
            message_draft_group_rotate(&s_draft_selector,key==BTN_ID_CW?-1:1);
        /* Holding selects SEND; a separate click confirms, avoiding accidental sends. */
        if(key==BTN_ID_LP) message_draft_select_send(&s_draft_selector);
        if(key==BTN_ID_SP) {
            unsigned choice=message_draft_group_choice(&s_draft_selector);
            if(s_draft_selector.selecting_group && s_draft_selector.group<=DRAFT_SYMBOLS)
                s_draft_selector.selecting_group=false;
            else if(choice==DRAFT_EMOTE) {ui_navigate_to(M_MESSAGE_EMOTES);return;}
            else if(choice==DRAFT_SAVE || choice==DRAFT_BOX){
                if(!draft_store_save_work(s_draft)){notice("SAVE ERROR");return;}
                if(choice==DRAFT_SAVE && !s_draft[0]){notice("Draft is empty");return;}
                s_box_saving=choice==DRAFT_SAVE;ui_navigate_to(M_DRAFT_BOX);return;
            }
            else if(choice==DRAFT_NEW){
                s_draft_confirm=DRAFT_CONFIRM_NEW;ui_navigate_to(M_DRAFT_CONFIRM);return;
            }
            else if(choice==DRAFT_SEND) {
                if(!s_draft[0]) notice("Draft is empty");
                else if(!draft_store_save_work(s_draft))notice("SAVE ERROR");
                else {
                    snprintf(s_send_text,sizeof(s_send_text),"%s",s_draft);
                    s_send_kind=MESSAGE_KIND_QUICK;
                    ui_navigate_to(s_reply?M_REPLY_CONFIRM:M_RECIPIENT_PICK);
                    return;
                }
            } else if(!message_draft_edit(s_draft,sizeof(s_draft),choice))
                notice("Maximum 128 characters");
        }
    }
    size_t length=strlen(s_draft);
    text_line(9,length?s_draft+(length>20?length-20:0):"Write a message...");
    char label[28];
    unsigned choice=message_draft_group_choice(&s_draft_selector);
    if(s_draft_selector.selecting_group)
        snprintf(label,sizeof(label),"<%s> %u/128",message_draft_group_name(s_draft_selector.group),(unsigned)length);
    else if(choice==DRAFT_SEND) snprintf(label,sizeof(label),"EDIT <SEND> %u/128",(unsigned)length);
    else if(choice==DRAFT_DELETE) snprintf(label,sizeof(label),"EDIT <DEL> %u/128",(unsigned)length);
    else if(!choice) snprintf(label,sizeof(label),"EDIT <SPACE> %u/128",(unsigned)length);
    else snprintf(label,sizeof(label),"%s <%c> %u/128",message_draft_group_name(s_draft_selector.group),(char)(32+choice),(unsigned)length);
    text_line(21,label);
    footer(s_draft_selector.selecting_group?
        (s_draft_selector.group<=DRAFT_SYMBOLS || s_draft_selector.group==DRAFT_EMOTE_GROUP?
            "CLICK:OPEN HOLD:SEND":"CLICK:ACT HOLD:SEND"):
        "BACK:GROUP HOLD:SEND");
}

static void content_edit_page(void){
    if(!s_field_target || s_field_capacity<2){back();return;}
    if(btn.pressed){unsigned key=btn.id;
        if(key==BTN_ID_BACK){
            if(message_draft_group_back(&s_field_selector))btn.pressed=false;
            /* Outer BACK cancels without changing the caller's value. */
        }else{
            btn.pressed=false;
            if(key==BTN_ID_CW || key==BTN_ID_CC){
                int step=key==BTN_ID_CW?-1:1;
                do{message_draft_group_rotate(&s_field_selector,step);}
                while(s_field_selector.selecting_group && s_field_selector.group>(s_field_target==s_draft?DRAFT_EMOTE_GROUP:DRAFT_SEND_GROUP));
            }
            if(key==BTN_ID_LP)message_draft_select_send(&s_field_selector);
            if(key==BTN_ID_SP){unsigned choice=message_draft_group_choice(&s_field_selector);
                if(s_field_selector.selecting_group && s_field_selector.group<=DRAFT_SYMBOLS)s_field_selector.selecting_group=false;
                else if(choice==DRAFT_EMOTE && s_field_target==s_draft){ui_navigate_to(M_MESSAGE_EMOTES);return;}
                else if(choice==DRAFT_SEND){
                    if(s_field_target==s_draft && !draft_store_save_work(s_field_edit)){notice("SAVE ERROR");return;}
                    snprintf(s_field_target,s_field_capacity,"%s",s_field_edit);back();return;
                }else if(!message_draft_edit(s_field_edit,s_field_capacity,choice))notice("Invalid length/code or SAVE ERROR");
            }
        }
    }
    size_t n=strlen(s_field_edit);char label[48];
    const char *name=s_field_selector.group==DRAFT_SEND_GROUP?"DONE":message_draft_group_name(s_field_selector.group);
    text_line(9,n?s_field_edit+(n>20?n-20:0):"Edit text...");
    if(s_field_selector.selecting_group)snprintf(label,sizeof(label),"<%s> %u/%u",name,(unsigned)n,(unsigned)s_field_capacity-1);
    else {unsigned c=message_draft_group_choice(&s_field_selector);snprintf(label,sizeof(label),"%s <%c> %u/%u",name,c?32+c:' ',(unsigned)n,(unsigned)s_field_capacity-1);}
    text_line(21,label);footer("BACK:CANCEL HOLD:DONE");
}

static void emotes_page(void)
{
    M_SELECT rows[EMOTE_GROUPS];
    for(unsigned i=0;i<EMOTE_GROUPS;i++)rows[i].m_select=(char *)message_emote_group_name(i);
    int selected=menu(rows,EMOTE_GROUPS);
    if(selected>=0){s_emote_group=(unsigned)selected;ui_navigate_to(M_MESSAGE_EMOTE_LIST);}
}
static void emote_list_page(void)
{
    static M_SELECT rows[8];static char labels[8][30];
    unsigned count=0;
    for(;count<COUNT(rows);count++) {
        unsigned i=message_emote_group_index(s_emote_group,count);
        if(i>=MESSAGE_EMOTE_COUNT)break;
        snprintf(labels[count],sizeof(labels[count]),"- %s %s",message_emotes[i].text,message_emotes[i].label);
        rows[count].m_select=labels[count];
    }
    int selected=menu(rows,count);
    if(selected<0)return;
    unsigned index=message_emote_group_index(s_emote_group,(unsigned)selected);
    for(unsigned layer=ui.layer;layer>0;layer--)if(ui.parent_index[layer]==M_CONTENT_EDIT && s_field_target==s_draft){
        if(message_emote_append(index,s_field_edit,s_field_capacity)){
            ui.layer=layer;ui.index=M_CONTENT_EDIT;ui.state=S_LAYER_OUT;
        }else notice("Maximum 128 characters");
        return;
    }
    bool inserting=false;
    for(unsigned layer=ui.layer;layer>0;layer--)if(ui.parent_index[layer]==M_MESSAGE_COMPOSE)inserting=true;
    if(inserting){
        if(message_emote_append(index,s_draft,sizeof(s_draft)))open_draft_editor();
        else notice("Maximum 128 characters");
        return;
    }
    if(message_emote_copy(index,s_send_text,sizeof(s_send_text))) {
        s_send_kind=MESSAGE_KIND_QUICK;
        ui_navigate_to(M_RECIPIENT_PICK);
    }
}

static void draft_box_page(void){
    load_draft();
    static M_SELECT rows[DRAFT_SLOTS];static char labels[DRAFT_SLOTS][32];
    for(unsigned i=0;i<DRAFT_SLOTS;i++){
        const char *text=draft_store_slot(i);
        snprintf(labels[i],sizeof(labels[i]),"- %u %s%.20s",i+1,text[0]?"":"[EMPTY]",text);
        rows[i].m_select=labels[i];
    }
    int selected=menu(rows,DRAFT_SLOTS);
    if(selected<0)return;
    s_draft_slot=(unsigned)selected;
    if(!s_box_saving){ui_navigate_to(M_DRAFT_SLOT);return;}
    if(draft_store_slot(s_draft_slot)[0]){
        s_draft_confirm=DRAFT_CONFIRM_OVERWRITE;ui_navigate_to(M_DRAFT_CONFIRM);
    }else if(draft_store_save_slot(s_draft_slot,s_draft)){s_box_saving=false;notice("Draft saved");back();}
    else notice("SAVE ERROR");
}
static void draft_slot_page(void){
    static M_SELECT rows[]={{"- Edit saved draft"},{"- Delete saved draft"}};
    int selected=menu(rows,COUNT(rows));
    if(selected==0){
        const char *text=draft_store_slot(s_draft_slot);
        if(s_draft[0] && strcmp(s_draft,text)){
            s_draft_confirm=DRAFT_CONFIRM_LOAD;ui_navigate_to(M_DRAFT_CONFIRM);return;
        }
        /* Loading a copy must not destroy unsaved current text on a failed commit. */
        if(!draft_store_save_work(text)){notice("SAVE ERROR");return;}
        snprintf(s_draft,sizeof(s_draft),"%s",text);
        s_draft_selector=(draft_selector_t){.selecting_group=true};open_draft_editor();
    }
    if(selected==1){s_draft_confirm=DRAFT_CONFIRM_DELETE;ui_navigate_to(M_DRAFT_CONFIRM);}
}
static void draft_confirm_page(void){
    M_SELECT rows[]={{"- Cancel"},{s_draft_confirm==DRAFT_CONFIRM_LOAD?"- Confirm load draft":(s_draft_confirm==DRAFT_CONFIRM_NEW?"- Confirm clear editor":
        (s_draft_confirm==DRAFT_CONFIRM_DELETE?"- Confirm delete draft":"- Confirm overwrite draft"))}};
    int selected=menu(rows,COUNT(rows));
    if(selected==0){back();return;}
    if(selected!=1)return;
    if(s_draft_confirm==DRAFT_CONFIRM_LOAD){
        const char *text=draft_store_slot(s_draft_slot);
        if(!draft_store_save_work(text)){notice("SAVE ERROR");return;}
        snprintf(s_draft,sizeof(s_draft),"%s",text);
        s_draft_selector=(draft_selector_t){.selecting_group=true};open_draft_editor();return;
    }
    bool ok=s_draft_confirm==DRAFT_CONFIRM_NEW?draft_store_save_work(""):
        draft_store_save_slot(s_draft_slot,s_draft_confirm==DRAFT_CONFIRM_DELETE?"":s_draft);
    if(!ok){notice("SAVE ERROR");return;}
    if(s_draft_confirm==DRAFT_CONFIRM_NEW){s_reply=false;s_draft[0]=0;s_draft_selector=(draft_selector_t){.selecting_group=true};}
    if(s_draft_confirm==DRAFT_CONFIRM_OVERWRITE)s_box_saving=false;
    notice(s_draft_confirm==DRAFT_CONFIRM_OVERWRITE?"Draft saved":"Draft cleared");back();
}

static void channel_hub(void)
{
    static char dest_label[32];
    static M_SELECT rows[]={ {"- Live Morse"},{dest_label},{"- Received messages"} };
    snprintf(dest_label,sizeof(dest_label),ui_zh_lookup("- Nearby room %02u"),message_service_get_channel());
    int selected=menu(rows,COUNT(rows));
    if (selected==0) ui_navigate_to(M_MORSE_INPUT);
    if (selected==1) ui_navigate_to(M_CHANNEL_SELECT);
    if (selected==2) ui_navigate_to(M_INBOX);
    if (btn.pressed && btn.id==BTN_ID_BACK) morse_playback_stop();
}

static void channel_select(void)
{
    char value[24];
    text_line(9,"Nearby talk room");
    snprintf(value,sizeof(value),"<  ROOM %02u  >",s_channel_value);
    text_line(21,value);
    footer(s_channel_pending?"Saving room...":"Same room = talk together");
    if (s_channel_pending) {
        message_service_snapshot_t status;
        message_service_snapshot(&status);
        if (status.channel==s_channel_value) { s_channel_pending=false; back(); return; }
        if (status.storage_errors!=s_channel_errors || esp_timer_get_time()>s_channel_deadline) {
            s_channel_pending=false;
            notice("Channel save failed");
        }
        btn.pressed=false;
        return;
    }
    if (!btn.pressed) return;
    if (btn.id==BTN_ID_CW) s_channel_value=s_channel_value==16?1:s_channel_value+1;
    if (btn.id==BTN_ID_CC) s_channel_value=s_channel_value==1?16:s_channel_value-1;
    if (btn.id==BTN_ID_SP) {
        message_service_snapshot_t status;
        message_service_snapshot(&status);
        s_channel_errors=status.storage_errors;
        if (message_service_set_channel(s_channel_value)) {
            s_channel_pending=true;
            s_channel_deadline=esp_timer_get_time()+2000000;
        } else notice("Channel save failed");
    }
    if (btn.id!=BTN_ID_BACK) btn.pressed=false;
}

static void quick_page(void)
{
    static M_SELECT rows[QUICK_MESSAGE_COUNT];
    static char labels[QUICK_MESSAGE_COUNT][QUICK_MESSAGE_MAX_LEN+3];
    for (unsigned i=0;i<QUICK_MESSAGE_COUNT;i++) {
        snprintf(labels[i],sizeof(labels[i]),"- %s",g_config.quick_msgs[i].text);
        rows[i].m_select=labels[i];
    }
    int selected=menu(rows,QUICK_MESSAGE_COUNT);
    if (selected>=0) {
        snprintf(s_send_text,sizeof(s_send_text),"%s",g_config.quick_msgs[selected].text);
        s_send_kind=MESSAGE_KIND_QUICK;
        ui_navigate_to(M_RECIPIENT_PICK);
    }
}

static void recipient_page(void)
{
    static M_SELECT rows[FRIEND_STORE_MAX+1];
    static char labels[FRIEND_STORE_MAX+1][32];
    unsigned count=friend_mgr_friend_count();
    if (count>FRIEND_STORE_MAX) count=FRIEND_STORE_MAX;
    snprintf(labels[0],sizeof(labels[0]),ui_zh_lookup("- Nearby room %02u"),message_service_get_channel());
    rows[0].m_select=labels[0];
    for(unsigned i=0;i<count;i++) {
        friend_record_t peer;
        if (!friend_mgr_get_friend(i,&peer)) { count=i; break; }
        snprintf(labels[i+1],sizeof(labels[i+1]),"- %.13s %02X%02X",peer.name,peer.mac[4],peer.mac[5]);
        rows[i+1].m_select=labels[i+1];
    }
    int selected=menu(rows,count+1);
    if (selected>=0) {
        s_private=selected>0;
        if (s_private) {
            friend_record_t peer;
            if (!friend_mgr_get_friend((unsigned)selected-1,&peer)) { notice("Friend unavailable"); return; }
            memcpy(s_destination,peer.mac,6);
            snprintf(s_recipient,sizeof(s_recipient),"%s",peer.name);
        } else memset(s_destination,0xff,6);
        send_text(s_send_text,s_send_kind);
    }
}

/* Resolve by full MAC: short device codes can collide. Keep unknown senders
 * identifiable without changing the stored inbox format. */
static void message_sender_name(const uint8_t mac[6], char out[FRIEND_NAME_MAX+1])
{
    friend_record_t peer;
    if (friend_store_get_active(mac,&peer) && peer.name[0])
        snprintf(out,FRIEND_NAME_MAX+1,"%.*s",FRIEND_NAME_MAX,peer.name);
    else
        snprintf(out,FRIEND_NAME_MAX+1,"%02X%02X",mac[4],mac[5]);
}

static void inbox_page(void)
{
    static M_SELECT rows[MESSAGE_INBOX_MAX];
    static char labels[MESSAGE_INBOX_MAX][32];
    static uint32_t ids[MESSAGE_INBOX_MAX];
    unsigned count=message_service_inbox_count();
    if (!count) { text_line(18,"Inbox is empty"); footer("Waiting for a message"); return; }
    for (unsigned i=0;i<count;i++) {
        message_inbox_entry_t entry;
        if (!message_service_inbox_get(i,&entry)) { count=i; break; }
        ids[i]=entry.local_id;
        char sender[FRIEND_NAME_MAX+1];
        message_sender_name(entry.src_mac,sender);
        snprintf(labels[i],sizeof(labels[i]),"- %c%s %.13s",entry.unread?'*':' ',
                 sender,entry.text);
        rows[i].m_select=labels[i];
    }
    int selected=menu(rows,count);
    if (selected>=0 && message_service_inbox_get_by_id(ids[selected],&s_message)) {
        message_service_inbox_mark_read(s_message.local_id);
        ui_navigate_to(M_MESSAGE_VIEW);
    }
}

static void message_view(void)
{
    char heading[40], sender[FRIEND_NAME_MAX+1];
    message_sender_name(s_message.src_mac,sender);
    /* Keep the full nickname visible on the 128px display. Signal strength is
     * secondary to sender identity and remains in the persisted message. */
    snprintf(heading,sizeof(heading),"%s CH%02u",sender,s_message.channel);
    if(!s_message.channel && s_message.len>=6 && !memcmp(s_message.text,"[ALL] ",6))snprintf(heading,sizeof(heading),"%s ALL",sender);
    if (morse_playback_is_active()) {
        morse_segment_t segment;
        morse_playback_get_segment(&segment);
        if (segment.character_index < s_message.len) {
            char letter=s_message.text[segment.character_index];
            const char *pattern=morse_codec_encode(letter);
            snprintf(heading,sizeof(heading),"%c  %s %s",letter,pattern?pattern:"/",
                     segment.tone?"*":"");
        }
    }
    text_line(8,heading);
    u8g2_SetFont(&u8g2,u8g2_font_6x10_tf);
    int width=u8g2_GetStrWidth(&u8g2,s_message.text);
    int offset=0;
    if(width>124) {
        int travel=width-124;
        int position=(int)((esp_timer_get_time()/50000)%(travel+50));
        offset=position>25?position-25:0;
        if(offset>travel) offset=travel;
    }
    u8g2_SetClipWindow(&u8g2,0,10,128,24);
    u8g2_DrawStr(&u8g2,2-offset,21,s_message.text);
    u8g2_SetMaxClipWindow(&u8g2);
    footer(morse_playback_is_active()?"MORSE AUDIO":"HOLD:REPLY / MANAGE");
    if(btn.pressed && btn.id==BTN_ID_LP){btn.pressed=false;ui_navigate_to(M_MESSAGE_ACTIONS);return;}
    if (btn.pressed && btn.id==BTN_ID_SP) {
        btn.pressed=false;
        if (morse_playback_is_active()) morse_playback_stop();
        else if (!morse_playback_start(s_message.text,s_message.len,g_config.morse_dot_ms))
            notice("Text has no Morse code");
    }
}

static void tx_page(void)
{
    static uint32_t sounded_id;
    static message_tx_status_t sounded_state;
    message_tx_status_t state=MESSAGE_TX_UNKNOWN;
    message_service_get_tx(s_tx_id,&state);
    static const char *names[]={"Unknown","Queued","Broadcasting","Sent","Awaiting receipt","Peer received","Failed"};
    if ((unsigned)state>=COUNT(names)) state=MESSAGE_TX_UNKNOWN;
    if (sounded_id!=s_tx_id || sounded_state!=state) {
        sounded_id=s_tx_id; sounded_state=state;
        if (state==MESSAGE_TX_SENT || state==MESSAGE_TX_DELIVERED) haptic_play(HAPTIC_MSG_SENT);
        if (state==MESSAGE_TX_FAILED) haptic_play(HAPTIC_ERROR);
    }
    text_line(9,"Message status");
    text_line(21,names[state]);
    footer(state==MESSAGE_TX_FAILED?"RETRY AVAILABLE":"MESSAGE STATUS");
    if (btn.pressed && btn.id==BTN_ID_SP) {
        btn.pressed=false;
        if (state==MESSAGE_TX_FAILED && !message_service_retry(s_tx_id)) notice("Retry rejected");
    }
}

static void friends_page(void)
{
    static M_SELECT rows[FRIEND_STORE_MAX+1];
    static char labels[FRIEND_STORE_MAX][32];
    rows[0].m_select="- Touch to add";
    unsigned count=friend_mgr_friend_count();
    if(count>FRIEND_STORE_MAX)count=FRIEND_STORE_MAX;
    for(unsigned i=0;i<count;i++) {
        friend_record_t peer;
        if(!friend_mgr_get_friend(i,&peer)) { count=i;break; }
        contact_presence_t presence;message_service_contact(peer.mac,&presence);
        snprintf(labels[i],sizeof(labels[i]),"- %.13s [%c]",peer.name,presence.state==CONTACT_DIRECT?'D':presence.state==CONTACT_RECENT?'R':'?');
        rows[i+1].m_select=labels[i];
    }
    int selected=menu(rows,count+1);
    if(selected==0) {
        if(friend_mgr_enter_add_mode(NULL,NULL,NULL)) ui_navigate_to(M_FRIEND_PAIR);
        else notice("Pairing unavailable");
    } else if(selected>0 && friend_mgr_get_friend((unsigned)selected-1,&s_friend))
        ui_navigate_to(M_FRIEND_DETAIL);
    friend_mgr_snapshot_t status;
    friend_mgr_snapshot(&status);
    if (status.state==FRIEND_MGR_ERROR && status.error[0]) footer(status.error);
}

static void pair_page(void)
{
    static int sounded_state=-1;
    friend_mgr_snapshot_t state;
    friend_mgr_snapshot(&state);
    if (sounded_state!=(int)state.state) {
        sounded_state=(int)state.state;
        if (state.state==FRIEND_MGR_DONE) haptic_play(HAPTIC_FRIEND_FOUND);
        else if (state.state==FRIEND_MGR_TIMEOUT || state.state==FRIEND_MGR_ERROR)
            haptic_play(HAPTIC_FRIEND_TIMEOUT);
        else if (state.state==FRIEND_MGR_WAIT_USER_CONFIRM) haptic_play(HAPTIC_WINDOW_OPEN);
    }
    const char *title="Touch to add";
    switch(state.state) {
    case FRIEND_MGR_DISCOVERY:title="Bring devices close";break;
    case FRIEND_MGR_WAIT_USER_CONFIRM:title="Add this device?";break;
    case FRIEND_MGR_WAIT_PEER_CONFIRM:title="Waiting for peer";break;
    case FRIEND_MGR_COMMITTING:title="Saving both sides";break;
    case FRIEND_MGR_SYNCING:title="Pending sync";break;
    case FRIEND_MGR_DONE:title="Friends confirmed";break;
    case FRIEND_MGR_TIMEOUT:title="Pairing timed out";break;
    case FRIEND_MGR_ERROR:title="Pairing failed";break;
    default:break;
    }
    text_line(9,title);
    char peer[28];
    if(state.error[0]) snprintf(peer,sizeof(peer),"%.21s",state.error);
    else if(state.peer_name[0]) snprintf(peer,sizeof(peer),"%.13s %ddB",state.peer_name,state.rssi);
    else snprintf(peer,sizeof(peer),"%.21s",state.error[0]?state.error:"Both open this page");
    text_line(21,peer);
    footer(state.state==FRIEND_MGR_WAIT_USER_CONFIRM?"Confirm friendship?":"BOTH SIDES MUST AGREE");
    if(btn.pressed && btn.id==BTN_ID_SP) {
        btn.pressed=false;
        if(state.state==FRIEND_MGR_WAIT_USER_CONFIRM && !friend_mgr_user_confirm()) notice("Confirm failed");
        else if(state.state==FRIEND_MGR_DONE) back();
    }
    if(btn.pressed && btn.id==BTN_ID_BACK) { friend_mgr_user_cancel(); back(); }
}

static void friend_detail(void)
{
    if(s_delete_pending) {
        static M_SELECT confirm[]={ {"- Keep friend"},{"- Revoke friendship"} };
        int selected=menu(confirm,COUNT(confirm));
        if(selected==0) { s_delete_pending=false;ui.init=false; }
        if(selected==1) {
            if(friend_mgr_remove_friend(s_friend.mac)) back();
            else notice("Revoke failed");
        }
        return;
    }
    static char name[32];
    static M_SELECT rows[]={ {name},{"- Send quick phrase"},{"- Remove friend"},{"- Contact status"},{"- Received from friend"} };
    snprintf(name,sizeof(name),"- %.13s %02X%02X",s_friend.name,s_friend.mac[4],s_friend.mac[5]);
    int selected=menu(rows,COUNT(rows));
    if(selected==1) {
        s_private=true;
        memcpy(s_destination,s_friend.mac,6);
        snprintf(s_recipient,sizeof(s_recipient),"%s",s_friend.name);
        ui_navigate_to(M_QUICK_MSG);
    }
    if(selected==2) { s_delete_pending=true;ui.select[ui.layer]=0;ui.init=false; }
    if(selected==3)ui_navigate_to(M_CONTACT_STATUS);
    if(selected==4){memcpy(s_filter_peer,s_friend.mac,6);s_message_filter=3;ui_navigate_to(M_FILTERED_INBOX);}
}

static void net_send_page(void){
    load_draft();
    char preview[96],send_label[96];
    snprintf(preview,sizeof(preview),ui_zh_lookup("- Edit: %.20s"),s_draft[0]?s_draft:ui_zh_lookup("[EMPTY]"));
    if(s_net_mode==NET_GROUP)snprintf(send_label,sizeof(send_label),ui_zh_lookup("- Send group CH%02u"),message_service_get_channel());
    else snprintf(send_label,sizeof(send_label),"%s",ui_zh_lookup(s_net_mode==NET_ANNOUNCEMENT?"- Send ALL (max122)":"- Pick offline friend"));
    M_SELECT rows[]={{preview},{send_label},{"- Network settings"},{"- Offline outbox"}};
    int selected=menu(rows,COUNT(rows));
    if(selected==0){open_content_editor(s_draft,sizeof(s_draft));return;}
    if(selected==2)ui_navigate_to(M_NETWORK_STATUS);
    if(selected==3)ui_navigate_to(M_OUTBOX);
    if(selected!=1)return;
    if(!s_draft[0]){notice("Draft is empty");return;}
    if(s_net_mode==NET_ANNOUNCEMENT && strlen(s_draft)>122){notice("Announcement max122");return;}
    if(!draft_store_save_work(s_draft)){notice("SAVE ERROR");return;}
    snprintf(s_send_text,sizeof(s_send_text),"%s",s_draft);
    ui_navigate_to(s_net_mode>=NET_OFFLINE?M_OFFLINE_RECIPIENT:M_NET_CONFIRM);
}
static void offline_recipient_page(void){
    static M_SELECT rows[FRIEND_STORE_MAX];static char labels[FRIEND_STORE_MAX][32];
    unsigned count=friend_mgr_friend_count();if(count>FRIEND_STORE_MAX)count=FRIEND_STORE_MAX;
    if(!count){text_line(11,"No paired friends");footer("Pair a friend first");return;}
    for(unsigned i=0;i<count;i++){
        friend_record_t peer;if(!friend_mgr_get_friend(i,&peer)){count=i;break;}
        snprintf(labels[i],sizeof(labels[i]),"- %.13s %02X%02X",peer.name,peer.mac[4],peer.mac[5]);rows[i].m_select=labels[i];
    }
    int selected=menu(rows,count);if(selected<0)return;
    friend_record_t peer;if(!friend_mgr_get_friend((unsigned)selected,&peer)){notice("Friend unavailable");return;}
    memcpy(s_offline_dst,peer.mac,6);ui_navigate_to(M_NET_CONFIRM);
}
static void net_confirm_page(void){
    char preview[64],action[96];
    snprintf(preview,sizeof(preview),"- %.20s",s_send_text);
    if(s_net_mode==NET_GROUP)snprintf(action,sizeof(action),ui_zh_lookup("- Confirm group CH%02u"),message_service_get_channel());
    else if(s_net_mode>=NET_OFFLINE)snprintf(action,sizeof(action),ui_zh_lookup(s_net_mode==NET_CUSTODY?"- Relay plaintext to %02X%02X":"- Queue for %02X%02X"),s_offline_dst[4],s_offline_dst[5]);
    else snprintf(action,sizeof(action),"%s",ui_zh_lookup("- Confirm ALL channels"));
    M_SELECT rows[]={{"- Cancel"},{preview},{action}};
    int selected=menu(rows,COUNT(rows));if(selected==0){back();return;}if(selected!=2)return;
    if(s_net_mode==NET_OFFLINE || s_net_mode==NET_CUSTODY){
        int slot=s_net_mode==NET_CUSTODY?message_service_custody_text(s_offline_dst,s_send_text):message_service_offline_text(s_offline_dst,s_send_text);
        if(slot<0){notice("Outbox full / save error");return;}
        s_outbox_slot=(unsigned)slot;notice("Queued; not delivered");ui_navigate_to(M_OUTBOX_ITEM);
    }else{
        if(!message_service_group_text(s_send_text,s_net_mode==NET_ANNOUNCEMENT,&s_tx_id)){
            notice("Enable mesh / busy");return;
        }
        ui_navigate_to(M_TX_STATUS);
    }
}
static const char *outbox_state(unsigned state){
    static const char *const names[]={"EMPTY","OUTBOX WAIT","DELIVERED","HELD"};return ui_zh_lookup(names[state<=OUTBOX_HELD?state:0]);
}
static void outbox_page(void){
    static M_SELECT rows[OUTBOX_SLOTS];static char labels[OUTBOX_SLOTS][96];
    bool error=false;
    for(unsigned i=0;i<OUTBOX_SLOTS;i++){
        outbox_entry_t entry={0};if(!message_service_outbox_get(i,&entry,&error))error=true;
        int length=entry.state?(entry.packet.len<16?entry.packet.len:16):0;
        snprintf(labels[i],sizeof(labels[i]),"- %u %s %.*s",i+1,error?ui_zh_lookup("SAVE ERROR"):outbox_state(entry.state),length,(const char *)entry.packet.payload);
        /* payload is length-delimited, not necessarily NUL terminated; precision bounds the preview. */
        rows[i].m_select=labels[i];
    }
    int selected=menu(rows,COUNT(rows));if(selected>=0){s_outbox_slot=(unsigned)selected;ui_navigate_to(M_OUTBOX_ITEM);}
}
static void outbox_item_page(void){
    outbox_entry_t entry={0};bool error=false;
    if(!message_service_outbox_get(s_outbox_slot,&entry,&error) || error){text_line(11,"Outbox SAVE ERROR");footer("BACK:EXIT / RETRY LATER");return;}
    char status[96],preview[32];
    snprintf(status,sizeof(status),ui_zh_lookup("- %s TRY %u/%u"),outbox_state(entry.state),entry.rounds,OUTBOX_ROUNDS_MAX);
    int length=entry.state?(entry.packet.len<20?entry.packet.len:20):0;
    snprintf(preview,sizeof(preview),"- %.*s",length,(const char *)entry.packet.payload);
    message_network_view_t network;message_service_network_view(&network);
    M_SELECT rows[]={{status},{preview},{"- Retry now"},{"- Delete / cancel"},{network.enabled?"- Network settings":"- Enable mesh to deliver"}};
    int selected=menu(rows,COUNT(rows));
    if(selected==2 && !message_service_outbox_retry(s_outbox_slot))notice("Retry rejected");
    if(selected==3)ui_navigate_to(M_OUTBOX_DELETE);
    if(selected==4)ui_navigate_to(M_NETWORK_STATUS);
}
static void outbox_delete_page(void){
    M_SELECT rows[]={{"- Cancel"},{"- Confirm cancel/delete"}};
    int selected=menu(rows,COUNT(rows));
    if(selected==0)back();
    if(selected==1){if(!message_service_outbox_remove(s_outbox_slot)){notice("SAVE ERROR");return;}back();}
}
#include "extras_pages.inc"
#include "topology_page.inc"
static void network_page(void){
    static message_network_view_t view;message_service_network_view(&view);
    unsigned peers[ROUTE_PEERS],count=0;
    for(unsigned i=0;i<ROUTE_PEERS;i++)if(view.peers[i].used && view.now-view.peers[i].seen<15000)peers[count++]=i;
    unsigned pages=count+2;if(s_network_page>=pages)s_network_page=0;
    if(btn.pressed){int key=btn.id;btn.pressed=false;
        if(key==BTN_ID_CW)s_network_page=(s_network_page+1)%pages;
        if(key==BTN_ID_CC)s_network_page=(s_network_page+pages-1)%pages;
        if(key==BTN_ID_BACK){back();return;}
        if(key==BTN_ID_SP && s_network_page==0){
            message_service_network_enable(!view.enabled);message_service_network_view(&view);
        }
    }
    char top[27],middle[27],bottom[27];
    if(s_network_page==0){
        snprintf(top,sizeof(top),"MESH %s%s MAX 3 HOPS",view.enabled?"ON":"OFF",view.paused?" PAUSED":"");
        snprintf(middle,sizeof(middle),"CH %u DIRECT PEERS %u",message_service_get_channel(),count);
        snprintf(bottom,sizeof(bottom),"SP:ON/OFF TURN:MORE");
    }else if(s_network_page==1){
        snprintf(top,sizeof(top),"FORWARDED %lu",(unsigned long)view.forwarded);
        snprintf(middle,sizeof(middle),"DUP %lu DROP %lu",(unsigned long)(view.duplicates%100000),(unsigned long)(view.dropped%100000));
        snprintf(bottom,sizeof(bottom),"HEARD %u HOPS; BACK:EXIT",view.last_rx_hops);
    }else{
        const route_peer_t *peer=&view.peers[peers[s_network_page-2]];
        snprintf(top,sizeof(top),"%s",peer->name[0]?peer->name:"UNNAMED PEER");
        snprintf(middle,sizeof(middle),"RSSI %d AGE %lus",peer->rssi,(unsigned long)((view.now-peer->seen)/1000));
        snprintf(bottom,sizeof(bottom),"%02X:%02X:%02X:%02X:%02X:%02X",peer->mac[0],peer->mac[1],peer->mac[2],peer->mac[3],peer->mac[4],peer->mac[5]);
    }
    u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    if(ui_language_chinese()){
        char a[96],b[96];
        if(s_network_page==0){
            snprintf(a,sizeof(a),ui_zh_lookup("Mesh %s / max 3 hops"),ui_zh_lookup(view.paused?"Paused":view.enabled?"ON":"OFF"));
            snprintf(b,sizeof(b),ui_zh_lookup("CH%u direct peers:%u"),message_service_get_channel(),count);
        }else if(s_network_page==1){
            snprintf(a,sizeof(a),ui_zh_lookup("Forwarded %lu / heard %u hops"),(unsigned long)view.forwarded,view.last_rx_hops);
            snprintf(b,sizeof(b),ui_zh_lookup("Duplicate %lu / drop %lu"),(unsigned long)view.duplicates,(unsigned long)view.dropped);
        }else{
            const route_peer_t *peer=&view.peers[peers[s_network_page-2]];
            snprintf(a,sizeof(a),"%.13s %d dB",peer->name,peer->rssi);
            snprintf(b,sizeof(b),ui_zh_lookup("Peer %u/%u / %lus ago"),s_network_page-1,count,(unsigned long)((view.now-peer->seen)/1000));
        }
        ui_zh_draw_fit(&u8g2,1,11,a,126);ui_zh_draw_fit(&u8g2,1,27,b,126);return;
    }
    u8g2_DrawStr(&u8g2,1,8,top);u8g2_DrawStr(&u8g2,1,19,middle);u8g2_DrawStr(&u8g2,1,30,bottom);
}

#include "team_pages.inc"
#include "crowd_pages.inc"
#include "social_pages.inc"

void local_pages_proc(void)
{
    switch(ui.index) {
    case M_PARTY_HUB:case M_PARTY_ROOM:case M_PARTY_VIEW:case M_PARTY_ACTION:case M_PARTY_HELP:case M_PARTY_CREATE:case M_PARTY_LEAVE:party_pages_proc();break;
    case M_TOOLS_ACTIVITIES:case M_SETTINGS_NETWORK:
    case M_TOOLS_REMOTE:case M_TOOLS_CONNECT:case M_TOOLS_DIAGNOSTIC:case M_TOOLS_SOCIAL:
    case M_TOOLS:case M_BLE_SCAN:case M_HID_SLIDES:case M_HID_MEDIA:case M_HID_CAMERA:case M_WIFI_SCAN:case M_TOOL_MEMORY:
    case M_WIFI_CONNECT:case M_WIFI_NETWORKS:case M_WIFI_PASSWORD:case M_WIFI_STATUS:case M_WIFI_FORGET:case M_CLOUD_SYNC:
        wireless_tools_proc();break;
    case M_MESSAGES_HUB:messages_hub();break;
    case M_MESSAGE_COMPOSE:compose_page();break;
    case M_CONTENT_EDIT:content_edit_page();break;
    case M_MESSAGE_EMOTES:emotes_page();break;
    case M_MESSAGE_EMOTE_LIST:emote_list_page();break;
    case M_NETWORK_STATUS:network_page();break;
    case M_NETWORK_TOPOLOGY:topology_page();break;
    case M_NETWORK_LAB:lab_page();break;
    case M_PET_GIFTS:gift_page();break;
    case M_NET_SEND:net_send_page();break;
    case M_OFFLINE_RECIPIENT:offline_recipient_page();break;
    case M_NET_CONFIRM:net_confirm_page();break;
    case M_OUTBOX:outbox_page();break;
    case M_OUTBOX_ITEM:outbox_item_page();break;
    case M_OUTBOX_DELETE:outbox_delete_page();break;
    case M_MESSAGE_FILTERS:message_filters_page();break;
    case M_FILTERED_INBOX:filtered_inbox_page();break;
    case M_MESSAGE_ACTIONS:message_actions_page();break;
    case M_CONTACT_STATUS:contact_status_page();break;
    case M_CONVERSATION_PEER:conversation_peer_page();break;
    case M_POLL:poll_page();break;
    case M_POLL_CREATE:team_create_page(true);break;
    case M_TEAM_CONFIRM:team_confirm_page();break;
    case M_TASK_BOARD:task_board_page();break;
    case M_TASK_CREATE:team_create_page(false);break;
    case M_TASK_DETAIL:task_detail_page();break;
    case M_REPLY_CONFIRM:reply_confirm_page();break;
    case M_MESSAGE_DELETE:message_delete_page();break;
    case M_CHECKIN:crowd_page(CROWD_CHECKIN);break;
    case M_QUIZ_BUZZER:crowd_page(CROWD_QUIZ);break;
    case M_CROWD_CREATE:crowd_create_page();break;
    case M_CROWD_CONFIRM:crowd_confirm_page();break;
    case M_NETWORK_DIAGNOSTICS:network_diagnostics_page();break;
    case M_ACTIVITY_HISTORY:activity_history_page();break;
    case M_ACTIVITY_DETAIL:activity_detail_page();break;
    case M_SOCIAL_HUB:social_hub_page();break;
    case M_SOCIAL_FORM:social_form_page();break;
    case M_PROFILE_CARD:profile_card_page();break;
    case M_PASSPORT:passport_page();break;
    case M_ENSEMBLE:ensemble_page();break;
    case M_DRAFT_BOX:draft_box_page();break;
    case M_DRAFT_SLOT:draft_slot_page();break;
    case M_DRAFT_CONFIRM:draft_confirm_page();break;
    case M_TREASURE:case M_REACTION:case M_NUMBER_BOMB:case M_TIME_GUESS:nearby_games_proc();break;
    case M_CHANNEL_HUB:channel_hub();break;
    case M_LEARN_HUB:learn_hub();break;
    case M_TRAINING:training_page();break;
    case M_MORSE_LEARN:case M_MORSE_INPUT:terminal_page();break;
    case M_QUICK_MSG:quick_page();break;
    case M_CHANNEL_SELECT:channel_select();break;
    case M_INBOX:inbox_page();break;
    case M_MESSAGE_VIEW:message_view();break;
    case M_RECIPIENT_PICK:recipient_page();break;
    case M_TX_STATUS:tx_page();break;
    case M_FRIENDS:friends_page();break;
    case M_FRIEND_PAIR:pair_page();break;
    case M_FRIEND_DETAIL:friend_detail();break;
    case M_FUN_PET:case M_FUN_SOLO:case M_FUN_NEARBY:
    case M_FUN_HUB: {
        static M_SELECT rows[]={ FUN_MENU_ITEMS(MENU_LABEL) };
        static const uint8_t pages[]={ FUN_MENU_ITEMS(MENU_PAGE) };
        static M_SELECT pet[]={FUN_PET_ITEMS(MENU_LABEL)},solo[]={FUN_SOLO_ITEMS(MENU_LABEL)},nearby[]={FUN_NEARBY_ITEMS(MENU_LABEL)};
        static const uint8_t pet_pages[]={FUN_PET_ITEMS(MENU_PAGE)},solo_pages[]={FUN_SOLO_ITEMS(MENU_PAGE)},nearby_pages[]={FUN_NEARBY_ITEMS(MENU_PAGE)};
        M_SELECT *items=rows;const uint8_t *targets=pages;unsigned count=COUNT(rows);
        if(ui.index==M_FUN_PET){items=pet;targets=pet_pages;count=COUNT(pet);}
        if(ui.index==M_FUN_SOLO){items=solo;targets=solo_pages;count=COUNT(solo);}
        if(ui.index==M_FUN_NEARBY){items=nearby;targets=nearby_pages;count=COUNT(nearby);}
        int selected=menu(items,count);
        if(selected>=0)ui_navigate_to(targets[selected]);
        break;
    }
    case M_SLEEP_CONFIRM: {
        static M_SELECT rows[]={ {"- Screen off now"},{"- Deep sleep: hold key"} };
        int selected=menu(rows,COUNT(rows));
        if(selected==0) { back(); power_mgmt_request_screen_off(); }
        if(selected==1 && !power_mgmt_request_sleep())notice("Wait for boot to finish");
        break;
    }
    default:break;
    }
    if(esp_timer_get_time()<s_notice_until){
        /* Experimental network actions use ASCII status; errors must not disappear
         * behind the normal three-second Chinese help alternation. */
        if(ui.index>=M_NET_SEND && ui.index<=M_ENSEMBLE){
            u8g2_SetDrawColor(&u8g2,0);
            if(ui_language_chinese())u8g2_DrawBox(&u8g2,0,15,128,17);
            else u8g2_DrawBox(&u8g2,0,24,128,8);
            u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
            if(ui_language_chinese())ui_zh_draw_fit(&u8g2,2,27,s_notice,124);
            else u8g2_DrawStr(&u8g2,2,31,s_notice);
        }else footer(s_notice);
    }
}
