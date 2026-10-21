/* Local product pages. Input, transport and playback retain separate owners. */
#include "local_pages.h"
#include "wououi.h"
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
static bool s_delete_pending;

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
    u8g2_DrawStr(&u8g2,2,y,text);
}

static void footer(const char *text)
{
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
    return page==M_MORSE_LEARN || page==M_MORSE_INPUT || page==M_TRAINING;
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
    ui.num[M_MAIN]=7;
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
    menu_pages_on_enter(page);
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
    static char inbox_label[28];
    static M_SELECT rows[]={ {"- Quick phrases"},{inbox_label},{"- Last send / retry"},{"- Nearby room"} };
    snprintf(inbox_label,sizeof(inbox_label),"- Inbox (%u new)",message_service_unread_count());
    int selected=menu(rows,COUNT(rows));
    if (selected==0) ui_navigate_to(M_QUICK_MSG);
    if (selected==1) ui_navigate_to(M_INBOX);
    if (selected==3) ui_navigate_to(M_CHANNEL_SELECT);
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

static void channel_hub(void)
{
    static char dest_label[32];
    static M_SELECT rows[]={ {"- Live Morse"},{dest_label},{"- Received messages"} };
    snprintf(dest_label,sizeof(dest_label),"- Nearby room %02u",message_service_get_channel());
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
    snprintf(labels[0],sizeof(labels[0]),"- Nearby room %02u",message_service_get_channel());
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
        snprintf(labels[i],sizeof(labels[i]),"- %c%02X%02X %.15s",entry.unread?'*':' ',
                 entry.src_mac[4],entry.src_mac[5],entry.text);
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
    char heading[24];
    snprintf(heading,sizeof(heading),"%02X%02X CH%02u %ddB",s_message.src_mac[4],s_message.src_mac[5],
             s_message.channel,s_message.rssi);
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
    case M_MESSAGES_HUB:messages_hub();break;
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
        static M_SELECT rows[]={ {"- Answer book"},{"- Dice"} };
        int selected=menu(rows,COUNT(rows));
        if(selected==0)ui_navigate_to(M_ANSWER_BOOK);
        if(selected==1)ui_navigate_to(M_DICE);
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
