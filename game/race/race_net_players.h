/* The network menu's list of players (004428fc, its callback 00443164):
 * the transport's getnetplayers (00479770) on the race's session copied into
 * a block of 0x60-byte entries that 00453e40 draws. */
#ifndef TNFS_RACE_NET_PLAYERS_H
#define TNFS_RACE_NET_PLAYERS_H

#include "game/net/net_request.h"

#include <stdint.h>

/* A player in the list: the record's name (+0x48 there) at +0, its text at
 * +0x14 (the record's +0x14), the callback's third argument at +0x48, the
 * record's first 0x14 bytes at +0x4c; the block holds eight */
enum { RACE_NET_PLAYER_SIZE = 0x60, RACE_NET_PLAYER_TEXT = 0x14, RACE_NET_PLAYER_SELF = 0x48,
       RACE_NET_PLAYER_ADDRESS = 0x4c, RACE_NET_PLAYERS_BYTES = 0x300 };

/* 004bc6cc: what 004428fc returns the address of, packed */
typedef struct {
    int16_t count;  /* 004bc6cc */
    uint8_t *list;  /* 004bc6ce: the block, made once */
} RaceNetPlayers;

extern const char RACE_NET_PLAYER_LIST[];  /* 004afeb0 "player", the block's name */

typedef struct {
    void *ctx;
    uint8_t *(*alloc)(void *ctx, const char *name, uint32_t size, uint32_t flags);  /* 00474c8c (cdecl) */
    /* 00475ac0(004434d8, 4, 0) / 00475b74(004434d8): the timer that keeps
     * the key pressed while waiting in 0050a364 */
    void (*wait_keys_on)(void *ctx);
    void (*wait_keys_off)(void *ctx);
    /* 00479770 getnetplayers (cdecl (0050a260, address, callback)) */
    int32_t (*players)(void *ctx, const uint8_t *address, const NetPlayerCallback *cb);
} RaceNetPlayersOps;

/* 00443164 (getnetplayers' callback, cdecl (session, record, self); ctx the
 * RaceNetPlayers): the record's +0x14 'V': 1, nothing kept. Else at entry
 * 004bc6cc (signed; the block and the count read again each time): strcpy
 * of the record's +0x48, strcpy of its +0x14 to +0x14, its first 0x14 bytes
 * to +0x4c, self to +0x48; 004bc6cc + 1; 1. */
int32_t race_net_player_found(void *ctx, NetSession *s, const uint8_t *record, int32_t self);

/* 004428fc (watcall (address), from 00453e40): 004bc6cc 0; no block:
 * 00474c8c("player", 0x300, 0); 0050a364 (*key) 0; getnetplayers(0050a260,
 * address, 00443164) with the waiting keys' timer on. Returns p (004bc6cc). */
RaceNetPlayers *race_net_players_get(const RaceNetPlayersOps *ops, RaceNetPlayers *p, int16_t *key,
                                     const uint8_t *address);

#endif
