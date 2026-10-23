#ifndef PET_SERVICE_H
#define PET_SERVICE_H
#include "pet.h"
/* All calls belong to the UI owner, including manual sleep and page cleanup. */
void pet_service_tick(void);
const pet_t *pet_service_state(void);
#define PET_NAME_MAX 12
const char *pet_service_name(void);
bool pet_service_set_name(const char *name);
bool pet_service_action(unsigned action);
bool pet_service_buy(unsigned item);
bool pet_service_use(unsigned item);
bool pet_service_game(bool won);
/* Caller pays PET_PLAY at start and settles a finished round only once. */
bool pet_service_complete_game(bool won);
bool pet_service_new_egg(void);
bool pet_service_help_needed(unsigned item);
void pet_service_help_seen(unsigned item);
/* outcome: 0 visit, 1 loss, 2 draw, 3 win. Returns actual credited gold;
 * gift is 0..2 when an item was added, -1 otherwise. UI settles once per round. */
unsigned pet_service_social(unsigned outcome,unsigned seed,int *gift);
enum {PET_REWARD_TETRIS,PET_REWARD_SNAKE,PET_REWARD_COUNT};
void pet_service_reward_begin(unsigned source);
void pet_service_reward_restore(unsigned source,unsigned progress);
unsigned pet_service_reward(unsigned source,unsigned progress);
bool pet_service_flush(void);
bool pet_service_error(void);
bool pet_service_pending(void);
#endif
