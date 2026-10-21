#ifndef RPS_GAME_H
#define RPS_GAME_H
#include "mesh_packet.h"
void rps_game_receive(const mesh_packet_t *packet, int8_t rssi);
void rps_game_leave(void);
void rps_proc(void);
#endif
