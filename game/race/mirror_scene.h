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

enum {
    MIRROR_SEGMENTS = 5,        /* 004c2e94's rows; 00509330 holds 5 segments' points */
    MIRROR_ROW_POINTS = 11,
    /* 00509330: camera points (3 words each), then at 005095c4 (word 165)
     * screen points (2 words each); a camera index past 55 reads on into them */
    MIRROR_SCREEN_WORDS_AT = MIRROR_SEGMENTS * MIRROR_ROW_POINTS * 3
};

typedef struct {
    void *ctx;
    const FshEntry *(*texture)(void *ctx, int32_t flat);  /* 0050eec4[flat], NULL for none */
    /* 00425ed4 at depth 0 and v cell 0 */
    void (*subdivide)(void *ctx, const FshEntry *t, int32_t u_level, int32_t u_cell, const int32_t *const cam[4],
                      const int32_t screen[8], int32_t near);
    void (*cars)(void *ctx, int32_t node);  /* 0045a8fc */
} MirrorSceneOps;

typedef struct {
    int32_t skip;               /* 004c535c */
    const ViewLayout *layout;   /* *(00511fe8) +0x1c */
    int32_t near;               /* *(00511fe8) +4 */
    int32_t camera_node;        /* chase +0x38 */
    int32_t segment_mask;       /* 00504210 */
    int32_t circuit;            /* 00504200: 0 on a stage, where a negative first segment becomes 0 */
    int32_t node_mask;          /* 00504208 */
    const uint8_t *segments;    /* 00504228: 6 bytes a segment, +1 the mirror's LOD */
    const uint8_t *cache;       /* 005041d8: the segment records (0x21c each, +2 the 10 texture bytes) */
    const int32_t *vertices;    /* 00509330, as described above */
} MirrorSceneInputs;

/* 0044baa4 (watcall count, 00504224): the mirror's box (00449f50), then
 * `count` segments from 4 before the camera's ((node + 3) / 4 - 4), one row
 * of points each: the LOD's polygons last to first, each culled (0044a154),
 * then subdivided (00425ed4) with texture [record +2 + strip] x 8 + its uv
 * variant + 004c2e94's row; polygons that end a row band draw the cars on
 * the next 5 nodes (0045a8fc, from the segment's first node less 5, at
 * least 0). Then the cars 2 and 1 nodes before the last. Nothing while
 * 004c535c is set. */
void mirror_scene(const MirrorSceneOps *ops, const MirrorSceneInputs *in, int32_t count);

enum { MIRROR_POINTS = MIRROR_SEGMENTS * MIRROR_ROW_POINTS };

typedef struct {
    int32_t near_plane;         /* 004c52c0, world 16.16 */
    int32_t world_shift;        /* 004c52bc (its low byte shifts) */
    const int32_t *position;    /* chase +0x10 */
    const int32_t *matrix;      /* 00511fe4 */
    const View *view;
    int32_t count;              /* 00509784 */
    const int32_t (*world)[3];  /* 0050909c: the points 0043f3a8 picked */
} MirrorTransformInputs;

/* 0044b970: the camera record's near (+4) = 004c52c0 >> shift; each point
 * (world >> shift) - (position >> shift) (0048cf08), through the matrix
 * (0048cf60) into `vertices`' camera words, projected (0048cdd0) into its
 * screen words; points nearer than 0x23333 >> shift are projected again at
 * twice that depth. */
void mirror_transform(const MirrorTransformInputs *in, int32_t *near, int32_t *vertices);

/* 00425ed4, the mirror's 00425718: nothing when near is past corners 2 and
 * 1; a leaf (004255ec, no clip test) once corner 3 is at the depth's limit
 * (004b95a0 >> 004c52bc) or the depth at 004b9590; else four children around
 * the edge midpoints (01, 23, their middle, 12; 03 only after the first two,
 * and not when near is past both 23 and 01), each a quarter of the cell. */
void mirror_draw_subdivided(const DrawContext *ctx, const FshEntry *texture, int u_level, int depth, int u_cell,
                            int v_cell, const int32_t *const cam[4], const int32_t screen[8], int32_t near);

#endif
