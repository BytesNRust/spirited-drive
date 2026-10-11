/* The camera that follows a car (race_chase.h). */
#include "game/race/race_chase.h"

#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

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

int32_t race_chase_distance(const int32_t a[3], const int32_t b[3])
{
    const int32_t dx = sub(a[0], b[0]), dz = sub(a[2], b[2]);
    const int32_t sum = (int32_t)((uint32_t)fx_mul_round(dx, dx) + (uint32_t)fx_mul_round(dz, dz));
    return rm_sqrt(sum, 8, sum);
}

static int32_t neg(int32_t a)
{
    return (int32_t)(0u - (uint32_t)a);
}

void race_chase_unturn(const int32_t from[3], int32_t out[3], int32_t pitch, int32_t roll, int32_t yaw)
{
    int32_t x[9], z[9], y[9], xz[9], m[9];
    mat3_rotate_x(x, neg(pitch));
    mat3_rotate_z(z, neg(roll));
    mat3_rotate_y(y, neg(yaw));
    mat3_mul(x, z, xz);
    mat3_mul(xz, y, m);
    const int32_t v[3] = { from[0] >> 8, from[1] >> 8, from[2] >> 8 };
    mat3_apply(v, m, out);
    for (int i = 0; i < 3; i++) out[i] = (int32_t)((uint32_t)out[i] << 8);
}
