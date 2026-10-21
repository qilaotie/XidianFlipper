#ifndef PET_LINK_H
#define PET_LINK_H
#include "mesh_packet.h"
void pet_link_receive(const mesh_packet_t *packet,int8_t rssi);
void pet_link_enter(bool duel);
void pet_link_leave(void);
/* UI owner only. BACK is handled by the parent page. */
void pet_link_proc(unsigned input);
#endif
