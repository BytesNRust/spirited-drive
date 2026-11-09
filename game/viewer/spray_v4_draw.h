/* port (spray v4, dev prototype): the particles of race/spray_v4.h drawn without piling up. A group (the particles of one road row, the farther or the nearer than their car) is rasterised into a
 * small alpha map as the MAXIMUM of its puffs, set against the frame's map (what the groups drawn before it already put on each pixel: only the excess is applied), and drawn as ONE recorded
 * sprite through tables that mix the pixel toward the scene's colour by at most a half (dithered). However many puffs overlap, a pixel never gets more than the cap: nothing burns to white. The
 * colour is the scene's fog's (so night is dark and the distance fades into the haze), never fixed; a puff also fades with the distance from the eye. */
#ifndef TNFS_SPRAY_V4_DRAW_H
#define TNFS_SPRAY_V4_DRAW_H

#include "game/gfx/quad.h"
#include "game/race/model_draw.h"
#include "game/race/race_state.h"
#include "game/race/spray_v4.h"

typedef struct SprayV4Host {
    const QuadTarget *target;
    const uint8_t *palette;      /* 768 bytes */
    const ModelView *view;
    const RaceState *race;
    int (*world_screen)(const int32_t place[3], int32_t camera[3], int32_t screen[2]);
    int (*rain_open)(int32_t node);
} SprayV4Host;

enum { SPRAY_V4_CLASSES = 5, SPRAY_V4_VARIANTS = 3, SPRAY_V4_SPRITE = 64 };

SprayV4 *spray_v4_state(void);

/* Starts a frame: the tables for the colours (the water's and the gravel's), the frame's map cleared, the last frame's sprites freed. */
void spray_v4_draw_frame(const SprayV4Host *host, const uint8_t water_rgb[3], const uint8_t gravel_rgb[3], const uint8_t dust_rgb[3]);

/* The group of a road row; far_side 1: the puffs farther from the eye than their car (before the row's cars), 0 the nearer (after). */
void spray_v4_draw_row(int32_t node, int far_side);

/* The cloud sprites (SPRAY_V4_SPRITE square, 8-bit alpha, irregular: many lumps, noisy broken edges): class 0..SPRAY_V4_CLASSES - 1 by size, each in SPRAY_V4_VARIANTS different shapes; class SPRAY_V4_CLASSES is the stone's dot (variant ignored). For the sheet and the tests. */
const uint8_t *spray_v4_sprite(int cls, int variant);

#endif
