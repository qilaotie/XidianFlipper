#ifndef ACTIVITY_LOG_H
#define ACTIVITY_LOG_H
#include <stdbool.h>
#include <stdint.h>
#define ACTIVITY_LOG_MAX 12
enum {ACTIVITY_POLL=1,ACTIVITY_TASK,ACTIVITY_CHECKIN,ACTIVITY_QUIZ};
typedef struct {
    uint32_t boot,id,sequence,uptime_s;
    uint8_t kind,channel,part,host[6],peer[6],values[4];char title[49];
} activity_record_t;
/* One owner, own namespace; unavailable/corrupt/future data is never erased. */
bool activity_log_init(void);
bool activity_log_append(const activity_record_t *);
bool activity_log_get(unsigned newest_first,activity_record_t *);
unsigned activity_log_count(void);
bool activity_log_error(void);
#endif
