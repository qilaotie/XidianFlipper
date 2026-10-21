#include "snake.h"
#include <string.h>
static void food(snake_t *s){
    if(s->length==SNAKE_CELLS){s->won=s->over=true;return;}
    uint32_t x=s->rng;x^=x<<13;x^=x>>17;x^=x<<5;s->rng=x;
    unsigned target=x%(SNAKE_CELLS-s->length);
    for(unsigned p=0;p<SNAKE_CELLS;p++){
        bool occupied=false;
        for(unsigned i=0;i<s->length;i++)if(s->body[i]==p){occupied=true;break;}
        if(!occupied){if(!target){s->food=(uint8_t)p;return;}target--;}
    }
}
void snake_init(snake_t *s,uint32_t seed){
    memset(s,0,sizeof(*s));s->rng=seed?seed:0x6D2B79F5u;s->length=3;
    for(unsigned i=0;i<3;i++)s->body[i]=(uint8_t)(4*SNAKE_W+6-i);
    food(s);
}
bool snake_turn(snake_t *s,int turn){
    if(s->over || s->turn || (turn!=1 && turn!=-1))return false;
    s->turn=(int8_t)turn;return true; /* At most one relative turn per movement. */
}
void snake_step(snake_t *s){
    if(s->over)return;
    s->direction=(uint8_t)((s->direction+4+s->turn)%4);s->turn=0;
    static const int8_t dx[]={1,0,-1,0},dy[]={0,1,0,-1};
    int x=s->body[0]%SNAKE_W+dx[s->direction],y=s->body[0]/SNAKE_W+dy[s->direction];
    if(x<0 || x>=SNAKE_W || y<0 || y>=SNAKE_H){s->over=true;return;}
    uint8_t head=(uint8_t)(y*SNAKE_W+x);bool grow=head==s->food;
    unsigned keep=s->length-(grow?0:1);
    for(unsigned i=0;i<keep;i++)if(s->body[i]==head){s->over=true;return;}
    memmove(s->body+1,s->body,keep);s->body[0]=head;
    if(grow){s->length++;food(s);}
}
unsigned snake_interval(const snake_t *s){unsigned eaten=s->length-3;return eaten>=15?90:240-eaten*10;}
