#ifndef TRAINING_H
#define TRAINING_H
#include "morse_codec.h"
typedef enum { TRAINING_CHOICE, TRAINING_FOLLOW, TRAINING_DICTATION, TRAINING_MODE_COUNT } training_mode_t;
typedef struct {
    training_mode_t mode;
    uint8_t lesson, mistakes;
    bool correct, hint;
} training_session_t;
void training_begin(training_session_t *session, training_mode_t mode, uint8_t lesson);
char training_target(const training_session_t *session);
bool training_submit_pattern(training_session_t *session, const char *pattern);
bool training_submit_character(training_session_t *session, char character);
bool training_next(training_session_t *session);
#endif