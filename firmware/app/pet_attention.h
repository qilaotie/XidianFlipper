#ifndef PET_ATTENTION_H
#define PET_ATTENTION_H
#include "pet.h"
typedef struct {uint32_t next_at,until;unsigned need;} pet_attention_t;
void pet_attention_init(pet_attention_t *a,uint32_t now);
unsigned pet_attention_poll(pet_attention_t *a,const pet_t *p,uint32_t now,bool safe,bool busy);
const char *pet_attention_text(const pet_attention_t *a,const pet_t *p,uint32_t now);
#endif
