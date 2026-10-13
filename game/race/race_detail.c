/* The draw distance asked for during the race (race_detail.h). */
#include "game/race/race_detail.h"

enum { NEAREST = 8, FARTHEST = 0x1c };

void race_detail_request(int32_t segments, int32_t *pending)
{
    if (segments > FARTHEST) segments = FARTHEST;
    else if (segments < NEAREST) segments = NEAREST;
    *pending = segments;
}
