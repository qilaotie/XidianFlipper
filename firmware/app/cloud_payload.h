#ifndef CLOUD_PAYLOAD_H
#define CLOUD_PAYLOAD_H
#include <stddef.h>
#include <stdint.h>
#include "pet.h"
typedef struct { bool present; uint32_t score,progress; } cloud_score_t;
void cloud_hex(char *out,const uint8_t *bytes,size_t count);
bool cloud_payload(char *out,size_t capacity,const char *id,const char *nickname,
                   const pet_t *pet,const cloud_score_t scores[2]);
#endif
