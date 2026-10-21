#ifndef FRIEND_STORE_H
#define FRIEND_STORE_H
#include <stdbool.h>
#include <stdint.h>
#define FRIEND_STORE_MAX 32
#define FRIEND_STORE_RECORD_MAX 64
#define FRIEND_NAME_MAX 13
typedef enum {
    FRIEND_RECORD_PREPARED = 1,  /* local user accepted and durable; peer not yet committed */
    FRIEND_RECORD_COMMITTED = 2, /* both users accepted; durable commit, waiting final handshake */
    FRIEND_RECORD_ACTIVE = 3,
    FRIEND_RECORD_REVOKED = 4,
    FRIEND_RECORD_REVOKED_ACKED = 5
} friend_record_state_t;
typedef struct {
    uint8_t mac[6];
    uint64_t transaction_id;
    char name[FRIEND_NAME_MAX + 1];
    uint8_t state;
} friend_record_t;
bool friend_store_init(void);
bool friend_store_ready(void);
bool friend_store_get(const uint8_t mac[6], uint64_t transaction_id, friend_record_t *out);
bool friend_store_get_active(const uint8_t mac[6], friend_record_t *out);
bool friend_store_get_record(unsigned index, friend_record_t *out);
bool friend_store_get_friend(unsigned index, friend_record_t *out);
unsigned friend_store_count(void);
bool friend_store_put(const friend_record_t *record);
bool friend_store_forget_prepared(const uint8_t mac[6], uint64_t transaction_id);
bool friend_store_has_capacity(void);
#endif
