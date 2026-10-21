/* One versioned NVS key per relationship; commits happen before protocol acknowledgements. */
#include "friend_store.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mesh_packet.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>
typedef struct {
    /* Fixed 32-byte representation saves NVS entries and never stores ABI padding. */
    uint8_t bytes[32];
} stored_friend_t;
static stored_friend_t encode_record(const friend_record_t *r) {
    stored_friend_t d = {{0}};
    d.bytes[0] = 1;
    d.bytes[1] = r->state;
    memcpy(d.bytes + 2, r->mac, 6);
    for (unsigned i = 0; i < 8; i++)
        d.bytes[8 + i] = (uint8_t)(r->transaction_id >> (8 * i));
    memcpy(d.bytes + 16, r->name, sizeof(r->name));
    return d;
}
static friend_record_t decode_record(const stored_friend_t *d) {
    friend_record_t r = {0};
    r.state = d->bytes[1];
    memcpy(r.mac, d->bytes + 2, 6);
    for (unsigned i = 0; i < 8; i++)
        r.transaction_id |= (uint64_t)d->bytes[8 + i] << (8 * i);
    memcpy(r.name, d->bytes + 16, sizeof(r.name));
    return r;
}
static friend_record_t s_records[FRIEND_STORE_RECORD_MAX];
static SemaphoreHandle_t s_lock;
static bool s_ready;
static const char *TAG = "friend_store";
static bool valid(const friend_record_t *r) {
    return mesh_mac_is_valid_peer(r->mac) && r->transaction_id &&
           r->state >= FRIEND_RECORD_PREPARED && r->state <= FRIEND_RECORD_REVOKED_ACKED &&
           memchr(r->name, 0, sizeof(r->name)) != NULL;
}
static int find(const uint8_t mac[6], uint64_t id) {
    for (unsigned i = 0; i < FRIEND_STORE_RECORD_MAX; i++)
        if (s_records[i].state && s_records[i].transaction_id == id &&
            mesh_mac_equal(mac, s_records[i].mac))
            return (int)i;
    return -1;
}
static unsigned live_count(void) {
    unsigned n = 0;
    for (unsigned i = 0; i < FRIEND_STORE_RECORD_MAX; i++)
        if (s_records[i].state && s_records[i].state < FRIEND_RECORD_REVOKED)
            n++;
    return n;
}
bool friend_store_init(void) {
    if (s_lock)
        return s_ready;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock)
        return false;
    nvs_handle_t h;
    esp_err_t rc = nvs_open("friends_v3", NVS_READWRITE, &h);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "open failed: %d", rc);
        return false;
    }
    for (unsigned i = 0; i < FRIEND_STORE_RECORD_MAX; i++) {
        char key[16];
        snprintf(key, sizeof(key), "f%02u", i);
        stored_friend_t s = {0};
        size_t n = sizeof(s);
        rc = nvs_get_blob(h, key, &s, &n);
        if (rc == ESP_ERR_NVS_NOT_FOUND)
            continue;
        friend_record_t r = decode_record(&s);
        if (rc != ESP_OK || n != sizeof(s) || s.bytes[0] != 1 || !valid(&r)) {
            /* Preserve unknown/corrupt data and fail closed instead of overwriting it. */
            ESP_LOGE(TAG, "invalid record %s; store read-only", key);
            nvs_close(h);
            return false;
        }
        s_records[i] = r;
    }
    nvs_close(h);
    s_ready = true;
    return true;
}
bool friend_store_ready(void) { return s_ready; }
bool friend_store_get(const uint8_t mac[6], uint64_t id, friend_record_t *out) {
    if (!s_lock || !mac || !out)
        return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int i = find(mac, id);
    if (i >= 0)
        *out = s_records[i];
    xSemaphoreGive(s_lock);
    return i >= 0;
}
bool friend_store_get_active(const uint8_t mac[6], friend_record_t *out) {
    if (!s_lock || !mac)
        return false;
    bool found = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (unsigned i = 0; i < FRIEND_STORE_RECORD_MAX; i++)
        if (s_records[i].state == FRIEND_RECORD_ACTIVE && mesh_mac_equal(mac, s_records[i].mac)) {
            if (out)
                *out = s_records[i];
            found = true;
            break;
        }
    xSemaphoreGive(s_lock);
    return s_ready && found;
}
bool friend_store_get_record(unsigned i, friend_record_t *out) {
    if (!s_lock || !out || i >= FRIEND_STORE_RECORD_MAX)
        return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_records[i];
    xSemaphoreGive(s_lock);
    return out->state != 0;
}
bool friend_store_get_friend(unsigned index, friend_record_t *out) {
    if (!s_lock || !out)
        return false;
    bool found = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (unsigned i = 0; i < FRIEND_STORE_RECORD_MAX; i++)
        if (s_records[i].state == FRIEND_RECORD_ACTIVE) {
            if (index-- == 0) {
                *out = s_records[i];
                found = true;
                break;
            }
        }
    xSemaphoreGive(s_lock);
    return s_ready && found;
}
unsigned friend_store_count(void) {
    if (!s_lock || !s_ready)
        return 0;
    unsigned n = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (unsigned i = 0; i < FRIEND_STORE_RECORD_MAX; i++)
        if (s_records[i].state == FRIEND_RECORD_ACTIVE)
            n++;
    xSemaphoreGive(s_lock);
    return n;
}
bool friend_store_has_capacity(void) {
    if (!s_ready)
        return false;
    bool free_slot = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (unsigned i = 0; i < FRIEND_STORE_RECORD_MAX; i++)
        if (!s_records[i].state || s_records[i].state == FRIEND_RECORD_REVOKED_ACKED)
            free_slot = true;
    bool ok = free_slot && live_count() < FRIEND_STORE_MAX;
    xSemaphoreGive(s_lock);
    return ok;
}
bool friend_store_put(const friend_record_t *r) {
    if (!s_ready || !r || !valid(r))
        return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int i = find(r->mac, r->transaction_id);
    if (i >= 0 && !memcmp(&s_records[i], r, sizeof(*r))) {
        xSemaphoreGive(s_lock);
        return true;
    }
    if (i < 0) {
        if (r->state < FRIEND_RECORD_REVOKED && live_count() >= FRIEND_STORE_MAX) {
            xSemaphoreGive(s_lock);
            return false;
        }
        for (unsigned k = 0; k < FRIEND_STORE_RECORD_MAX; k++)
            if (!s_records[k].state || s_records[k].state == FRIEND_RECORD_REVOKED_ACKED) {
                i = (int)k;
                break;
            }
    }
    if (i < 0) {
        xSemaphoreGive(s_lock);
        return false;
    }
    char key[16];
    snprintf(key, sizeof(key), "f%02u", (unsigned)i);
    stored_friend_t s = encode_record(r);
    nvs_handle_t h;
    esp_err_t rc = nvs_open("friends_v3", NVS_READWRITE, &h);
    if (rc == ESP_OK) {
        rc = nvs_set_blob(h, key, &s, sizeof(s));
        if (rc == ESP_OK)
            rc = nvs_commit(h);
        nvs_close(h);
    }
    if (rc == ESP_OK)
        s_records[i] = *r;
    else
        ESP_LOGE(TAG, "commit failed: %d", rc);
    xSemaphoreGive(s_lock);
    return rc == ESP_OK;
}
bool friend_store_forget_prepared(const uint8_t mac[6], uint64_t id) {
    if (!s_ready)
        return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int i = find(mac, id);
    if (i < 0) {
        xSemaphoreGive(s_lock);
        return true;
    }
    if (s_records[i].state != FRIEND_RECORD_PREPARED) {
        xSemaphoreGive(s_lock);
        return false;
    }
    char key[16];
    snprintf(key, sizeof(key), "f%02u", (unsigned)i);
    nvs_handle_t h;
    esp_err_t rc = nvs_open("friends_v3", NVS_READWRITE, &h);
    if (rc == ESP_OK) {
        rc = nvs_erase_key(h, key);
        if (rc == ESP_OK)
            rc = nvs_commit(h);
        nvs_close(h);
    }
    if (rc == ESP_OK)
        memset(&s_records[i], 0, sizeof(s_records[i]));
    xSemaphoreGive(s_lock);
    return rc == ESP_OK;
}
#ifdef FRIEND_MGR_TEST
void friend_store_test_reboot(void) {
    if (s_lock)
        vSemaphoreDelete(s_lock);
    s_lock = NULL;
    s_ready = false;
    memset(s_records, 0, sizeof(s_records));
}
#endif
