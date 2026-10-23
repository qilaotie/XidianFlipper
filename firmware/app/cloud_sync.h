#ifndef CLOUD_SYNC_H
#define CLOUD_SYNC_H
#include <stdbool.h>
/* All APIs called only by the UI owner. Worker receives immutable snapshots. */
typedef struct {bool enabled,pending_remove,busy,storage_error;int result;char id[7];const char *text;} cloud_sync_status_t;
void cloud_sync_tick(void);
void cloud_sync_status(cloud_sync_status_t *out);
bool cloud_sync_enable(bool enabled);
bool cloud_sync_now(void);
bool cloud_sync_remove(void);
#endif
