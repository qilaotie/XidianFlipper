#ifndef NEARBY_GAMES_H
#define NEARBY_GAMES_H
#include "mesh_packet.h"
void nearby_games_receive(const mesh_packet_t *packet,int8_t rssi);
void nearby_games_enter(uint8_t page);
void nearby_games_proc(void);
#endif
