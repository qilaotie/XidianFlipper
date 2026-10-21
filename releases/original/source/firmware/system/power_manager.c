#include "power_manager.h"
#include "power_mgmt.h"
#include "constants.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
static uint64_t s_boot_time_us;
void enter_deep_sleep(void)
{
    if (!power_mgmt_request_sleep()) ESP_LOGW("power", "Manual sleep not available yet");
}
void power_configure_wakeup_sources(void)
{
    esp_err_t err = esp_sleep_enable_timer_wakeup(POWER_MGMT_IDLE_POLL_US);
    if (err != ESP_OK) ESP_LOGE("power", "RTC wake setup failed: %d", err);
}
int power_get_wakeup_cause(void) { return (int)esp_sleep_get_wakeup_cause(); }
void power_set_cold_boot_time(void)
{
    s_boot_time_us = esp_timer_get_time();
    if (!s_boot_time_us) s_boot_time_us = 1;
}
bool power_is_grace_period(void)
{
    return s_boot_time_us && (uint64_t)esp_timer_get_time() - s_boot_time_us <
        (uint64_t)DEEPSLEEP_GRACE_PERIOD_SEC * 1000000;
}
