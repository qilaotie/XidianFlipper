#ifndef ARCADE_STORE_H
#define ARCADE_STORE_H
#include <stdbool.h>
#include <stdint.h>
enum {ARCADE_TETRIS,ARCADE_SNAKE,ARCADE_COUNT};
/* load: 1 resumable, 0 none/completed, -1 invalid/unreadable (not overwritten). */
int arcade_load(unsigned kind,void *game);
bool arcade_new(unsigned kind,const void *game);
unsigned arcade_tick(unsigned kind,const void *game,uint32_t now);
bool arcade_flush(void);
bool arcade_error(unsigned kind);
uint32_t arcade_best(unsigned kind,bool progress);
/* UI-owner read-only summary; never resets a game or advances reward marks. */
bool arcade_record_best(unsigned kind,uint32_t *score,uint32_t *progress);
#endif
