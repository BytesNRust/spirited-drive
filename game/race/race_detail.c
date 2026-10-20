/* The draw distance asked for during the race (race_detail.h). */
#include "game/race/race_detail.h"

#include "game/gfx/x86_int.h"

enum { NEAREST = 8, FARTHEST = 0x1c, RATE_AFTER = 0xa, RATE_SCALE = 100 };

static int32_t farthest = FARTHEST;

void race_detail_set_ceiling(int32_t segments)
{
    farthest = segments >= FARTHEST && segments <= DRAW_SEGMENTS_ULTRA ? segments : FARTHEST;
}

void race_detail_request(int32_t segments, int32_t *pending)
{
    if (segments > farthest) segments = farthest;
    else if (segments < NEAREST) segments = NEAREST;
    *pending = segments;
}

void race_detail_step(int32_t step, int32_t draw_segments, int32_t *pending)
{
    int32_t segments = x_add(step, draw_segments);
    if (segments > farthest) segments = farthest;
    else if (segments < NEAREST) segments = NEAREST;
    *pending = segments;
}

void race_frame_rate(RaceFrameRate *fr, int32_t elapsed)
{
    int32_t frames = x_add(fr->frames, 1);
    int32_t time = x_add(fr->elapsed, elapsed);
    if (time > RATE_AFTER) {
        fr->rate = x_mul(frames, RATE_SCALE) / time;
        frames = 0;
        time = 0;
    }
    fr->frames = frames;
    fr->elapsed = time;
}
