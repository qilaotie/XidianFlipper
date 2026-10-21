#include "pet_games.h"
#include <assert.h>
#include <stdio.h>
static void win(pet_game_t *g){
    for(unsigned t=0;t<=20000 && !g->done;t+=10){
        unsigned key=PG_NONE;
        switch(g->kind){
        case PG_NOTES:case PG_HEADING:
            if(g->lane<g->target)key=PG_RIGHT;
            else if(g->lane>g->target)key=PG_LEFT;
            else if(g->kind==PG_HEADING && t+280>=g->deadline)key=PG_TAP;
            break;
        case PG_BUMP:if((t/35)%20>=7 && (t/35)%20<=13)key=PG_TAP;break;
        case PG_FLAGS:key=g->target+1;break;
        case PG_MEMORY:if(g->phase)key=g->sequence[g->index];break;
        case PG_SPRINT:key=PG_TAP;break;
        }
        pet_games_step(g,t,key);
    }
}
int main(void){
    for(unsigned k=0;k<PG_COUNT;k++)for(unsigned seed=0;seed<200;seed++){
        pet_game_t g;pet_games_start(&g,k,seed);win(&g);assert(g.done && g.won);
        unsigned score=g.score;pet_games_step(&g,22000,PG_TAP);assert(g.score==score);
        pet_games_start(&g,k,seed);pet_games_step(&g,20000,PG_NONE);assert(g.done && !g.won);
        pet_games_start(&g,k,seed);pet_games_step(&g,100,PG_NONE);pet_game_t saved=g;
        pet_games_step(&g,50,PG_TAP);assert(g.score==saved.score && g.last==100);
    }
    pet_game_t g;pet_games_start(&g,PG_MEMORY,123);
    while(!g.phase)pet_games_step(&g,g.deadline,PG_NONE);
    pet_games_step(&g,g.last+1,g.sequence[0]%3+1);assert(g.done && !g.won);
    pet_games_start(&g,PG_FLAGS,123);pet_games_step(&g,0,g.target+1);
    unsigned score=g.score;for(unsigned i=0;i<100;i++)pet_games_step(&g,0,g.target+1);assert(g.score==score);
    puts("PASS six pet games: 1200 seeded wins, timeout/loss, result freeze, memory errors, input debounce");
}
