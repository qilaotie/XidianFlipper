/* Ephemeral ordered Morse characters. Pure bounded state; no UI, RTOS or storage calls. */
#ifndef MORSE_LIVE_H
#define MORSE_LIVE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MORSE_LIVE_BATCH 8
#define MORSE_LIVE_TX_MAX 32
#define MORSE_LIVE_GLYPHS 32
#define MORSE_LIVE_FLOW_LEFT 0
#define MORSE_LIVE_PEERS 4
#define MORSE_LIVE_REORDER 16
#define MORSE_LIVE_PAYLOAD_MAX (9 + MORSE_LIVE_BATCH)
#define MORSE_LIVE_ATTEMPTS 3
#define MORSE_LIVE_QUEUE_MS 5000
#define MORSE_LIVE_REORDER_MS 250
typedef enum {
    MORSE_LIVE_LOCAL,
    MORSE_LIVE_QUEUED,
    MORSE_LIVE_SENDING,
    MORSE_LIVE_SENT,
    MORSE_LIVE_FAILED,
    MORSE_LIVE_RECEIVED,
    MORSE_LIVE_GAP
} morse_live_status_t;
typedef struct {
    float x;
    uint32_t sequence, event_id;
    char ch;
    uint8_t status;
} morse_live_glyph_t;
typedef struct {
    morse_live_glyph_t glyphs[MORSE_LIVE_GLYPHS];
    unsigned count;
    uint32_t last_ms, next_event;
    float shift;
    uint8_t left;
} morse_live_flow_t;
typedef struct {
    morse_live_flow_t flow;
    uint32_t stream;
    uint32_t queued, sent, failed, duplicates, gaps, rejected;
    uint32_t last_failure_ms;
    uint8_t pending;
} morse_live_snapshot_t;
typedef struct {
    uint32_t sequence, created, due;
    char ch;
    uint8_t attempts, successes;
} morse_live_tx_t;
typedef struct {
    uint8_t mac[6];
    uint32_t session, stream, expected, last_ms, gap_since;
    uint32_t sequence[MORSE_LIVE_REORDER];
    char chars[MORSE_LIVE_REORDER];
} morse_live_peer_t;
typedef struct {
    morse_live_snapshot_t view;
    morse_live_tx_t tx[MORSE_LIVE_TX_MAX];
    unsigned tx_count;
    morse_live_peer_t peers[MORSE_LIVE_PEERS];
    uint32_t stream, next_sequence, flight_first, flight_last;
    bool in_flight;
} morse_live_t;
void morse_live_init(morse_live_t *live, uint32_t stream, uint32_t now_ms);
void morse_live_tick(morse_live_t *live, uint32_t now_ms);
bool morse_live_push(morse_live_t *live, char character, uint32_t now_ms, uint32_t *sequence);
bool morse_live_prepare_packet(morse_live_t *live, uint32_t now_ms, uint8_t *payload,
                               uint8_t *length);
void morse_live_complete_packet(morse_live_t *live, bool transmitted, uint32_t now_ms);
bool morse_live_receive(morse_live_t *live, const uint8_t mac[6], uint32_t session,
                        const uint8_t *payload, size_t length, uint32_t now_ms);
void morse_live_snapshot(const morse_live_t *live, morse_live_snapshot_t *snapshot);
void morse_live_flow_init(morse_live_flow_t *flow, uint32_t now_ms);
void morse_live_flow_push(morse_live_flow_t *flow, char ch, uint32_t sequence,
                          morse_live_status_t status);
void morse_live_flow_tick(morse_live_flow_t *flow, uint32_t now_ms);
#endif
