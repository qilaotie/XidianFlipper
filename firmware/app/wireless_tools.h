#pragma once
#include <stddef.h>
#include <stdint.h>
void wireless_tools_ble_report(const uint8_t mac[6], uint8_t address_type,
                               const uint8_t *data, size_t len, int8_t rssi);
void wireless_tools_on_enter(uint8_t page);
void wireless_tools_proc(void);
