/* The network transport's game server side (EA's library): 00478030
 * answers the requests that reach the server and the pieces it uses. */
#ifndef TNFS_NET_SERVER_H
#define TNFS_NET_SERVER_H

#include "game/net/net_session.h"

#include <stdint.h>

/* 00477c6c (watcall (bits), from 00478030): how many bits are set (the
 * lowest cleared, x & ~-x, until none). */
int32_t net_bit_count(uint32_t bits);

enum { NET_GAME_INFO = (int32_t)0xea646563, NET_GAME_INFO_SIZE = 0x4c };

typedef struct {
    void *ctx;
    const int32_t *trace_level;  /* 004c62b8 */
    void (*trace)(void *ctx, const char *name, const int32_t *message, uint8_t *out, int32_t size);  /* 0047f244 */
    /* the driver's +0x54 (eax handle, edx address, ebx message, ecx out, the size on the stack); its eax unused */
    int32_t (*driver_send)(void *ctx, uint32_t handle, uint8_t *address, int32_t message, uint8_t *out, int32_t size);
} NetGameInfoOps;

/* 00477c88 (watcall (server, address, id), from 00478030): an id not 0 or
 * the server's +8 0: the game's description (0x4c bytes) to the address
 * through the server's session (+4, read once): +0 the id, +4 the players'
 * bits (+0x4c), +8 00482890 (strncpy) of +0x50 (0x20), +0x28 of +0x70
 * (0x10), +0x38..+0x48 +0x80..+0x90 (0049c6a8, big-endian words); 004c62b8
 * >= 3: 0047f244("GameInfo->" 004b279c, &0xea646563, out, 0x4c, 0, 0); the
 * driver's +0x54(+0x10, address, 0xea646563, out, 0x4c). Its message word
 * sits in the saved ebp's slot, so the original returns with ebp
 * 0xea646563 (the caller's ebp, not this function's output). */
void net_game_info(const NetGameInfoOps *ops, NetServer *v, uint8_t *address, uint32_t id);

#endif
