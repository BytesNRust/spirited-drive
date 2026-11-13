/* port: the wheels a car throws its spray and dust from (rain_spray.h). */
#include "game/race/rain_spray.h"

#include "game/sys/fixed_math.h"

#include <math.h>

int32_t rain_spray_speed(int32_t speed, int32_t vx, int32_t vz)
{
    if (speed != 0) return speed;
    const int64_t ax = vx < 0 ? -(int64_t)vx : vx, az = vz < 0 ? -(int64_t)vz : vz;
    const int64_t big = ax > az ? ax : az, small = ax > az ? az : ax;
    const int64_t v = big + small / 4;
    return v > INT32_MAX ? INT32_MAX : (int32_t)v;
}

void rain_spray_heading(int use_matrix, int32_t yaw, int32_t matrix6, int32_t matrix8, int32_t forward[2])
{
    const double len = sqrt((double)matrix6 * matrix6 + (double)matrix8 * matrix8);
    if (use_matrix && len >= 1.0) {
        forward[0] = (int32_t)((double)matrix6 / len * 65536.0), forward[1] = (int32_t)((double)matrix8 / len * 65536.0);
        return;
    }
    const int32_t angle = (yaw >> 8) & 0xffff;
    forward[0] = fx_sin16(angle), forward[1] = fx_cos16(angle);
}

void rain_spray_wheels(const int32_t at[3], const int32_t forward[2], int32_t wheel[2][3], int32_t right[2])
{
    const int32_t behind = 0x16000, across = 0xd000;   /* a rear wheel: 1.4 m behind the centre, 0.8 m out */
    right[0] = forward[1], right[1] = -forward[0];
    for (int w = 0; w < 2; w++) {
        const int32_t a = w == 0 ? -across : across;
        wheel[w][0] = at[0] - (int32_t)(((int64_t)forward[0] * behind) >> 16) + (int32_t)(((int64_t)forward[1] * a) >> 16);
        wheel[w][1] = at[1] - 0x5000;
        wheel[w][2] = at[2] - (int32_t)(((int64_t)forward[1] * behind) >> 16) - (int32_t)(((int64_t)forward[0] * a) >> 16);
    }
}
