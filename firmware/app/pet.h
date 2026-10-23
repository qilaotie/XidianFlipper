#ifndef HALFSTEP_PET_H
#define HALFSTEP_PET_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint32_t age;
    uint8_t version, fullness, mood, energy, clean, sleeping, reserved[2];
} pet_v1_t;
typedef struct {
    uint32_t age;
    uint8_t version, fullness, mood, energy, clean, sleeping, reserved[2];
    uint16_t coins, care_good, games_won;
    uint8_t items[3], owned, decor, form, dex, spare;
} pet_t;
enum {PET_FEED,PET_PLAY,PET_CLEAN,PET_SLEEP,PET_CALLS,PET_ACTIONS};
enum {PET_NEED_FOOD=1,PET_NEED_REST=2,PET_NEED_WASH=4,PET_NEED_COMPANY=8};
enum {PET_APPLE,PET_SNACK,PET_SOAP,PET_BALL,PET_PLANT,PET_STARS,PET_RUG,PET_ITEMS};
enum {PET_SUNNY=1,PET_SPORT,PET_MELLOW};
#define PET_ROOMS 4
unsigned pet_room(const pet_t *p);
bool pet_room_owned(const pet_t *p,unsigned room);
unsigned pet_room_price(unsigned room);
bool pet_room_choose(pet_t *p,unsigned room);
void pet_new(pet_t *p);
bool pet_valid(const pet_t *p);
bool pet_migrate(pet_t *p,const pet_v1_t *old);
unsigned pet_stage(const pet_t *p);
unsigned pet_growth_form(const pet_t *p);
unsigned pet_needs(const pet_t *p);
unsigned pet_add_gold(pet_t *p,unsigned amount);
void pet_advance(pet_t *p,uint32_t seconds);
bool pet_act(pet_t *p,unsigned action);
unsigned pet_price(unsigned item);
bool pet_buy(pet_t *p,unsigned item);
bool pet_use(pet_t *p,unsigned item);
bool pet_game(pet_t *p,bool won);
void pet_new_egg(pet_t *p);
#endif
