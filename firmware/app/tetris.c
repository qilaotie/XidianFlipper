/* Allocation-free falling blocks. Four-by-four masks and a shuffled seven bag. */
#include "tetris.h"
#include <string.h>
static const uint16_t shapes[7][4]={
    {0x00F0,0x4444,0x0F00,0x2222}, /* I */
    {0x0066,0x0066,0x0066,0x0066}, /* O */
    {0x0072,0x0262,0x0270,0x0232}, /* T */
    {0x0036,0x0462,0x0360,0x0231}, /* S */
    {0x0063,0x0264,0x0630,0x0132}, /* Z */
    {0x0071,0x0226,0x0470,0x0322}, /* J */
    {0x0074,0x0622,0x0170,0x0223}, /* L */
};
uint16_t tetris_shape(unsigned p,unsigned r){return shapes[p%7][r%4];}
static uint32_t random_next(tetris_t *g){
    uint32_t x=g->rng;x^=x<<13;x^=x>>17;x^=x<<5;return g->rng=x;
}
static uint8_t draw(tetris_t *g){
    if(!g->remaining){
        for(unsigned i=0;i<7;i++)g->bag[i]=(uint8_t)i;
        for(unsigned i=6;i>0;i--){unsigned j=random_next(g)%(i+1);uint8_t t=g->bag[i];g->bag[i]=g->bag[j];g->bag[j]=t;}
        g->remaining=7;
    }
    return g->bag[--g->remaining];
}
bool tetris_fits(const tetris_t *g,int x,int y,unsigned r){
    uint16_t shape=tetris_shape(g->piece,r);
    for(int row=0;row<4;row++)for(int col=0;col<4;col++)if(shape&(1u<<(row*4+col))){
        int px=x+col,py=y+row;
        if(px<0 || px>=TETRIS_W || py<0 || py>=TETRIS_H || (g->rows[py]&(1u<<px)))return false;
    }
    return true;
}
static void spawn(tetris_t *g){
    g->piece=g->next;g->next=draw(g);g->x=3;g->y=0;g->rotation=0;
    if(!tetris_fits(g,g->x,g->y,0))g->over=true;
}
void tetris_init(tetris_t *g,uint32_t seed){
    memset(g,0,sizeof(*g));g->rng=seed?seed:0x6D2B79F5u;g->next=draw(g);spawn(g);
}
bool tetris_move(tetris_t *g,int dx){
    if(g->over || (dx!=1 && dx!=-1) || !tetris_fits(g,g->x+dx,g->y,g->rotation))return false;
    g->x=(int8_t)(g->x+dx);return true;
}
bool tetris_rotate(tetris_t *g){
    if(g->over)return false;
    static const int8_t kicks[]={0,-1,1,-2,2};
    unsigned r=(g->rotation+1)%4;
    for(unsigned i=0;i<sizeof(kicks);i++)if(tetris_fits(g,g->x+kicks[i],g->y,r)){
        g->x=(int8_t)(g->x+kicks[i]);g->rotation=(uint8_t)r;return true;
    }
    return false; /* Simple horizontal kicks, not guideline SRS. */
}
static void lock_piece(tetris_t *g){
    uint16_t shape=tetris_shape(g->piece,g->rotation);
    for(int row=0;row<4;row++)for(int col=0;col<4;col++)if(shape&(1u<<(row*4+col)))
        g->rows[g->y+row]|=(uint16_t)(1u<<(g->x+col));
    g->clear_mask=0;
    for(unsigned row=0;row<TETRIS_H;row++)if(g->rows[row]==0x3FF)g->clear_mask|=1u<<row;
    if(g->clear_mask)memcpy(g->clear_rows,g->rows,sizeof(g->rows));
    unsigned cleared=0;int dst=TETRIS_H-1;
    for(int src=TETRIS_H-1;src>=0;src--){
        if(g->rows[src]==0x3FF){cleared++;continue;}
        g->rows[dst--]=g->rows[src];
    }
    while(dst>=0)g->rows[dst--]=0;
    static const unsigned points[]={0,100,300,500,800};
    g->score+=points[cleared]*(1+g->lines/10);g->lines+=cleared;spawn(g);
}
bool tetris_step(tetris_t *g){
    if(g->over)return false;
    if(tetris_fits(g,g->x,g->y+1,g->rotation)){g->y++;return true;}
    lock_piece(g);return false;
}
unsigned tetris_drop(tetris_t *g){
    if(g->over)return 0;
    unsigned distance=0;
    while(tetris_fits(g,g->x,g->y+1,g->rotation)){g->y++;distance++;}
    g->score+=distance*2;lock_piece(g);return distance;
}
unsigned tetris_interval(const tetris_t *g){unsigned level=g->lines/10;return level>=10?120:820-level*70;}
