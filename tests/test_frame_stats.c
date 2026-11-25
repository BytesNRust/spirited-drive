/* The frame-time histogram: which bin an interval falls in, and the line it prints. */
#include "game/platform/frame_stats.h"
#include "test.h"

#include <string.h>

int main(void)
{
    FrameStats s;
    const uint64_t p = 16666667;
    frame_stats_reset(&s, p);
    frame_stats_add(&s, p);              /* on the beat */
    frame_stats_add(&s, p * 3 / 4);      /* a little short */
    frame_stats_add(&s, p * 14 / 10);    /* a little long, still one frame */
    frame_stats_add(&s, p / 4);          /* two presents together */
    frame_stats_add(&s, p * 2);          /* a frame shown twice */
    frame_stats_add(&s, p * 24 / 10);
    frame_stats_add(&s, p * 3);          /* a stall */
    frame_stats_add(&s, p * 50);
    CHECK_EQ_INT((int)s.count, 8);
    CHECK_EQ_INT((int)s.on_time, 3);
    CHECK_EQ_INT((int)s.early, 1);
    CHECK_EQ_INT((int)s.one_late, 2);
    CHECK_EQ_INT((int)s.more, 2);
    CHECK(s.max_ns == p * 50);
    char line[200];
    frame_stats_format(&s, 1, line, sizeof line);
    CHECK(strstr(line, "frames 8, target 16.67 ms, vsync on: on time 3, early 1, one late 2, more 2") == line);
    frame_stats_format(&s, 0, line, sizeof line);
    CHECK(strstr(line, "vsync off") != NULL);
    frame_stats_reset(&s, 8333333);
    CHECK_EQ_INT((int)s.count, 0);
    CHECK(s.period_ns == 8333333);
    frame_stats_format(&s, 0, line, sizeof line);   /* nothing counted: no division by zero */
    CHECK(strstr(line, "mean 0.00 ms") != NULL);
    frame_stats_add(&s, 8333333);
    CHECK_EQ_INT((int)s.on_time, 1);
    return TEST_REPORT();
}
