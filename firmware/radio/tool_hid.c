/* HID over GATT on the existing NimBLE host. Advertising instance 0 is leased
 * only while message_service is paused. No second Bluetooth host is created. */
#include "tool_hid.h"
#include "tool_hid_report.h"
#include "ble_radio.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

void ble_store_config_init(void);
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static tool_hid_status_t s_status;
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE, s_kbd_handle, s_media_handle;
static bool s_initialized, s_advertising, s_release_pending, s_release_media;
static uint16_t s_release_conn;
static int64_t s_release_at;
static int64_t s_adv_attempt;
static const uint8_t s_report_map[] = {
    /* Report 1: standard eight-byte keyboard input and LED output. */
    0x05,0x01,0x09,0x06,0xA1,0x01,0x85,0x01,
    0x05,0x07,0x19,0xE0,0x29,0xE7,0x15,0x00,0x25,0x01,
    0x75,0x01,0x95,0x08,0x81,0x02,
    0x95,0x01,0x75,0x08,0x81,0x01,
    0x95,0x05,0x75,0x01,0x05,0x08,0x19,0x01,0x29,0x05,0x91,0x02,
    0x95,0x01,0x75,0x03,0x91,0x01,
    0x95,0x06,0x75,0x08,0x15,0x00,0x25,0x65,0x05,0x07,0x19,0x00,0x29,0x65,0x81,0x00,0xC0,
    /* Report 2: one 16-bit Consumer Control usage. */
    0x05,0x0C,0x09,0x01,0xA1,0x01,0x85,0x02,
    0x15,0x00,0x26,0xFF,0x03,0x19,0x00,0x2A,0xFF,0x03,
    0x75,0x10,0x95,0x01,0x81,0x00,0xC0
};
enum { HID_INFO, HID_MAP, HID_CONTROL, HID_PROTOCOL, HID_KBD, HID_MEDIA, HID_LED,
       REF_KBD, REF_MEDIA, REF_LED, DIS_MANUFACTURER, DIS_PNP };
static uint8_t s_protocol = 1, s_led;
static int access_cb(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn; (void)attr;
    unsigned field = (uintptr_t)arg;
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t value;
        if (OS_MBUF_PKTLEN(ctxt->om) != 1 || ble_hs_mbuf_to_flat(ctxt->om, &value, 1, NULL))
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        if (field == HID_PROTOCOL) {
            if (value != 1) return BLE_ATT_ERR_VALUE_NOT_ALLOWED; /* Report protocol only. */
            s_protocol = value;
        } else if (field == HID_LED) s_led = value;
        else if (field != HID_CONTROL || value > 1) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        return 0;
    }
    const uint8_t info[] = {0x11,0x01,0x00,0x02};
    const uint8_t kref[] = {1,1}, mref[] = {2,1}, lref[] = {1,2};
    const uint8_t zeros[8] = {0};
    /* Bluetooth SIG vendor ID source, generic product identity. */
    const uint8_t pnp[] = {1,0xFF,0xFF,0x01,0x00,0x01,0x00};
    const void *data = zeros; size_t len = 1;
    switch (field) {
    case HID_INFO: data = info; len = sizeof(info); break;
    case HID_MAP: data = s_report_map; len = sizeof(s_report_map); break;
    case HID_PROTOCOL: data = &s_protocol; break;
    case HID_KBD: len = 8; break;
    case HID_MEDIA: len = 2; break;
    case HID_LED: data = &s_led; break;
    case REF_KBD: data = kref; len = 2; break;
    case REF_MEDIA: data = mref; len = 2; break;
    case REF_LED: data = lref; len = 2; break;
    case DIS_MANUFACTURER: data = "Halfstep"; len = 8; break;
    case DIS_PNP: data = pnp; len = sizeof(pnp); break;
    default: return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }
    return os_mbuf_append(ctxt->om, data, len) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
}
#define UUID(name, value) static const ble_uuid16_t name = BLE_UUID16_INIT(value)
UUID(u_hid,0x1812); UUID(u_info,0x2A4A); UUID(u_map,0x2A4B);
UUID(u_control,0x2A4C); UUID(u_protocol,0x2A4E); UUID(u_report,0x2A4D); UUID(u_ref,0x2908);
UUID(u_dis,0x180A); UUID(u_manufacturer,0x2A29); UUID(u_pnp,0x2A50);
#define ARG(n) ((void *)(uintptr_t)(n))
#define REF(n) ((struct ble_gatt_dsc_def[]){{.uuid=&u_ref.u,.att_flags=BLE_ATT_F_READ,.access_cb=access_cb,.arg=ARG(n)}, {0}})
static const struct ble_gatt_svc_def s_services[] = {
    {.type=BLE_GATT_SVC_TYPE_PRIMARY, .uuid=&u_hid.u,
     .characteristics=(struct ble_gatt_chr_def[]){
        {.uuid=&u_info.u,.access_cb=access_cb,.arg=ARG(HID_INFO),.flags=BLE_GATT_CHR_F_READ},
        {.uuid=&u_map.u,.access_cb=access_cb,.arg=ARG(HID_MAP),.flags=BLE_GATT_CHR_F_READ},
        {.uuid=&u_control.u,.access_cb=access_cb,.arg=ARG(HID_CONTROL),.flags=BLE_GATT_CHR_F_WRITE_NO_RSP},
        {.uuid=&u_protocol.u,.access_cb=access_cb,.arg=ARG(HID_PROTOCOL),.flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_WRITE_NO_RSP},
        {.uuid=&u_report.u,.access_cb=access_cb,.arg=ARG(HID_KBD),.flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_READ_ENC|BLE_GATT_CHR_F_NOTIFY,
         .val_handle=&s_kbd_handle,.descriptors=REF(REF_KBD)},
        {.uuid=&u_report.u,.access_cb=access_cb,.arg=ARG(HID_MEDIA),.flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_READ_ENC|BLE_GATT_CHR_F_NOTIFY,
         .val_handle=&s_media_handle,.descriptors=REF(REF_MEDIA)},
        {.uuid=&u_report.u,.access_cb=access_cb,.arg=ARG(HID_LED),.flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_WRITE_NO_RSP,
         .descriptors=REF(REF_LED)}, {0}}},
    {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=&u_dis.u,
     .characteristics=(struct ble_gatt_chr_def[]){
        {.uuid=&u_manufacturer.u,.access_cb=access_cb,.arg=ARG(DIS_MANUFACTURER),.flags=BLE_GATT_CHR_F_READ},
        {.uuid=&u_pnp.u,.access_cb=access_cb,.arg=ARG(DIS_PNP),.flags=BLE_GATT_CHR_F_READ}, {0}}}, {0}
};
int tool_hid_init(void) {
    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_gatts_count_cfg(s_services);
    if (!rc) rc = ble_gatts_add_svcs(s_services);
    if (rc) return rc;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();
    s_initialized = true;
    return 0;
}
static int gap_cb(struct ble_gap_event *e, void *arg) {
    (void)arg;
    switch (e->type) {
    case BLE_GAP_EVENT_CONNECT:
        portENTER_CRITICAL(&s_lock);
        s_advertising = false;
        s_status.connected = e->connect.status == 0;
        s_conn = s_status.connected ? e->connect.conn_handle : BLE_HS_CONN_HANDLE_NONE;
        s_status.error = e->connect.status;
        bool allowed = s_status.enabled;
        portEXIT_CRITICAL(&s_lock);
        if (e->connect.status == 0) {
            if (allowed) ble_gap_security_initiate(e->connect.conn_handle);
            else ble_gap_terminate(e->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        portENTER_CRITICAL(&s_lock);
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        s_status.connected = s_status.encrypted = false;
        s_status.keyboard_ready = s_status.media_ready = false;
        portEXIT_CRITICAL(&s_lock);
        break;
    case BLE_GAP_EVENT_ENC_CHANGE: {
        struct ble_gap_conn_desc desc;
        bool encrypted = !e->enc_change.status && !ble_gap_conn_find(e->enc_change.conn_handle, &desc) && desc.sec_state.encrypted;
        portENTER_CRITICAL(&s_lock);
        s_status.encrypted = encrypted; s_status.error = e->enc_change.status;
        portEXIT_CRITICAL(&s_lock);
        break;
    }
    case BLE_GAP_EVENT_SUBSCRIBE:
        portENTER_CRITICAL(&s_lock);
        if (e->subscribe.attr_handle == s_kbd_handle) s_status.keyboard_ready = e->subscribe.cur_notify;
        if (e->subscribe.attr_handle == s_media_handle) s_status.media_ready = e->subscribe.cur_notify;
        portEXIT_CRITICAL(&s_lock);
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        portENTER_CRITICAL(&s_lock); s_advertising = false; portEXIT_CRITICAL(&s_lock);
        break;
    default: break;
    }
    return 0;
}
static bool advertise(void) {
    s_adv_attempt = esp_timer_get_time();
    struct ble_gap_ext_adv_params p = {0};
    p.legacy_pdu = p.connectable = p.scannable = 1;
    p.own_addr_type = BLE_OWN_ADDR_RANDOM;
    p.primary_phy = p.secondary_phy = BLE_HCI_LE_PHY_1M;
    p.itvl_min = 160; p.itvl_max = 240;
    /* The controller retains extended-PDU parameters and data on this handle.
     * Remove the inactive messaging set before converting it to legacy HID. */
    ble_gap_ext_adv_remove(0);
    int rc = ble_gap_ext_adv_configure(0, &p, NULL, gap_cb, NULL);
    /* Flags, HID UUID, keyboard appearance, complete device name. */
    const uint8_t data[] = {2,1,6,3,3,0x12,0x18,3,0x19,0xC1,0x03,
        16,9,'H','a','l','f','s','t','e','p',' ','R','e','m','o','t','e'};
    if (!rc) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(data, sizeof(data));
        rc = om ? ble_gap_ext_adv_set_data(0, om) : BLE_HS_ENOMEM;
    }
    if (!rc) rc = ble_gap_ext_adv_start(0, 0, 0);
    portENTER_CRITICAL(&s_lock); s_advertising = rc == 0; s_status.error = rc; portEXIT_CRITICAL(&s_lock);
    if (rc) ESP_LOGE("tool_hid", "connectable advertising: %d", rc);
    return rc == 0;
}
bool tool_hid_start(void) {
    if (!s_initialized || !ble_radio_is_ready()) return false;
    portENTER_CRITICAL(&s_lock);
    s_status.enabled = true; s_status.error = 0;
    bool busy = s_conn != BLE_HS_CONN_HANDLE_NONE;
    portEXIT_CRITICAL(&s_lock);
    s_release_pending = false;
    ble_svc_gap_device_name_set("Halfstep Remote");
    if (busy) return true;
    return advertise();
}
void tool_hid_status(tool_hid_status_t *out) {
    portENTER_CRITICAL(&s_lock); *out = s_status; portEXIT_CRITICAL(&s_lock);
}
static int notify(uint16_t conn, bool media, const uint8_t *bytes) {
    struct os_mbuf *om = ble_hs_mbuf_from_flat(bytes, media ? 2 : 8);
    return om ? ble_gatts_notify_custom(conn, media ? s_media_handle : s_kbd_handle, om) : BLE_HS_ENOMEM;
}
bool tool_hid_press(bool media, uint16_t usage) {
    return tool_hid_press_for(media,usage,45);
}
bool tool_hid_press_for(bool media,uint16_t usage,uint16_t milliseconds) {
    uint8_t bytes[8];
    if(!tool_hid_pulse_valid(milliseconds) || !tool_hid_encode(media,usage,bytes))return false;
    tool_hid_status_t status; tool_hid_status(&status);
    if (!status.enabled || !status.connected || !status.encrypted ||
        !(media ? status.media_ready : status.keyboard_ready) || s_release_pending) return false;
    portENTER_CRITICAL(&s_lock); uint16_t conn = s_conn; portEXIT_CRITICAL(&s_lock);
    int rc = notify(conn, media, bytes);
    if (rc) return false;
    s_release_conn = conn; s_release_media = media;
    s_release_at = esp_timer_get_time() + (int64_t)milliseconds*1000; s_release_pending = true;
    return true;
}
void tool_hid_tick(void) {
    tool_hid_status_t status; tool_hid_status(&status);
    if (!status.enabled) return;
    portENTER_CRITICAL(&s_lock); uint16_t conn = s_conn; bool adv = s_advertising; portEXIT_CRITICAL(&s_lock);
    if (s_release_pending && esp_timer_get_time() >= s_release_at) {
        const uint8_t zeros[8] = {0};
        if (conn != s_release_conn || !status.connected || notify(conn, s_release_media, zeros) == 0)
            s_release_pending = false;
    }
    if (!status.connected && !adv && esp_timer_get_time() - s_adv_attempt > 1000000) advertise();
}
void tool_hid_stop(void) {
    portENTER_CRITICAL(&s_lock);
    s_status.enabled = false;
    uint16_t conn = s_conn; bool adv = s_advertising; s_advertising = false;
    portEXIT_CRITICAL(&s_lock);
    if (adv) ble_gap_ext_adv_stop(0);
    ble_gap_ext_adv_remove(0);
    if (conn != BLE_HS_CONN_HANDLE_NONE) {
        const uint8_t zeros[8] = {0};
        notify(conn, false, zeros); notify(conn, true, zeros);
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    }
    s_release_pending = false;
    ble_svc_gap_device_name_set("BLUE_LONG_MESH");
}
