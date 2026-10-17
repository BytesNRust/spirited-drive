/* The transport's packet driver over a session (EA's library): the table
 * at 004c62c8 (open 00479b40, close 00479be4, +8 00479c74, room 00479c94,
 * +0x10 00479cb0, +0x14 00479cc4, send 00479e40; packets 0x102 bytes at
 * most, 0x74 at least, flags 4) that opennetconnection hands to
 * openpacketconnection. Its methods take a packet handle whose session
 * (+0x24) does the work. */
#ifndef TNFS_NET_LINK_H
#define TNFS_NET_LINK_H

#include "game/net/net_packet.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    const NetMutexOps *mutex;     /* 00476c78 / 00476cfc */
    void *const *lock;            /* 004c62c4: the sessions' mutex (read again for each use) */
    const int32_t *trace_level;   /* 004c62b8 */
    void (*trace)(void *ctx, const char *name);  /* 0047f244(name, 0, 0, 0, 0, 0) */
    /* the session's driver's +0x48 (eax handle, edx address); NULL where the table holds 0 */
    void (*driver_prepare)(void *ctx, uint32_t handle, uint8_t *address);
    /* the session's driver's +0x5c (eax handle, edx address, ebx message, ecx 0 to open, 1 to close) */
    void (*driver_link)(void *ctx, uint32_t handle, uint8_t *address, int32_t message, int32_t closing);
} NetLinkOps;

/* 00479b40 (cdecl (handle), 004c62c8's +0): the session (+0x24, its words
 * read again) with no driver handle (+0x10): +0x3c = 0. Else the driver's
 * +0x48 (when set) (+0x10, handle + 0x28), its +0x5c(+0x10, handle + 0x28,
 * dat, 0), at 004c62b8 >= 3 0047f244("Flushed1"), under 004c62c4's mutex the
 * session's +0xa4 + 1, +0x3c = 1. +0x3c returned. */
int32_t net_link_open(const NetLinkOps *ops, NetPacketHandle *h);

/* 00479be4 (cdecl (handle), 004c62c8's +4): the session's driver handle
 * (+0x10) and its open links (+0xa4) both set: under 004c62c4's mutex +0xa4
 * - 1, the driver's +0x5c(+0x10, handle + 0x28, dat, 1), at 004c62b8 >= 3
 * 0047f244("Flushed2"). +0x3c = 0; 1. */
int32_t net_link_close(const NetLinkOps *ops, NetPacketHandle *h);

#endif
