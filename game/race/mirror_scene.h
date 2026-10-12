/* The rear-view mirror's scene (0044baa4 and the pieces it calls): the
 * segments behind the camera drawn into the mirror's picture. */
#ifndef TNFS_MIRROR_SCENE_H
#define TNFS_MIRROR_SCENE_H

#include "game/race/car_struct.h"
#include "game/race/race_view.h"
#include "game/race/track_draw.h"

#include <stdint.h>

/* The camera's screen box (00511f88..00511f94). */
typedef struct {
    int32_t bottom;             /* 00511f88: top + the layout's +0x24 */
    int32_t right;              /* 00511f8c: left + the layout's +0x20 */
    int32_t left, top;          /* 00511f90, 00511f94: the layout's +0x18, +0x1c */
} MirrorBounds;

/* 00449f50: the box of the camera's layout (*(00511fe8) +0x1c). */
void mirror_bounds(const ViewLayout *layout, MirrorBounds *b);

/* 0044a154: 0 when the quad's four x (screen[0, 2, 4, 6]) are all left of
 * the box or all right of it, or its points 1 and 2 are both below it
 * (screen[3], screen[5] past bottom); else 1. */
int mirror_quad_visible(const MirrorBounds *b, const int32_t screen[8]);

enum { MIRROR_SEGMENT_RECORD = 0x21c, MIRROR_SEGMENT_CACHE = 0x8700 };

/* 0043e95c: the byte offset of a segment's record in the segment cache
 * (005041d8): (segment & 00504210) x 0x21c (32-bit) mod 0x8700, unsigned. */
uint32_t mirror_segment_offset(int32_t segment, int32_t mask);

enum { MIRROR_CAR_ROWS = 11 };

typedef struct {
    void *ctx;
    void (*prepare)(void *ctx, Car *car, int mirror);  /* 0045a724 */
    void (*draw)(void *ctx, Car *car, int mirror);     /* 004591d4 */
} MirrorCarsOps;

typedef struct {
    int32_t camera_node;        /* chase +0x38 */
    int32_t segments;           /* 0050420c */
    const uint8_t *rows;        /* 0051c7a1: MIRROR_CAR_ROWS flags, by nodes behind the camera */
    Car *const *list;           /* 00509e8c: the cars to draw */
    int32_t count;              /* 00509e14 */
    const Car *followed;        /* cars[004fbb08] */
    int32_t chase_kind;         /* 004f801c */
} MirrorCarsInputs;

/* 0045a8fc: the cars on `node` in the mirror (the mirror pass of 0045a848).
 * Nothing unless the node is 0..10 behind the camera (around the ring of
 * 0050420c x 4 nodes) with that row's flag set; then each listed car whose
 * physics (+0x512) is not 6 and whose draw_node2 (+0x345) is the node,
 * except the followed car while the chase kind is 0: 0045a724, 004591d4. */
void mirror_cars(const MirrorCarsOps *ops, const MirrorCarsInputs *in, int32_t node);

/* 00425ed4, the mirror's 00425718: nothing when near is past corners 2 and
 * 1; a leaf (004255ec, no clip test) once corner 3 is at the depth's limit
 * (004b95a0 >> 004c52bc) or the depth at 004b9590; else four children around
 * the edge midpoints (01, 23, their middle, 12; 03 only after the first two,
 * and not when near is past both 23 and 01), each a quarter of the cell. */
void mirror_draw_subdivided(const DrawContext *ctx, const FshEntry *texture, int u_level, int depth, int u_cell,
                            int v_cell, const int32_t *const cam[4], const int32_t screen[8], int32_t near);

#endif
