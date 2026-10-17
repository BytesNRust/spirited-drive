/* The network menu's New: a game hosted under the player's name (00442970). */
#ifndef TNFS_RACE_NET_HOST_H
#define TNFS_RACE_NET_HOST_H

#include <stdint.h>

/* 0050a320: newnetgame's params, the name at +0, 12 at +0x40 (the most
 * players), +0x48..+0x53 cleared; +0x44 is 0050a364, left alone */
enum { RACE_NET_PARAMS_MOST = 0x40, RACE_NET_PARAMS_TAIL = 0x48, RACE_NET_PARAMS_BYTES = 0x54, RACE_NET_PLAYERS_MOST = 12 };

extern const char RACE_NET_CREATED[];          /* 004aff00 "Game Created" */
extern const char RACE_NET_CREATED_DE[];       /* 004afef0 "Spiel erstellt" */
extern const char RACE_NET_CREATE_FAILED[];    /* 004afedc "Create Game Failed" */
extern const char RACE_NET_CREATE_FAILED_DE[]; /* 004afeb8 "Spiel konnte nicht erstellt werden" */

typedef struct {
    void *ctx;
    void (*prepare)(void *ctx);  /* 0044655c */
    /* 00479338 newnetgame (cdecl (0050a260, params, password, 00442ec0)) */
    int32_t (*new_game)(void *ctx, uint8_t *params, const char *password);
    void (*banner_wait)(void *ctx, const char *text, int32_t time);  /* 00467214: eax, edx */
    void (*close)(void *ctx);       /* 0047ab80 */
    void (*free_lists)(void *ctx);  /* 00442dec: the games' and players' blocks freed */
} RaceNetHostOps;

typedef struct {
    int16_t w524e7b;     /* config.dat's 00524e7b */
    int16_t link_lost;   /* 0052b314 */
    int16_t w4f81d2;
    int16_t w4f81d4;
    int16_t w4f81d6;
    int16_t w4bc6d8;
    int16_t w4bc6da;
    int32_t w4fbb0c;
    int32_t w50a5a8;
    char *names;         /* 004f8166: the link game's players' names, the first ours */
    uint8_t *params;     /* 0050a320 */
    int16_t hosting;     /* 004bc6ac */
    int16_t link_game;   /* 0052b312 */
    int16_t german;      /* 0052b31c */
} RaceNetHostState;

/* 00442970 (watcall (name), from 00453ef4): 00524e7b 1, 0052b314 0, 004f81d6
 * 0, 004f81d2 1, 004bc6d8 1, 004bc6da 0, 004fbb0c 1, 0050a5a8 0, 004f81d4 0,
 * the params' +0x48..+0x53 0; name copied to 004f8166 and to the params,
 * +0x40 12; 0044655c; newnetgame(0050a260, params, "" (the byte at
 * 00442510), 00442ec0). 0: "Create Game Failed" (German: "Spiel konnte
 * nicht erstellt werden") for 300 ticks (00467214), 0047ab80, 0. Else
 * 004bc6ac 1, 0052b312 1, 00442dec, "Game Created" ("Spiel erstellt") for
 * 200, 1. */
int32_t race_net_host(const RaceNetHostOps *ops, RaceNetHostState *st, const char *name);

#endif
