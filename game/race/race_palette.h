/* The race's screen palette and the colour table that maps a car's or a
 * track bank's own colours onto it (00455a5c builds it; see STATE.md, Race
 * palette). */
#ifndef TNFS_RACE_PALETTE_H
#define TNFS_RACE_PALETTE_H

#include "game/gfx/palette_match.h"

#include <stdint.h>

enum {
    RACE_PALETTE_COLOURS = 256
};

typedef struct {
    uint8_t screen[RACE_PALETTE_COLOURS * PALETTE_MATCH_ENTRY_BYTES];  /* 0052f178: B, G, R, 0xff */
    uint8_t screen_rgbx[RACE_PALETTE_COLOURS * 4];                     /* 0052ced8: R, G, B, 0 */
    uint8_t enabled[RACE_PALETTE_COLOURS];                             /* 004c5ff8: entries the search may pick */
    const uint8_t *inverse;                                            /* 004c5ff4: NULL for the search */
    uint8_t table[RACE_PALETTE_COLOURS];                               /* 004c79b4 */
} RacePalette;

/* 004a2bf0: table[first .. first + count) = the screen entry nearest each
 * of `rgb`'s 8-bit colours (R, G, B), through 00473140's 8-bit path
 * (palette_match_rgb). */
void race_palette_table(RacePalette *p, int first, int count, const uint8_t *rgb);

#endif
