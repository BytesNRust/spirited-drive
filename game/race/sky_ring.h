/* The 3D sky ring, the horizon of the tracks whose INFO file sets ring type 1
 * (track_info.h): 32 points on a circle around the camera (two turns of 16
 * steps), and 7 consecutive ones, picked by the camera yaw, form 7 textured
 * slices of the FAM "horz" bitmap. The bitmap is split into 8 cells; the
 * second set of cells is mirrored. Above and below the slices the sky is
 * filled with the bitmap's first and last pixel, along a line sloped by the
 * camera roll.
 *
 *   sky_ring_build   0043abfc: the ring, once per track
 *   sky_ring_draw    0043b298: per frame; 0043b0c8: the fills
 *
 * Pixels leave through a sink (triangles of the texture mapper 00486400 /
 * 004858f0, rectangle fills of 004829b0), as in track_draw.h. */
#ifndef TNFS_SKY_RING_H
#define TNFS_SKY_RING_H

#include "formats/fsh.h"
#include "game/race/track_info.h"
#include "game/race/view.h"

#include <stdint.h>

enum {
    SKY_RING_POINTS = 32,   /* two turns, 1/16 turn apart */
    SKY_RING_CELLS = 8,     /* uv cells the bitmap is cut into */
    SKY_RING_SLICES = 7     /* ring[8]: slices drawn */
};

/* The ring object that 0043abfc allocates (004fccf0). */
typedef struct {
    int32_t enabled;                            /* [0]: height_far > 0 */
    int32_t vert_offset, z_offset, radius;      /* [2], [3], [4] */
    int32_t slices;                             /* [8] */
    int32_t points[SKY_RING_POINTS][3];         /* [9..] 16.16 */
    /* [0x91..], [0xd1..]: u0 v0 u1 v1 u2 v2 u3 v3 per cell, 16.16 */
    int32_t uv[2][SKY_RING_CELLS][8];
} SkyRing;

/* 0043abfc(height_far, radius_far) with 004fccf8 / 004fcd00 / 004fcd1c from
 * the INFO file; horz is the FAM "horz" bitmap (size only). With ring type 0
 * only `enabled` is set, as in the original. port: ring[1], [5], [6] and the
 * table at [0x69..0x88] (0043abfc's copy of uninitialised memory) are not
 * kept; the original never reads them. */
void sky_ring_build(SkyRing *ring, const TrackInfo *info, const FshEntry *horz);

/* *(*(00511fe8) + 0x1c): the viewport the sky is drawn in. */
typedef struct {
    int32_t scale;       /* +0x04, 16.16 */
    int32_t width;       /* +0x10 */
    int32_t x, y;        /* +0x18, +0x1c */
    int32_t w, h;        /* +0x20, +0x24 */
} SkyViewport;

typedef struct {
    const View *view;           /* projection state (004c6xxx) */
    const int32_t *matrix;      /* camera matrix, *00511fe4 */
    int32_t position[3];        /* param_2: -camera position >> 10 (0043af80) */
    int32_t yaw;                /* 004f8060, 24-bit, 0x1000000 per turn */
    int32_t roll;               /* camera +0x48, 24-bit */
    int mirrored;               /* 004fbad8 */
    int world_shift;            /* 004bb8d4 (8) */
    int screen_shift;           /* 004c2d84 (0) */
    int32_t horizon[2];         /* param_3: projected horizon point (horizon_point) */
    SkyViewport viewport;
    int32_t reference_width;    /* 0051201c (640) */
    int32_t clip_right;         /* 00512024 (640) */
    int32_t clip_left;          /* 0051202c (0) */
    int sky_off;                /* 004fb786 (F4, 004381f8) */
    int port_wide;              /* port: slices past the exe's seven, clipped at the view's edges, for a field wider
                                 * than they reach; 0 the exe's */
} SkyFrame;

typedef struct {
    void *user;
    /* 00486400, or 004858f0 when the texture's type is 0x45 (transparent):
     * uv in texels, screen in pixels, (x, y) x 3. */
    void (*triangle)(void *user, const FshEntry *texture, const int32_t uv[6], const int32_t screen[6]);
    /* 004829b0: a solid rectangle. */
    void (*fill)(void *user, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t colour);
    /* 00484389: a flat polygon through 4 points (x, y). */
    void (*polygon)(void *user, uint8_t colour, const int32_t points[8]);
} SkySink;

/* 0043b298. info supplies ring_y_offset, ring_height (004fcd20, 004fcd24) and
 * sky_flag (004fcd74). With the sky off (004fb786) there are no fills, and
 * each slice that would be drawn is two flat polygons instead: the first
 * pixel's colour from the viewport's top down to the ring's line, the last
 * pixel's from the line down to the viewport's bottom. */
void sky_ring_draw(const SkyRing *ring, const TrackInfo *info, const SkyFrame *frame, const FshEntry *horz,
                   const SkySink *sink);

#endif
