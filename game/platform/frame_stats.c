/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game/platform/frame_stats.h"

#include <stdio.h>

void frame_stats_reset(FrameStats *s, uint64_t period_ns)
{
    *s = (FrameStats){ .period_ns = period_ns };
}

void frame_stats_add(FrameStats *s, uint64_t interval_ns)
{
    const uint64_t twice = interval_ns * 2, p = s->period_ns;
    s->count++;
    s->sum_ns += interval_ns;
    if (interval_ns > s->max_ns) s->max_ns = interval_ns;
    if (twice < p) s->early++;
    else if (twice < 3 * p) s->on_time++;
    else if (twice < 5 * p) s->one_late++;
    else s->more++;
}

int frame_stats_format(const FrameStats *s, int vsync_interval, char *out, size_t cap)
{
    const double ms = 1e-6, mean = s->count > 0 ? (double)s->sum_ns / s->count : 0.0;
    return snprintf(out, cap, "frames %u, target %.2f ms, vsync %s: on time %u, early %u, one late %u, more %u; mean %.2f ms, max %.2f ms", s->count,
                    (double)s->period_ns * ms, vsync_interval > 0 ? "on" : "off", s->on_time, s->early, s->one_late, s->more, mean * ms, (double)s->max_ns * ms);
}
