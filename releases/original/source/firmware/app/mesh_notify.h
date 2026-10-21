#ifndef MESH_NOTIFY_H
#define MESH_NOTIFY_H
#include "mesh_packet.h"
#include "message_service.h"
typedef bool (*mesh_notify_allowed_fn_t)(void);
typedef void (*mesh_notify_message_fn_t)(const message_inbox_entry_t *);
void mesh_notify_init(void);
void mesh_notify_dispatch(const mesh_packet_t *pkt, int8_t rssi);
void mesh_notify_process(void); /* UI task, including screen-off; never persists incoming packets */
void mesh_notify_set_handlers(mesh_notify_allowed_fn_t allowed,
                              mesh_notify_message_fn_t on_message);
#endif