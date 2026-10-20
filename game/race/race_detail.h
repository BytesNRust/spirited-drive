/* The draw distance asked for during the race: a count of track segments
 * (00504220) taken at the next frame by 0044c118. */
#ifndef TNFS_RACE_DETAIL_H
#define TNFS_RACE_DETAIL_H

#include <stdint.h>

/* port: the "ultra" view distance: the most segments the track's cache window holds (64 records of 0x21c in its
 * 0x8700 bytes: 56 drawn + 2 ahead + 5 behind + the camera's) */
enum { DRAW_SEGMENTS_ULTRA = 56 };

/* port: the bounds' top, 0x1c as the exe holds it unless "ultra" (port.cfg's view_ultra) sets a higher one, up to
 * DRAW_SEGMENTS_ULTRA; out of 0x1c..DRAW_SEGMENTS_ULTRA it goes back to 0x1c */
void race_detail_set_ceiling(int32_t segments);

/* 0044c1bc (watcall segments): *pending (004c5360) = segments held to
 * 8..0x1c (the port's ceiling). 0044c118 sets 00504220 from it unless it is -1. */
void race_detail_request(int32_t segments, int32_t *pending);

/* 0044c0ec (watcall step, from 0044cc74): *pending (004c5360) = the draw
 * segments (00504220) + step (32-bit), held to 8..0x1c (the port's ceiling). */
void race_detail_step(int32_t step, int32_t draw_segments, int32_t *pending);

typedef struct {
    int32_t frames;             /* 004c5364 */
    int32_t elapsed;            /* 004c5368 */
    int32_t rate;               /* 005104c4: frames x 100 / elapsed, what 0044cc74 steps the draw distance by */
} RaceFrameRate;

/* 0044c150 (watcall elapsed, from 0044cc74 with 00473334()'s time since the
 * last frame): frames + 1, elapsed + it (32-bit); past 10: rate = frames x
 * 100 / elapsed (wrapping product, idiv) and both 0. */
void race_frame_rate(RaceFrameRate *fr, int32_t elapsed);

#endif
