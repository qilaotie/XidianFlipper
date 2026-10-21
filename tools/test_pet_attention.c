#include "pet_attention.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 pet_t p;pet_new(&p);pet_attention_t a;pet_attention_init(&a,0);
 assert(!pet_attention_poll(&a,&p,10000,true,false));
 pet_advance(&p,60);p.fullness=24;
 assert(!pet_attention_poll(&a,&p,9999,true,false));
 assert(!pet_attention_poll(&a,&p,10000,false,false));
 assert(!pet_attention_poll(&a,&p,10000,true,true));
 assert(pet_attention_poll(&a,&p,10000,true,false)==PET_NEED_FOOD);
 assert(pet_attention_text(&a,&p,10000));assert(!pet_attention_text(&a,&p,14000));
 assert(!pet_attention_poll(&a,&p,189999,true,false));
 assert(pet_attention_poll(&a,&p,190000,true,false)==PET_NEED_FOOD);
 p.spare=1;assert(!pet_attention_poll(&a,&p,400000,true,false));assert(!pet_attention_text(&a,&p,190001));
 p.spare=0;p.sleeping=1;assert(!pet_needs(&p));assert(!pet_attention_poll(&a,&p,400001,true,false));
 p.sleeping=0;p.fullness=100;p.energy=24;assert(pet_attention_poll(&a,&p,400002,true,false)==PET_NEED_REST);
 p.energy=100;p.clean=24;assert(pet_needs(&p)==PET_NEED_WASH);
 p.clean=100;p.mood=24;assert(pet_needs(&p)==PET_NEED_COMPANY);
 p.mood=100;assert(!pet_needs(&p));assert(!pet_attention_poll(&a,&p,600000,true,false));
 uint32_t start=0xFFFFFF00u;pet_attention_init(&a,start);p.fullness=0;
 assert(!pet_attention_poll(&a,&p,start+9999,true,false));
 assert(pet_attention_poll(&a,&p,start+10000,true,false)==PET_NEED_FOOD);
 assert(pet_attention_text(&a,&p,start+10001));assert(!pet_attention_text(&a,&p,start+14000));
 assert(pet_valid(&p));pet_act(&p,PET_CALLS);assert(pet_valid(&p)&&p.spare==1);
 pet_new_egg(&p);assert(p.spare==1&&pet_valid(&p));
 puts("PASS pet calls: thresholds, warmup, safe-page/busy deferral, 3min cooldown, sleep/mute, banner lifetime, timer wrap");
}
