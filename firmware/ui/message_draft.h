#ifndef MESSAGE_DRAFT_H
#define MESSAGE_DRAFT_H
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#define DRAFT_DELETE 95
#define DRAFT_SEND 96
#define DRAFT_EMOTE 97
#define DRAFT_CHOICES 97
enum { DRAFT_UPPER,DRAFT_LOWER,DRAFT_DIGITS,DRAFT_SYMBOLS,DRAFT_SPACE_GROUP,
       DRAFT_DELETE_GROUP,DRAFT_SEND_GROUP,DRAFT_EMOTE_GROUP,DRAFT_GROUPS };
typedef struct {
    unsigned group, position[DRAFT_GROUPS];
    bool selecting_group;
} draft_selector_t;
static inline const char *message_draft_group_name(unsigned group) {
    static const char *const names[]={"ABC","abc","123","SYM","SPACE","DELETE","SEND","EMOTES"};
    return names[group<DRAFT_GROUPS?group:0];
}
static inline unsigned message_draft_group_count(unsigned group) {
    static const unsigned counts[]={26,26,10,32,1,1,1,1};
    return counts[group<DRAFT_GROUPS?group:0];
}
static inline void message_draft_group_rotate(draft_selector_t *selector,int step) {
    unsigned count=selector->selecting_group?DRAFT_GROUPS:message_draft_group_count(selector->group);
    unsigned *position=selector->selecting_group?&selector->group:&selector->position[selector->group];
    int next=((int)*position+step)%(int)count;
    *position=(unsigned)(next<0?next+(int)count:next);
}
static inline unsigned message_draft_group_choice(const draft_selector_t *selector) {
    static const char symbols[]="!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    unsigned position=selector->position[selector->group];
    switch(selector->group) {
    case DRAFT_UPPER:return 'A'+position-32;
    case DRAFT_LOWER:return 'a'+position-32;
    case DRAFT_DIGITS:return '0'+position-32;
    case DRAFT_SYMBOLS:return (unsigned char)symbols[position]-32;
    case DRAFT_SPACE_GROUP:return 0;
    case DRAFT_DELETE_GROUP:return DRAFT_DELETE;
    case DRAFT_SEND_GROUP:return DRAFT_SEND;
    default:return DRAFT_EMOTE;
    }
}
static inline void message_draft_select_send(draft_selector_t *selector) {
    selector->group=DRAFT_SEND_GROUP;selector->selecting_group=true;
}
static inline bool message_draft_group_back(draft_selector_t *selector) {
    if(selector->selecting_group)return false;
    selector->selecting_group=true;return true;
}
/* Printable ASCII, followed by explicit editor actions. */
static inline unsigned message_draft_rotate(unsigned selected, int step) {
    int next=(int)selected+step;
    next%=DRAFT_CHOICES;
    return (unsigned)(next<0?next+DRAFT_CHOICES:next);
}
static inline bool message_draft_edit(char *text,size_t capacity,unsigned selected) {
    size_t length=strlen(text);
    if (selected==DRAFT_DELETE) {
        if(length) text[length-1]=0;
        return true;
    }
    if(selected>=95 || length+1>=capacity) return false;
    text[length]=(char)(32+selected); text[length+1]=0;
    return true;
}
#endif
