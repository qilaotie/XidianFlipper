#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool enabled, connected, encrypted, keyboard_ready, media_ready;
    int error;
} tool_hid_status_t;
/* Register once, before starting the existing NimBLE host. */
int tool_hid_init(void);
bool tool_hid_start(void);
void tool_hid_stop(void);
void tool_hid_tick(void);
bool tool_hid_press(bool media, uint16_t usage);
/* One bounded pulse, always followed by a release; no automatic key fallback. */
bool tool_hid_press_for(bool media,uint16_t usage,uint16_t milliseconds);
void tool_hid_status(tool_hid_status_t *out);
