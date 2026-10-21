#include "rps_host_stubs.h"
enum { M_TREASURE=48,M_REACTION=49,M_NUMBER_BOMB=51,M_TIME_GUESS=52 };
enum { HAPTIC_RADAR=24 };
enum { EC06_EVENT_NONE,EC06_EVENT_KEY_DOWN,EC06_EVENT_ENC_DOWN,EC06_EVENT_ENC_UP };
typedef struct { int type; uint32_t duration_ms; uint64_t timestamp_us; } ec06_event_t;
ec06_event_t ec06_pop_event(void);
void u8g2_DrawFrame(void *,int,int,int,int);
void u8g2_DrawBox(void *,int,int,int,int);
