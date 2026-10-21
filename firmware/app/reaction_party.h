#ifndef REACTION_PARTY_H
#define REACTION_PARTY_H
#include "mesh_packet.h"
enum { PARTY_GAME_REACTION,PARTY_GAME_BOMB,PARTY_GAME_TIME };
void party_game_enter(uint8_t game,bool active);
void reaction_party_enter(bool active);
void reaction_party_receive(const mesh_packet_t *packet);
void reaction_party_proc(void);
#endif
