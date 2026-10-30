#ifndef GIFT_RUNTIME_H
#define GIFT_RUNTIME_H
#include "pet_gifts.h"
#include "mesh_packet.h"
void gift_runtime_tick(bool armed);
void gift_runtime_receive(const mesh_packet_t *);
pet_gifts_t *gift_runtime_state(void); /* UI owner only */
#endif
