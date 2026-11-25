/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: how evenly the frames reach the screen: the time between two presents counted against the game's period, so the log of a run with TNFS_VSYNC=0 can be set
 * beside one with TNFS_VSYNC=1 (TNFS_FRAME_STATS=1 prints a line every FRAME_STATS_EVERY presents). */
#ifndef TNFS_FRAME_STATS_H
#define TNFS_FRAME_STATS_H

#include <stddef.h>
#include <stdint.h>

enum { FRAME_STATS_EVERY = 600 };

typedef struct {
    uint64_t period_ns;      /* the game's frame, the yardstick */
    uint32_t count;          /* intervals counted */
    uint32_t early;          /* under half a period: two presents close together */
    uint32_t on_time;        /* from half to one and a half periods */
    uint32_t one_late;       /* from one and a half to two and a half: a frame shown twice */
    uint32_t more;           /* longer: a stall */
    uint64_t sum_ns, max_ns;
} FrameStats;

void frame_stats_reset(FrameStats *s, uint64_t period_ns);
void frame_stats_add(FrameStats *s, uint64_t interval_ns);
/* "frames 600, target 16.67 ms, vsync 1: on time 598, early 0, one late 2, more 0; mean 16.70 ms, max 33.40 ms" into out; returns its length */
int frame_stats_format(const FrameStats *s, int vsync_interval, char *out, size_t cap);

#endif
