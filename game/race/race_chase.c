/* The camera that follows a car (race_chase.h). */
#include "game/race/race_chase.h"

static int32_t sub(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a - (uint32_t)b);
}

void race_chase_glide(RaceChase *chase, const int32_t to[3], int32_t divide, int32_t ticks)
{
    if (ticks == 0) return;
    for (int i = 0; i < 3; i++) {
        const int32_t gap = sub(to[i], chase->target[i]);
        if (divide == 0) chase->step[i] = gap >> (ticks & 31);
        else if (ticks == -1 && gap == INT32_MIN) chase->step[i] = gap;  /* port guard: idiv faults there */
        else chase->step[i] = gap / ticks;
    }
}
