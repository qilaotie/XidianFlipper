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
static char s_notice[25];
static int64_t s_notice_until;
static uint8_t s_channel_value = 1;
static bool s_channel_pending;
static uint32_t s_channel_errors;
static int64_t s_channel_deadline;
static message_kind_t s_send_kind = MESSAGE_KIND_MORSE;
static char s_send_text[MESSAGE_TEXT_MAX+1];
static char s_draft[MESSAGE_TEXT_MAX+1];
static draft_selector_t s_draft_selector={.selecting_group=true};
static bool s_delete_pending;
static bool s_draft_loaded,s_box_saving,s_draft_list_enter;
static unsigned s_draft_slot;
static unsigned s_emote_group;
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
    if(ui_language_chinese() && ui.index!=M_MESSAGE_VIEW && ui.index!=M_MESSAGE_COMPOSE){
        ui_zh_draw_fit(&u8g2,2,y<=12?11:27,text,124);
    }else u8g2_DrawStr(&u8g2,2,y,text);
}

static void footer(const char *text)
{
    if(ui_language_chinese() && ui.index!=M_MESSAGE_VIEW && ui.index!=M_MESSAGE_COMPOSE){
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
    s_last_page=page;
    s_draft_list_enter=page==M_DRAFT_BOX || page==M_DRAFT_SLOT || page==M_DRAFT_CONFIRM || page==M_MESSAGE_EMOTES || page==M_MESSAGE_EMOTE_LIST;
    menu_pages_on_enter(page);
    { extern void game_help_page_enter(unsigned); game_help_page_enter(page); }
    tetris_page_enter(page);
    snake_page_enter(page);
    pet_page_enter(page);
    wireless_tools_on_enter(page);
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
    static M_SELECT rows[]={ {"- Quick phrases"},{inbox_label},{"- Last send / retry"},{"- Nearby room"},{"- Write message"},{"- ASCII faces"},{"- Draft box"} };
    snprintf(inbox_label,sizeof(inbox_label),ui_zh_lookup("- Inbox (%u new)"),message_service_unread_count());
    int selected=menu(rows,COUNT(rows));
    if (selected==0) ui_navigate_to(M_QUICK_MSG);
    if (selected==1) ui_navigate_to(M_INBOX);
    if (selected==3) ui_navigate_to(M_CHANNEL_SELECT);
    if (selected==4) ui_navigate_to(M_MESSAGE_COMPOSE);
    if (selected==5) ui_navigate_to(M_MESSAGE_EMOTES);
    if (selected==6) {s_box_saving=false;ui_navigate_to(M_DRAFT_BOX);}
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
                    ui_navigate_to(M_RECIPIENT_PICK);
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
    if(s_draft_confirm==DRAFT_CONFIRM_NEW){s_draft[0]=0;s_draft_selector=(draft_selector_t){.selecting_group=true};}
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
    footer(morse_playback_is_active()?"MORSE AUDIO":"REPLAY AVAILABLE");
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
        snprintf(labels[i],sizeof(labels[i]),"- %.13s %02X%02X",peer.name,peer.mac[4],peer.mac[5]);
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
    static M_SELECT rows[]={ {name},{"- Send quick phrase"},{"- Remove friend"} };
    snprintf(name,sizeof(name),"- %.13s %02X%02X",s_friend.name,s_friend.mac[4],s_friend.mac[5]);
    int selected=menu(rows,COUNT(rows));
    if(selected==1) {
        s_private=true;
        memcpy(s_destination,s_friend.mac,6);
        snprintf(s_recipient,sizeof(s_recipient),"%s",s_friend.name);
        ui_navigate_to(M_QUICK_MSG);
    }
    if(selected==2) { s_delete_pending=true;ui.select[ui.layer]=0;ui.init=false; }
}

void local_pages_proc(void)
{
    switch(ui.index) {
    case M_TOOLS:case M_BLE_SCAN:case M_HID_SLIDES:case M_HID_MEDIA:case M_HID_CAMERA:case M_WIFI_SCAN:case M_TOOL_MEMORY:
    case M_WIFI_CONNECT:case M_WIFI_NETWORKS:case M_WIFI_PASSWORD:case M_WIFI_STATUS:case M_WIFI_FORGET:case M_CLOUD_SYNC:
        wireless_tools_proc();break;
    case M_MESSAGES_HUB:messages_hub();break;
    case M_MESSAGE_COMPOSE:compose_page();break;
    case M_MESSAGE_EMOTES:emotes_page();break;
    case M_MESSAGE_EMOTE_LIST:emote_list_page();break;
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
    case M_FUN_HUB: {
        static M_SELECT rows[]={ FUN_MENU_ITEMS(MENU_LABEL) };
        static const uint8_t pages[]={ FUN_MENU_ITEMS(MENU_PAGE) };
        int selected=menu(rows,COUNT(rows));
        if(selected>=0)ui_navigate_to(pages[selected]);
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
    if(esp_timer_get_time()<s_notice_until)footer(s_notice);
}
