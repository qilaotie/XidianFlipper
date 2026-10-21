/**
 * @file    mesh_crypto.c
 * @brief   V9.0 加密存根 — 简化版不启用加密
 */

#include "mesh_crypto.h"
#include "esp_log.h"

static const char *TAG = "crypto";

void mesh_crypto_init(void)
{
    ESP_LOGI(TAG, "V9.0: crypto disabled (simplified)");
}

bool mesh_crypto_encrypt_packet(mesh_packet_t *pkt, uint16_t seq)
{
    (void)pkt;
    (void)seq;
    return true;  /* V9.0: no encryption */
}

bool mesh_crypto_decrypt_packet(mesh_packet_t *pkt)
{
    (void)pkt;
    return true;  /* V9.0: no encryption */
}

void mesh_crypto_set_key(const uint8_t key[MESH_AES_KEY_LEN])
{
    (void)key;
    ESP_LOGI(TAG, "V9.0: set_key ignored (no encryption)");
}
