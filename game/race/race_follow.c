/* The follow camera's yaw (race_follow.h). */
#include "game/race/race_follow.h"

#include "game/race/follow_yaw.h"
#include "game/race/race_keys.h"
#include "game/sys/fixed_math.h"

enum { TICKS_A_SECOND = 30, ANGLE_MASK = 0xffffff, ANGLE_SHIFT = 8, HALF_TURN = 0x800000, LOOK_BOOST = 3, LOOK_SETTLED = 0x20000 };   /* look back's spring: 3 times as fast, 0.117 s; settled within 2.8 degrees */

int32_t race_follow_yaw(RaceChase *chase, const Car *car)
{
    if (chase->port_follow_car != car) {
        chase->port_follow_car = car;
        chase->port_follow_yaw = car->yaw & ANGLE_MASK;
        chase->port_follow_velocity = 0;
    }
    FollowYaw s = { chase->port_follow_yaw, chase->port_follow_velocity };
    const int32_t heading = (car->yaw + (chase->port_look_back != 0 ? HALF_TURN : 0)) & ANGLE_MASK;   /* look back: the spring's target is behind the car */
    if (chase->port_look_back != 0) chase->port_look_fast = 1;
    else if (chase->port_look_fast != 0) {   /* released: the quick spring goes on until the camera is back behind the car */
        int32_t off = (s.yaw - car->yaw) & ANGLE_MASK;
        if (off > HALF_TURN) off = ANGLE_MASK + 1 - off;
        if (off < LOOK_SETTLED) chase->port_look_fast = 0;
    }
    follow_yaw_step_boost(&s, heading, car->yaw_rate / TICKS_A_SECOND, chase->port_look_fast != 0 ? LOOK_BOOST : 1);
    chase->port_follow_yaw = s.yaw;
    chase->port_follow_velocity = s.velocity;
    return s.yaw;
}

int32_t race_look_back_held(int32_t follow_camera, const uint8_t *keys, int32_t codes, int32_t key)
{
    if (follow_camera == 0) return 0;
    const int own = key > 0 && key < codes && keys[key] != 0;
    const int pad = RACE_KEYS_PAD_LOOK_BACK < codes && keys[RACE_KEYS_PAD_LOOK_BACK] != 0;
    return own || pad;
}

void race_follow_direction(int32_t yaw, int32_t dir[3])
{
    const int32_t angle = yaw >> ANGLE_SHIFT;   /* 16 bits a turn */
    dir[0] = fx_sin16(angle);
    dir[1] = 0;
    dir[2] = fx_cos16(angle);
}
