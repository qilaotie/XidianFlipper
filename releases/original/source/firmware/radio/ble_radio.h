/** NimBLE v3 transport. message_service is the sole runtime owner of start/stop. */
#ifndef BLE_RADIO_H
#define BLE_RADIO_H
#include "mesh_packet.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define BLE_MESH_COMPANY_ID 0xFFFF
#define BLE_ADV_DATA_MAX 251
#define BLE_ADV_ENVELOPE_SIZE 7
#define BLE_MESH_WIRE_MAX (BLE_ADV_DATA_MAX - BLE_ADV_ENVELOPE_SIZE)
#define BLE_RADIO_ADV_INTERVAL_MIN_MS 30
#define BLE_RADIO_ADV_INTERVAL_MAX_MS 50
#define BLE_ADV_INTERVAL_FAST_MIN_MS 20
#define BLE_ADV_INTERVAL_FAST_MAX_MS 30
#define BLE_SCAN_FAST_WINDOW_MS 30
#define BLE_SCAN_WINDOW_DEFAULT_MS 30
#define BLE_SCAN_DURATION_MIN_MS 80
#define BLE_SCAN_DURATION_MAX_MS 120
#define BLE_EMERGENCY_LISTEN_MS 150
#define SCAN_PROTECT_TIMEOUT_MS 1000
typedef enum { BLE_PHY_1M = 1, BLE_PHY_CODED_S2 = 4, BLE_PHY_CODED_S8 = 5 } ble_phy_t;
typedef enum {
    BLE_RADIO_OFF,
    BLE_RADIO_IDLE,
    BLE_RADIO_ADVERTISING,
    BLE_RADIO_SCANNING,
    BLE_RADIO_EMERGENCY,
    BLE_RADIO_SCAN_ADV
} ble_radio_state_t;
typedef void (*ble_radio_rx_cb_t)(const mesh_packet_t *, int8_t);
typedef void (*ble_radio_state_cb_t)(ble_radio_state_t, ble_radio_state_t);
void ble_radio_port_init(void);
void ble_radio_init(ble_radio_rx_cb_t rx_cb, ble_radio_state_cb_t state_cb);
bool ble_radio_advertise_start(const uint8_t *, size_t, ble_phy_t, bool);
bool ble_radio_advertise_timed(const uint8_t *, size_t, uint32_t duration_ms);
void ble_radio_advertise_stop(void);
bool ble_radio_last_advertising_ok(void);
bool ble_radio_scan_start(uint32_t, ble_phy_t);
bool ble_radio_scan_continuous_start(uint32_t, ble_phy_t);
bool ble_radio_adv_continuous_start(const uint8_t *, size_t, ble_phy_t, bool);
void ble_radio_scan_stop(void);
void ble_radio_set_phy(ble_phy_t);
ble_radio_state_t ble_radio_get_state(void);
uint32_t ble_radio_get_src_id(void);
uint8_t ble_radio_get_fatal_error(void);
bool ble_radio_is_ready(void);
bool ble_radio_is_advertising(void);
bool ble_radio_is_scanning(void);
uint32_t ble_radio_advertising_duration_ms(void);
bool ble_radio_get_my_mac(uint8_t *);
void ble_radio_set_fast_mode(bool);
bool ble_radio_is_fast_mode(void);
/* Compatibility: starts a timed advertisement and returns immediately. */
bool ble_radio_advertise_cover(const uint8_t *, size_t, uint32_t);
bool ble_radio_is_cover_active(void);
void ble_radio_adv_protect_set(bool);
bool ble_radio_is_adv_protected(void);
void ble_radio_scan_protect_set(bool);
bool ble_radio_is_scan_protected(void);
void ble_radio_scan_protect_tick(void);
bool ble_radio_force_scan_start(uint32_t);
#ifdef __cplusplus
}
#endif
#endif