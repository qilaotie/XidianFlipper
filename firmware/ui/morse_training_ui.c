#include "morse_training_ui.h"
#include "morse_input.h"
#include "morse_playback.h"
#include "wououi.h"
#include "nvs_config.h"
#include "nvs.h"
#include "mcpwm_buzzer.h"
#include "haptic_feedback.h"
#include "rmt_ws2812.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "ui_zh.h"
#include "ui_language.h"
#include <stdio.h>
#include <string.h>
typedef enum { PICK_LESSON, SET_SPEED, DEMONSTRATE, ANSWER, WRONG, SUCCESS } phase_t;
typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t last[TRAINING_MODE_COUNT];
    uint64_t completed[TRAINING_MODE_COUNT];
    uint64_t hints_used[TRAINING_MODE_COUNT];
    uint16_t mistakes[TRAINING_MODE_COUNT][MORSE_CHARACTER_COUNT];
} progress_t;
typedef struct {
    uint32_t magic;
    uint8_t version, last[TRAINING_MODE_COUNT];
    uint64_t completed[TRAINING_MODE_COUNT];
} progress_v1_t;
static progress_t s_progress;
static training_session_t s_session;
static phase_t s_phase;
static bool s_active, s_inited, s_save_failed;
static bool s_progress_dirty, s_progress_writable = true;
static uint8_t s_selected, s_success_option;
static char s_choices[4], s_pattern[9];
static unsigned s_pattern_len;
static uint64_t s_feedback_until;
static uint64_t s_success_started;
static bool s_rhythm_off;
static const char *s_names[] = { "Listen", "Follow", "Dictation" };
static void save_progress(void)
{
    if (!s_progress_dirty) return;
    if (!s_progress_writable) { s_save_failed = true; return; }
    nvs_handle_t h;
    s_save_failed = true;
    if (nvs_open("morse_train2", NVS_READWRITE, &h) != ESP_OK) return;
    esp_err_t err = nvs_set_blob(h, "progress", &s_progress, sizeof(s_progress));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    s_save_failed = err != ESP_OK;
    if (!s_save_failed) s_progress_dirty = false;
}
void morse_training_ui_init(void)
{
    if (s_inited) return;
    s_inited = true;
    memset(&s_progress, 0, sizeof(s_progress));
    nvs_handle_t h;
    esp_err_t open_err = nvs_open("morse_train2", NVS_READONLY, &h);
    if (open_err == ESP_OK) {
        size_t len = 0;
        esp_err_t err = nvs_get_blob(h, "progress", NULL, &len);
        if (err == ESP_OK && len == sizeof(progress_v1_t)) {
            progress_v1_t old;
            if (nvs_get_blob(h, "progress", &old, &len) == ESP_OK &&
                old.magic == 0x4D545232 && old.version == 1) {
                memcpy(s_progress.last, old.last, sizeof(old.last));
                memcpy(s_progress.completed, old.completed, sizeof(old.completed));
            } else s_progress_writable = false;
        } else if (err == ESP_OK && len == sizeof(s_progress)) {
            if (nvs_get_blob(h, "progress", &s_progress, &len) != ESP_OK ||
                s_progress.magic != 0x4D545232 || s_progress.version != 2) {
                s_progress_writable = false;
                memset(&s_progress, 0, sizeof(s_progress));
            }
        } else if (err != ESP_ERR_NVS_NOT_FOUND) s_progress_writable = false;
        nvs_close(h);
    } else if (open_err != ESP_ERR_NVS_NOT_FOUND) s_progress_writable = false;
    s_progress.magic = 0x4D545232;
    s_progress.version = 2;
    for (unsigned i = 0; i < TRAINING_MODE_COUNT; ++i)
        if (s_progress.last[i] >= MORSE_CHARACTER_COUNT) s_progress.last[i] = 0;
}
static void clear_input(void)
{
    s_pattern_len = 0; s_pattern[0] = 0;
    morse_input_reset();
    buzzer_stop();
    haptic_set_morse_priority(false);
    s_rhythm_off = false;
}
static void make_choices(void)
{
    s_choices[0] = training_target(&s_session);
    for (unsigned i = 1; i < 4; ++i) {
        char ch;
        bool duplicate;
        do {
            ch = morse_codec_lesson_character(esp_random() % MORSE_CHARACTER_COUNT);
            duplicate = false;
            for (unsigned j = 0; j < i; ++j) if (s_choices[j] == ch) duplicate = true;
        } while (duplicate);
        s_choices[i] = ch;
    }
    for (unsigned i = 3; i > 0; --i) {
        unsigned j = esp_random() % (i + 1);
        char ch = s_choices[i]; s_choices[i] = s_choices[j]; s_choices[j] = ch;
    }
    s_selected = 0;
}
static void demonstrate(void)
{
    clear_input();
    char text[2] = { training_target(&s_session), 0 };
    haptic_stop_all();
    if (morse_playback_start(text, 1, morse_input_get_dot_ms())) s_phase = DEMONSTRATE;
    else s_phase = ANSWER;
}
static void start_question(void)
{
    training_begin(&s_session, s_session.mode, s_session.lesson);
    clear_input();
    make_choices();
    s_success_option = 0;
    if (s_session.mode == TRAINING_DICTATION) s_phase = ANSWER;
    else demonstrate();
}
void morse_training_ui_enter(training_mode_t mode)
{
    morse_training_ui_init();
    morse_playback_stop();
    training_begin(&s_session, mode, s_progress.last[mode < TRAINING_MODE_COUNT ? mode : 0]);
    s_phase = PICK_LESSON;
    s_active = true;
    clear_input();
}
void morse_training_ui_exit(void)
{
    morse_playback_stop();
    clear_input();
    haptic_set_morse_priority(false);
    rgb_set_color(0, 0, 0);
    s_active = false;
    save_progress();
}
bool morse_training_ui_is_active(void) { return s_active; }
bool morse_training_ui_notifications_allowed(void) { return !s_active; }
void morse_training_ui_suspend_input(void)
{
    if (!s_active) return;
    morse_playback_stop();
    haptic_stop_all();
    clear_input();
    haptic_set_morse_priority(false);
    rgb_set_color(0, 0, 0);
    if (s_phase == DEMONSTRATE || s_phase == ANSWER || s_phase == WRONG) s_phase = ANSWER;
    save_progress();
}
static void result(bool correct)
{
    buzzer_stop();
    haptic_set_morse_priority(false);
    if (correct) {
        s_phase = SUCCESS;
        s_success_started = esp_timer_get_time();
        s_success_option = 0;
        s_progress.completed[s_session.mode] |= 1ULL << s_session.lesson;
        s_progress.last[s_session.mode] = s_session.lesson;
        s_progress_dirty = true;
        save_progress();
        haptic_play(HAPTIC_MORSE_CORRECT);
        if (g_config.led_enabled) rgb_set_color(0, 100, 0);
    } else {
        uint16_t *mistakes = &s_progress.mistakes[s_session.mode][s_session.lesson];
        if (*mistakes < UINT16_MAX) ++*mistakes;
        if (s_session.hint) s_progress.hints_used[s_session.mode] |= 1ULL << s_session.lesson;
        s_progress_dirty = true;
        s_phase = WRONG;
        s_feedback_until = esp_timer_get_time() + 600000;
        haptic_play(HAPTIC_MORSE_WRONG);
        if (g_config.led_enabled) rgb_set_color(100, 0, 0);
    }
}
bool morse_training_ui_handle_event(const ec06_event_t *ev)
{
    if (!s_active || !ev) return false;
    if (ev->type == EC06_EVENT_KEY_DOWN) {
        if (s_phase == SET_SPEED) { s_phase = PICK_LESSON; return false; }
        morse_training_ui_exit();
        return true;
    }
    int delta = ev->type == EC06_EVENT_CW ? 1 : ev->type == EC06_EVENT_CCW ? -1 : 0;
    bool press = ev->type == EC06_EVENT_SHORT_PRESS || ev->type == EC06_EVENT_DOUBLE_CLICK;
    bool hold = ev->type == EC06_EVENT_LONG_PRESS;
    if (s_phase == PICK_LESSON) {
        if (delta) s_session.lesson = (s_session.lesson + MORSE_CHARACTER_COUNT + delta) % MORSE_CHARACTER_COUNT;
        if (hold) s_phase = SET_SPEED;
        if (press) start_question();
    } else if (s_phase == SET_SPEED) {
        if (delta) {
            int ms = (int)morse_input_get_dot_ms() - delta * 10;
            morse_input_set_dot_ms(ms < 40 ? 40 : ms);
            g_config.morse_dot_ms = morse_input_get_dot_ms();
            nvs_config_mark_dirty();
        }
        if (press) s_phase = PICK_LESSON;
    } else if (s_phase == ANSWER && s_session.mode == TRAINING_CHOICE) {
        if (delta) s_selected = (s_selected + 4 + delta) % 4;
        if (hold) demonstrate();
        if (press) result(training_submit_character(&s_session, s_choices[s_selected]));
    } else if (s_phase == ANSWER && s_session.mode != TRAINING_CHOICE) {
        if (delta < 0) clear_input();
        /* A dictation example is withheld until the third wrong attempt. */
        if (delta > 0 && (s_session.mode == TRAINING_FOLLOW || s_session.hint)) demonstrate();
    } else if (s_phase == SUCCESS) {
        if (delta) s_success_option ^= 1;
        if (press) {
            if (s_success_option || !training_next(&s_session)) {
                morse_training_ui_exit(); return true;
            }
            rgb_set_color(0, 0, 0);
            start_question();
        }
    }
    return false;
}
void morse_training_ui_tick(void)
{
    if (!s_active) return;
    if (s_phase == DEMONSTRATE && !morse_playback_is_active()) {
        clear_input();
        s_phase = ANSWER;
    }
    if (s_phase == WRONG && (uint64_t)esp_timer_get_time() >= s_feedback_until) {
        clear_input(); rgb_set_color(0, 0, 0); s_phase = ANSWER;
    }
    if (s_phase != ANSWER || s_session.mode == TRAINING_CHOICE) return;
    morse_input_event_t ev;
    while ((ev = morse_input_poll()) != MORSE_IN_NONE) {
        if (ev == MORSE_IN_KEY_DOWN) {
            haptic_stop_all(); haptic_set_morse_priority(true); buzzer_start();
        } else if (ev == MORSE_IN_KEY_UP) {
            buzzer_stop(); haptic_set_morse_priority(false);
        } else if (ev == MORSE_IN_DOT || ev == MORSE_IN_DASH) {
            uint32_t duration = morse_input_get_duration_ms();
            uint32_t expected = morse_input_get_dot_ms() * (ev == MORSE_IN_DASH ? 3 : 1);
            if (duration && ((uint64_t)duration * 100 < (uint64_t)expected * 60 ||
                             (uint64_t)duration * 100 > (uint64_t)expected * 140)) s_rhythm_off = true;
            if (s_pattern_len < 8) { s_pattern[s_pattern_len++] = ev == MORSE_IN_DOT ? '.' : '-'; s_pattern[s_pattern_len] = 0; }
        } else if (ev == MORSE_IN_CHAR_GAP && s_pattern_len) {
            result(training_submit_pattern(&s_session, s_pattern));
            break;
        }
    }
}
static void draw_center(const char *text, int y)
{
    int x = (DISP_W - ui_zh_width(&u8g2, text)) / 2;
    if(x<1)x=1;
    ui_zh_draw_fit(&u8g2,x,ui_language_chinese() && y>12 && y<26?18:y,text,126);
}
void morse_training_ui_render(void)
{
    u8g2_SetMaxClipWindow(&u8g2); u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    char line[32];
    snprintf(line, sizeof(line), "%s %02u/36", s_names[s_session.mode], s_session.lesson + 1);
    if (s_phase == SUCCESS && s_rhythm_off) snprintf(line, sizeof(line), "Timing: keep steady");
    u8g2_DrawStr(&u8g2, 0, 7, line);
    snprintf(line, sizeof(line), "%luWPM", (unsigned long)(1200 / morse_input_get_dot_ms()));
    u8g2_DrawStr(&u8g2, 128 - u8g2_GetStrWidth(&u8g2, line), 7, line);
    char target[2] = { training_target(&s_session), 0 };
    if (s_phase == SET_SPEED) {
        snprintf(line, sizeof(line), "%lu WPM", (unsigned long)(1200 / morse_input_get_dot_ms()));
        u8g2_SetFont(&u8g2, u8g2_font_6x10_tf); draw_center(line, 21);
        u8g2_SetFont(&u8g2, u8g2_font_5x7_tr); draw_center("TRAINING SPEED", 31);
        return;
    }
    if (s_phase == PICK_LESSON) {
        u8g2_SetFont(&u8g2, ui_language_chinese()?u8g2_font_6x10_tf:u8g2_font_9x15_tr); draw_center(target, 23);
        u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
        bool done = (s_progress.completed[s_session.mode] & (1ULL << s_session.lesson)) != 0;
        draw_center(done ? "COMPLETED" : "READY TO LEARN", 31);
        return;
    }
    if (s_phase == SUCCESS) {
        static const int bounce[] = { 2, -1, 1, 0 };
        unsigned beat = (esp_timer_get_time() - s_success_started) / 60000;
        int dy = beat < 4 ? bounce[beat] : 0;
        u8g2_DrawLine(&u8g2, 15, 16 + dy, 18, 19 + dy);
        u8g2_DrawLine(&u8g2, 18, 19 + dy, 25, 12 + dy);
        u8g2_SetFont(&u8g2, u8g2_font_6x10_tf); draw_center("CORRECT!", 20 + dy);
        u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
        if (s_session.lesson == 35) draw_center("LESSON COMPLETE", 31);
        else draw_center(s_success_option ? "Next   [EXIT]" : "[NEXT]   Exit", 31);
        if (s_save_failed) u8g2_DrawStr(&u8g2, 0, 20, "!");
        return;
    }
    if (s_phase == WRONG) {
        int shake = ((s_feedback_until - esp_timer_get_time()) / 60000) % 2 ? 3 : -3;
        u8g2_DrawLine(&u8g2, 17 + shake, 13, 24 + shake, 20);
        u8g2_DrawLine(&u8g2, 24 + shake, 13, 17 + shake, 20);
        u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
        ui_zh_draw_fit(&u8g2, 36 + shake, ui_language_chinese()?18:21, "TRY AGAIN",86);
        u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
        if (s_session.hint) snprintf(line, sizeof(line), "Hint: %c %s", target[0], morse_codec_encode(target[0]));
        else snprintf(line, sizeof(line), "Attempt %u", s_session.mistakes);
        draw_center(line, 31);
        return;
    }
    if (s_session.mode == TRAINING_CHOICE) {
        u8g2_SetFont(&u8g2, ui_language_chinese()?u8g2_font_6x10_tf:u8g2_font_9x15_tr);
        for (unsigned i = 0; i < 4; ++i) {
            char option[2] = { s_choices[i], 0 };
            if (i == s_selected) {
                u8g2_DrawBox(&u8g2, 7 + i * 30, ui_language_chinese()?8:10, 23, ui_language_chinese()?12:14);
                u8g2_SetDrawColor(&u8g2, 0);
            }
            u8g2_DrawStr(&u8g2, 14 + i * 30, ui_language_chinese()?18:22, option);
            u8g2_SetDrawColor(&u8g2, 1);
        }
        u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
        if (!g_config.buzzer_enabled) draw_center("SOUND IS MUTED", 31);
        else if (s_phase == DEMONSTRATE) draw_center("LISTEN...", 31);
        else if (s_session.hint) {
            snprintf(line, sizeof(line), "Hint: %c %s", target[0], morse_codec_encode(target[0])); draw_center(line, 31);
        } else draw_center("Choose what you heard", 31);
    } else {
        u8g2_SetFont(&u8g2, ui_language_chinese()?u8g2_font_6x10_tf:u8g2_font_9x15_tr);
        u8g2_DrawStr(&u8g2, 2, ui_language_chinese()?18:23, target);
        u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
        if (s_session.mode == TRAINING_FOLLOW || s_session.hint) {
            const char *pattern = morse_codec_encode(target[0]);
            if (s_session.mode == TRAINING_FOLLOW && s_session.hint) {
                /* After three errors, highlight the next expected element. */
                for (unsigned i = 0; pattern[i]; ++i) {
                    char symbol[2] = { pattern[i], 0 };
                    if (i == s_pattern_len && s_phase == ANSWER) {
                        u8g2_DrawBox(&u8g2, 23 + i * 9, 10, 8, 12);
                        u8g2_SetDrawColor(&u8g2, 0);
                    }
                    u8g2_DrawStr(&u8g2, 24 + i * 9, 20, symbol);
                    u8g2_SetDrawColor(&u8g2, 1);
                }
            } else u8g2_DrawStr(&u8g2, 24, 20, pattern);
        }
        else ui_zh_draw_fit(&u8g2, 24,ui_language_chinese()?18:20,"FROM MEMORY",100);
        u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
        if (s_phase == DEMONSTRATE) draw_center("LISTEN...", 31);
        else if (s_pattern_len) draw_center(s_pattern, 31);
        else draw_center(s_session.mode == TRAINING_FOLLOW || s_session.hint ? "YOUR TURN" : "FROM MEMORY", 31);
    }
}
