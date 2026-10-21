#include "ble_radio.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include <string.h>
#if !CONFIG_BT_NIMBLE_EXT_ADV
#error "Local messaging v3 requires CONFIG_BT_NIMBLE_EXT_ADV"
#endif
static const char *TAG = "ble_radio";
static ble_radio_rx_cb_t s_rx;
static ble_radio_state_cb_t s_state_cb;
static volatile bool s_ready, s_adv, s_scan, s_last_ok;
static bool s_port_inited, s_host_started, s_configured, s_fast;
static uint8_t s_mac[6], s_efuse[8], s_error;
static ble_phy_t s_phy = BLE_PHY_1M, s_config_phy = BLE_PHY_1M;
static bool s_config_fast;
static volatile uint32_t s_adv_start_ms, s_adv_duration_ms;
static ble_radio_state_t s_state = BLE_RADIO_OFF;
static uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}
static void publish_state(void) {
    ble_radio_state_t n = !s_ready ? BLE_RADIO_OFF
                          : s_adv  ? (s_scan ? BLE_RADIO_SCAN_ADV : BLE_RADIO_ADVERTISING)
                                   : (s_scan ? BLE_RADIO_SCANNING : BLE_RADIO_IDLE);
    ble_radio_state_t old = s_state;
    s_state = n;
    if (old != n && s_state_cb)
        s_state_cb(old, n);
}
static void receive_ad(const uint8_t *data, size_t len, int8_t rssi) {
    if (!data)
        return;
    for (size_t pos = 0; pos + 2 <= len;) {
        size_t n = data[pos];
        if (!n || pos + 1 + n > len)
            return;
        if (data[pos + 1] == 0xff && n >= 3 && data[pos + 2] == 0xff && data[pos + 3] == 0xff) {
            mesh_packet_t p;
            if (mesh_packet_unpack(&p, data + pos + 4, n - 3) &&
                mesh_mac_is_valid_peer(p.src_mac) && !mesh_mac_equal(p.src_mac, s_mac) && s_rx)
                s_rx(&p, rssi);
        }
        pos += n + 1;
    }
}
static int gap_event(struct ble_gap_event *e, void *arg) {
    (void)arg;
    switch (e->type) {
    case BLE_GAP_EVENT_EXT_DISC:
        /* The v3 envelope fits a single report. Never parse incomplete data. */
        if (e->ext_disc.data_status == 0)
            receive_ad(e->ext_disc.data, e->ext_disc.length_data, e->ext_disc.rssi);
        break;
    case BLE_GAP_EVENT_DISC:
        receive_ad(e->disc.data, e->disc.length_data, e->disc.rssi);
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        s_last_ok = e->adv_complete.reason == 0 || e->adv_complete.reason == BLE_HS_ETIMEOUT ||
                    (s_adv_duration_ms && now_ms() - s_adv_start_ms + 20 >= s_adv_duration_ms);
        s_adv = false;
        publish_state();
        break;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        s_scan = false;
        publish_state();
        break;
    case BLE_GAP_EVENT_CONNECT:
        if (e->connect.status == 0)
            ble_gap_terminate(e->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        break;
    default:
        break;
    }
    return 0;
}
static void on_sync(void) {
    memset(s_efuse, 0, sizeof(s_efuse));
    if (esp_efuse_mac_get_default(s_efuse) != ESP_OK) {
        s_error = 1;
        return;
    }
    /* Preserve the identity used by the baseline firmware. */
    s_mac[0] = s_efuse[0] ^ s_efuse[6];
    s_mac[1] = s_efuse[1] ^ s_efuse[7];
    memcpy(s_mac + 2, s_efuse + 2, 4);
    s_mac[5] |= 0xc0;
    if (ble_hs_id_set_rnd(s_mac)) {
        s_error = 2;
        return;
    }
    s_error = 0;
    s_configured = false;
    s_adv = false;
    s_scan = false;
    s_ready = true;
    publish_state();
    ESP_LOGI(TAG, "v3 ready %02X:%02X:%02X:%02X:%02X:%02X", s_mac[0], s_mac[1], s_mac[2], s_mac[3],
             s_mac[4], s_mac[5]);
}
static void on_reset(int reason) {
    s_ready = false;
    s_scan = false;
    s_adv = false;
    s_last_ok = false;
    s_configured = false;
    publish_state();
    ESP_LOGW(TAG, "host reset: %d", reason);
}
static void host_task(void *arg) {
    (void)arg;
    nimble_port_run();
    nimble_port_freertos_deinit();
}
void ble_radio_port_init(void) {
    if (s_port_inited)
        return;
    esp_err_t rc = nimble_port_init();
    if (rc != ESP_OK) {
        s_error = 3;
        ESP_LOGE(TAG, "NimBLE init: %d", rc);
        return;
    }
    s_port_inited = true;
}
void ble_radio_init(ble_radio_rx_cb_t rx, ble_radio_state_cb_t cb) {
    s_rx = rx;
    s_state_cb = cb;
    ble_radio_port_init();
    if (!s_port_inited || s_host_started)
        return;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    ble_svc_gap_device_name_set("BLUE_LONG_MESH");
    s_host_started = true;
    nimble_port_freertos_init(host_task);
}
static bool advertise(const uint8_t *data, size_t len, ble_phy_t phy, uint32_t duration) {
    if (!s_ready || !data || !len || len > BLE_MESH_WIRE_MAX || s_adv || duration > 655350)
        return false;
    if (!s_configured || s_config_phy != phy || s_config_fast != s_fast) {
        struct ble_gap_ext_adv_params p = {0};
        p.own_addr_type = BLE_OWN_ADDR_RANDOM;
        p.legacy_pdu = 0;
        p.connectable = 0;
        p.scannable = 0;
        p.primary_phy = p.secondary_phy =
            phy == BLE_PHY_CODED_S2 ? BLE_HCI_LE_PHY_CODED : BLE_HCI_LE_PHY_1M;
        p.itvl_min = (s_fast ? BLE_ADV_INTERVAL_FAST_MIN_MS : BLE_RADIO_ADV_INTERVAL_MIN_MS) * 1000 / 625;
        p.itvl_max = (s_fast ? BLE_ADV_INTERVAL_FAST_MAX_MS : BLE_RADIO_ADV_INTERVAL_MAX_MS) * 1000 / 625;
        int rc = ble_gap_ext_adv_configure(0, &p, NULL, gap_event, NULL);
        if (rc) {
            ESP_LOGW(TAG, "adv configure: %d", rc);
            return false;
        }
        s_configured = true;
        s_config_phy = phy;
        s_config_fast = s_fast;
    }
    uint8_t b[BLE_ADV_DATA_MAX] = {2, 1, 6};
    b[3] = (uint8_t)(len + 3);
    b[4] = 0xff;
    b[5] = 0xff;
    b[6] = 0xff;
    memcpy(b + 7, data, len);
    struct os_mbuf *om = os_msys_get_pkthdr(0, 0);
    if (!om)
        return false;
    if (os_mbuf_append(om, b, len + 7)) {
        os_mbuf_free_chain(om);
        return false;
    }
    int rc = ble_gap_ext_adv_set_data(0, om); /* NimBLE owns om on every return path. */
    if (rc) {
        ESP_LOGW(TAG, "adv data: %d", rc);
        return false;
    }
    s_adv_duration_ms = duration;
    s_adv_start_ms = now_ms();
    s_last_ok = false;
    rc = ble_gap_ext_adv_start(0, duration ? (duration + 9) / 10 : 0, 0);
    if (rc) {
        ESP_LOGW(TAG, "adv start: %d", rc);
        return false;
    }
    s_adv = true;
    s_phy = phy;
    publish_state();
    return true;
}
bool ble_radio_advertise_timed(const uint8_t *d, size_t n, uint32_t ms) {
    return advertise(d, n, BLE_PHY_1M, ms);
}
bool ble_radio_advertise_start(const uint8_t *d, size_t n, ble_phy_t p, bool ext) {
    return ext && advertise(d, n, p, 0);
}
void ble_radio_advertise_stop(void) {
    if (!s_adv)
        return;
    int rc = ble_gap_ext_adv_stop(0);
    if (rc && rc != BLE_HS_EALREADY)
        ESP_LOGW(TAG, "adv stop: %d", rc);
    s_adv = false;
    publish_state();
}
bool ble_radio_scan_start(uint32_t ms, ble_phy_t phy) {
    if (!s_ready || ms > 655350)
        return false;
    if (s_scan)
        return true;
    struct ble_gap_ext_disc_params uncoded = {
        .itvl = 48, .window = 48, .passive = 1, .disable_observer_mode = 0};
    struct ble_gap_ext_disc_params coded = uncoded;
    int rc = ble_gap_ext_disc(BLE_OWN_ADDR_RANDOM, ms ? (ms + 9) / 10 : 0, 0, 0, 0, 0, &uncoded,
                              phy == BLE_PHY_CODED_S2 ? &coded : NULL, gap_event, NULL);
    if (rc && rc != BLE_HS_EALREADY) {
        ESP_LOGW(TAG, "scan start: %d", rc);
        return false;
    }
    s_scan = true;
    publish_state();
    return true;
}
void ble_radio_scan_stop(void) {
    if (!s_scan)
        return;
    int rc = ble_gap_disc_cancel();
    if (rc && rc != BLE_HS_EALREADY)
        ESP_LOGW(TAG, "scan stop: %d", rc);
    s_scan = false;
    publish_state();
}
bool ble_radio_scan_continuous_start(uint32_t window, ble_phy_t phy) {
    (void)window;
    return ble_radio_scan_start(0, phy);
}
bool ble_radio_adv_continuous_start(const uint8_t *d, size_t n, ble_phy_t p, bool ext) {
    return ble_radio_advertise_start(d, n, p, ext);
}
void ble_radio_set_fast_mode(bool fast) {
    s_fast = fast;
} /* Never stops the receiver. */
bool ble_radio_is_fast_mode(void) {
    return s_fast;
}
void ble_radio_set_phy(ble_phy_t p) {
    s_phy = p;
}
ble_radio_state_t ble_radio_get_state(void) {
    return s_state;
}
uint32_t ble_radio_get_src_id(void) {
    uint32_t a = 0, b = 0;
    memcpy(&a, s_efuse, 4);
    memcpy(&b, s_efuse + 4, 4);
    return a ^ b;
}
uint8_t ble_radio_get_fatal_error(void) {
    return s_error;
}
bool ble_radio_is_ready(void) {
    return s_ready;
}
bool ble_radio_is_advertising(void) {
    return s_adv;
}
bool ble_radio_is_scanning(void) {
    return s_scan;
}
bool ble_radio_last_advertising_ok(void) {
    return s_last_ok;
}
uint32_t ble_radio_advertising_duration_ms(void) {
    return s_adv ? now_ms() - s_adv_start_ms : 0;
}
bool ble_radio_get_my_mac(uint8_t *p) {
    if (!p || !s_ready)
        return false;
    memcpy(p, s_mac, 6);
    return true;
}
bool ble_radio_advertise_cover(const uint8_t *d, size_t n, uint32_t ms) {
    return ble_radio_advertise_timed(d, n, ms);
}
bool ble_radio_is_cover_active(void) {
    return s_adv;
}
/* Old callers compile, but arbitration now belongs to message_service. */
void ble_radio_adv_protect_set(bool b) {
    (void)b;
}
bool ble_radio_is_adv_protected(void) {
    return false;
}
void ble_radio_scan_protect_set(bool b) {
    (void)b;
}
bool ble_radio_is_scan_protected(void) {
    return false;
}
void ble_radio_scan_protect_tick(void) {}
bool ble_radio_force_scan_start(uint32_t ms) {
    return ble_radio_scan_start(ms, BLE_PHY_1M);
}