/* The network host / join screen (00453ef4) and the players it lists (00453e40). */
#ifndef TNFS_NET_HOST_H
#define TNFS_NET_HOST_H

#include "game/race/race_net_players.h"

#include <stdint.h>

typedef struct NetPlayersDrawOps {
    RaceNetPlayers *(*players)(void *ctx, const uint8_t *address);          /* 004428fc: eax */
    void (*text_select)(void *ctx, int32_t style);                           /* 004638c0: eax */
    void (*text_boxed)(void *ctx, const char *text, int32_t x, int32_t y);  /* 00463934: eax, edx, ebx */
    void *ctx;
} NetPlayersDrawOps;

/* 00453e40 (watcall (_, address), from 00453ef4): 004428fc(address)'s list;
 * style 2: "Racers:" (or "Rennfahrer" with 0052b31c (german) set) at (0x3a,
 * 0xf8); style 0: each player's name at (0x3a, 0x113 + 0x12 i), the count
 * (signed) and the block read again each time; none with no list. */
void net_players_draw(int16_t german, const uint8_t *address, const NetPlayersDrawOps *ops);

#endif
