#ifndef MORSE_PLAYBACK_H
#define MORSE_PLAYBACK_H
#include "morse_codec.h"
void morse_playback_init(void);
bool morse_playback_start(const char *text, uint16_t length, uint32_t dot_ms);
void morse_playback_stop(void);
bool morse_playback_is_active(void);
void morse_playback_get_segment(morse_segment_t *out);
#endif