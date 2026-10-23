#ifndef HALFSTEP_SNAKE_H
#define HALFSTEP_SNAKE_H
#include <stdbool.h>
#include <stdint.h>
#define SNAKE_W 24
#define SNAKE_H 8
#define SNAKE_CELLS (SNAKE_W*SNAKE_H)
#define SNAKE_MAPS 3
typedef struct {
    uint8_t body[SNAKE_CELLS], food, direction;
    int8_t turn;
    uint16_t length;
    uint32_t rng;
    bool over, won;
    uint8_t map; /* Uses legacy zeroed tail padding; storage ABI stays unchanged. */
} snake_t;
_Static_assert(sizeof(snake_t)==208,"Keep legacy snake save size");
bool snake_wall(unsigned map,unsigned cell);
unsigned snake_capacity(unsigned map);
const char *snake_map_name(unsigned map);
void snake_init_map(snake_t *s,uint32_t seed,unsigned map);
void snake_init(snake_t *s,uint32_t seed);
bool snake_turn(snake_t *s,int turn);
void snake_step(snake_t *s);
unsigned snake_interval(const snake_t *s);
#endif
