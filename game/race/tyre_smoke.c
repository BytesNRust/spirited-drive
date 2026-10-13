/* The smoke off a skidding car's tyres (00458988). */
#include "game/race/tyre_smoke.h"

#include "game/gfx/x86_int.h"

enum { HALF_BASE = 0x8000, FULL_BASE = 0x10000, BASE_DEPTH = 0x38400, SKID_SHIFT = 18, MOST = 0x180,
       SIDE_FROM = 0xc0000, SIDE_TO = 0x740000 };

/* idiv by 2^n: toward zero */
static int32_t div_pow2(int32_t v, int n)
{
    return x_sar(x_add(v, v < 0 ? (1 << n) - 1 : 0), n);
}

static void pair(const TyreSmokeOps *ops, int32_t skid, int32_t base, int32_t first, int in_car, int32_t angle)
{
    int32_t n = div_pow2(x_mul(skid, x_add(ops->rand_below(ops->ctx, 4), 2)), SKID_SHIFT);
    if (n > MOST) n = MOST;
    if (n < 1) n = 1;
    const int32_t size = x_mul(div_pow2(base, 8), n);
    if (in_car || (angle > SIDE_FROM && angle < SIDE_TO)) ops->puffs(ops->ctx, first, size);
    if (in_car || (angle < -SIDE_FROM && angle > -SIDE_TO)) ops->puffs(ops->ctx, first + 1, size);
}

void tyre_smoke(const TyreSmokeOps *ops, const Car *car, int in_car, int32_t roll_add)
{
    if (car->tyre_flags_front == 0 && car->tyre_flags_rear == 0) return;
    const int32_t front = car->skid_front, rear = car->skid_rear;
    if (front == 0 && rear == 0) return;
    const int32_t angle = ops->view_angle(ops->ctx, car);
    if (car->far != 0) return;
    const int32_t z = car->camera_position[2];
    const int32_t base = x_mul(div_pow2(in_car && roll_add == 0 ? HALF_BASE : FULL_BASE, 8), x_div(BASE_DEPTH, z));
    if (front != 0) pair(ops, front, base, 0, in_car, angle);
    if (rear != 0) pair(ops, rear, base, 2, in_car, angle);
}
