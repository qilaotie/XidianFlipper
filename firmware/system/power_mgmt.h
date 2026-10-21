/* Manual screen standby keeps the radio alive. Deep sleep is an explicit UI action.
 * Display transitions are owned by ui_task; input tasks only publish activity. */
#ifndef POWER_MGMT_H
#define POWER_MGMT_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { WAKE_REASON_COLD_BOOT = 0, WAKE_REASON_IDLE_KEY = 1 } wake_reason_t;
extern wake_reason_t g_wake_reason;
/* True only while preparing real deep sleep, never for screen standby. */
extern volatile bool g_power_off_in_progress;
/* RTC marker for the manual deep-sleep polling path (legacy symbol name). */
extern bool g_idle_deep_sleep_active;
/* GPIO18/19 are HP_IO; poll from RTC instead of unsupported EXT1 wake. */
#define POWER_MGMT_IDLE_POLL_US 500000ULL
void power_mgmt_init(void);
void power_mgmt_notify_activity(void);
void power_mgmt_tick(void);
bool power_mgmt_is_screen_off(void);
/* Compatibility API: automatic standby is unsupported and always disabled.
 * The setter is a no-op; neither old callers nor stored settings can enable it. */
bool power_mgmt_auto_standby_enabled(void);
void power_mgmt_set_auto_standby(bool enabled);
void power_mgmt_request_screen_off(void);
/* Suppress wake presses and their derived clicks until all keys settle. */
bool power_mgmt_input_guard(void);
/* UI owner can also consume the release gesture when leaving a raw keying page. */
void power_mgmt_guard_input(void);
bool power_mgmt_request_sleep(void);
uint32_t power_mgmt_get_idle_sec(void);
bool power_mgmt_poll_and_wake(void);
#ifdef __cplusplus
}
#endif
#endif
