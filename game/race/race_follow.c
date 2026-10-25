/* The follow camera's yaw (race_follow.h). */
#include "game/race/race_follow.h"

#include "game/race/follow_yaw.h"
#include "game/sys/fixed_math.h"

enum { TICKS_A_SECOND = 30, ANGLE_MASK = 0xffffff, ANGLE_SHIFT = 8 };

int32_t race_follow_yaw(RaceChase *chase, const Car *car)
{
    if (chase->port_follow_car != car) {
        chase->port_follow_car = car;
        chase->port_follow_yaw = car->yaw & ANGLE_MASK;
        chase->port_follow_velocity = 0;
    }
    FollowYaw s = { chase->port_follow_yaw, chase->port_follow_velocity };
    follow_yaw_step(&s, car->yaw & ANGLE_MASK, car->yaw_rate / TICKS_A_SECOND);
    chase->port_follow_yaw = s.yaw;
    chase->port_follow_velocity = s.velocity;
    return s.yaw;
}

void race_follow_direction(int32_t yaw, int32_t dir[3])
{
    const int32_t angle = yaw >> ANGLE_SHIFT;   /* 16 bits a turn */
    dir[0] = fx_sin16(angle);
    dir[1] = 0;
    dir[2] = fx_cos16(angle);
}
