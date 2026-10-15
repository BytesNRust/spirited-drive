/* The followed car's skid marks (004596a8, from 0045a3e8 while its shadow is
 * placed): a ring of 125 marks along its track, 00459eb8 draws them. */
#ifndef TNFS_SKID_MARKS_H
#define TNFS_SKID_MARKS_H

#include "game/gfx/quad.h"
#include "game/race/car_struct.h"
#include "game/race/model_draw.h"

#include <stdint.h>

enum {
    SKID_RECORDS = 125,
    SKID_WORDS = 40,            /* 0xa0 bytes a mark */
    /* a mark's words: front tyres' two corners, rear's two, the point, then
     * 0x8c the node, 0x90 / 0x94 the front / rear skid (0 none), 0x98 the
     * tick, 0x9c 1 or 2 by the surface */
    SKID_FRONT = 0, SKID_REAR = 6, SKID_POINT = 12, SKID_NODE = 35, SKID_FRONT_ON = 36, SKID_REAR_ON = 37,
    SKID_TICK = 38, SKID_SURFACE = 39,
    SKID_ROWS = 15
};

typedef struct {
    uint8_t alternate;          /* 004fb787: nothing is recorded or drawn */
    int32_t node_mask;          /* 00504208 */
    int32_t segments;           /* 0050420c: the ring is 4 x as many nodes */
} SkidRing;

typedef struct {
    /* 00517620; one more: 004596a8 reads the mark before mark 0 as mark 125,
     * the memory just past the table (0051c7c0), zero here */
    int32_t marks[SKID_RECORDS + 1][SKID_WORDS];
    int32_t index;              /* 004c565c */
    int32_t front, rear;        /* 004c5824, 004c5828: the last call's skids */
    int32_t last[3];            /* 0051c7fc: where the last mark began */
    /* 0051c7cf: the rows from the camera's node - 2 with a mark (00459c50
     * sets 15); 00459eb8 reads one more, 0051c7de, another table's first */
    uint8_t rows[SKID_ROWS + 1];
} SkidMarks;

typedef struct {
    void *ctx;
    void (*draw)(void *ctx, int32_t index);  /* 00459eb8(index, 0, 0, 0; 0, 0) */
} SkidOps;

typedef struct {
    uint8_t alternate;          /* 004fb787: nothing is recorded */
    int32_t tick;               /* 00513e70 */
    int32_t surface_word;       /* 004fa9b0[car +0x4a2] +0xc */
} SkidInputs;

/* 00459e1c (watcall (x0, y0, x1, y1; s, t, width), ret 0xc): the stroke
 * from (x0, y0) to (x1, y1) drawn (00491fa0) when either end lies on the
 * left of s -> t or on it (the 32-bit cross products' signs). */
void skid_stroke(const QuadTarget *target, const uint8_t *remap, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                 const int32_t s[2], const int32_t t[2], int32_t width);

/* 00457560 (watcall (), from 00457594): marks 0..124 get node (+0x8c)
 * 0x10000 and no skids (+0x90, +0x94 0); the index (004c565c) 0. */
void skid_reset(SkidMarks *m);

/* 00459c50 (watcall (node)): which of the 15 rows from node - 2 on (round
 * the ring) hold a mark with either skid. */
void skid_rows(SkidMarks *m, const SkidRing *ring, int32_t node);

typedef struct {
    const QuadTarget *target;   /* 00491fa0's surface */
    const uint8_t *remap;       /* and its shade (004c6604) */
    const ModelView *mv;        /* the camera (00511fe0 +0x10, 00511fe4) and 00475eac's view */
    int32_t high_res;           /* 004fa0c0: 0 halves the width */
    uint8_t screen_kind;        /* 004fb789: not 0 halves it */
} SkidView;

/* 00459eb8 (watcall (index, node, row, halve; s, t), ret 8). index -999: the
 * marks about road row `row` (when rows[] has it, counted from node - 2),
 * newest first, 123 looked at, each stroke kept to s -> t's side
 * (00459e1c); else mark `index` and the two older ones (00491fa0). A mark
 * within 2 nodes of the row (of mark index's node) is placed in camera space
 * (words 15..: its point less the camera's, >> 9, turned, plus each corner
 * turned) and projected (27..); each corner joined to the newer mark's when
 * that one was within too: clipped at z 0x40 (the newer one projected again
 * when it moved; both behind, nothing) and stroked when both marks had that
 * pair's skid (corners 0, 1 front, 2, 3 rear), 0x2580 / the farther z wide,
 * at most 15, halved for each of high_res 0, screen_kind and halve, at
 * least 1. */
void skid_draw(const SkidView *v, const SkidRing *ring, SkidMarks *m, int32_t index, int32_t node, int32_t row,
               int32_t halve, const int32_t s[2], const int32_t t[2]);

typedef struct {
    void *ctx;
    void (*puff_1)(void *ctx, int32_t x, int32_t y, int32_t size);      /* 00456bd4: a mark's surface word 1 */
    void (*puff_other)(void *ctx, int32_t x, int32_t y, int32_t size);  /* 00456c28 */
} SkidPuffOps;

/* 004588ac (watcall (corner, size)): the 4 newest marks (from 004c565c - 1
 * back): each with corner's pair skid (0, 1 front, 2, 3 rear), 1..0x2f ticks
 * old and its screen x there (word 27 + 2 corner) not INT32_MIN gets a puff
 * at that point, size x 1, 2, 3, 4 newest first, by its surface word. */
void skid_puffs(const SkidPuffOps *ops, const SkidMarks *m, int32_t tick, int32_t corner, int32_t size);

/* 004596a8 (car; forward, side, point: 0045a3e8's box >> 10 and road
 * point). */
void skid_record(const SkidOps *ops, SkidMarks *m, const Car *car, const SkidInputs *in, const int32_t forward[3],
                 const int32_t side[3], const int32_t point[3]);

#endif
