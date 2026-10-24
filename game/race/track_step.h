/* The track's streaming each frame (0043f540, from 0043f8a3 and 0044cd7e):
 * the segment cache, the renderer's point list when the camera's segment
 * changes, and the mirror's points when asked for. */
#ifndef TNFS_TRACK_STEP_H
#define TNFS_TRACK_STEP_H

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*stream)(void *ctx, int32_t segment);                          /* 0043e300 */
    void (*build)(void *ctx, int32_t node, int32_t segment, int32_t count);  /* 0043f48c */
    void (*mirror_row)(void *ctx, int32_t segment);                      /* 0043f3a8 */
} TrackStepOps;

typedef struct {
    int32_t node;               /* chase +0x38 */
    int32_t *rows_again;        /* chase +0x3c (004f8058 bit 0, set by 0043f51c): the mirror's rows to be made again; cleared */
    int32_t stream_node;        /* 00504218: the node 0043f48c last built for (-1: build again) */
    int32_t *mirror_segment;    /* 0050421c: the mirror's first segment last made */
    int32_t *unknown_4c53a4;    /* cleared */
    int32_t *mirror_count;      /* 00509784 */
    int32_t circuit;            /* 00504200 */
    int32_t segment_mask;       /* 00504210 */
    int32_t draw_segments;      /* 00504220 */
    int32_t port_eye_ahead;     /* port: 1, the rows from the node one further (a camera on a segment's first node keeps
                                 * the road between it and the eye); 0, the exe's (node + 3) / 4 */
} TrackStepInputs;

/* 0043f540 (watcall (chase)): 0043e300(node / 4); 004c53a4 = 0; 0043f48c(node,
 * node / 4, 00504220) unless 00504218 / 4 is that segment and 00504218 >= 0.
 * With chase +0x3c set: when ((node + 3) / 4 - 4) & 00504210 differs from
 * 0050421c it becomes 0050421c, 00509784 = 0 and 0043f3a8 runs for it and the
 * next 4 (& 00504210; on a stage, 00504200 0, a negative one as 0); +0x3c = 0. */
void track_stream_step(const TrackStepOps *ops, const TrackStepInputs *in);

/* The mirror's first segment for a camera at `node`, which both the rows (above) and the scene (mirror_scene) start
 * from: ((node + 3) / 4 - 4) & segment_mask, the exe's. port_eye_ahead 1 takes the node one further, so a camera on a
 * segment's first node (node % 4 == 0) still has the road between it and the eye in the rows' last segment. */
int32_t track_mirror_first(int32_t node, int32_t segment_mask, int port_eye_ahead);

#endif
