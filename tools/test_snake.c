#include "snake.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void valid(const snake_t *s){
    assert(s->length>=3 && s->length<=SNAKE_CELLS);
    for(unsigned i=0;i<s->length;i++){
        assert(s->body[i]<SNAKE_CELLS);
        if(!s->won)assert(s->food!=s->body[i]);
        for(unsigned j=0;j<i;j++)assert(s->body[i]!=s->body[j]);
    }
}
int main(void){
    snake_t s,t;snake_init(&s,0);valid(&s);assert(s.length==3 && snake_interval(&s)==240);
    snake_init(&t,0);assert(memcmp(&s,&t,sizeof(s))==0);
    s.food=s.body[0]+1;snake_step(&s);assert(s.length==4 && snake_interval(&s)==230);valid(&s);
    assert(snake_turn(&s,1));assert(!snake_turn(&s,1));snake_step(&s);assert(s.direction==1);valid(&s);
    snake_init(&s,1);s.body[0]=23;s.direction=0;snake_step(&s);assert(s.over);
    snake_init(&s,1);s.body[0]=25;s.body[1]=26;s.body[2]=27;snake_step(&s);assert(s.over);
    snake_init(&s,1);s.length=4;s.body[0]=25;s.body[1]=1;s.body[2]=0;s.body[3]=24;
    s.direction=2;s.food=100;snake_step(&s);assert(!s.over && s.body[0]==24);valid(&s);
    snake_init(&s,1);s.length=191;for(unsigned i=0;i<191;i++)s.body[i]=(uint8_t)(i+1);
    s.food=0;s.direction=2;snake_step(&s);assert(s.won && s.over && s.length==192);valid(&s);
    assert(snake_interval(&s)==90);assert(!snake_turn(&s,1));
    uint32_t r=123;
    for(unsigned i=0;i<50000;i++){
        r=r*1664525u+1013904223u;if(s.over)snake_init(&s,r);
        if(r&1)snake_turn(&s,(r&2)?1:-1);
        snake_step(&s);valid(&s);
    }
    puts("PASS snake: seed, growth, food placement, turn guard, wall/self/tail, full-board win, speed, 50000 steps");
}
