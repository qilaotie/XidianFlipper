#ifndef MORSE_TRAINING_UI_H
#define MORSE_TRAINING_UI_H
#include "training.h"
#include "ec06.h"
void morse_training_ui_init(void);
void morse_training_ui_enter(training_mode_t mode);
void morse_training_ui_exit(void);
/* Suspend a partial answer without grading it; keep the current lesson. */
void morse_training_ui_suspend_input(void);
/* Returns true when the owning menu should navigate back. */
bool morse_training_ui_handle_event(const ec06_event_t *event);
void morse_training_ui_tick(void);
void morse_training_ui_render(void);
bool morse_training_ui_is_active(void);
bool morse_training_ui_notifications_allowed(void);
#endif
