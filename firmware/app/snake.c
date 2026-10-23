#include "snake.h"
#include <string.h>
bool snake_wall(unsigned map,unsigned cell){
    unsigned x=cell%SNAKE_W,y=cell/SNAKE_W;
    if(map==1)return (x==10 || x==16) && (y==1 || y==2 || y==5 || y==6);
    if(map==2)return (y==2 || y==5) && x>=4 && x<=19 && x!=11 && x!=12;
    return false;
}
unsigned snake_capacity(unsigned map){
    unsigned n=SNAKE_CELLS;for(unsigned p=0;p<SNAKE_CELLS;p++)if(snake_wall(map,p))n--;return n;
}
const char *snake_map_name(unsigned map){
    static const char *const names[]={"CLASSIC","PILLARS","GATES"};return names[map<SNAKE_MAPS?map:0];
}
static void food(snake_t *s){
    unsigned capacity=snake_capacity(s->map);
    if(s->length==capacity){s->won=s->over=true;return;}
    uint32_t x=s->rng;x^=x<<13;x^=x>>17;x^=x<<5;s->rng=x;
    unsigned target=x%(capacity-s->length);
    for(unsigned p=0;p<SNAKE_CELLS;p++){
        bool occupied=snake_wall(s->map,p);
        for(unsigned i=0;i<s->length;i++)if(s->body[i]==p){occupied=true;break;}
        if(!occupied){if(!target){s->food=(uint8_t)p;return;}target--;}
    }
}
void snake_init(snake_t *s,uint32_t seed){
    snake_init_map(s,seed,0);
}
void snake_init_map(snake_t *s,uint32_t seed,unsigned map){
    memset(s,0,sizeof(*s));s->rng=seed?seed:0x6D2B79F5u;s->length=3;
    s->map=map<SNAKE_MAPS?(uint8_t)map:0;
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
    if(snake_wall(s->map,head)){s->over=true;return;}
    unsigned keep=s->length-(grow?0:1);
    for(unsigned i=0;i<keep;i++)if(s->body[i]==head){s->over=true;return;}
    memmove(s->body+1,s->body,keep);s->body[0]=head;
    if(grow){s->length++;food(s);}
}
unsigned snake_interval(const snake_t *s){unsigned eaten=s->length-3;return eaten>=15?90:240-eaten*10;}
