#include "pet.h"
#include <string.h>
#define MAX_AGE 31536000u
static uint8_t add(unsigned a,unsigned b){return (uint8_t)(a+b>100?100:a+b);}
static uint8_t sub(unsigned a,unsigned b){return (uint8_t)(b>a?0:a-b);}
unsigned pet_stage(const pet_t *p){return p->age<60?0:(p->age<600?1:(p->age<1800?2:3));}
static bool healthy(const pet_t *p){return p->fullness>=40 && p->mood>=40 && p->energy>=20 && p->clean>=40;}
unsigned pet_growth_form(const pet_t *p){return p->form?p->form:(p->games_won>=5?PET_SPORT:(p->care_good>=24?PET_SUNNY:PET_MELLOW));}
unsigned pet_needs(const pet_t *p){
    if(!pet_stage(p) || p->sleeping)return 0;
    return (p->fullness<25?PET_NEED_FOOD:0) | (p->energy<25?PET_NEED_REST:0) |
        (p->clean<25?PET_NEED_WASH:0) | (p->mood<25?PET_NEED_COMPANY:0);
}
unsigned pet_add_gold(pet_t *p,unsigned amount){
    unsigned credit=9999-p->coins;if(amount<credit)credit=amount;p->coins=(uint16_t)(p->coins+credit);return credit;
}
static void unlock(pet_t *p){
    unsigned stage=pet_stage(p);p->dex|=1;
    if(stage>=1)p->dex|=2;
    if(stage>=2)p->dex|=4;
    if(stage==3){
        if(!p->form)p->form=(uint8_t)pet_growth_form(p);
        p->dex|=(uint8_t)(1u<<(p->form+2));
    }
}
void pet_new(pet_t *p){
    memset(p,0,sizeof(*p));p->version=2;p->fullness=80;p->mood=80;p->energy=p->clean=100;
    p->coins=30;p->items[PET_APPLE]=p->items[PET_SOAP]=1;unlock(p);
}
bool pet_valid(const pet_t *p){
    if(!p || p->version!=2 || p->age>MAX_AGE || p->fullness>100 || p->mood>100 ||
       p->energy>100 || p->clean>100 || p->sleeping>1 || p->reserved[0] || p->reserved[1] ||
       p->spare>1 || p->coins>9999 || p->care_good>30 || p->owned>15 || p->decor>3 ||
       p->form>3 || p->dex>63 || !(p->dex&1))return false;
    for(unsigned i=0;i<3;i++)if(p->items[i]>9)return false;
    if(p->decor && !(p->owned&(1u<<p->decor)))return false;
    unsigned stage=pet_stage(p);
    if((stage==3)!=(p->form!=0))return false;
    unsigned bit=stage==3?p->form+2u:stage;
    return (p->dex&(1u<<bit))!=0;
}
bool pet_migrate(pet_t *p,const pet_v1_t *old){
    if(!old || old->version!=1 || old->age>MAX_AGE || old->fullness>100 || old->mood>100 ||
       old->energy>100 || old->clean>100 || old->sleeping>1 || old->reserved[0] || old->reserved[1])return false;
    pet_new(p);p->age=old->age;p->fullness=old->fullness;p->mood=old->mood;p->energy=old->energy;
    p->clean=old->clean;p->sleeping=old->sleeping;
    if(healthy(p))p->care_good=(uint16_t)(p->age/60>30?30:p->age/60);
    unlock(p);return true;
}
void pet_advance(pet_t *p,uint32_t seconds){
    if(seconds>MAX_AGE-p->age)seconds=MAX_AGE-p->age;
    while(seconds){
        unsigned step=60-p->age%60;if(step>seconds)step=seconds;
        p->age+=step;seconds-=step;
        if(p->age%60==0){
            p->fullness=sub(p->fullness,p->sleeping?1:2);p->clean=sub(p->clean,1);p->mood=sub(p->mood,1);
            p->energy=p->sleeping?add(p->energy,8):sub(p->energy,2);
            if(p->fullness<20 || p->clean<20)p->mood=sub(p->mood,1);
            if(!p->form && healthy(p) && p->care_good<30)p->care_good++;
        }
        unlock(p);
    }
}
bool pet_act(pet_t *p,unsigned action){
    if(action==PET_CALLS){p->spare^=1;return true;}
    if(action>=PET_ACTIONS || !pet_stage(p))return false;
    if(action==PET_SLEEP){p->sleeping=!p->sleeping;return true;}
    if(p->sleeping)return false;
    if(action==PET_FEED){p->fullness=add(p->fullness,20);p->mood=add(p->mood,3);}
    if(action==PET_PLAY){if(p->energy<8 || p->fullness<3)return false;
        p->energy=sub(p->energy,8);p->fullness=sub(p->fullness,3);p->mood=add(p->mood,15);}
    if(action==PET_CLEAN){p->clean=100;p->mood=add(p->mood,3);}
    return true;
}
unsigned pet_price(unsigned item){static const uint8_t prices[]={10,8,12,40,25,35,20};return item<PET_ITEMS?prices[item]:0;}
bool pet_buy(pet_t *p,unsigned item){
    unsigned price=pet_price(item);if(!price || p->coins<price)return false;
    if(item<3){if(p->items[item]>=9)return false;p->items[item]++;}
    else {unsigned bit=1u<<(item-3);if(p->owned&bit)return false;p->owned|=(uint8_t)bit;}
    p->coins=(uint16_t)(p->coins-price);return true;
}
bool pet_use(pet_t *p,unsigned item){
    if(item==PET_ITEMS){if(!p->decor)return false;p->decor=0;return true;}
    if(item>=PET_ITEMS)return false;
    if(item>=PET_PLANT){if(!(p->owned&(1u<<(item-3))))return false;p->decor=(uint8_t)(item-3);return true;}
    if(!pet_stage(p) || p->sleeping)return false;
    if(item<3 && !p->items[item])return false;
    if(item==PET_APPLE){if(p->fullness==100)return false;p->fullness=add(p->fullness,35);p->mood=add(p->mood,5);}
    if(item==PET_SNACK){if(p->mood==100 && p->fullness==100)return false;p->mood=add(p->mood,25);p->fullness=add(p->fullness,5);}
    if(item==PET_SOAP){if(p->clean==100)return false;p->clean=100;p->mood=add(p->mood,5);}
    if(item==PET_BALL){if(!(p->owned&1) || p->energy<4 || p->mood==100)return false;
        p->energy=sub(p->energy,4);p->mood=add(p->mood,12);}
    if(item<3)p->items[item]--;
    return true;
}
bool pet_game(pet_t *p,bool won){
    if(!pet_act(p,PET_PLAY))return false;
    if(won){pet_add_gold(p,8);if(p->games_won<65535)p->games_won++;}
    return true;
}
void pet_new_egg(pet_t *p){
    pet_t old=*p;pet_new(p);p->coins=old.coins;memcpy(p->items,old.items,sizeof(p->items));
    p->owned=old.owned;p->decor=old.decor;p->dex|=old.dex;p->spare=old.spare;
}
