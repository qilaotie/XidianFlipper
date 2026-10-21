#ifndef CAMERA_REMOTE_H
#define CAMERA_REMOTE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { const char *name;bool media;uint16_t usage; } camera_key_t;
static const camera_key_t camera_keys[]={
    {"VOL+",true,0xE9},{"VOL-",true,0xEA},{"ENTER",false,0x28},{"SPACE",false,0x2C}
};
static const uint16_t camera_pulses[]={80,120,200};
#define CAMERA_KEY_COUNT (sizeof(camera_keys)/sizeof(camera_keys[0]))
#define CAMERA_PULSE_COUNT (sizeof(camera_pulses)/sizeof(camera_pulses[0]))
#endif
