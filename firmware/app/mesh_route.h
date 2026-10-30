#ifndef MESH_ROUTE_H
#define MESH_ROUTE_H
#include "mesh_packet.h"
#define ROUTE_PEERS 16
#define ROUTE_PENDING 8
#define ROUTE_SEEN 64
#define ROUTE_EVENTS 8
typedef struct {bool used;uint8_t src[6],dst[6],type,hops;int8_t rssi;uint32_t session,id,at;} route_event_t;
typedef struct {uint8_t mac[6];char name[14];int8_t rssi;uint32_t seen;bool used;} route_peer_t;
typedef struct {uint8_t mac[6],type;uint32_t session,id,time;bool used;} route_seen_t;
typedef struct {mesh_packet_t packet;uint32_t due,created;bool used;} route_pending_t;
typedef struct {
    bool enabled;uint8_t last_rx_hops;
    uint32_t forwarded,duplicates,dropped,last_forward,beacon_at;
    bool beacon_sent,forward_sent;
    route_peer_t peers[ROUTE_PEERS];
    route_seen_t seen[ROUTE_SEEN];unsigned cursor;
    route_pending_t pending[ROUTE_PENDING];
    route_event_t events[ROUTE_EVENTS];unsigned event_cursor;
} mesh_route_t;
void mesh_route_enable(mesh_route_t *s,bool enabled);
void mesh_route_receive(mesh_route_t *s,const mesh_packet_t *p,const uint8_t self[6],uint8_t channel,int8_t rssi,uint32_t now,uint32_t random);
bool mesh_route_pop(mesh_route_t *s,mesh_packet_t *p,uint32_t now);
bool mesh_route_beacon_due(mesh_route_t *s,uint32_t now);
unsigned mesh_route_peers(const mesh_route_t *s,uint32_t now);
#endif
