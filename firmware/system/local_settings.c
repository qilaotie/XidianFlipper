#include "local_settings.h"
/* Legacy API only. Leave local_ui/screen_sec in NVS untouched for compatibility;
 * automatic screen-off settings are no longer loaded, applied, or persisted. */
void local_settings_init(void) {}
uint16_t local_settings_screen_timeout(void) { return 0; }
bool local_settings_set_screen_timeout(uint16_t seconds) { return seconds == 0; }
