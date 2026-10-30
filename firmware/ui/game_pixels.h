#ifndef GAME_PIXELS_H
#define GAME_PIXELS_H
/* Small, bounded one-bit game drawings shared by nearby game pages. */
static inline void game_caption(int x,int y,const char *text){char label[27];snprintf(label,sizeof(label),"%.*s",(128-x)/5,text);u8g2_DrawStr(&u8g2,x,y,label);}
static inline void game_ascii_fit(void *d,int x,int y,const char *text,unsigned width){char label[27];unsigned n=width/5;if(n>(unsigned)(128-x)/5)n=(128-x)/5;snprintf(label,sizeof(label),"%.*s",(int)n,text);u8g2_DrawStr(d,x,y,label);}
static inline void game_go(int x,int y){
    static const uint8_t letters[2][7]={{14,17,16,23,17,17,14},{14,17,17,17,17,17,14}};
    for(unsigned c=0;c<2;c++)for(unsigned row=0;row<7;row++)for(unsigned col=0;col<5;col++)if(letters[c][row]&(1u<<(4-col)))u8g2_DrawBox(&u8g2,x+c*14+col*2,y+row*2,2,2);
}
static inline void game_digit(int x,int y,unsigned value){
    static const unsigned masks[]={63,6,91,79,102,109,125,7,127,111};unsigned m=masks[value%10];
    if(m&1)u8g2_DrawBox(&u8g2,x+2,y,8,2);
    if(m&2)u8g2_DrawBox(&u8g2,x+10,y+2,2,6);
    if(m&4)u8g2_DrawBox(&u8g2,x+10,y+10,2,6);
    if(m&8)u8g2_DrawBox(&u8g2,x+2,y+16,8,2);
    if(m&16)u8g2_DrawBox(&u8g2,x,y+10,2,6);
    if(m&32)u8g2_DrawBox(&u8g2,x,y+2,2,6);
    if(m&64)u8g2_DrawBox(&u8g2,x+2,y+8,8,2);
}
static inline void game_number(int x,int y,unsigned value){
    if(value>=100){game_digit(x,y,value/100);x+=14;}
    if(value>=10){game_digit(x,y,value/10);x+=14;}
    game_digit(x,y,value);
}
static inline void game_hand(int x,int y,unsigned kind){
    static const uint16_t shapes[3][16]={
      {0,0,0,0x07f0,0x0ff8,0x1248,0x1248,0x1ff8,0x3008,0x2008,0x2008,0x1010,0x0ff0,0x0810,0x0810,0x0ff0},
      {0x0810,0x1428,0x1428,0x1428,0x1428,0x1248,0x1248,0x1248,0x1ff8,0x2008,0x2008,0x1010,0x0ff0,0x0810,0x0810,0x0ff0},
      {0x0240,0x05a0,0x05a8,0x15b4,0x35b4,0x35b4,0x35b4,0x35b4,0x35fc,0x2004,0x2004,0x1008,0x0ff0,0x0810,0x0810,0x0ff0}};
    if(kind<1 || kind>3)return;
    for(unsigned row=0;row<16;row++)for(unsigned col=0;col<16;col++)if(shapes[kind-1][row]&(1u<<col))u8g2_DrawBox(&u8g2,x+col,y+row,1,1);
}
static inline void game_ring(int cx,int cy,int radius){
    int x=radius,y=0,err=1-radius;
    while(x>=y){
        u8g2_DrawBox(&u8g2,cx+x,cy+y,1,1);u8g2_DrawBox(&u8g2,cx+y,cy+x,1,1);
        u8g2_DrawBox(&u8g2,cx-y,cy+x,1,1);u8g2_DrawBox(&u8g2,cx-x,cy+y,1,1);
        u8g2_DrawBox(&u8g2,cx-x,cy-y,1,1);u8g2_DrawBox(&u8g2,cx-y,cy-x,1,1);
        u8g2_DrawBox(&u8g2,cx+y,cy-x,1,1);u8g2_DrawBox(&u8g2,cx+x,cy-y,1,1);
        y++;if(err<0)err+=2*y+1;else{x--;err+=2*(y-x)+1;}
    }
}
#endif
