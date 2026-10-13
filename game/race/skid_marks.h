/* The followed car's skid marks (004596a8, from 0045a3e8 while its shadow is
 * placed): a ring of 125 marks along its track, 00459eb8 draws them. */
#ifndef TNFS_SKID_MARKS_H
#define TNFS_SKID_MARKS_H

#include "game/gfx/quad.h"
#include "game/race/car_struct.h"

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

/* 00459c50 (watcall (node)): which of the 15 rows from node - 2 on (round
 * the ring) hold a mark with either skid. */
void skid_rows(SkidMarks *m, const SkidRing *ring, int32_t node);

/* 004596a8 (car; forward, side, point: 0045a3e8's box >> 10 and road
 * point). */
void skid_record(const SkidOps *ops, SkidMarks *m, const Car *car, const SkidInputs *in, const int32_t forward[3],
                 const int32_t side[3], const int32_t point[3]);

#endif
