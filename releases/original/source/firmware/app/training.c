#include "training.h"
void training_begin(training_session_t *s, training_mode_t mode, uint8_t lesson)
{
    *s = (training_session_t){ .mode = mode < TRAINING_MODE_COUNT ? mode : TRAINING_CHOICE,
                              .lesson = lesson < MORSE_CHARACTER_COUNT ? lesson : 0 };
}
char training_target(const training_session_t *s) { return morse_codec_lesson_character(s->lesson); }
bool training_submit_character(training_session_t *s, char ch)
{
    if (s->correct) return true;
    s->correct = ch == training_target(s);
    if (!s->correct) {
        if (s->mistakes < 255) ++s->mistakes;
        s->hint = s->mistakes >= 3;
    }
    return s->correct;
}
bool training_submit_pattern(training_session_t *s, const char *pattern)
{ return training_submit_character(s, morse_codec_decode(pattern)); }
bool training_next(training_session_t *s)
{
    if (!s->correct || s->lesson + 1 >= MORSE_CHARACTER_COUNT) return false;
    training_begin(s, s->mode, s->lesson + 1);
    return true;
}