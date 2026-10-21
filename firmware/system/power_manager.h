/* Compatibility helpers; power_mgmt owns standby and manual deep sleep. */
#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H
#include <stdint.h>
#include <stdbool.h>
/* Queue an explicit request. This function returns; it never sleeps directly. */
void enter_deep_sleep(void);
/* RTC polling only: GPIO18/19 cannot wake the C6 through EXT1. */
void power_configure_wakeup_sources(void);
int power_get_wakeup_cause(void);
void power_set_cold_boot_time(void);
bool power_is_grace_period(void);
#endif
