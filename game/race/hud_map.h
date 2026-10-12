/* The dash's track map (00422ce0, drawn on 004c2d9c over 'mpbd') and its
 * pieces. */
#ifndef TNFS_HUD_MAP_H
#define TNFS_HUD_MAP_H

#include "core/formats/tri.h"
#include "game/gfx/quad.h"
#include "game/race/hud_mask.h"

#include <stdint.h>

enum { HUD_MAP_CARS = 9, HUD_MAP_COLOURS = 8, HUD_MAP_TRACKS = 9 };

typedef struct {
    void *ctx;
    QuadTarget *(*select)(void *ctx, HudSurface s);  /* 00473680(004c2d9c): the map's surface, its own clip */
} HudMapOps;

typedef struct {
    const Tri *tri;                 /* 00505038's nodes: x (+8), z (+0x10), heading (+0x18) */
    int32_t node_mask;              /* 00504208 */
    int32_t ring_nodes;             /* 004f9b58: 0x20000 on a stage (0043e4e0) */
    int32_t span;                   /* 004f9b40 (CockpitWorld.unknown_4f9b40): the nodes ahead and behind shown */
    int32_t high;                   /* 004fa0c0 */
    int32_t use_parts;              /* 004f9b3c: 0, nothing drawn */
    int32_t mirrored;               /* 004fbad8: x turned over */
    int32_t race_type;              /* 004fb7d0: 0 one car, 1 two (the police chase's), else 8 */
    int32_t track, variant;         /* 004fb7c2, 004fb7be */
    int32_t chase_kind;             /* *00511fe0: not 0 (or 004f9b7c), 'mpbd' instead of the back shape */
    int32_t dash_camera;            /* 004f9b7c */
    int32_t back_saved;             /* 004f9b64 */
    const uint8_t *back_shape;      /* [004c2d98 + 0x2c] */
    uint8_t *bank;                  /* 005137f0: 'mpbd', 'aro1', 'aru1', 'pos1' */
    int32_t viewed;                 /* 004fbb04: the car the map turns with */
    int16_t car_node[HUD_MAP_CARS]; /* 00509e1c[i] + 0x48 */
    int32_t police_present;         /* 00509e24 (slot 2) not NULL */
    uint8_t police_flags;           /* its +0x51a: bit 4 the police car shown */
    int32_t police_chase;           /* 004fbab8 */
    int32_t place;                  /* 004f9a18 */
    const uint8_t *car_places;      /* 004f9e83, HUD_MAP_CARS */
    const uint8_t *car_colours;     /* 0050b7d4.., HUD_MAP_COLOURS (00446bac) */
    uint32_t frame_colour;          /* 004f9b60 */
    uint32_t road_colour;           /* 0050b7b0 */
    uint32_t arrow_colour;          /* 0050b790 */
    int32_t surface_w, surface_h;   /* 004c78b4 / 004c78b8 after the select */
} HudMapInputs;

/* 00423aec (watcall x, y, colour): a car's marker on the map, the clip set
 * to the map's box for it and to the next box after. High resolution
 * (004fa0c0): a 7 x 5 block at (x - 3, y - 3), a 3-pixel line above and
 * below it, 'pos1' at (x - 4, y - 5) with 0xff skipped; low: a 4 x 4 block
 * at (x - 2, y + 1), 'pos1' at (x - 3, y). surface_w / surface_h are
 * 004c78b4 / 004c78b8. Nothing is drawn past a missing 'pos1' (port guard:
 * the original aborts). */
void hud_map_marker(QuadTarget *t, uint8_t *bank, int32_t high, int32_t x, int32_t y, uint32_t colour,
                    int32_t surface_w, int32_t surface_h);

/* 00422ce0 (from 0044cc74), when 004f9b3c is set: on 004c2d9c the back
 * shape (004737d0 at (2, 2) high / (1, 1) low, while 004f9b64) or, with a
 * chase camera or 004f9b7c, 'mpbd' (0xff skipped); the frame (004827f0 in
 * 004f9b60, twice high); the clip to the box; then, turned with the viewed
 * car's node heading (>> 4, a 10-bit angle) about its node (x, z >> 0x13
 * high / 0x14 low, x mirrored by 004fbad8): the road as a line through the
 * nodes 004b9178[track]..004b919c[track] (step 16, 13.5 on track 3) about
 * it (0050b7b0), the direction at the start node (004b9574 or 0; 0050b790);
 * each car (count by 004fb7d0) within 004f9b40 nodes and inside the box as
 * its marker (00423aec), else as an arrow up ('aro1') or down ('aru1') at
 * its column; the police car's marker blinking (004f9b5c) while 004fbab8,
 * 004fb7d0 == 1 and its +0x51a bit 4. *blink is 004f9b5c. Returns -1 where
 * the original aborts (a shape missing from the bank), else 0. Nodes past
 * the TRI's own are read as 0 (port guard: the original reads on). */
int hud_map_draw(const HudMapOps *ops, const HudMapInputs *in, int32_t *blink);

#endif
