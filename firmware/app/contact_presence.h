#ifndef CONTACT_PRESENCE_H
#define CONTACT_PRESENCE_H
#include "mesh_packet.h"
enum {CONTACT_UNKNOWN,CONTACT_RECENT,CONTACT_DIRECT};
typedef struct {uint8_t state,hops;int8_t rssi;uint32_t age_ms;} contact_presence_t;
typedef struct {bool used,direct;uint8_t mac[6],hops;int8_t rssi;uint32_t heard,direct_at;} contact_seen_t;
void contact_observe(contact_seen_t cache[32],const mesh_packet_t *,int8_t rssi,uint32_t now);
void contact_status(const contact_seen_t cache[32],const uint8_t mac[6],uint32_t now,contact_presence_t *);
#endif
