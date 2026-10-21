#ifndef MORSE_CODEC_H
#define MORSE_CODEC_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MORSE_TEXT_MAX 128
#define MORSE_CHARACTER_COUNT 36
const char *morse_codec_encode(char character);
char morse_codec_decode(const char *pattern);
char morse_codec_lesson_character(unsigned index);
bool morse_codec_validate(const char *text, size_t len);

/* A streaming iterator uses bounded text storage; no expanded dot/dash buffer. */
typedef struct {
    char text[MORSE_TEXT_MAX + 1];
    uint16_t length, character_index;
    uint8_t symbol_index;
    bool gap_next, finished;
} morse_stream_t;
typedef struct {
    bool tone;
    uint8_t units;
    uint16_t character_index;
    uint8_t symbol_index;
} morse_segment_t;
bool morse_stream_init(morse_stream_t *stream, const char *text, size_t len);
bool morse_stream_next(morse_stream_t *stream, morse_segment_t *segment);
#endif
