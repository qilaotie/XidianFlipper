#ifndef HALFSTEP_TETRIS_H
#define HALFSTEP_TETRIS_H
#include <stdbool.h>
#include <stdint.h>
#define TETRIS_W 10
#define TETRIS_H 20
typedef struct {
    uint16_t rows[TETRIS_H];
    uint16_t clear_rows[TETRIS_H];
    uint32_t clear_mask; /* Locked board snapshot for the presentation-only clear effect. */
    uint32_t rng, score;
    unsigned lines;
    uint8_t piece, next, rotation, bag[7], remaining;
    int8_t x, y;
    bool over;
} tetris_t;
uint16_t tetris_shape(unsigned piece, unsigned rotation);
void tetris_init(tetris_t *g, uint32_t seed);
bool tetris_move(tetris_t *g, int dx);
bool tetris_rotate(tetris_t *g);
bool tetris_step(tetris_t *g);
unsigned tetris_drop(tetris_t *g);
bool tetris_fits(const tetris_t *g, int x, int y, unsigned rotation);
unsigned tetris_interval(const tetris_t *g);
#endif
