#include "morse_codec.h"
#include <string.h>
typedef struct { char ch; const char *pattern; } entry_t;
static const entry_t entries[] = {
    {'A',".-"},{'B',"-..."},{'C',"-.-."},{'D',"-.."},{'E',"."},
    {'F',"..-."},{'G',"--."},{'H',"...."},{'I',".."},{'J',".---"},
    {'K',"-.-"},{'L',".-.."},{'M',"--"},{'N',"-."},{'O',"---"},
    {'P',".--."},{'Q',"--.-"},{'R',".-."},{'S',"..."},{'T',"-"},
    {'U',"..-"},{'V',"...-"},{'W',".--"},{'X',"-..-"},{'Y',"-.--"},
    {'Z',"--.."},{'0',"-----"},{'1',".----"},{'2',"..---"},{'3',"...--"},
    {'4',"....-"},{'5',"....."},{'6',"-...."},{'7',"--..."},{'8',"---.."},
    {'9',"----."},{'.',".-.-.-"},{',',"--..--"},{'?',"..--.."},
    {'/',"-..-."},{'=',"-...-"},{'+',".-.-."},{'-',"-....-"},{'@',".--.-."}
};
static char uppercase(char ch) { return ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch; }
const char *morse_codec_encode(char ch)
{
    ch = uppercase(ch);
    for (size_t i = 0; i < sizeof(entries)/sizeof(entries[0]); ++i)
        if (entries[i].ch == ch) return entries[i].pattern;
    return NULL;
}
char morse_codec_decode(const char *pattern)
{
    if (!pattern || !*pattern) return '\0';
    for (size_t i = 0; i < sizeof(entries)/sizeof(entries[0]); ++i)
        if (strcmp(entries[i].pattern, pattern) == 0) return entries[i].ch;
    return '\0';
}
char morse_codec_lesson_character(unsigned index)
{
    static const char lessons[] = "ETIAMNSURWDKGOHVFLPJBXCYZQ0123456789";
    return index < MORSE_CHARACTER_COUNT ? lessons[index] : '\0';
}
bool morse_codec_validate(const char *text, size_t len)
{
    if (!text || len == 0 || len > MORSE_TEXT_MAX) return false;
    bool have_character = false;
    for (size_t i = 0; i < len; ++i) {
        if (text[i] == ' ') continue;
        if (!morse_codec_encode(text[i])) return false;
        have_character = true;
    }
    return have_character;
}
bool morse_stream_init(morse_stream_t *s, const char *text, size_t len)
{
    if (!s || !morse_codec_validate(text, len)) return false;
    memset(s, 0, sizeof(*s));
    while (len > 0 && text[len - 1] == ' ') --len;
    for (size_t i = 0; i < len; ++i) s->text[i] = uppercase(text[i]);
    s->length = (uint16_t)len;
    while (s->text[s->character_index] == ' ') ++s->character_index;
    return true;
}
bool morse_stream_next(morse_stream_t *s, morse_segment_t *out)
{
    if (!s || !out || s->finished) return false;
    const char *pattern = morse_codec_encode(s->text[s->character_index]);
    if (!pattern) { s->finished = true; return false; }
    out->character_index = s->character_index;
    out->symbol_index = s->symbol_index;
    if (!s->gap_next) {
        out->tone = true;
        out->units = pattern[s->symbol_index] == '-' ? 3 : 1;
        ++s->symbol_index;
        s->gap_next = true;
        return true;
    }
    out->tone = false;
    if (pattern[s->symbol_index]) {
        out->units = 1;
    } else {
        unsigned next = s->character_index + 1;
        bool word_gap = false;
        while (next < s->length && s->text[next] == ' ') { word_gap = true; ++next; }
        if (next >= s->length) { s->finished = true; return false; }
        s->character_index = (uint16_t)next;
        s->symbol_index = 0;
        out->units = word_gap ? 7 : 3;
    }
    s->gap_next = false;
    return true;
}
