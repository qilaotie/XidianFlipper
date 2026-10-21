#include "pet_games.h"
#include <string.h>
static uint32_t random_next(pet_game_t *g){uint32_t x=g->random;x^=x<<13;x^=x>>17;x^=x<<5;return g->random=x;}
const char *pet_games_name(unsigned k){
    static const char *const names[]={"GET NOTES","BUMP","FLAGS","HEADING","MEMORY","SPRINT"};
    return k<PG_COUNT?names[k]:"?";
}
static void finish(pet_game_t *g){
    g->done=true;
    static const uint8_t goals[]={6,3,6,6,3,30};
    g->won=g->score>=goals[g->kind];
}
static void memory_show(pet_game_t *g,uint32_t now){
    g->phase=0;g->index=0;g->deadline=now+700;
    g->target=g->sequence[0];
}
void pet_games_start(pet_game_t *g,unsigned k,uint32_t seed){
    memset(g,0,sizeof(*g));g->kind=k<PG_COUNT?k:0;g->random=seed?seed:1;
    g->lane=1;g->target=(uint8_t)(random_next(g)%3);g->deadline=1500;g->length=2;
    for(unsigned i=0;i<8;i++)g->sequence[i]=(uint8_t)(1+random_next(g)%3);
    if(k==PG_MEMORY)memory_show(g,0);
}
void pet_games_step(pet_game_t *g,uint32_t t,unsigned input){
    if(g->done || t<g->last)return;
    g->last=t;
    if(t>=20000){finish(g);return;}
    if(g->kind==PG_MEMORY){
        if(!g->phase){
            if(t>=g->deadline){
                if(++g->index>=g->length){g->phase=1;g->index=0;g->target=0;g->deadline=t+5000;}
                else {g->target=g->sequence[g->index];g->deadline=t+700;}
            }
        }else if(input){
            if(input!=g->sequence[g->index]){g->mistakes++;finish(g);}
            else if(++g->index>=g->length){
                g->score++;if(++g->length>5)finish(g);else memory_show(g,t);
            }else g->deadline=t+5000;
        }else if(t>=g->deadline){g->mistakes++;finish(g);}
        return;
    }
    if(g->kind==PG_SPRINT){
        if(input==PG_TAP && (!g->pressed || t-g->pressed>=110)){g->score++;g->pressed=t;}
        if(g->score>=40)finish(g);
        return;
    }
    if(g->kind==PG_BUMP){
        g->meter=(uint8_t)((t/35)%20);if(g->meter>10)g->meter=20-g->meter;
        if(input==PG_TAP && t>=g->pressed){
            if(g->meter>=7)g->score++;else g->mistakes++;
            g->round++;g->pressed=t+500;
            if(g->round>=5)finish(g);
        }
        return;
    }
    if(g->kind==PG_FLAGS){
        if(input && t>=g->pressed && t<g->deadline){
            if(input==g->target+1u)g->score++;else g->mistakes++;
            g->round++;g->target=(uint8_t)(random_next(g)%3);g->deadline=t+1200;g->pressed=t+250;
        }else if(t>=g->deadline){g->mistakes++;g->round++;g->target=(uint8_t)(random_next(g)%3);g->deadline=t+1200;}
        if(g->round>=10)finish(g);
        return;
    }
    if(input==PG_LEFT && g->lane)g->lane--;
    if(input==PG_RIGHT && g->lane<2)g->lane++;
    if(g->kind==PG_HEADING && input==PG_TAP && t+300>=g->deadline && t<=g->deadline){
        if(g->lane==g->target)g->score++;else g->mistakes++;
        g->round++;g->target=(uint8_t)(random_next(g)%3);g->deadline=t+1300;
    }else if(t>=g->deadline){
        if(g->kind==PG_NOTES && g->lane==g->target)g->score++;else g->mistakes++;
        g->round++;g->target=(uint8_t)(random_next(g)%3);g->deadline=t+1300;
    }
    if(g->round>=10)finish(g);
}
