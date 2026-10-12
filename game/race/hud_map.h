/* The dash's track map (00422ce0, drawn on 004c2d9c over 'mpbd') and its
 * pieces. */
#ifndef TNFS_HUD_MAP_H
#define TNFS_HUD_MAP_H

#include "game/gfx/quad.h"

#include <stdint.h>

/* 00423aec (watcall x, y, colour): a car's marker on the map, the clip set
 * to the map's box for it and to the next box after. High resolution
 * (004fa0c0): a 7 x 5 block at (x - 3, y - 3), a 3-pixel line above and
 * below it, 'pos1' at (x - 4, y - 5) with 0xff skipped; low: a 4 x 4 block
 * at (x - 2, y + 1), 'pos1' at (x - 3, y). surface_w / surface_h are
 * 004c78b4 / 004c78b8. Nothing is drawn past a missing 'pos1' (port guard:
 * the original aborts). */
void hud_map_marker(QuadTarget *t, uint8_t *bank, int32_t high, int32_t x, int32_t y, uint32_t colour,
                    int32_t surface_w, int32_t surface_h);

#endif
