/* Storage sink for UI-only geometry tests. Real persistence tested separately. */
#include "arcade_store.h"
#include "snake.h"
#include "tetris.h"
static unsigned arcade_mock_earned,arcade_mock_best[2];
int arcade_load(unsigned k,void *g){(void)k;(void)g;return 0;}
bool arcade_new(unsigned k,const void *g){(void)g;arcade_mock_earned=0;pet_service_reward_begin(k);return true;}
unsigned arcade_tick(unsigned k,const void *g,uint32_t now){
 (void)now;unsigned p=k==ARCADE_SNAKE?(unsigned)((const snake_t *)g)->length-3u:((const tetris_t *)g)->lines;
 if(p>arcade_mock_best[k])arcade_mock_best[k]=p;
 arcade_mock_earned+=pet_service_reward(k,p);return arcade_mock_earned;
}
bool arcade_flush(void){return true;}
bool arcade_error(unsigned k){(void)k;return false;}
uint32_t arcade_best(unsigned k,bool progress){(void)progress;return arcade_mock_best[k];}
