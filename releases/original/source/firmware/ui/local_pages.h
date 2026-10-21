#pragma once
#include <stdbool.h>
#include <stdint.h>

void local_pages_init(void);
void local_pages_sync_page(void);
void local_pages_proc(void);
bool local_pages_uses_raw_input(uint8_t page);
void local_pages_render_waking(void);
void local_pages_open_channel(void);
