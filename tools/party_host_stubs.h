#include "rps_host_stubs.h"
#define ESP_LOGI(...) ((void)0)
enum { EC06_EVENT_NONE,EC06_EVENT_KEY_DOWN,EC06_EVENT_ENC_DOWN,EC06_EVENT_ENC_UP,EC06_EVENT_CW,EC06_EVENT_CCW };
typedef struct { int type;uint32_t duration_ms;uint64_t timestamp_us; } ec06_event_t;
ec06_event_t ec06_pop_event(void);
