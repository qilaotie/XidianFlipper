#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Deprecated compatibility API: timeout is always zero (disabled), and only
 * setting zero succeeds. Existing NVS keys remain untouched. */
void local_settings_init(void);
uint16_t local_settings_screen_timeout(void);
bool local_settings_set_screen_timeout(uint16_t seconds);
