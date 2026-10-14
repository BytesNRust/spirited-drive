/* The draw distance asked for during the race (race_detail.h). */
#include "game/race/race_detail.h"

#include "game/gfx/x86_int.h"

enum { NEAREST = 8, FARTHEST = 0x1c };

void race_detail_request(int32_t segments, int32_t *pending)
{
    if (segments > FARTHEST) segments = FARTHEST;
    else if (segments < NEAREST) segments = NEAREST;
    *pending = segments;
}

void race_detail_step(int32_t step, int32_t draw_segments, int32_t *pending)
{
    int32_t segments = x_add(step, draw_segments);
    if (segments > FARTHEST) segments = FARTHEST;
    else if (segments < NEAREST) segments = NEAREST;
    *pending = segments;
}
