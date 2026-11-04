/* port: see track_gravity.h. */
#include "game/race/track_gravity.h"

int32_t track_gravity(int32_t moon, int32_t pull)
{
    return moon != 0 ? pull / LOW_GRAVITY_DIV : pull;
}

int32_t track_rain_grip(int32_t wet, int32_t grip)
{
    return wet != 0 ? grip * RAIN_GRIP_NUM / RAIN_GRIP_DEN : grip;
}

int32_t track_grip(int32_t moon, int32_t grip)
{
    return moon != 0 ? grip * LOW_GRIP_NUM / LOW_GRIP_DEN : grip;
}
