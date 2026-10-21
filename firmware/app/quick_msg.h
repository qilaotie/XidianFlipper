/** Quick-message presets; v3 sends the current UTF-8 text through message_service. */
#ifndef QUICK_MSG_H
#define QUICK_MSG_H
#include <stdint.h>
#include <stdbool.h>
void quick_msg_init(void);
const char *quick_msg_get(uint8_t index);
/* true means queued, not delivered. The service owns retries and actual receipts. */
bool quick_msg_send_by_id(uint8_t msg_id, const uint8_t *dst_mac);
#endif
