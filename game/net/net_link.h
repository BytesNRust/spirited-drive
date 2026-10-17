/* The transport's packet driver over a session (EA's library): the table
 * at 004c62c8 (open 00479b40, close 00479be4, +8 00479c74, room 00479c94,
 * +0x10 00479cb0, +0x14 00479cc4, send 00479e40; packets 0x102 bytes at
 * most, 0x74 at least, flags 4) that opennetconnection hands to
 * openpacketconnection. Its methods take a packet handle whose session
 * (+0x24) does the work. */
#ifndef TNFS_NET_LINK_H
#define TNFS_NET_LINK_H

#include "game/net/net_packet.h"
#include "game/net/net_server.h"

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
    int32_t (*driver_ready)(void *ctx, uint32_t handle);  /* the session's driver's +0x4c (eax handle) */
    const uint32_t *room;                                /* 004c62e4 */
    int32_t (*driver_query)(void *ctx, uint32_t handle);  /* the session's driver's +0x50 (eax handle) */
    const NetServeOps *serve;  /* 00478030's; its trace for "->DataRecv" */
    /* the session's driver's +0x58 (eax handle, edx address, ebx &message, ecx data, the size on the stack) */
    int32_t (*driver_receive)(void *ctx, uint32_t handle, uint8_t *address, int32_t *message, uint8_t *data,
                              int32_t size);
    /* the session's driver's +0x54 (eax handle, edx address, ebx message, ecx data, the size on the stack) */
    int32_t (*driver_send)(void *ctx, uint32_t handle, uint8_t *address, int32_t message, uint8_t *data, int32_t size);
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

/* 00479c74 (cdecl (handle), 004c62c8's +8): 1 with +0x3c set and the
 * session's +0xa0 (cleared by killnetservice) set, else 0. */
int32_t net_link_alive(const NetPacketHandle *h);

/* 00479c94 (cdecl (handle), 004c62c8's +0xc room): the session's driver's
 * +0x4c(+0x10) not 0: 004c62e4, else 0. */
uint32_t net_link_room(const NetLinkOps *ops, const NetPacketHandle *h);

/* 00479cb0 (cdecl (handle), 004c62c8's +0x10): the session's driver's
 * +0x50(+0x10) returned. */
int32_t net_link_query(const NetLinkOps *ops, const NetPacketHandle *h);

/* 00479cc4 (cdecl (handle, data, size), 004c62c8's +0x14, the receive): a
 * request of 00477b60's 24 zero bytes with +0xc the session (read once).
 * Until a dat: the word set to dat, the handle's +0x28 to an address (0x14
 * bytes on the stack), the session's driver's +0x58(+0x10, address, &word,
 * data, size) its length; a word not 0: the session's +0x94 0 gets the
 * request, at 004c62b8 >= 5 0047f244("->DataRecv", &word, data, length, 0,
 * 0), 00478030(+0x94, address, word, data, length), +0x94 back to 0 if it
 * still holds the request; a dat: the address to the handle's +0x28, the
 * length returned. The word 0: the session's +0xa4 0: at >= 5
 * 0047f244("NullFlush "), the driver's +0x5c(+0x10, 0, dat, 0); else at >= 6
 * 0047f244("NullRecv  "); 0. */
int32_t net_link_receive(const NetLinkOps *ops, NetPacketHandle *h, uint8_t *data, int32_t size);

/* 00479e40 (cdecl (handle, data, size), 004c62c8's send): at 004c62b8 >= 5
 * 0047f244("DataSend->", &dat, data, size, 0, 0); the session's driver's
 * +0x54(+0x10, handle + 0x28, dat, data, size) not 0: the size, else 0. */
int32_t net_link_send(const NetLinkOps *ops, NetPacketHandle *h, uint8_t *data, int32_t size);

#endif
