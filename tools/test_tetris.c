#include "tetris.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned popcount(uint16_t n){unsigned count=0;while(n){count+=n&1;n>>=1;}return count;}
static void valid(const tetris_t *g){
    for(unsigned i=0;i<TETRIS_H;i++)assert(!(g->rows[i]&~0x3FF));
    if(!g->over)assert(tetris_fits(g,g->x,g->y,g->rotation));
}
int main(void){
    tetris_t g,other;
    for(unsigned p=0;p<7;p++)for(unsigned r=0;r<4;r++)assert(popcount(tetris_shape(p,r))==4);
    tetris_init(&g,123);tetris_init(&other,123);assert(memcmp(&g,&other,sizeof(g))==0);
    unsigned seen=1u<<g.piece;
    for(unsigned i=1;i<7;i++){
        memset(g.rows,0,sizeof(g.rows));tetris_drop(&g);assert(!(seen&(1u<<g.piece)));seen|=1u<<g.piece;
    }
    assert(seen==127);
    tetris_init(&g,0);g.piece=1;g.x=3;g.y=0;g.rotation=0;
    while(tetris_move(&g,-1)){}assert(!tetris_move(&g,-1));assert(g.x==-1);valid(&g);
    while(tetris_move(&g,1)){}assert(g.x==7);valid(&g);
    /* Vertical I at the right wall kicks left when rotating horizontal. */
    memset(g.rows,0,sizeof(g.rows));g.piece=0;g.rotation=1;g.x=7;g.y=3;
    assert(tetris_rotate(&g));assert(g.rotation==2 && g.x==6);valid(&g);
    /* Fill two rows except the O-piece gap, then drop the square. */
    tetris_init(&g,7);g.piece=1;g.rotation=0;g.x=3;g.y=0;
    g.rows[18]=g.rows[19]=(uint16_t)(0x3FF&~(3u<<4));
    assert(tetris_drop(&g)==18);assert(g.lines==2 && g.score==336);
    for(unsigned i=0;i<TETRIS_H;i++)assert(!g.rows[i]);
    /* A vertical I clears four rows. */
    tetris_init(&g,8);g.piece=0;g.rotation=1;g.x=2;g.y=0;
    for(unsigned y=16;y<20;y++)g.rows[y]=(uint16_t)(0x3FF&~(1u<<4));
    tetris_drop(&g);assert(g.lines==4 && g.score==832);valid(&g);
    /* Ceiling obstruction makes the next spawn end the game without writes out of bounds. */
    tetris_init(&g,9);g.piece=1;g.rotation=0;g.x=0;g.y=18;
    g.rows[0]=g.rows[1]=0x3FE;tetris_step(&g);assert(g.over);
    uint32_t score=g.score;assert(!tetris_move(&g,1));assert(!tetris_rotate(&g));assert(!tetris_step(&g));
    assert(tetris_drop(&g)==0 && g.score==score);
    tetris_init(&g,10);assert(tetris_interval(&g)==820);g.lines=100;assert(tetris_interval(&g)==120);
    g.lines=10000;assert(tetris_interval(&g)==120);
    /* Repeated seeded play checks collision and board invariants. */
    uint32_t random=77;
    for(unsigned i=0;i<50000;i++){
        random=random*1664525u+1013904223u;
        if(g.over)tetris_init(&g,random);
        switch((random>>16)%5){case 0:tetris_move(&g,-1);break;case 1:tetris_move(&g,1);break;
        case 2:tetris_rotate(&g);break;case 3:tetris_step(&g);break;default:tetris_drop(&g);break;}
        valid(&g);
    }
    puts("PASS tetris: masks, deterministic seven bag, walls/kicks, 2/4-line clear, scoring, top-out, speed, 50000 actions");
    return 0;
}
