#ifndef MESSAGE_OUTBOX_H
#define MESSAGE_OUTBOX_H
#include "mesh_packet.h"
#define OUTBOX_SLOTS 6
#define OUTBOX_ROUNDS_MAX 120
enum {OUTBOX_EMPTY,OUTBOX_WAITING,OUTBOX_DELIVERED,OUTBOX_HELD};
typedef struct {uint8_t state;uint16_t rounds;mesh_packet_t packet;} outbox_entry_t;
/* Owner task/recursive message-service lock only. Never called from BLE callbacks. */
bool message_outbox_get(unsigned slot,outbox_entry_t *out);
int message_outbox_add(const mesh_packet_t *packet,uint32_t now);
int message_outbox_take(mesh_packet_t *packet,uint32_t now);
bool message_outbox_ack(const uint8_t peer[6],uint32_t session,uint32_t id);
bool message_outbox_retry(unsigned slot,uint32_t now);
bool message_outbox_remove(unsigned slot);
bool message_outbox_hold(unsigned slot);
bool message_outbox_error(void);
#endif
