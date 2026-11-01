/* The follow camera's yaw spring (follow_yaw.h). */
#include "game/race/follow_yaw.h"

enum { HALF_TURN = 0x800000, FULL_TURN = 0x1000000, ANGLE_MASK = 0xffffff, FIXED_SHIFT = 16 };

void follow_yaw_step(FollowYaw *s, int32_t target, int32_t target_rate)
{
    follow_yaw_step_boost(s, target, target_rate, 1);
}

void follow_yaw_step_boost(FollowYaw *s, int32_t target, int32_t target_rate, int32_t boost)
{
    int32_t gap = (int32_t)(((uint32_t)target - (uint32_t)s->yaw) & ANGLE_MASK);
    if (gap > HALF_TURN) gap -= FULL_TURN;
    else if (gap == HALF_TURN && target_rate < 0) gap = -HALF_TURN;
    const int64_t pull = ((int64_t)gap * FOLLOW_YAW_STIFFNESS * boost * boost + (int64_t)(target_rate - s->velocity) * FOLLOW_YAW_DAMPING * boost) >> FIXED_SHIFT;
    int64_t velocity = s->velocity + pull;
    if (velocity > FOLLOW_YAW_MAX_VELOCITY) velocity = FOLLOW_YAW_MAX_VELOCITY;
    else if (velocity < -FOLLOW_YAW_MAX_VELOCITY) velocity = -FOLLOW_YAW_MAX_VELOCITY;
    s->velocity = (int32_t)velocity;
    s->yaw = (int32_t)(((uint32_t)s->yaw + (uint32_t)s->velocity) & ANGLE_MASK);
}
