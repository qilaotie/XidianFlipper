#ifndef MESSAGE_EMOTES_H
#define MESSAGE_EMOTES_H
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
typedef struct { const char *label,*text; } message_emote_t;
static const message_emote_t message_emotes[]={
    {"Happy", "(^_^)"}, {"Laugh", "(^o^)"}, {"Wink", "(^_~)"},
    {"Speechless", "-_-"}, {"Crying", "T_T"}, {"Surprised", "O_O"}, {"Heart", "<3"}
};
#define MESSAGE_EMOTE_COUNT (sizeof(message_emotes)/sizeof(message_emotes[0]))
/* Dedicated send buffer only: callers never replace the editor's draft. */
static inline bool message_emote_copy(unsigned index,char *out,size_t capacity) {
    if(index>=MESSAGE_EMOTE_COUNT || !out || !capacity)return false;
    size_t needed=0;while(message_emotes[index].text[needed])++needed;
    if(needed>=capacity)return false;
    snprintf(out,capacity,"%s",message_emotes[index].text);return true;
}
#endif
