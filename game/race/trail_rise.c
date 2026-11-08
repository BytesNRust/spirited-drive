/* port (trail_rise): see trail_rise.h. */
#include "game/race/trail_rise.h"

enum { FADE_END_PERCENT = 30 };

int32_t trail_rise_height(int age, int life)
{
    if (life <= 0 || age <= 0) return 0;
    if (age > life) age = life;
    const int64_t gone = life - age;   /* it eases out: 1 - (1 - f)^2 of the cap */
    return (int32_t)((int64_t)TRAIL_RISE_CAP * ((int64_t)life * life - gone * gone) / ((int64_t)life * life));
}

int trail_rise_fade(int age, int life)
{
    if (life <= 0 || age <= 0) return 100;
    if (age > life) age = life;
    return 100 - (100 - FADE_END_PERCENT) * age / life;
}
