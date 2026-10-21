#ifndef WIFI_STATION_H
#define WIFI_STATION_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_wifi_types.h"
#define WIFI_STATION_AP_MAX 24
typedef struct {
    bool initialized,scanning,connecting,online,saved,automatic;
    unsigned count;int error;uint16_t reason;int8_t rssi;
    char ssid[33],saved_ssid[33],ip[16];
} wifi_station_status_t;
void wifi_station_init(void);
void wifi_station_status(wifi_station_status_t *out);
bool wifi_station_ap(unsigned index,wifi_ap_record_t *out);
bool wifi_station_scan(void);
bool wifi_station_connect(const wifi_ap_record_t *ap,const char *password);
bool wifi_station_connect_saved(void);
bool wifi_station_disconnect(void);
bool wifi_station_forget(void);
bool wifi_station_auto(bool enabled);
/* Close an unused scanner, never disconnect an established/requested link. */
void wifi_station_idle(void);
bool wifi_station_supported(wifi_auth_mode_t auth);
#endif
