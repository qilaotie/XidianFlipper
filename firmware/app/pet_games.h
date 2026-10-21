#ifndef PET_GAMES_H
#define PET_GAMES_H
#include <stdbool.h>
#include <stdint.h>
enum {PG_NOTES,PG_BUMP,PG_FLAGS,PG_HEADING,PG_MEMORY,PG_SPRINT,PG_COUNT};
enum {PG_NONE,PG_LEFT,PG_RIGHT,PG_TAP};
typedef struct {
    uint32_t random,last,deadline,pressed;
    uint16_t score;
    uint8_t kind,lane,target,round,mistakes,phase,meter,sequence[8],length,index;
    bool done,won;
} pet_game_t;
const char *pet_games_name(unsigned kind);
void pet_games_start(pet_game_t *g,unsigned kind,uint32_t seed);
/* elapsed milliseconds from start; no wall clock or storage dependencies. */
void pet_games_step(pet_game_t *g,uint32_t elapsed,unsigned input);
#endif
