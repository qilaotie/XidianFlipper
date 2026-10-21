#include "pet_attention.h"
void pet_attention_init(pet_attention_t *a,uint32_t now){a->next_at=now+10000;a->until=now;a->need=0;}
unsigned pet_attention_poll(pet_attention_t *a,const pet_t *p,uint32_t now,bool safe,bool busy){
    unsigned needs=pet_needs(p);
    if(p->spare || !needs){a->need=0;return 0;}
    if(!safe || busy || (int32_t)(now-a->next_at)<0)return 0;
    a->need=needs&(~needs+1u);a->until=now+4000;a->next_at=now+180000;return a->need;
}
const char *pet_attention_text(const pet_attention_t *a,const pet_t *p,uint32_t now){
    if(p->spare || !(pet_needs(p)&a->need) || (int32_t)(now-a->until)>=0)return 0;
    if(a->need==PET_NEED_FOOD)return "PET: HUNGRY";
    if(a->need==PET_NEED_REST)return "PET: TIRED";
    if(a->need==PET_NEED_WASH)return "PET: NEED WASH";
    if(a->need==PET_NEED_COMPANY)return "PET: PLAY WITH ME";
    return 0;
}
