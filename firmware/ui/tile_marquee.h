#ifndef TILE_MARQUEE_H
#define TILE_MARQUEE_H
#include <stdint.h>
/* Pixels, time based rather than frame based. Pause 900ms at both ends. */
static inline unsigned tile_marquee_offset(unsigned width,unsigned available,uint32_t elapsed){
    if(width<=available)return 0;
    unsigned distance=width-available;
    uint32_t travel=distance*35,phase=elapsed%(2*travel+1800);
    if(phase<900)return 0;
    phase-=900;
    if(phase<travel)return phase/35;
    phase-=travel;
    if(phase<900)return distance;
    return distance-(phase-900)/35;
}
#endif
