/* The network transport's packet layer (EA's library, 00479ed4..0047bfff):
 * openpacketconnection (0047ac18), closepacketconnection (0047b250) and the
 * packets' headers and checksums. */
#ifndef TNFS_NET_PACKET_H
#define TNFS_NET_PACKET_H

#include "game/net/net_queue.h"

#include <stdint.h>

/* A connection's checksum (its +0xc4, cdecl (data, length)) */
typedef uint32_t (*NetChecksum)(const uint8_t *data, uint32_t length);

/* The driver's link record (a connection's +8); read again after every call */
typedef struct {
    int32_t now;    /* +8 */
    int32_t acked;  /* +0xc */
} NetLink;

typedef struct NetPacketHandle NetPacketHandle;

/* A connection's driver's methods (its +4: a table; the link its first argument) */
typedef struct NetDriver NetDriver;
struct NetDriver {
    void *ctx;
    void (*close)(void *ctx, NetPacketHandle *h);                                     /* +4 (cdecl) */
    uint32_t (*room)(void *ctx, NetLink *link);                                      /* +0xc (cdecl) */
    uint32_t (*send)(void *ctx, NetLink *link, const uint8_t *data, uint32_t length);  /* +0x18 (cdecl) */
};

/* A packet: a queue node (+0), its sequence (+4), +8, then its bytes on the
 * wire (+0xc: 0xfe, 0, the kind, the sequence acknowledged, the checksum,
 * the data); 0047ac18 lays them (sum_size + the most + 0x13) & ~3 apart */
typedef struct {
    NetNode node;
    int32_t seq;     /* +4 */
    int32_t sent;    /* +8: 1 while unsent, then the link's +0xc */
    uint8_t wire[];  /* +0xc */
} NetPacket;

/* A packet connection (0047ac18's 200 bytes); its words read again after every call */
typedef struct {
    NetNode node;             /* +0: in the open (0052ce40) or closed (0052ce24) list */
    const NetDriver *driver;  /* +4 */
    NetLink *link;            /* +8 */
    uint8_t id;               /* +0xc: 004c62f8, counted up */
    void *mutex;              /* +0x14 */
    NetPacket *control;       /* +0x1c: the header-only packet */
    int32_t sum_size;         /* +0x2c: the checksum's bytes (0047ac18: 0 or 2) */
    int32_t stamp;            /* +0x40: the link's +8 at the last sequence taken */
    NetQueue spare;           /* +0x4c: the packets free, no key */
    NetQueue queue_68;        /* +0x68: keyed by 0047a800 */
    int32_t next_seq;         /* +0x88: the next receive slot's sequence */
    NetQueue incoming;        /* +0x8c: the receive slots, keyed by sequence (0047a800): the head the one expected */
    NetQueue queue_a8;        /* +0xa8: keyed by 0047a800 */
    NetChecksum sum;          /* +0xc4: 0049d158 or NULL */
} NetPacketConn;

/* A caller's handle on a packet connection (openpacketconnection's first argument) */
struct NetPacketHandle {
    int32_t open;          /* +0: 1000000000 once open, 0 once closing */
    NetPacketConn *conn;   /* +0x20 */
};

typedef struct {
    void *ctx;
    const NetMutexOps *mutex;
    const int32_t *trace_level;  /* 004c62fc: the packet layer's */
    void (*trace)(void *ctx, const char *name, const int32_t *data, uint32_t text, int32_t value, int32_t flagged,
                  int32_t kind);  /* 0047f244 (cdecl): the trace ring */
} NetPacketOps;

extern const char NET_ACK_TRACE[];  /* 004b2b50 */

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

/* 00479f1c (watcall (connection, data, out, length), from 0047a50c, the
 * receive): with +0x2c 0 1; else out[0..n) kept (0048ec60), 00479ed4(+0xc4,
 * data, out, length, +0x2c) and the n bytes compared with the kept ones, the
 * last first: all the same 1; else out put back and 0. Port guard: n above 4
 * (its keep is 4 bytes on the stack) or negative (0048ec60 stops the game) 0. */
int32_t net_checksum_check(const NetPacketConn *c, const uint8_t *data, uint8_t *out, uint32_t length);

/* 0047a204 (watcall (packet, connection, kind), from 0047a2ac, 0047a82c and
 * 0047b250): a control packet (its header alone) sent. length = +0x2c + 4;
 * packet +8 = 1; the driver's room (+4's +0xc (+8)) below length: 0. Else
 * the header 0xfe, 0, kind, 00479f94's; 00479ed4(+0xc4, +0xc, +0x10, length,
 * +0x2c read again); the driver's send (+4's +0x18 (+8, +0xc, length), +4
 * and +8 read again); all sent: 1, and with kind (all 32 bits) at most 0x7f
 * packet +8 = the link's (+8 read again) +0xc; else 0. */
int32_t net_packet_send_control(const NetPacketOps *ops, NetPacket *p, NetPacketConn *c, uint32_t kind);

typedef struct {
    void *ctx;
    void (*service)(void *ctx);  /* 00475c14(0) */
} NetWaitOps;

/* 0047337d (cdecl (ticks), from 00479174, 0047b250 and three more): the
 * clock (0052eebc) plus ticks kept; 00475c14(0) until the clock (read again
 * after each) less that is not negative (32-bit, wrapping); at least once.
 * (00475c14's stores to the stack are its locals': the 0 pushed once
 * stays 0.) */
void net_wait_ticks(const NetWaitOps *ops, const int32_t *clock, int32_t ticks);

typedef struct {
    void *ctx;
    const NetPacketOps *packet;
    const NetWaitOps *wait;
    const int32_t *clock;                                 /* 0052eebc */
    NetQueue *open;                                       /* 0052ce40 */
    NetQueue *closed;                                     /* 0052ce24 */
    void (*message)(void *ctx, const char *text);          /* 0048a080 (cdecl): the debug printf */
    void (*release)(void *ctx, NetPacketConn *c);         /* 00475260 (cdecl) */
} NetCloseOps;

extern const char NET_CLOSE_TRACE[];       /* 004b2e5c */
extern const char NET_CLOSE_NOT_OPEN[];    /* 004b2e6c */
extern const char NET_CLOSE_NOT_CLOSED[];  /* 004b2eb0 */

/* 0047b250 closepacketconnection (cdecl (handle), from 004463ec, 004465e4,
 * 0047ab80 and 0047ac18): no handle or no +0x20: 0. Else the connection's
 * +4 kept; with 004c62fc at least 1 0047f244("ClosePktConn", 0, 0, 0, 0,
 * +0xc). Taken off the open list (0049c9d8 on 0052ce40): with +0 set, +0 =
 * 0, 0047a204(+0x1c, the connection, 0xff) tried up to 5 times with
 * 0047337d(1) between, 0047337d(1), the kept driver's +4 (handle); then
 * 0049c804 onto the closed list. Not there: with +0 set the message
 * 004b2e6c. Then taken off the closed list (0049c9d8 on 0052ce24): the
 * queues +0x4c, +0x68, +0x8c and +0xa8 closed (0049c7f0), the mutex +0x14
 * (00476c58), 00475260 and +0x20 = 0; not there the message 004b2eb0.
 * 00475c14(0); 1. */
int32_t net_close_packet_connection(const NetCloseOps *ops, NetPacketHandle *h);

#endif
