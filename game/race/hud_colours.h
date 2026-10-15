/* The HUD's colours (00446bac, from 00455a5c after the race palette is
 * built): the nearest entries to its fixed colours, the shade table the
 * shadows and skids darken through, the map's car colours. */
#ifndef TNFS_HUD_COLOURS_H
#define TNFS_HUD_COLOURS_H

#include "game/race/race_palette.h"

#include <stdint.h>

enum {
    HUD_COLOURS = 17,           /* 0050b790..0050b7d0 */
    HUD_COLOUR_WHITE = 8,       /* 0050b7b0: (0x3f, 0x3f, 0x3f) */
    HUD_COLOUR_BLACK = 9,       /* 0050b7b4: (0, 0, 0), the strip's */
    HUD_COLOUR_GREY = 12,       /* 0050b7c0: (0x20, 0x20, 0x20) */
    HUD_COLOUR_GREEN_DARK = 3,  /* 0050b79c: (0, 0x20, 0) */
    HUD_MAP_CAR_COLOURS = 8,    /* 0050b7d4..0050b7db */
    HUD_SHADE_SCALE = 0x9999    /* 00482698's: 0.6 */
};

typedef struct {
    int32_t colours[HUD_COLOURS];   /* 0050b790 */
    int32_t unknown_4f9b54;         /* 004f9b54: (0xa, 0xe, 0x12) */
    int32_t frame_colour;           /* 004f9b60: (0x1a, 0x1a, 0x1a) */
    uint8_t shade[256];             /* 0050b674: 00482698's table */
    uint8_t map_cars[HUD_MAP_CAR_COLOURS];  /* 0050b7d4 */
    uint8_t remap[256];             /* 004c6604: 004826f0's copy of the shade */
} HudColours;

/* 00446bac (watcall ()): 00446880 (race_palette_nearest) for 0050b790..
 * 0050b7d0: (0x3f,0,0) (0x20,0,0) (0,0x3f,0) (0,0x20,0) (0,0,0x3f)
 * (0,0,0x20) (0x3f,0x3f,0) (0x20,0x20,0) (0x3f,0x3f,0x3f) (0,0,0) (6,6,6)
 * (0x10,..) (0x20,..) (0x30,..) (0x3f,0x14,0) (0x3f,0,0x3f) (0,0x3f,0x3f);
 * 004f9b54 (0xa,0xe,0x12), 004f9b60 (0x1a,0x1a,0x1a); the palette read
 * (004766b8) into 0050ad34 (p->copy); 00482698(0x9999, it, 0050b674)
 * (palette_shade_table over the enabled entries); 004826f0 copies that to
 * 004c6604; the map's car colours 0xf5 0xf8 0xf0 0xfa, white, 0xf9 0xfc
 * 0xfb. 0048263c fills only the entries 004c6504 marks (all of them in the
 * image; its one writer 004826b4 has no caller), and 004824a0 leaves its
 * byte unset when no enabled entry is within 9999 (none black enough): the
 * port fills all and gives 0 there (palette_shade_table). */
void hud_colours(RacePalette *p, HudColours *c);

#endif
