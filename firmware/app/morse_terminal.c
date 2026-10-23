#include "morse_terminal.h"
#include "esp_timer.h"
#include "haptic_feedback.h"
#include "mcpwm_buzzer.h"
#include "message_service.h"
#include "morse_codec.h"
#include "morse_input.h"
#include "morse_live.h"
#include "morse_playback.h"
#include "morse_tree.h"
#include "nvs_config.h"
#include "rmt_ws2812.h"
#include "wououi.h"
#include "ui_zh.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static MorseNode *s_node;
static char s_pattern[9], s_message[MORSE_TERM_MSG_MAX + 1];
static unsigned s_pattern_len;
static bool s_practice = true, s_live, s_invalid, s_have_character, s_word_added;
static morse_live_flow_t s_local_flow;
static float s_x, s_y, s_x_target, s_y_target;
static uint64_t s_last_frame, s_speed_until, s_feedback_until;
typedef struct {
    char ch;
    uint32_t queued_ms;
} live_audio_item_t;
static live_audio_item_t s_audio[16];
static unsigned s_audio_count;
static uint32_t s_audio_cursor, s_audio_stream, s_audio_due;
static bool s_audio_active;
static char s_audio_char;
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static void stop_receive_audio(void) {
    if (s_audio_active)
        morse_playback_stop();
    s_audio_active = false;
    s_audio_count = 0;
    s_audio_due = 0;
}
static void receive_audio(void) {
    if (!s_live)
        return;
    morse_live_snapshot_t snapshot;
    message_service_live_snapshot(&snapshot);
    uint32_t now = now_ms();
    if (snapshot.stream != s_audio_stream) {
        stop_receive_audio();
        s_audio_stream = snapshot.stream;
        s_audio_cursor = 0;
    }
    for (unsigned i = 0; i < snapshot.flow.count; i++) {
        morse_live_glyph_t *g = &snapshot.flow.glyphs[i];
        if (g->event_id <= s_audio_cursor)
            continue;
        s_audio_cursor = g->event_id;
        if (g->status != MORSE_LIVE_RECEIVED)
            continue;
        if (s_audio_count == 16) {
            memmove(s_audio, s_audio + 1, 15 * sizeof(*s_audio));
            s_audio_count--;
        }
        s_audio[s_audio_count++] = (live_audio_item_t){g->ch, now};
    }
    /* Old queued sounds disappear; remote playback never catches up over a local conversation. */
    while (s_audio_count && now - s_audio[0].queued_ms > 2000) {
        memmove(s_audio, s_audio + 1, (--s_audio_count) * sizeof(*s_audio));
    }
    if (s_audio_active) {
        if (morse_playback_is_active())
            return;
        s_audio_active = false;
        s_audio_due = now + 3 * morse_input_get_dot_ms();
    }
    if (morse_terminal_is_keying() || morse_playback_is_active() ||
        (s_audio_due && (int32_t)(now - s_audio_due) < 0) || !s_audio_count)
        return;
    char ch = s_audio[0].ch;
    memmove(s_audio, s_audio + 1, (--s_audio_count) * sizeof(*s_audio));
    if (ch == ' ') {
        s_audio_due = now + 4 * morse_input_get_dot_ms();
        return;
    }
    s_audio_active = morse_playback_start(&ch, 1, morse_input_get_dot_ms());
    s_audio_char = ch;
}
static void reset_sequence(void) {
    s_node = morse_tree_get_root();
    s_pattern_len = 0;
    s_pattern[0] = 0;
    s_invalid = false;
    int x, y;
    morse_tree_position(s_node, &x, &y);
    s_x_target = x;
    s_y_target = y;
}
void morse_terminal_init(void) {
    morse_playback_stop();
    stop_receive_audio();
    s_audio_cursor = s_audio_stream = 0;
    morse_input_reset();
    s_live = false;
    s_practice = true;
    s_message[0] = 0;
    s_have_character = s_word_added = false;
    morse_live_flow_init(&s_local_flow, now_ms());
    s_local_flow.left = 30;
    reset_sequence();
    s_x = s_x_target;
    s_y = s_y_target;
    s_last_frame = esp_timer_get_time();
    s_speed_until = s_feedback_until = 0;
}
void morse_terminal_exit(void) {
    stop_receive_audio();
    buzzer_stop();
    haptic_set_morse_priority(false);
    morse_input_reset();
    rgb_set_color(0, 0, 0);
    s_live = false; /* A page must explicitly opt in again; training can never inherit TX mode. */
}
void morse_terminal_set_practice_mode(bool practice) {
    s_practice = practice;
    if (practice) {
        s_live = false;
        stop_receive_audio();
    }
}
void morse_terminal_set_live_mode(bool live) {
    s_live = live;
    stop_receive_audio();
    if (live) {
        s_practice = false;
        morse_live_snapshot_t snapshot;
        message_service_live_snapshot(&snapshot);
        s_audio_cursor = snapshot.flow.next_event;
        s_audio_stream = snapshot.stream;
    }
}
void morse_terminal_feed_key_up(uint32_t ms) { (void)ms; }
void morse_terminal_feed_rotate(int delta) {
    int ms = (int)morse_input_get_dot_ms() - delta * MORSE_IN_DOT_STEP;
    morse_input_set_dot_ms(ms < 40 ? 40 : ms);
    g_config.morse_dot_ms = morse_input_get_dot_ms();
    nvs_config_mark_dirty();
    s_speed_until = esp_timer_get_time() + 1200000;
}
static void append(char ch) {
    if (s_live) {
        if (!message_service_send_live_char(ch, NULL))
            s_feedback_until = esp_timer_get_time() + 1500000;
    } else
        morse_live_flow_push(&s_local_flow, ch, 0, MORSE_LIVE_LOCAL);
    s_have_character = true;
    s_word_added = ch == ' ';
}
static void confirm(void) {
    char ch = s_invalid ? 0 : morse_codec_decode(s_pattern);
    if (ch)
        append(ch);
    else if (s_pattern_len)
        s_feedback_until = esp_timer_get_time() + 600000;
    reset_sequence();
}
void morse_terminal_feed_double_click(void) {
    if (s_pattern_len)
        confirm();
}
void morse_terminal_clear_input(void) {
    reset_sequence();
    morse_input_reset();
    buzzer_stop();
    haptic_set_morse_priority(false);
}
void morse_terminal_clear_message(void) {
    morse_live_flow_init(&s_local_flow, now_ms());
    s_local_flow.left = 30;
    s_message[0] = 0;
    s_have_character = s_word_added = false;
    morse_terminal_clear_input();
}
bool morse_terminal_set_message(const char *text) {
    if (!text)
        return false;
    size_t n = 0;
    while (n <= MORSE_TERM_MSG_MAX && text[n])
        n++;
    if (n > MORSE_TERM_MSG_MAX || (n && !morse_codec_validate(text, n)))
        return false;
    morse_live_flow_init(&s_local_flow, now_ms());
    s_local_flow.left = 30;
    for (size_t i = 0; i < n; i++)
        morse_live_flow_push(&s_local_flow, text[i], 0, MORSE_LIVE_LOCAL);
    return true; /* Compatibility only: restoring display text never sends it. */
}
void morse_terminal_backspace(void) {
    morse_terminal_clear_input(); /* Live characters cannot be edited after transmission. */
}
void morse_terminal_tick(void) {
    morse_live_flow_tick(&s_local_flow, now_ms());
    morse_input_event_t ev;
    while ((ev = morse_input_poll()) != MORSE_IN_NONE) {
        switch (ev) {
        case MORSE_IN_KEY_DOWN:
            /* Stop the remote timer before starting the local key tone, never after it. */
            if (s_audio_active) {
                morse_playback_stop();
                s_audio_active = false;
            }
            haptic_stop_all();
            haptic_set_morse_priority(true);
            buzzer_start();
            if (g_config.led_enabled)
                rgb_set_color(80, 60, 0);
            break;
        case MORSE_IN_KEY_UP:
            buzzer_stop();
            haptic_set_morse_priority(false);
            rgb_set_color(0, 0, 0);
            break;
        case MORSE_IN_DOT:
        case MORSE_IN_DASH:
            if (s_pattern_len < 8) {
                s_pattern[s_pattern_len++] = ev == MORSE_IN_DOT ? '.' : '-';
                s_pattern[s_pattern_len] = 0;
                MorseNode *next = ev == MORSE_IN_DOT ? s_node->dot_child : s_node->dash_child;
                if (next && !s_invalid)
                    s_node = next;
                else
                    s_invalid = true;
                int x, y;
                morse_tree_position(s_node, &x, &y);
                s_x_target = x;
                s_y_target = y;
            } else
                s_invalid = true;
            break;
        case MORSE_IN_CHAR_GAP:
            if (s_pattern_len)
                confirm();
            break;
        case MORSE_IN_WORD_GAP:
            if (s_pattern_len)
                confirm();
            if (s_have_character && !s_word_added)
                append(' ');
            break;
        default:
            break;
        }
    }
    receive_audio();
}
uint32_t morse_terminal_get_dot_ms(void) { return morse_input_get_dot_ms(); }
const char *morse_terminal_get_message(void) {
    morse_live_snapshot_t v = {0};
    if (s_live)
        message_service_live_snapshot(&v);
    else
        v.flow = s_local_flow;
    unsigned n = 0;
    for (unsigned i = 0; i < v.flow.count && n < MORSE_TERM_MSG_MAX; i++)
        s_message[n++] = v.flow.glyphs[i].ch;
    s_message[n] = 0;
    return s_message;
}
int morse_terminal_get_message_len(void) { return (int)strlen(morse_terminal_get_message()); }
bool morse_terminal_is_keying(void) { return s_pattern_len || morse_input_is_held(); }
static bool on_path(MorseNode *node) {
    for (MorseNode *p = s_node; p; p = p->parent)
        if (p == node)
            return true;
    return false;
}
static void render_tree(float alpha) {
    s_x += (s_x_target - s_x) * alpha;
    s_y += (s_y_target - s_y) * alpha;
    u8g2_SetClipWindow(&u8g2, 0, 0, 64, 22);
    /* Every node stays in the fixed minimap; only the focus marker moves with easing. */
    for (unsigned i = 0; i < morse_tree_count(); i++) {
        MorseNode *n = morse_tree_node(i);
        int x, y;
        morse_tree_position(n, &x, &y);
        if (n->parent) {
            int px, py;
            morse_tree_position(n->parent, &px, &py);
            u8g2_DrawLine(&u8g2, px, py, x, y);
        }
        if (on_path(n))
            u8g2_DrawBox(&u8g2, x - 1, y, 3, 2);
        else
            u8g2_DrawPixel(&u8g2, x, y);
    }
    int fx = (int)lroundf(s_x), fy = (int)lroundf(s_y);
    u8g2_SetDrawColor(&u8g2, 2);
    u8g2_DrawFrame(&u8g2, fx - 2, fy - 2, 5, 5);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetMaxClipWindow(&u8g2);
    u8g2_DrawVLine(&u8g2, 64, 0, 22);
    u8g2_SetClipWindow(&u8g2, 66, 0, 128, 22);
    char route[17] = {0};
    char reversed[9];
    unsigned n = 0;
    for (MorseNode *p = s_node; p && n < 9; p = p->parent)
        reversed[n++] = p->parent ? (p->character ? p->character : '+') : '*';
    for (unsigned i = 0; i < n && i < 8; i++) {
        route[i * 2] = reversed[n - 1 - i];
        route[i * 2 + 1] = ' ';
    }
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(&u8g2, 67, 7, route);
    u8g2_DrawStr(&u8g2, 67, 15, s_pattern_len ? s_pattern : s_live ? "LIVE .-" : "LOCAL .-");
    char label[8];
    snprintf(label, sizeof(label), ".%c",
             s_node->dot_child && s_node->dot_child->character ? s_node->dot_child->character
                                                               : '-');
    u8g2_DrawStr(&u8g2, 67, 22, label);
    snprintf(label, sizeof(label), "-%c",
             s_node->dash_child && s_node->dash_child->character ? s_node->dash_child->character
                                                                 : '-');
    u8g2_DrawStr(&u8g2, 116, 22, label);
    char ch[2] = {s_invalid ? '!' : s_node->character ? s_node->character : '*', 0};
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&u8g2, 98, 22, ch);
    u8g2_SetMaxClipWindow(&u8g2);
}
static void render_live(uint64_t now) {
    morse_live_snapshot_t live;
    message_service_live_snapshot(&live);
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    char title[20];
    snprintf(title, sizeof(title), "ROOM %02u", message_service_get_channel());
    u8g2_DrawStr(&u8g2, 0, 7, title);
    snprintf(title, sizeof(title), "%luWPM", (unsigned long)(1200 / morse_input_get_dot_ms()));
    u8g2_DrawStr(&u8g2, 128 - u8g2_GetStrWidth(&u8g2, title), 7, title);
    u8g2_DrawHLine(&u8g2, 0, 9, 128);
    u8g2_SetClipWindow(&u8g2, 0, 10, 128, 25);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    for (unsigned i = 0; i < live.flow.count; i++) {
        morse_live_glyph_t *g = &live.flow.glyphs[i];
        int x = (int)lroundf(g->x);
        if (x < -5 || x > 127)
            continue;
        char ch[2] = {g->ch, 0};
        u8g2_DrawStr(&u8g2, x, 22, ch);
        if (g->status == MORSE_LIVE_RECEIVED)
            u8g2_DrawHLine(&u8g2, x, 24, 5);
        if (g->status == MORSE_LIVE_QUEUED || g->status == MORSE_LIVE_SENDING)
            u8g2_DrawPixel(&u8g2, x + 2, 24);
        if (g->status == MORSE_LIVE_FAILED) {
            u8g2_DrawLine(&u8g2, x, 13, x + 4, 22);
            u8g2_DrawLine(&u8g2, x + 4, 13, x, 22);
        }
    }
    u8g2_SetMaxClipWindow(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    const char *pattern = s_pattern;
    if (s_audio_active)
        pattern = morse_codec_encode(s_audio_char);
    char progress[22];
    snprintf(progress, sizeof(progress), "%s%s", s_audio_active ? "RX " : "",
             pattern && *pattern ? pattern : "...");
    u8g2_DrawStr(&u8g2, 0, 31, progress);
    if (morse_input_is_held())
        u8g2_DrawVLine(&u8g2, u8g2_GetStrWidth(&u8g2, progress) + 2, 26, 6);
    if (now < s_feedback_until || (live.last_failure_ms && now_ms() - live.last_failure_ms < 4000))
        snprintf(title, sizeof(title), "TX FAILED");
    else if (live.pending)
        snprintf(title, sizeof(title), "Q%02u", live.pending);
    else
        snprintf(title, sizeof(title), "%s", live.sent ? "SENT" : "READY");
    u8g2_DrawStr(&u8g2, 128 - u8g2_GetStrWidth(&u8g2, title), 31, title);
}
void morse_terminal_render(void) {
    uint64_t now = esp_timer_get_time();
    float dt = s_last_frame ? (now - s_last_frame) / 1000.0f : 16;
    if (dt > 60)
        dt = 60;
    s_last_frame = now;
    float alpha = 1.0f - expf(-dt / 90.0f);
    u8g2_SetDrawColor(&u8g2, 1);
    if (s_live) {
        render_live(now);
        return;
    }
    render_tree(alpha);
    morse_live_snapshot_t live = {0};
    if (s_live)
        message_service_live_snapshot(&live);
    else
        live.flow = s_local_flow;
    u8g2_DrawHLine(&u8g2, 0, 22, 128);
    u8g2_SetClipWindow(&u8g2, 0, 23, 128, 32);
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    for (unsigned i = 0; i < live.flow.count; i++) {
        morse_live_glyph_t *g = &live.flow.glyphs[i];
        int x = (int)lroundf(g->x);
        char c[2] = {g->ch, 0};
        if (x < -5 || x > 127)
            continue;
        u8g2_DrawStr(&u8g2, x, 30, c);
        if (g->status == MORSE_LIVE_RECEIVED)
            u8g2_DrawHLine(&u8g2, x, 31, 4);
        if (g->status == MORSE_LIVE_FAILED)
            u8g2_DrawStr(&u8g2, x, 30, "!");
        if (g->status == MORSE_LIVE_QUEUED || g->status == MORSE_LIVE_SENDING)
            u8g2_DrawPixel(&u8g2, x + 2, 31);
    }
    u8g2_SetMaxClipWindow(&u8g2);
    /* Compact status uses the otherwise empty left of the flowing row. */
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawBox(&u8g2, 0, 23, 30, 9);
    u8g2_SetDrawColor(&u8g2, 1);
    char speed[12];
    if (s_live && (now < s_feedback_until ||
                   (live.last_failure_ms && now_ms() - live.last_failure_ms < 4000)))
        snprintf(speed, sizeof(speed), "TX!");
    else
        snprintf(speed, sizeof(speed), "%luWPM", (unsigned long)(1200 / morse_input_get_dot_ms()));
    u8g2_DrawStr(&u8g2, 0, 31, speed);
    if (now < s_speed_until || (!s_live && now < s_feedback_until)) {
        u8g2_SetDrawColor(&u8g2, 0);
        u8g2_DrawBox(&u8g2, 0, 0, 128, 22);
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawFrame(&u8g2, 14, 1, 100, 21);
        char caption[20];
        if (now < s_speed_until) {
            snprintf(caption, sizeof(caption), "%luWPM",
                     (unsigned long)(1200 / morse_input_get_dot_ms()));
            u8g2_SetFont(&u8g2, u8g2_font_9x15_tr);
            u8g2_DrawStr(&u8g2, 23, 17, caption);
            snprintf(caption, sizeof(caption), "%lums", (unsigned long)morse_input_get_dot_ms());
            u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
            u8g2_DrawStr(&u8g2, 80, 15, caption);
        } else
            ui_zh_draw_fit(&u8g2, 34, 15, "Invalid code",90);
    }
    (void)s_practice;
}
