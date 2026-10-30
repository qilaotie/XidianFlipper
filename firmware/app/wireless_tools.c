#include "wireless_tools.h"
#include "wououi.h"
#include "ui_zh.h"
#include "menu_layout.h"
#include "local_pages.h"
#include "message_service.h"
#include "ble_radio.h"
#include "tool_hid.h"
#include "camera_remote.h"
#include "wifi_station.h"
#include "cloud_sync.h"
#include "message_draft.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOOL_PEERS 24
typedef struct {
    uint8_t mac[6], type;
    int8_t rssi;
    uint16_t company;
    bool manufacturer, service;
    char name[25];
    uint32_t seen;
} scan_peer_t;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static scan_peer_t s_peers[TOOL_PEERS];
static unsigned s_peer_count;
static bool s_observe;
static uint8_t s_page = 255;
static unsigned s_selected;
static unsigned s_camera_key,s_camera_pulse=1;
static bool s_detail, s_entered, s_remote_lease;
static uint8_t s_target[6], s_target_type, s_history[48];
static uint32_t s_sample;
static int64_t s_notice_until;
static int64_t s_retry_at;
static char s_notice[26];
static wifi_ap_record_t s_network;
static char s_wifi_password[65];
static draft_selector_t s_wifi_selector={.group=DRAFT_LOWER,.selecting_group=true};
static uint32_t ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static void printable(char *dst, size_t cap, const uint8_t *src, size_t n) {
    size_t i = 0;
    while (i + 1 < cap && i < n && src[i]) {
        dst[i] = src[i] >= 32 && src[i] < 127 ? src[i] : '?'; ++i;
    }
    dst[i] = 0;
}
void wireless_tools_ble_report(const uint8_t mac[6], uint8_t type,
                               const uint8_t *data, size_t len, int8_t rssi) {
    portENTER_CRITICAL(&s_lock); bool active = s_observe; portEXIT_CRITICAL(&s_lock);
    if (!active || !data) return;
    char name[25] = {0}; bool manufacturer = false, service = false; uint16_t company = 0;
    for (size_t p = 0; p < len;) {
        size_t n = data[p];
        if (!n || p + n + 1 > len) break;
        uint8_t field = data[p + 1];
        if (field == 9 || (field == 8 && !name[0])) printable(name, sizeof(name), data + p + 2, n - 1);
        if (field == 0xFF && n >= 3) { manufacturer = true; company = data[p+2] | (uint16_t)data[p+3] << 8; }
        if (field >= 2 && field <= 7) service = true;
        if (field == 0x16 || field == 0x20 || field == 0x21) service = true;
        p += n + 1;
    }
    uint32_t now = ms();
    portENTER_CRITICAL(&s_lock);
    if (s_observe) {
        unsigned slot = s_peer_count;
        for (unsigned i = 0; i < s_peer_count; ++i)
            if (s_peers[i].type == type && !memcmp(s_peers[i].mac, mac, 6)) { slot = i; break; }
        if (slot == TOOL_PEERS) {
            slot = 0;
            for (unsigned i = 1; i < TOOL_PEERS; ++i)
                if ((uint32_t)(now - s_peers[i].seen) > (uint32_t)(now - s_peers[slot].seen)) slot = i;
            memset(&s_peers[slot], 0, sizeof(s_peers[slot]));
        } else if (slot == s_peer_count) { ++s_peer_count; memset(&s_peers[slot], 0, sizeof(s_peers[slot])); }
        scan_peer_t *p = &s_peers[slot];
        memcpy(p->mac, mac, 6); p->type = type; p->seen = now; p->rssi = rssi;
        if (name[0]) memcpy(p->name, name, sizeof(name));
        if (manufacturer) { p->manufacturer = true; p->company = company; }
        p->service |= service;
    }
    portEXIT_CRITICAL(&s_lock);
}
static bool wifi_page_family(uint8_t page) {
    return page==M_WIFI_SCAN || page==M_WIFI_CONNECT || page==M_WIFI_NETWORKS ||
        page==M_WIFI_PASSWORD || page==M_WIFI_STATUS || page==M_WIFI_FORGET;
}
void wireless_tools_on_enter(uint8_t page) {
    if (page == s_page) return;
    portENTER_CRITICAL(&s_lock); s_observe = false; portEXIT_CRITICAL(&s_lock);
    if(wifi_page_family(s_page) && !wifi_page_family(page))wifi_station_idle();
    if(s_page==M_WIFI_PASSWORD)memset(s_wifi_password,0,sizeof(s_wifi_password));
    if(page==M_WIFI_PASSWORD){
        memset(s_wifi_password,0,sizeof(s_wifi_password));
        memset(&s_wifi_selector,0,sizeof(s_wifi_selector));
        s_wifi_selector.group=DRAFT_LOWER;s_wifi_selector.selecting_group=true;
    }
    if (s_remote_lease) {
        tool_hid_stop();
        ble_radio_tools_restore();
        message_service_resume(); s_remote_lease = false;
    }
    s_page = page; s_selected = 0; s_detail = false; s_entered = false;
    s_notice_until = s_retry_at = 0; memset(s_history, 0, sizeof(s_history));
    if (page == M_BLE_SCAN) {
        portENTER_CRITICAL(&s_lock);
        s_peer_count = 0; memset(s_peers, 0, sizeof(s_peers)); s_observe = true;
        portEXIT_CRITICAL(&s_lock);
    }
}
typedef struct {int y;bool raw;char text[96];} tool_row_t;
static tool_row_t s_rows[3];
static uint32_t s_view_at;
static void row(int y,const char *text,bool raw){
    tool_row_t *r=&s_rows[y<=10?0:(y<=20?1:2)];r->y=y;r->raw=raw;
    snprintf(r->text,sizeof(r->text),"%s",text);
}
static void line(int y,const char *text){row(y,text,false);}
static void raw_line(int y,const char *text){row(y,text,true);}
static void render_rows(void){
    bool chinese=false;
    for(unsigned i=0;i<3;i++)if(!s_rows[i].raw && ui_zh_lookup(s_rows[i].text)!=s_rows[i].text)chinese=true;
    if(s_page==M_WIFI_PASSWORD)chinese=false;
    if(!chinese){
        if(esp_timer_get_time()<s_notice_until){u8g2_SetDrawColor(&u8g2,0);u8g2_DrawBox(&u8g2,0,23,128,9);u8g2_SetDrawColor(&u8g2,1);}
        for(unsigned i=0;i<3;i++)if(s_rows[i].text[0])u8g2_DrawStr(&u8g2,2,s_rows[i].y,s_rows[i].text);
        return;
    }
    u8g2_SetDrawColor(&u8g2,0);u8g2_DrawBox(&u8g2,0,0,128,32);u8g2_SetDrawColor(&u8g2,1);
    /* A 32px display cannot fit three 12px Chinese rows. Keep the title, then
     * alternate detail/help every 3s; a control input shows the detail first. */
    unsigned second=(ms()-s_view_at)/3000%2?2:1;
    if(!s_rows[second].text[0])second=second==1?2:1;
    if(esp_timer_get_time()<s_notice_until)second=2;
    for(unsigned n=0;n<2;n++){
        tool_row_t *r=&s_rows[n?second:0];int y=n?27:11;
        if(r->raw)u8g2_DrawStr(&u8g2,2,y,r->text);else ui_zh_draw(&u8g2,2,y,r->text);
    }
}
static void notice(const char *text) {
    snprintf(s_notice, sizeof(s_notice), "%.25s", text); s_notice_until = esp_timer_get_time() + 1200000;
}
static void rotate(int direction, unsigned count) {
    if (count) s_selected = direction > 0 ? (s_selected + 1) % count : (s_selected + count - 1) % count;
}
static void back(void) {
    ui.index = ui.parent_index[ui.layer]; ui.state = S_LAYER_OUT;
    wireless_tools_on_enter(ui.index);
}
static void ble_page(int key) {
    unsigned count; scan_peer_t selected = {0}; bool found = false;
    portENTER_CRITICAL(&s_lock);
    count = s_peer_count;
    if (count && s_selected >= count) s_selected = count - 1;
    if (!s_detail && count) { selected = s_peers[s_selected]; found = true; }
    if (s_detail) for (unsigned i = 0; i < count; ++i)
        if (s_peers[i].type == s_target_type && !memcmp(s_peers[i].mac, s_target, 6)) {
            selected = s_peers[i]; found = true; break;
        }
    portEXIT_CRITICAL(&s_lock);
    if (key == BTN_ID_BACK) { if (s_detail) s_detail = false; else back(); return; }
    if (!s_detail) {
        if (key == BTN_ID_CW || key == BTN_ID_CC) rotate(key == BTN_ID_CW ? 1 : -1, count);
        if (key == BTN_ID_LP) {
            portENTER_CRITICAL(&s_lock); s_peer_count = 0; portEXIT_CRITICAL(&s_lock); s_selected = 0;
        }
        if (key == BTN_ID_SP && found) {
            memcpy(s_target, selected.mac, 6); s_target_type = selected.type; s_detail = true;
            memset(s_history, 0, sizeof(s_history)); s_sample = ms() - 250;
        }
    }
    char text[26];
    if (!s_detail) {
        snprintf(text, sizeof(text), "BLE %02u/%02u %ddBm", count ? s_selected+1 : 0, count, selected.rssi); raw_line(7,text);
        if (found && selected.name[0]) snprintf(text,sizeof(text),"%.24s",selected.name);
        else if (found) snprintf(text,sizeof(text),"%02X:%02X:%02X:%02X:%02X:%02X",selected.mac[0],selected.mac[1],selected.mac[2],selected.mac[3],selected.mac[4],selected.mac[5]);
        else snprintf(text,sizeof(text),"Scanning broadcasts...");
        raw_line(18,text); line(30,"SP:SIGNAL HOLD:CLEAR");
    } else {
        if (!found) { line(7,"DEVICE EXPIRED"); line(19,"BACK: DEVICE LIST"); return; }
        if (selected.name[0]) snprintf(text,sizeof(text),"%.24s",selected.name);
        else snprintf(text,sizeof(text),"%02X:%02X:%02X:%02X:%02X:%02X",selected.mac[0],selected.mac[1],selected.mac[2],selected.mac[3],selected.mac[4],selected.mac[5]);
        raw_line(7,text);
        uint32_t age = ms() - selected.seen;
        if (key == BTN_ID_SP) s_entered = !s_entered;
        if (s_entered) snprintf(text,sizeof(text),"MFG:%04X UUID:%s",selected.company,selected.service?"YES":"NO");
        else snprintf(text,sizeof(text),"%ddBm  %s %lus",selected.rssi, age < 3000 ? "LIVE" : "LAST",(unsigned long)(age/1000));
        raw_line(17,text);
        if (ms() - s_sample >= 250) {
            s_sample = ms(); memmove(s_history,s_history+1,sizeof(s_history)-1);
            int level = (selected.rssi+100)/7;
            s_history[47] = age < 3000 ? (uint8_t)(level < 1 ? 1 : level > 10 ? 10 : level) : 0;
        }
        for (unsigned i=0;i<48;i++) if(s_history[i]) u8g2_DrawVLine(&u8g2,3+i*2,32-s_history[i],s_history[i]);
    }
}
static const char *auth_name(wifi_auth_mode_t auth) {
    switch (auth) {
    case WIFI_AUTH_OPEN:return "OPEN"; case WIFI_AUTH_WEP:return "WEP";
    case WIFI_AUTH_WPA_PSK:return "WPA"; case WIFI_AUTH_WPA2_PSK:return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:return "WPA/WPA2"; case WIFI_AUTH_WPA3_PSK:return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:return "WPA2/WPA3"; default:return "ENTERPRISE/OTHER";
    }
}
static void wifi_page(int key) {
    if (key == BTN_ID_BACK) { back(); return; }
    if (!s_entered) {
        wifi_station_status_t current;wifi_station_status(&current);
        if(!current.scanning && !wifi_station_scan())notice("WI-FI BUSY / RETRY");
        s_entered = true;
    }
    wifi_station_status_t status;wifi_station_status(&status);
    unsigned count=status.count; int error=status.error; wifi_ap_record_t ap = {0};
    if (count && s_selected >= count) s_selected = count-1;
    if (count)wifi_station_ap(s_selected,&ap);
    if (key == BTN_ID_CW || key == BTN_ID_CC) rotate(key == BTN_ID_CW ? 1 : -1,count);
    if (key == BTN_ID_SP) s_detail = !s_detail;
    if (key == BTN_ID_LP && !status.scanning)wifi_station_scan();
    char text[26];
    if(error) { snprintf(text,sizeof(text),"WIFI ERROR %X",error); raw_line(10,text); line(27,"BACK: EXIT / RETRY"); return; }
    if (!count) { line(10,status.scanning ? "SCANNING WI-FI..." : "NO HOTSPOTS FOUND"); line(27,"HOLD:SCAN BACK:EXIT"); return; }
    snprintf(text,sizeof(text),"WIFI %02u/%02u %ddBm",(s_selected+1)%100,count%100,ap.rssi); raw_line(7,text);
    if (s_detail) snprintf(text,sizeof(text),"%02X:%02X:%02X:%02X:%02X:%02X",ap.bssid[0],ap.bssid[1],ap.bssid[2],ap.bssid[3],ap.bssid[4],ap.bssid[5]);
    else { printable(text,sizeof(text),ap.ssid,sizeof(ap.ssid)); if(!text[0]) snprintf(text,sizeof(text),"<HIDDEN SSID>"); }
    raw_line(18,text);
    snprintf(text,sizeof(text),"CH%02u %.9s%s",ap.primary%100,auth_name(ap.authmode),status.scanning?" SCAN":""); raw_line(30,text);
}
static int wifi_menu(M_SELECT *rows,unsigned count,int key){
    ui.num[ui.index]=(uint8_t)count;
    if(ui.select[ui.layer]>=count)ui.select[ui.layer]=0;
    list_show(rows,ui.index);
    if(key==BTN_ID_BACK){back();return -1;}
    if(key==BTN_ID_CW || key==BTN_ID_CC)list_rotate_switch();
    return key==BTN_ID_SP?ui.select[ui.layer]:-1;
}
static void wifi_connection_page(int key){
    wifi_station_status_t status;wifi_station_status(&status);
    static char auto_label[32];
    static M_SELECT rows[]={{"- Scan / choose Wi-Fi"},{"- Connection status"},{"- Connect saved"},
        {"- Disconnect"},{"- Forget saved"},{auto_label}};
    snprintf(auto_label,sizeof(auto_label),"- Auto reconnect: %s",status.automatic?"ON":"OFF");
    int selected=wifi_menu(rows,6,key);
    if(selected==0)ui_navigate_to(M_WIFI_NETWORKS);
    if(selected==1)ui_navigate_to(M_WIFI_STATUS);
    if(selected==2){
        if(!status.saved)notice("NO SAVED NETWORK");
        else if(wifi_station_connect_saved())ui_navigate_to(M_WIFI_STATUS);
        else notice("WI-FI BUSY / RETRY");
    }
    if(selected==3){if(wifi_station_disconnect())ui_navigate_to(M_WIFI_STATUS);else notice("WI-FI BUSY / RETRY");}
    if(selected==4){if(status.saved)ui_navigate_to(M_WIFI_FORGET);else notice("NO SAVED NETWORK");}
    if(selected==5){
        if(!status.saved)notice("CONNECT AND SAVE FIRST");
        else if(!wifi_station_auto(!status.automatic))notice("WI-FI BUSY / RETRY");
    }
}
static void wifi_networks_page(int key){
    if(key==BTN_ID_BACK){back();return;}
    wifi_station_status_t status;wifi_station_status(&status);
    if(!s_entered){if(!status.scanning && !wifi_station_scan())notice("WI-FI BUSY / RETRY");s_entered=true;}
    if(key==BTN_ID_LP && !status.scanning)wifi_station_scan();
    wifi_station_status(&status);
    if(status.scanning){line(10,"SCANNING 2.4GHz...");line(28,"BACK: CANCEL VIEW");return;}
    if(status.error){char text[26];snprintf(text,sizeof(text),"SCAN ERROR %X",status.error);raw_line(10,text);line(28,"HOLD:RETRY BACK:EXIT");return;}
    if(!status.count){line(10,"NO HOTSPOTS FOUND");line(28,"HOLD:SCAN BACK:EXIT");return;}
    static M_SELECT rows[WIFI_STATION_AP_MAX];
    static char labels[WIFI_STATION_AP_MAX][32];
    for(unsigned i=0;i<status.count;i++){
        wifi_ap_record_t ap;char name[21];
        if(!wifi_station_ap(i,&ap))return;
        printable(name,sizeof(name),ap.ssid,sizeof(ap.ssid));
        snprintf(labels[i],sizeof(labels[i]),"- %s %ddB",name[0]?name:"<HIDDEN>",ap.rssi);rows[i].m_select=labels[i];
    }
    int selected=wifi_menu(rows,status.count,key);
    if(selected>=0 && wifi_station_ap((unsigned)selected,&s_network)){
        if(!s_network.ssid[0]){notice("HIDDEN SSID NOT SUPPORTED");return;}
        if(!wifi_station_supported(s_network.authmode)){notice("WPA3/WEP/ENTERPRISE: NO");return;}
        if(s_network.authmode==WIFI_AUTH_OPEN){
            if(wifi_station_connect(&s_network,""))ui_navigate_to(M_WIFI_STATUS);
            else notice("WI-FI BUSY / RETRY");
        }else ui_navigate_to(M_WIFI_PASSWORD);
    }
}
static void wifi_password_page(int key){
    if(key==BTN_ID_BACK){if(!message_draft_group_back(&s_wifi_selector))back();return;}
    if(key==BTN_ID_CW || key==BTN_ID_CC){
        int direction=key==BTN_ID_CW?-1:1;
        message_draft_group_rotate(&s_wifi_selector,direction);
        if(s_wifi_selector.selecting_group && s_wifi_selector.group==DRAFT_EMOTE_GROUP)
            message_draft_group_rotate(&s_wifi_selector,direction);
    }
    if(key==BTN_ID_LP)message_draft_select_send(&s_wifi_selector);
    unsigned choice=message_draft_group_choice(&s_wifi_selector);
    if(key==BTN_ID_SP){
        if(s_wifi_selector.selecting_group && s_wifi_selector.group<=DRAFT_SYMBOLS)s_wifi_selector.selecting_group=false;
        else if(choice==DRAFT_SEND){
            if(wifi_station_connect(&s_network,s_wifi_password)){
                memset(s_wifi_password,0,sizeof(s_wifi_password));ui_navigate_to(M_WIFI_STATUS);return;
            }else notice("8-63 ASCII OR 64 HEX");
        }else if(!message_draft_edit(s_wifi_password,sizeof(s_wifi_password),choice))notice("PASSWORD MAXIMUM 64");
    }
    unsigned length=(unsigned)strlen(s_wifi_password);char text[28],mask[22];
    snprintf(text,sizeof(text),"PASSWORD %u/64",length);raw_line(7,text);
    unsigned shown=length>20?20:length;memset(mask,'*',shown);mask[shown]=0;raw_line(18,mask);
    const char *name=s_wifi_selector.group==DRAFT_SEND_GROUP?"CONNECT":message_draft_group_name(s_wifi_selector.group);
    if(s_wifi_selector.selecting_group)snprintf(text,sizeof(text),"<%s> HOLD:CONNECT",name);
    else snprintf(text,sizeof(text),"%s <%c> BACK:GROUP",name,(char)(32+choice));
    raw_line(30,text);
}
static void wifi_status_page(int key){
    if(key==BTN_ID_BACK){back();return;}
    if(key==BTN_ID_LP)wifi_station_disconnect();
    wifi_station_status_t status;wifi_station_status(&status);char text[28],name[23];
    printable(name,sizeof(name),(const uint8_t*)(status.ssid[0]?status.ssid:status.saved_ssid),33);
    if(status.online){
        raw_line(7,name);line(18,status.ip);
        if(status.error){snprintf(text,sizeof(text),"LINK OK / ERR %X",status.error);raw_line(30,text);}
        else {snprintf(text,sizeof(text),"CONNECTED %ddB HOLD:OFF",status.rssi);raw_line(30,text);}
    }else{
        line(7,status.connecting?"CONNECTING / DHCP...":"WI-FI DISCONNECTED");raw_line(18,name);
        if(status.error){snprintf(text,sizeof(text),"ERR %X REASON %u",status.error,status.reason);raw_line(30,text);}
        else line(30,"BACK:MENU HOLD:STOP");
    }
}
static void wifi_forget_page(int key){
    static M_SELECT rows[]={{"- Keep saved network"},{"- Forget + disconnect"}};
    int selected=wifi_menu(rows,2,key);
    if(selected==0)back();
    if(selected==1){if(wifi_station_forget())back();else notice("WI-FI BUSY / RETRY");}
}
static void cloud_page(int key){
    static unsigned selected;
    if(key==BTN_ID_BACK){back();return;}
    if(key==BTN_ID_CW)selected=(selected+1)%4;
    if(key==BTN_ID_CC)selected=(selected+3)%4;
    cloud_sync_status_t state;cloud_sync_status(&state);
    if(key==BTN_ID_LP && selected==0){if(!cloud_sync_enable(!state.enabled))notice("SAVE/REMOVE ERROR");}
    if(key==BTN_ID_SP && selected==0)notice("PUBLIC DATA: HOLD TO SET");
    if(key==BTN_ID_SP && selected==1){if(!cloud_sync_now())notice("ENABLE SHARING FIRST");}
    if(key==BTN_ID_LP && selected==2){if(!cloud_sync_remove())notice("CLOUD SAVE ERROR");}
    if(key==BTN_ID_SP && selected==2)notice("HOLD:REMOVE ONLINE DATA");
    if(key==BTN_ID_SP && selected==3){ui_navigate_to(M_WIFI_CONNECT);return;}
    cloud_sync_status(&state);char value[28];
    if(state.result && state.result!=200 && !state.busy)
        snprintf(value,sizeof(value),"CLOUD #%s %s ERR%d",state.id[0]?state.id:"------",state.enabled?"ON":"OFF",state.result);
    else snprintf(value,sizeof(value),"CLOUD #%s SHARE:%s",state.id[0]?state.id:"------",state.enabled?"ON":"OFF");
    raw_line(7,value);
    static const char *const labels[]={"HOLD: SHARE ON/OFF","CLICK: SYNC NOW","HOLD: REMOVE ONLINE","CLICK: WI-FI SETTINGS"};line(18,labels[selected]);
    line(30,state.text);
}
static void remote_page(int key) {
    if (key == BTN_ID_BACK) { back(); return; }
    bool camera=s_page==M_HID_CAMERA;
    if(camera) {
        if(key==BTN_ID_CW)s_camera_key=(s_camera_key+1)%CAMERA_KEY_COUNT;
        if(key==BTN_ID_CC)s_camera_key=(s_camera_key+CAMERA_KEY_COUNT-1)%CAMERA_KEY_COUNT;
        if(key==BTN_ID_LP)s_camera_pulse=(s_camera_pulse+1)%CAMERA_PULSE_COUNT;
    }
    bool media = camera?camera_keys[s_camera_key].media:s_page == M_HID_MEDIA;
    if (!s_entered) {
        wifi_station_status_t wifi;wifi_station_status(&wifi);
        if (wifi.scanning) { line(10,"FINISHING WI-FI SCAN"); return; }
        if (!ble_radio_is_ready()) { line(10,"RADIO STARTING..."); return; }
        if (esp_timer_get_time() < s_retry_at) { line(10,"WAITING FOR MESSAGES"); line(27,"BACK: EXIT"); return; }
        if (!message_service_pause_for_tools()) { notice("MESSAGES BUSY: RETRY"); s_retry_at=esp_timer_get_time()+700000; }
        else {
            s_remote_lease = true;
            ble_radio_tools_restore();
            if (!tool_hid_start()) { notice("HID START FAILED"); tool_hid_stop(); ble_radio_tools_restore(); message_service_resume(); s_remote_lease=false; }
            s_entered = true;
        }
    }
    if (!s_remote_lease) { line(10,"REMOTE UNAVAILABLE"); line(27,"BACK: RETRY"); return; }
    tool_hid_tick();
    tool_hid_status_t state; tool_hid_status(&state);
    bool usable = state.connected && state.encrypted && (media ? state.media_ready : state.keyboard_ready);
    if(camera){
        char title[26];snprintf(title,sizeof(title),"CAMERA %s %ums",camera_keys[s_camera_key].name,camera_pulses[s_camera_pulse]);raw_line(7,title);
    }else line(7,media?"MEDIA REMOTE":"SLIDE REMOTE");
    if (state.error) {
        char text[26]; snprintf(text,sizeof(text),"BLUETOOTH ERROR %d",state.error);
        raw_line(18,text); line(30,"BACK: EXIT / RETRY");
    }
    else if (!state.connected) { line(18,"PAIR: Halfstep Remote"); line(30,"MESSAGING PAUSED"); }
    else if (!usable) {
        line(18,media?"WAITING: MEDIA REPORT":"WAITING: KEYBOARD REPORT");
        line(30,camera?"TURN:TRY ANOTHER KEY":"WAIT FOR HOST...");
    }
    else {
        if(camera){
            line(18,"TURN:KEY CLICK:SHOOT");line(30,"HOLD:PULSE BACK:EXIT");
            if(key==BTN_ID_SP){
                if(tool_hid_press_for(media,camera_keys[s_camera_key].usage,camera_pulses[s_camera_pulse]))notice("KEY SENT - CHECK PHONE");
                else notice("KEY BUSY / NOT SENT");
            }
            return;
        }
        line(18,media?"TURN:VOL SP:PLAY/PAUSE":"TURN:PREV/NEXT SP:SPACE");
        line(30,media?"HOLD:NEXT TRACK":"HOLD:START SLIDES (F5)");
        uint16_t usage = 0;
        if(key==BTN_ID_CW) usage=media?0xEA:0x4B;
        if(key==BTN_ID_CC) usage=media?0xE9:0x4E;
        if(key==BTN_ID_SP) usage=media?0xCD:0x2C;
        if(key==BTN_ID_LP) usage=media?0xB5:0x3E;
        if(usage && !tool_hid_press(media,usage)) notice("KEY BUSY / NOT SENT");
    }
}
void wireless_tools_proc(void) {
    memset(s_rows,0,sizeof(s_rows));
    u8g2_SetDrawColor(&u8g2,1); u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    if (ui.index == M_TOOLS || ui.index==M_TOOLS_ACTIVITIES || ui.index==M_SETTINGS_NETWORK || (ui.index>=M_TOOLS_REMOTE && ui.index<=M_TOOLS_SOCIAL)) {
        static M_SELECT root[]={TOOLS_MENU_ITEMS(MENU_LABEL)},remote[]={TOOLS_REMOTE_ITEMS(MENU_LABEL)},connect[]={TOOLS_CONNECT_ITEMS(MENU_LABEL)},diagnostic[]={TOOLS_DIAGNOSTIC_ITEMS(MENU_LABEL)},social[]={TOOLS_SOCIAL_ITEMS(MENU_LABEL)};
        static const uint8_t root_pages[]={TOOLS_MENU_ITEMS(MENU_PAGE)},remote_pages[]={TOOLS_REMOTE_ITEMS(MENU_PAGE)},connect_pages[]={TOOLS_CONNECT_ITEMS(MENU_PAGE)},diagnostic_pages[]={TOOLS_DIAGNOSTIC_ITEMS(MENU_PAGE)},social_pages[]={TOOLS_SOCIAL_ITEMS(MENU_PAGE)};
        M_SELECT *rows=root;const uint8_t *pages=root_pages;unsigned count=sizeof(root_pages);
        static M_SELECT activities[]={TOOLS_ACTIVITIES_ITEMS(MENU_LABEL)},network[]={SETTINGS_NETWORK_ITEMS(MENU_LABEL)};
        static const uint8_t activities_pages[]={TOOLS_ACTIVITIES_ITEMS(MENU_PAGE)},network_pages[]={SETTINGS_NETWORK_ITEMS(MENU_PAGE)};
        if(ui.index==M_TOOLS_ACTIVITIES){rows=activities;pages=activities_pages;count=sizeof(activities_pages);}
        if(ui.index==M_SETTINGS_NETWORK){rows=network;pages=network_pages;count=sizeof(network_pages);}
        if(ui.index==M_TOOLS_REMOTE){rows=remote;pages=remote_pages;count=sizeof(remote_pages);}
        if(ui.index==M_TOOLS_CONNECT){rows=connect;pages=connect_pages;count=sizeof(connect_pages);}
        if(ui.index==M_TOOLS_DIAGNOSTIC){rows=diagnostic;pages=diagnostic_pages;count=sizeof(diagnostic_pages);}
        if(ui.index==M_TOOLS_SOCIAL){rows=social;pages=social_pages;count=sizeof(social_pages);}
        ui.num[ui.index]=count;list_show(rows,ui.index);
        if(btn.pressed) {
            int key=btn.id; btn.pressed=false;
            if(key==BTN_ID_CW || key==BTN_ID_CC) list_rotate_switch();
            if(key==BTN_ID_BACK) back();
            if(key==BTN_ID_SP && ui.select[ui.layer]<count)ui_navigate_to(pages[ui.select[ui.layer]]);
        }
        return;
    }
    int key = btn.pressed ? btn.id : -1; btn.pressed=false;
    static uint8_t previous_page=255;
    if(previous_page!=ui.index || key!=-1){s_view_at=ms();previous_page=ui.index;}
    if (ui.index == M_BLE_SCAN) ble_page(key);
    else if (ui.index == M_WIFI_SCAN) wifi_page(key);
    else if (ui.index == M_WIFI_CONNECT)wifi_connection_page(key);
    else if (ui.index == M_WIFI_NETWORKS)wifi_networks_page(key);
    else if (ui.index == M_WIFI_PASSWORD)wifi_password_page(key);
    else if (ui.index == M_WIFI_STATUS)wifi_status_page(key);
    else if (ui.index == M_WIFI_FORGET)wifi_forget_page(key);
    else if (ui.index == M_CLOUD_SYNC)cloud_page(key);
    else if (ui.index == M_HID_SLIDES || ui.index == M_HID_MEDIA || ui.index==M_HID_CAMERA) remote_page(key);
    else if (ui.index == M_TOOL_MEMORY) {
        char text[26];
        line(7,"RUNTIME HEAP");
        snprintf(text,sizeof(text),"FREE %uK MIN %uK",(unsigned)(heap_caps_get_free_size(MALLOC_CAP_8BIT)/1024)%10000,(unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)/1024)%10000); raw_line(18,text);
        snprintf(text,sizeof(text),"LARGEST BLOCK %uK",(unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)/1024)); raw_line(30,text);
        if(key==BTN_ID_BACK) back();
    }
    if (esp_timer_get_time() < s_notice_until) {
        line(30,s_notice);
    }
    render_rows();
}
