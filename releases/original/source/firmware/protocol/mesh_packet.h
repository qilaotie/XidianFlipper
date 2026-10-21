/** BLE local messaging v3. Wire integers are little endian; CRC is not authentication. */
#ifndef MESH_PACKET_H
#define MESH_PACKET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MESH_MAGIC 0x4D45
#define MESH_VERSION 3
#define MESH_PAYLOAD_MAX 128
#define MORSE_PAYLOAD_MAX_LEN MESH_PAYLOAD_MAX
#define MESH_HEADER_SIZE 27
#define MESH_CRC_SIZE 2
#define MESH_MIC_SIZE 4
#define MESH_PACKET_MAX_LEN (MESH_HEADER_SIZE + MESH_PAYLOAD_MAX + MESH_CRC_SIZE)
#define MESH_PACKET_WIRE_SIZE(n) (MESH_HEADER_SIZE + (n) + MESH_CRC_SIZE)
#define MESH_DST_BROADCAST_MAC {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
#define MESH_CHANNEL_MIN 1
#define MESH_CHANNEL_MAX 16
#define MESH_CHANNEL_DEFAULT 1
#define MESH_FLAG_ACK_REQUEST 0x01
typedef enum {
    MESH_TYPE_QUICK_MSG = 0,
    MESH_TYPE_MORSE = 1,
    MESH_TYPE_FRIEND_REQ = 2,
    MESH_TYPE_FRIEND_ACK = 3,
    MESH_TYPE_BEACON = 4,
    MESH_TYPE_EMERGENCY = 5,
    MESH_TYPE_RECEIPT = 6,
    MESH_TYPE_MORSE_LIVE = 7,
    MESH_TYPE_MAX
} mesh_msg_type_t;
typedef struct {
    uint16_t magic;
    uint8_t version;
    uint8_t src_mac[6], dst_mac[6];
    uint8_t type, len, flags, channel;
    uint32_t session_id, msg_id;
    uint8_t payload[MESH_PAYLOAD_MAX];
    uint16_t crc16;
} mesh_packet_t;
/* Retained declarations; v3 QUICK_MSG sends actual UTF-8 text. */
#define QUICK_MSG_PAYLOAD_SIZE 2
typedef struct {
    uint8_t msg_id, reserved;
} quick_msg_payload_t;
#define FRIEND_PAYLOAD_SIZE 26
typedef struct {
    uint8_t requester_mac[6], responder_mac[6];
    char device_name[14];
} friend_payload_t;
bool mesh_packet_pack(mesh_packet_t *pkt, const uint8_t *src_mac, const uint8_t *dst_mac,
                      uint8_t type, const uint8_t *payload, uint8_t len);
bool mesh_packet_unpack(mesh_packet_t *pkt, const uint8_t *data, size_t len);
size_t mesh_packet_to_bytes(const mesh_packet_t *pkt, uint8_t *buf, size_t buf_len);
uint16_t mesh_packet_compute_crc(const mesh_packet_t *pkt);
bool mesh_packet_verify_crc(const mesh_packet_t *pkt);
bool mesh_packet_is_valid_magic(const uint8_t *data, size_t len);
bool mesh_packet_is_broadcast(const uint8_t *mac);
bool mesh_mac_equal(const uint8_t *a, const uint8_t *b);
bool mesh_mac_is_valid_peer(const uint8_t *mac);
#ifdef __cplusplus
}
#endif
#endif
