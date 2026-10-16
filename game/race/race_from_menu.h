/* The frontend's settings into the race (0042702c, from 004275f8): the
 * setup's words from config.dat's (00524e5d..00524ea4), the network's or
 * the single player's counts, the cars by race type, the memory's cuts,
 * and the two player records 004273d0 sends. */
#ifndef TNFS_RACE_FROM_MENU_H
#define TNFS_RACE_FROM_MENU_H

#include <stdint.h>

enum {
    RACE_FROM_MENU_FRONTEND = 0xb5,   /* 00524df0..00524ea4: the player's name, then config.dat's words from 00524e5d */
    RACE_FROM_MENU_AT = 0x524df0      /* the address RaceFromMenu.frontend stands for */
};

/* Read again after every call */
typedef struct {
    uint8_t *setup;                   /* 004fb7bc: RACE_SESSION_SETUP bytes */
    uint8_t *frontend;                /* 00524df0: RACE_FROM_MENU_FRONTEND bytes */
    int16_t play;                     /* 0052b312 */
    int32_t unknown_52b306;           /* to 004fb7d8 */
    int32_t unknown_52b30a;
    int16_t sound;                    /* 0052b30e: 0 without the sound */
    int16_t net_humans;               /* 004f81d2 */
    int16_t net_player;               /* 004f81d6 */
    int32_t memory_4baec8;            /* the memory sizes, compared */
    int32_t memory_4baecc;
    int32_t viewed;                   /* 004fbb04 */
    int32_t followed;                 /* 004fbb08 */
    int32_t unknown_4fbb0c;
    int32_t unknown_4b95ac;
    int32_t unknown_4bafe8;
    int32_t unknown_4bafec;
    uint8_t unknown_4ae1d4;
    uint8_t unknown_4fa040;
} RaceFromMenu;

/* 0042702c (eax first, edx second: RACE_SESSION_RECORD bytes each). The
 * setup: +2 00524e83, +6 00524e85, +0x14 00524e5d, +0x34 00524e87, +0x308
 * 00524ea1, +0x300 1, +0x304 4, +0x318 00524ea3, +0x1c 0052b306. Played
 * (0052b312): 004fbb0c 00524e7b, followed and viewed 004f81d6, +0x18 1,
 * +0x24 004f81d2; else 1, 0 and 0, 0, 1. By the type 00524e5d: 2 or 3 8,
 * 0, 0 at +0x20 / +0x28 / +0x30; 0 1, 0, 0; else 2 and, with +0x1c bit 2
 * clear and +6 at most 2, (0052b312 == 0) and 6, else 0, 0. The memory:
 * 004baec8 below 0x57800 clears 00524e6d; 004baecc below 0x47b760 with
 * 00524e6f 0 clears 00524e73 and 00524e6d and sets 00524e6f, 00524e61,
 * 00524e77 and 004b95ac; else below 0x4b3a90 (00524e6f 0) sets 00524e6f
 * and clears 00524e73; else with 00524e61 0, below 0x50df20 and 00524e6f 0
 * sets 00524e61. The first record: +0x2f / +0x1f / +0x23 / +0x2b 00524e6d
 * / 00524e69 / 00524e65 / 0052b30e (0 without the sound), +0x1b 0052b30a,
 * +0x27 00524e61, the name at +2 (over them), +0xb 00426fec(00524e8b), +0xf
 * 00524e7d, +0x13 and 004bafec (00524e7f == 0), +0x17 and 004bafe8
 * (00524e89 == 0). The second its copy with +0x3f, +0x43 0 and +0xb
 * 00426fec(00524e81). 004fa040 = 004ae1d4; +0x304 = (+0x1c bit 2 clear);
 * +0x314 00524e9d. Port guard: the name within the record. */
void race_from_menu(RaceFromMenu *m, uint8_t *first, uint8_t *second);

#endif
