#ifndef SOCIAL_H
#define SOCIAL_H
#include "mesh_packet.h"
enum {SOC_FRAGMENT=1,SOC_CHAT,SOC_CARD=4,SOC_MEMBER,SOC_BEAT};
typedef bool (*social_send_fn)(void *,const uint8_t *,uint8_t);
typedef struct {uint8_t mac[6];char nickname[13],signature[25],pet[13];uint32_t seen;bool used;} social_card_t;
typedef struct {uint8_t mac[6];char pet[13];uint32_t visits,last_token;} passport_stamp_t;
typedef struct {uint32_t room;bool active,sharing,error;char room_name[19],nickname[13],signature[25],pet[13];passport_stamp_t stamps[12];unsigned stamps_count;social_card_t peers[8];} social_view_t;
void social_init(social_send_fn,void *);
void social_tick(const uint8_t self[6],uint8_t channel,bool enabled,uint32_t now);
void social_view(social_view_t *);
bool social_room(uint32_t code,const char *name); /* code 0 leaves; memory only activation */
bool social_edit(unsigned field,const char *); /* nickname/signature/pet */
bool social_share(bool);
bool social_stamp(const uint8_t mac[6],const char *pet,uint32_t token);
bool social_wrap(const uint8_t *bytes,uint8_t len);
/* Produces a validated local COLLAB/QUICK packet only after full same-room reassembly. */
bool social_receive(const mesh_packet_t *,mesh_packet_t *local);
uint32_t social_scope(void);
void social_ensemble_arm(bool on,unsigned role);
bool social_ensemble_start(unsigned tune,unsigned bpm);
bool social_ensemble_event(uint32_t *sequence,unsigned *note,unsigned *role,unsigned *beat);
#endif
