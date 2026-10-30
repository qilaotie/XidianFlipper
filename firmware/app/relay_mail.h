#ifndef RELAY_MAIL_H
#define RELAY_MAIL_H
#include "mesh_packet.h"
#define RELAY_MAIL_SLOTS 4
typedef struct {uint16_t attempts,minutes;mesh_packet_t packet;} relay_mail_entry_t;
typedef struct {bool enabled,error;unsigned count;uint32_t stored,delivered,expired;} relay_mail_view_t;
void relay_mail_enable(bool,uint32_t now);
void relay_mail_view(relay_mail_view_t *);
bool relay_mail_get(unsigned,relay_mail_entry_t *);
bool relay_mail_remove(unsigned);
void relay_mail_observe(const mesh_packet_t *,const uint8_t self[6],uint32_t now);
bool relay_mail_take(mesh_packet_t *,uint32_t now);
bool relay_mail_cancelled(const mesh_packet_t *);
#endif
