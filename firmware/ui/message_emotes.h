#ifndef MESSAGE_EMOTES_H
#define MESSAGE_EMOTES_H
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
enum {EMOTE_HAPPY,EMOTE_SAD,EMOTE_SURPRISE,EMOTE_LOVE,EMOTE_REACTION,EMOTE_ANIMALS,EMOTE_GROUPS};
typedef struct { const char *label,*text; unsigned group; } message_emote_t;
static const message_emote_t message_emotes[]={
    /* Keep the original seven indices stable. All payloads are printable ASCII. */
    {"Happy","(^_^)",EMOTE_HAPPY},{"Laugh","(^o^)",EMOTE_HAPPY},
    {"Wink","(^_~)",EMOTE_LOVE},{"Speechless","-_-",EMOTE_REACTION},
    {"Crying","T_T",EMOTE_SAD},{"Surprised","O_O",EMOTE_SURPRISE},{"Heart","<3",EMOTE_LOVE},
    {"Smile",":)",EMOTE_HAPPY},{"Cheer",":D",EMOTE_HAPPY},{"Grin",":-D",EMOTE_HAPPY},
    {"Joy","^_^",EMOTE_HAPPY},{"Yay","\\(^o^)/",EMOTE_HAPPY},{"Dance","o(^_^)o",EMOTE_HAPPY},
    {"Sad",":(",EMOTE_SAD},{"Tear",":'(",EMOTE_SAD},{"Weep",";_;",EMOTE_SAD},
    {"Sob","(T_T)",EMOTE_SAD},{"Down","(._.)",EMOTE_SAD},{"Sigh","(u_u)",EMOTE_SAD},
    {"Tired","(-_-)zz",EMOTE_SAD},
    {"Wow",":O",EMOTE_SURPRISE},{"Gasp",":-O",EMOTE_SURPRISE},{"Stare","(o_o)",EMOTE_SURPRISE},
    {"Shock","(O_O)",EMOTE_SURPRISE},{"Wide eyes","(0_0)",EMOTE_SURPRISE},
    {"Ouch","(>_<)",EMOTE_SURPRISE},{"Really","(?!)",EMOTE_SURPRISE},
    {"Wink small",";)",EMOTE_LOVE},{"Wink nose",";-)",EMOTE_LOVE},{"Kiss",":*",EMOTE_LOVE},
    {"Hello","(^-^)/",EMOTE_LOVE},{"Blush","(o^^o)",EMOTE_LOVE},{"Hug","<(^_^)>",EMOTE_LOVE},
    {"Unsure",":/",EMOTE_REACTION},{"Flat",":|",EMOTE_REACTION},{"Unimpressed","-.-",EMOTE_REACTION},
    {"Sleepy","(=_=)",EMOTE_REACTION},{"Look right","(>_>)",EMOTE_REACTION},
    {"Look left","(<_<)",EMOTE_REACTION},{"Dizzy","(x_x)",EMOTE_REACTION},
    {"Cat","(=^.^=)",EMOTE_ANIMALS},{"Cat smile","(=^-^=)",EMOTE_ANIMALS},
    {"Cat happy","=^_^=",EMOTE_ANIMALS},{"Cat dots","(='.'=)",EMOTE_ANIMALS},
    {"Dog","(U^_^U)",EMOTE_ANIMALS},{"Bear","(o'_'o)",EMOTE_ANIMALS},
    {"Owl","(o,o)",EMOTE_ANIMALS},{"Bunny","(\\_/)",EMOTE_ANIMALS}
};
#define MESSAGE_EMOTE_COUNT (sizeof(message_emotes)/sizeof(message_emotes[0]))
static inline const char *message_emote_group_name(unsigned group){
    static const char *const names[]={"- Happy / cheer","- Sad / tired","- Surprise / shock","- Love / hello","- Reactions","- Animals"};
    return names[group<EMOTE_GROUPS?group:0];
}
static inline unsigned message_emote_group_index(unsigned group,unsigned row){
    for(unsigned i=0;i<MESSAGE_EMOTE_COUNT;i++)if(message_emotes[i].group==group){if(!row)return i;row--;}
    return MESSAGE_EMOTE_COUNT;
}
/* Copy for standalone sending; append for the editor (all or nothing). */
static inline bool message_emote_append(unsigned index,char *out,size_t capacity){
    if(index>=MESSAGE_EMOTE_COUNT || !out || !capacity)return false;
    size_t length=0;while(length<capacity && out[length])++length;
    size_t needed=0;while(message_emotes[index].text[needed])++needed;
    if(length>=capacity || needed>=capacity-length)return false;
    for(size_t i=0;i<=needed;i++)out[length+i]=message_emotes[index].text[i];
    return true;
}
static inline bool message_emote_copy(unsigned index,char *out,size_t capacity) {
    if(index>=MESSAGE_EMOTE_COUNT || !out || !capacity)return false;
    size_t needed=0;while(message_emotes[index].text[needed])++needed;
    if(needed>=capacity)return false;
    snprintf(out,capacity,"%s",message_emotes[index].text);return true;
}
#endif
