/* The network transport's packet layer (EA's library, 00479ed4..0047bfff):
 * openpacketconnection (0047ac18), closepacketconnection (0047b250) and the
 * packets' headers and checksums. */
#ifndef TNFS_NET_PACKET_H
#define TNFS_NET_PACKET_H

#include "game/net/net_queue.h"

#include <stdint.h>

/* The driver's link record (a connection's +8); read again after every call */
typedef struct {
    int32_t now;    /* +8 */
    int32_t acked;  /* +0xc */
} NetLink;

/* A packet connection's words read here; read again after every call */
typedef struct {
    NetLink *link;       /* +8 */
    uint8_t id;          /* +0xc */
    int32_t stamp;       /* +0x40: the link's +8 at the last sequence taken */
    int32_t next_seq;    /* +0x88: the sequence with nothing unacknowledged */
    NetQueue unacked;    /* +0x8c: the packets sent, keyed by sequence */
} NetPacketConn;

typedef struct {
    void *ctx;
    const NetMutexOps *mutex;
    const int32_t *trace_level;  /* 004c62fc: the packet layer's */
    void (*trace)(void *ctx, const char *name, const int32_t *data, uint32_t text, int32_t value, int32_t flagged,
                  int32_t kind);  /* 0047f244 (cdecl): the trace ring */
} NetPacketOps;

extern const char NET_ACK_TRACE[];  /* 004b2b50 */

/* A connection's checksum (its +0xc4, cdecl (data, length)) */
typedef uint32_t (*NetChecksum)(const uint8_t *data, uint32_t length);

/* 00479ed4 (watcall (sum, data, out, length, n), from 00479f1c, 0047a068 and
 * 0047a204): n 0 nothing; else out[0..n) zeroed (0048f158), then sum(data,
 * length) to out[0..n), the last byte its lowest (out may lie in data: the
 * sum then sees it zero). Port guard: n negative writes nothing (the
 * original's 0048f158 stops the game on it). */
void net_checksum_put(NetChecksum sum, const uint8_t *data, uint8_t *out, uint32_t length, int32_t n);

/* 00479f94 (watcall (connection), from 0047a068 and 0047a204): the
 * sequence acknowledged so far, in al (& 0x7f; eax's other bytes not
 * returned): k = the unacknowledged queue's head key (0049caa0 on +0x8c), -1
 * there +0x88, less 1; +0x40 = the link's (+8) +8; with 004c62fc at least 3
 * 0047f244("ACKt", &k, 0, 0, 0, +0xc), k read after it. */
uint8_t net_packet_acked(const NetPacketOps *ops, NetPacketConn *c);

#endif
