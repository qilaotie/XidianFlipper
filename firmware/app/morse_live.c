#include "morse_live.h"
#include "morse_codec.h"
#include <string.h>
static uint32_t read32(const uint8_t *p) {
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void write32(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(n >> (i * 8));
}
static char upper(char ch) { return ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch; }
static bool character_valid(char ch) { return ch == ' ' || morse_codec_encode(ch) != NULL; }
void morse_live_flow_init(morse_live_flow_t *f, uint32_t now) {
    memset(f, 0, sizeof(*f));
    f->last_ms = now;
}
void morse_live_flow_push(morse_live_flow_t *f, char ch, uint32_t sequence,
                          morse_live_status_t status) {
    if (f->count == MORSE_LIVE_GLYPHS) {
        memmove(f->glyphs, f->glyphs + 1, (--f->count) * sizeof(*f->glyphs));
    }
    float x = f->count ? f->glyphs[f->count - 1].x + 6.0f : 80.0f;
    if (x < 80)
        x = 80;
    f->glyphs[f->count++] = (morse_live_glyph_t){
        .x = x, .sequence = sequence, .event_id = ++f->next_event, .ch = ch, .status = status};
    if (x > 121)
        f->shift = x - 121;
}
void morse_live_flow_tick(morse_live_flow_t *f, uint32_t now) {
    uint32_t elapsed = now - f->last_ms;
    f->last_ms = now;
    /* Real elapsed time removes off-screen content even while its page is closed. */
    float extra = f->shift * ((float)elapsed / (100.0f + elapsed));
    f->shift -= extra;
    for (unsigned i = 0; i < f->count; i++)
        f->glyphs[i].x -= elapsed * 0.008f + extra;
    unsigned gone = 0;
    while (gone < f->count && f->glyphs[gone].x + 6 <= f->left)
        gone++;
    if (gone) {
        f->count -= gone;
        memmove(f->glyphs, f->glyphs + gone, f->count * sizeof(*f->glyphs));
    }
}
static void status(morse_live_t *s, uint32_t seq, morse_live_status_t value) {
    for (unsigned i = 0; i < s->view.flow.count; i++) {
        morse_live_glyph_t *g = &s->view.flow.glyphs[i];
        if (g->sequence == seq && g->status != MORSE_LIVE_RECEIVED && g->status != MORSE_LIVE_GAP)
            g->status = value;
    }
}
static void failed(morse_live_t *s, uint32_t seq, uint32_t now) {
    status(s, seq, MORSE_LIVE_FAILED);
    s->view.failed++;
    s->view.last_failure_ms = now;
}
void morse_live_init(morse_live_t *s, uint32_t stream, uint32_t now) {
    memset(s, 0, sizeof(*s));
    s->stream = stream ? stream : 1;
    s->view.stream = s->stream;
    morse_live_flow_init(&s->view.flow, now);
}
bool morse_live_push(morse_live_t *s, char ch, uint32_t now, uint32_t *seq) {
    if (!s || !character_valid(ch))
        return false;
    ch = upper(ch);
    if (s->tx_count == MORSE_LIVE_TX_MAX || s->next_sequence >= UINT32_MAX - 1) {
        morse_live_flow_push(&s->view.flow, ch, 0, MORSE_LIVE_FAILED);
        failed(s, 0, now);
        return false;
    }
    uint32_t n = ++s->next_sequence;
    s->tx[s->tx_count++] = (morse_live_tx_t){.sequence = n, .created = now, .ch = ch};
    s->view.pending = (uint8_t)s->tx_count;
    s->view.queued++;
    morse_live_flow_push(&s->view.flow, ch, n, MORSE_LIVE_QUEUED);
    if (seq)
        *seq = n;
    return true;
}
static void remove_first(morse_live_t *s) {
    memmove(s->tx, s->tx + 1, (--s->tx_count) * sizeof(*s->tx));
    s->view.pending = (uint8_t)s->tx_count;
}
static void release_ready(morse_live_t *s, morse_live_peer_t *p, uint32_t now) {
    for (;;) {
        unsigned slot = p->expected % MORSE_LIVE_REORDER;
        if (p->sequence[slot] != p->expected)
            break;
        morse_live_flow_push(&s->view.flow, p->chars[slot], p->expected, MORSE_LIVE_RECEIVED);
        p->sequence[slot] = 0;
        p->expected++;
        p->gap_since = 0;
    }
    for (unsigned i = 0; i < MORSE_LIVE_REORDER; i++)
        if (p->sequence[i]) {
            if (!p->gap_since)
                p->gap_since = now ? now : 1;
            break;
        }
}
void morse_live_tick(morse_live_t *s, uint32_t now) {
    morse_live_flow_tick(&s->view.flow, now);
    while (s->tx_count && !s->in_flight && now - s->tx[0].created >= MORSE_LIVE_QUEUE_MS) {
        if (s->tx[0].successes) {
            s->view.sent++;
            status(s, s->tx[0].sequence, MORSE_LIVE_SENT);
        } else
            failed(s, s->tx[0].sequence, now);
        remove_first(s);
    }
    for (unsigned n = 0; n < MORSE_LIVE_PEERS; n++) {
        morse_live_peer_t *p = &s->peers[n];
        if (!p->stream || !p->gap_since || now - p->gap_since < MORSE_LIVE_REORDER_MS)
            continue;
        uint32_t next = UINT32_MAX;
        for (unsigned i = 0; i < MORSE_LIVE_REORDER; i++)
            if (p->sequence[i] && p->sequence[i] < next)
                next = p->sequence[i];
        if (next != UINT32_MAX && next > p->expected) {
            s->view.gaps += next - p->expected;
            morse_live_flow_push(&s->view.flow, '~', 0, MORSE_LIVE_GAP);
            p->expected = next;
            p->gap_since = 0;
            release_ready(s, p, now);
        }
    }
}
bool morse_live_prepare_packet(morse_live_t *s, uint32_t now, uint8_t *payload, uint8_t *len) {
    if (!s || !payload || !len || s->in_flight || !s->tx_count)
        return false;
    morse_live_tick(s, now);
    if (!s->tx_count || (s->tx[0].due && (int32_t)(now - s->tx[0].due) < 0))
        return false;
    unsigned count = 0;
    uint32_t first = s->tx[0].sequence;
    while (count < s->tx_count && count < MORSE_LIVE_BATCH &&
           s->tx[count].sequence == first + count) {
        morse_live_tx_t *t = &s->tx[count];
        payload[9 + count] = (uint8_t)t->ch;
        t->attempts++;
        status(s, t->sequence, MORSE_LIVE_SENDING);
        count++;
    }
    write32(payload, s->stream);
    write32(payload + 4, first);
    payload[8] = (uint8_t)count;
    *len = (uint8_t)(9 + count);
    s->flight_first = first;
    s->flight_last = first + count - 1;
    s->in_flight = true;
    return true;
}
void morse_live_complete_packet(morse_live_t *s, bool transmitted, uint32_t now) {
    if (!s || !s->in_flight)
        return;
    for (unsigned i = 0; i < s->tx_count; i++) {
        morse_live_tx_t *t = &s->tx[i];
        if (t->sequence < s->flight_first || t->sequence > s->flight_last)
            continue;
        if (transmitted)
            t->successes++;
        t->due = now + 60;
        status(s, t->sequence, transmitted ? MORSE_LIVE_SENT : MORSE_LIVE_QUEUED);
    }
    s->in_flight = false;
    while (s->tx_count && s->tx[0].attempts >= MORSE_LIVE_ATTEMPTS) {
        if (s->tx[0].successes) {
            s->view.sent++;
            status(s, s->tx[0].sequence, MORSE_LIVE_SENT);
        } else
            failed(s, s->tx[0].sequence, now);
        remove_first(s);
    }
}
bool morse_live_receive(morse_live_t *s, const uint8_t mac[6], uint32_t session, const uint8_t *b,
                        size_t len, uint32_t now) {
    if (!s || !mac || !b || len < 10 || len > MORSE_LIVE_PAYLOAD_MAX || !session)
        return false;
    uint32_t stream = read32(b), first = read32(b + 4);
    unsigned count = b[8];
    if (!stream || !first || !count || count > MORSE_LIVE_BATCH || len != 9 + count ||
        first > UINT32_MAX - count)
        return false;
    for (unsigned i = 0; i < count; i++)
        if (!character_valid((char)b[9 + i]))
            return false;
    morse_live_peer_t *p = NULL, *free_peer = NULL;
    for (unsigned i = 0; i < MORSE_LIVE_PEERS; i++) {
        morse_live_peer_t *candidate = &s->peers[i];
        if (candidate->stream == stream && candidate->session == session &&
            !memcmp(candidate->mac, mac, 6)) {
            p = candidate;
            break;
        }
        if (!candidate->stream || now - candidate->last_ms > 30000)
            free_peer = candidate;
    }
    if (!p) {
        if (!free_peer) {
            s->view.rejected++;
            return false;
        }
        p = free_peer;
        memset(p, 0, sizeof(*p));
        memcpy(p->mac, mac, 6);
        p->stream = stream;
        p->session = session;
        /* At stream start allow early packets to arrive out of order; joining later starts here. */
        p->expected = first <= MORSE_LIVE_REORDER ? 1 : first;
    }
    p->last_ms = now;
    for (unsigned i = 0; i < count; i++) {
        uint32_t seq = first + i;
        if (seq < p->expected) {
            s->view.duplicates++;
            continue;
        }
        if (seq - p->expected >= MORSE_LIVE_REORDER) {
            s->view.gaps += seq - p->expected;
            morse_live_flow_push(&s->view.flow, '~', 0, MORSE_LIVE_GAP);
            memset(p->sequence, 0, sizeof(p->sequence));
            p->expected = seq;
            p->gap_since = 0;
        }
        unsigned slot = seq % MORSE_LIVE_REORDER;
        if (p->sequence[slot] == seq) {
            s->view.duplicates++;
            continue;
        }
        p->sequence[slot] = seq;
        p->chars[slot] = upper((char)b[9 + i]);
    }
    release_ready(s, p, now);
    return true;
}
void morse_live_snapshot(const morse_live_t *s, morse_live_snapshot_t *out) {
    if (s && out)
        *out = s->view;
}
