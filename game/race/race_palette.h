/* The race's screen palette and the colour table that maps a car's or a
 * track bank's own colours onto it (00455a5c builds it; see STATE.md, Race
 * palette). */
#ifndef TNFS_RACE_PALETTE_H
#define TNFS_RACE_PALETTE_H

#include "game/gfx/palette_match.h"

#include <stdint.h>

enum {
    RACE_PALETTE_COLOURS = 256,
    RACE_PALETTE_SLOTS = 16      /* 0051c798: one byte a car slot */
};

typedef struct {
    uint8_t screen[RACE_PALETTE_COLOURS * PALETTE_MATCH_ENTRY_BYTES];  /* 0052f178: B, G, R, 0xff */
    uint8_t screen_rgbx[RACE_PALETTE_COLOURS * 4];                     /* 0052ced8: R, G, B, 0 */
    uint8_t enabled[RACE_PALETTE_COLOURS];                             /* 004c5ff8: entries the search may pick */
    const uint8_t *inverse;                                            /* 004c5ff4: NULL for the search */
    uint8_t table[RACE_PALETTE_COLOURS];                               /* 004c79b4 */
    uint8_t unknown_50ad2e[5];  /* 0050ad2e: 0xfa..0xfd for the police lights, then the slot's own colour */
    uint8_t slot_colours[RACE_PALETTE_SLOTS];  /* 0051c798: a slot's own entry (0x47, 0x5f), kept out of the search */
} RacePalette;

/* 004a2bf0: table[first .. first + count) = the screen entry nearest each
 * of `rgb`'s 8-bit colours (R, G, B), through 00473140's 8-bit path
 * (palette_match_rgb). */
void race_palette_table(RacePalette *p, int first, int count, const uint8_t *rgb);

/* 004470e0: maps a car's shape bank (an SHPI in memory) from its own 6-bit
 * palette onto the screen: the table for colours 0..0xf9 (0048f220), entry
 * 0xfe the slot's own colour when it has one, 0xfa..0xfd themselves for slot
 * 2 when `police` is set, then every shape not named "!..." through it, last
 * shape first. */
void race_palette_remap_bank(RacePalette *p, uint8_t *bank, const uint8_t *palette6, int32_t slot, int32_t police);

#endif
