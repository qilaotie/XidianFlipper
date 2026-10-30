#include "mesh_packet.h"
#include <string.h>
typedef char packet_must_fit_one_ext_adv_command[(MESH_PACKET_MAX_LEN + 7 <= 251) ? 1 : -1];
static void put32(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(n >> (8 * i));
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t crc(const uint8_t *p, size_t n) {
    uint16_t c = 0xffff;
    while (n--) {
        c ^= (uint16_t)*p++ << 8;
        for (unsigned i = 0; i < 8; i++)
            c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
}
static bool valid(const mesh_packet_t *p) {
    return p && p->magic == MESH_MAGIC && p->version == MESH_VERSION && p->type < MESH_TYPE_MAX &&
           p->len <= MESH_PAYLOAD_MAX && !(p->flags & ~(MESH_FLAG_ACK_REQUEST|MESH_FLAG_ROUTED|MESH_HOP_MASK|MESH_FLAG_ANNOUNCEMENT|MESH_FLAG_CUSTODY)) &&
           (!(p->flags&MESH_FLAG_CUSTODY) || (p->type==MESH_TYPE_QUICK_MSG && !p->channel && !mesh_packet_is_broadcast(p->dst_mac) && (p->flags&(MESH_FLAG_ROUTED|MESH_FLAG_ACK_REQUEST))==(MESH_FLAG_ROUTED|MESH_FLAG_ACK_REQUEST))) &&
           (!(p->flags&MESH_FLAG_ANNOUNCEMENT) || ((p->flags&MESH_FLAG_ROUTED) && !(p->flags&MESH_FLAG_ACK_REQUEST) &&
             p->type==MESH_TYPE_QUICK_MSG && p->channel==0 && mesh_packet_is_broadcast(p->dst_mac))) &&
           (!(p->flags&MESH_HOP_MASK) || (p->flags&MESH_FLAG_ROUTED)) &&
           (!(p->flags&MESH_FLAG_ROUTED) || ((p->flags&MESH_HOP_MASK)>>MESH_HOP_SHIFT)<MESH_TOTAL_HOPS_MAX) &&
           (!(p->flags&MESH_FLAG_ROUTED) || p->type==MESH_TYPE_QUICK_MSG || p->type==MESH_TYPE_MORSE || p->type==MESH_TYPE_RECEIPT || p->type==MESH_TYPE_COLLAB || p->type==MESH_TYPE_SOCIAL || p->type==MESH_TYPE_CUSTODY_CANCEL) &&
           p->channel <= MESH_CHANNEL_MAX;
}
static size_t encode(const mesh_packet_t *p, uint8_t *b) {
    b[0] = (uint8_t)p->magic;
    b[1] = (uint8_t)(p->magic >> 8);
    b[2] = p->version;
    memcpy(b + 3, p->src_mac, 6);
    memcpy(b + 9, p->dst_mac, 6);
    b[15] = p->type;
    b[16] = p->len;
    b[17] = p->flags;
    b[18] = p->channel;
    put32(b + 19, p->session_id);
    put32(b + 23, p->msg_id);
    memcpy(b + MESH_HEADER_SIZE, p->payload, p->len);
    return MESH_HEADER_SIZE + p->len;
}
bool mesh_packet_pack(mesh_packet_t *p, const uint8_t *s, const uint8_t *d, uint8_t t,
                      const uint8_t *v, uint8_t n) {
    if (!p || !s || !d || t >= MESH_TYPE_MAX || n > MESH_PAYLOAD_MAX || (!v && n))
        return false;
    memset(p, 0, sizeof(*p));
    p->magic = MESH_MAGIC;
    p->version = MESH_VERSION;
    memcpy(p->src_mac, s, 6);
    memcpy(p->dst_mac, d, 6);
    p->type = t;
    p->len = n;
    if (n)
        memcpy(p->payload, v, n);
    p->crc16 = mesh_packet_compute_crc(p);
    return true;
}
uint16_t mesh_packet_compute_crc(const mesh_packet_t *p) {
    if (!valid(p))
        return 0;
    uint8_t b[MESH_PACKET_MAX_LEN];
    size_t n = encode(p, b);
    return crc(b, n);
}
bool mesh_packet_verify_crc(const mesh_packet_t *p) {
    return valid(p) && p->crc16 == mesh_packet_compute_crc(p);
}
bool mesh_packet_is_valid_magic(const uint8_t *p, size_t n) {
    return p && n >= MESH_HEADER_SIZE && p[0] == (MESH_MAGIC & 255) && p[1] == (MESH_MAGIC >> 8) &&
           p[2] == MESH_VERSION;
}
size_t mesh_packet_to_bytes(const mesh_packet_t *p, uint8_t *b, size_t cap) {
    if (!valid(p) || !b || cap < MESH_PACKET_WIRE_SIZE(p->len))
        return 0;
    size_t n = encode(p, b);
    uint16_t c = crc(b, n);
    b[n] = (uint8_t)c;
    b[n + 1] = (uint8_t)(c >> 8);
    return n + 2;
}
bool mesh_packet_unpack(mesh_packet_t *p, const uint8_t *b, size_t n) {
    if (!p || !mesh_packet_is_valid_magic(b, n) || n < MESH_HEADER_SIZE + 2)
        return false;
    uint8_t len = b[16];
    if (len > MESH_PAYLOAD_MAX || n != MESH_PACKET_WIRE_SIZE(len))
        return false;
    uint16_t c = (uint16_t)b[n - 2] | ((uint16_t)b[n - 1] << 8);
    if (crc(b, n - 2) != c)
        return false;
    memset(p, 0, sizeof(*p));
    p->magic = MESH_MAGIC;
    p->version = b[2];
    memcpy(p->src_mac, b + 3, 6);
    memcpy(p->dst_mac, b + 9, 6);
    p->type = b[15];
    p->len = len;
    p->flags = b[17];
    p->channel = b[18];
    p->session_id = get32(b + 19);
    p->msg_id = get32(b + 23);
    memcpy(p->payload, b + MESH_HEADER_SIZE, len);
    p->crc16 = c;
    return valid(p);
}
bool mesh_packet_is_broadcast(const uint8_t *p) {
    if (!p)
        return false;
    for (unsigned i = 0; i < 6; i++)
        if (p[i] != 255)
            return false;
    return true;
}
bool mesh_mac_equal(const uint8_t *a, const uint8_t *b) {
    return a && b && !memcmp(a, b, 6);
}
bool mesh_mac_is_valid_peer(const uint8_t *p) {
    static const uint8_t z[6] = {0};
    return p && !mesh_packet_is_broadcast(p) && memcmp(p, z, 6) != 0;
}
