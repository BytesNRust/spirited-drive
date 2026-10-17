/* The network menu's list of games (00442684, its callback 00443368): the
 * service picked started on the race's session (initnetservice), then the
 * transport's getnetgames (00479174) copied into a block of 0x54-byte
 * entries, sorted by name for 00453ef4. */
#ifndef TNFS_RACE_NET_GAMES_H
#define TNFS_RACE_NET_GAMES_H

#include "game/net/net_request.h"
#include "game/race/race_net_services.h"

#include <stdint.h>

/* A game in the list: the info's name (+0) at +0, its text at +0x20, its
 * four dwords +0x30..0x3f at +0x30, the server's address (0x14 bytes) at
 * +0x40; the block holds 0x14 */
enum { RACE_NET_GAME_SIZE = 0x54, RACE_NET_GAME_TEXT = 0x20, RACE_NET_GAME_INFO = 0x30, RACE_NET_GAME_ADDRESS = 0x40,
       RACE_NET_GAME_ADDRESS_BYTES = 0x14, RACE_NET_GAMES_MOST = 0x14, RACE_NET_GAMES_BYTES = 0x690 };

/* 004bc6c4: what 00442684 returns the address of, packed */
typedef struct {
    int16_t count;  /* 004bc6c4 */
    uint8_t *list;  /* 004bc6c6: the block, made once */
} RaceNetGames;

extern const char RACE_NET_GAME_LIST[];      /* 004afea8 "game", the block's name */
extern const char RACE_NET_PLAY[];           /* 004afe7c "Play", initnetservice's text */
extern const char RACE_NET_VIEW_ONLY[];      /* 004afe70 "ViewOnly" */
extern const char RACE_NET_INIT_FAILED[];    /* 004afe84 */

typedef struct {
    void *ctx;
    void (*kill_service)(void *ctx);  /* 004790b0 (cdecl (0050a260)) */
    /* 00478f0c initnetservice (cdecl (0050a260, 004bc6b0, service, text, 00524df0 the player's name)) */
    int32_t (*init_service)(void *ctx, const uint8_t *service, const char *text);
    void (*message)(void *ctx, const char *text);  /* 00477a40 (cdecl) */
    uint8_t *(*alloc)(void *ctx, const char *name, uint32_t size, uint32_t flags);  /* 00474c8c (cdecl) */
    void (*wait_keys_on)(void *ctx);   /* 00475ac0(004434d8, 4, 0) */
    void (*wait_keys_off)(void *ctx);  /* 00475b74(004434d8) */
    /* 00479174 getnetgames (cdecl (0050a260, callback, timeout)) */
    int32_t (*games)(void *ctx, const NetGamesCallback *cb, int32_t timeout);
    /* 00479ad4 (cdecl (session, a, b)): the same address? */
    int32_t (*same)(void *ctx, NetSession *s, const uint8_t *a, const uint8_t *b);
} RaceNetGamesOps;

typedef struct {
    RaceNetGames games;
    RaceNetServices *services;
    int16_t view;        /* the short 0052b316: only watching */
    int32_t open_view;   /* 004bc6e0: the view the service was started with */
    int16_t key;         /* 0050a364: cleared before asking */
} RaceNetGamesState;

/* getnetgames' callback's ctx */
typedef struct {
    const RaceNetGamesOps *ops;
    RaceNetGames *games;
} RaceNetGamesFound;

/* 00443368 (getnetgames' callback, cdecl (session, info, players,
 * address)): 004bc6c4 (signed) 0x14 or more: 0. An entry whose address
 * 00479ad4(session, address, its +0x40) finds the same: 1. Else at entry
 * 004bc6c4 (the block and the count read again each time): strcpy of the
 * info, strcpy of its +0x20 to +0x20, its +0x30..0x3f to +0x30, the
 * address's 0x14 bytes to +0x40; 004bc6c4 + 1; 1. players unused. */
int32_t race_net_game_found(void *ctx, NetSession *s, const uint8_t *info, int32_t players, uint8_t *address);

/* 00442684 (watcall (timeout), from 00453ef4): a service started
 * (004bc6d2) with another view than 004bc6e0: stopped (004790b0). None
 * started: initnetservice(0050a260, 004bc6b0, the picked service's GUID
 * (004bc6c0 + 004bc6d6 * 0x59 + 0x49), "ViewOnly" with 0052b316 set else
 * "Play", 00524df0); 0: 00477a40("BUG: init network service FAILED"),
 * NULL; else 004bc6d2 1, 004bc6e0 the view. No block: 00474c8c("game",
 * 0x690, 0). 004bc6c4 0, 0050a364 0; getnetgames(0050a260, 00443368,
 * timeout) with the waiting keys' timer on; not 0 and more than one game:
 * a selection sort by name (strcmp, unsigned bytes; the first of the
 * smallest kept), the entries swapped whole. Returns &state->games
 * (004bc6c4). The original copies the smallest name into 12 bytes on its
 * stack, so names of 12 or more characters run over its loop's counters;
 * the port compares in place and keeps only names that fit. */
RaceNetGames *race_net_games_list(const RaceNetGamesOps *ops, RaceNetGamesState *st, int32_t timeout);

#endif
