/* port (spray v4, TNFS_SPRAY_V4 / port.cfg's spray_v4): the particles of race/spray_v4.h drawn without piling up. A group (the particles of one road row, the farther or the nearer than their car) is rasterised, a kind at a time, into a
 * small alpha map as the MAXIMUM of its sprites (viewer/spray_v4_art.h: each material its own), set against the frame's map of that kind (what the groups drawn before it already put on each pixel: only the excess is applied), and
 * drawn as ONE recorded sprite through tables that mix the pixel toward the kind's colour by at most the kind's cap (dithered). However many puffs overlap, a pixel never gets more than the cap: nothing burns to white. The colours are
 * the scene's (so night is dark and the distance fades into the haze), never fixed; a puff also fades with the distance from the eye. */
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

SprayV4 *spray_v4_state(void);

/* What the frame drawn last cost in particles (dev: TNFS_FRAME_TIMES prints them): the live particles, the puffs drawn (after the fades), of them the big ones (made on the half map), the strips recorded and their texels, the work of the big puffs (cells rasterised, and the rows' set-up). */
typedef struct SprayV4Stats { int live, puffs, big, strips; long texels, cells; } SprayV4Stats;
const SprayV4Stats *spray_v4_stats(void);

/* Starts a frame: the tables for the colours of each kind (SprayV4Kind order; the stones' glint takes the gravel dust's), the frame's maps cleared, the last frame's sprites freed. */
void spray_v4_draw_frame(const SprayV4Host *host, const uint8_t rgb[SPRAY_V4_KINDS][3]);

/* The group of a road row; far_side 1: the puffs farther from the eye than their car (before the row's cars), 0 the nearer (after). */
void spray_v4_draw_row(int32_t node, int far_side);

#endif
