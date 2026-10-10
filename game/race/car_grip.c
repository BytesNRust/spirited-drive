#include "game/race/car_grip.h"

#include "game/sys/fixed_math.h"

/* A 24-bit angle folded into -0x800000..0x800000. */
static int32_t signed_angle(int32_t a)
{
    return a > 0x800000 ? a - 0x1000000 : a;
}

static int32_t slope_at(const RaceState *race, int32_t node)
{
    return signed_angle(race_node(race, node).slope * 0x400);
}

void car_axle_loads(Car *car, const RaceState *race)
{
    const int32_t here = slope_at(race, car->node), next = slope_at(race, car->node + 1);
    const int32_t speed = car->local_velocity_z;
    int32_t shift = fx_mul_round(fx_sin((uint32_t)((next - here) >> 14)), speed) * 4;
    if (shift > 0) shift += shift >> 1;
    int32_t roll = signed_angle(car->roll);
    if (roll <= 0) roll = -roll;
    shift += (fx_mul_round(roll, speed > 0 ? speed : -speed) >> 7) * 2;
    car->load_shift = shift;
    car->axle_load_b += shift;
    if (car->axle_load_b < 0) car->axle_load_b = 0;
    car->axle_load_a += car->load_shift;
    if (car->axle_load_a < 0) car->axle_load_a = 0;
    if (speed <= 0) return;
    const int32_t square = (speed >> 16) * (speed >> 16);
    const int32_t b = car->downforce_a * square, a = car->downforce_b * square;
    car->axle_load_b += b;
    car->axle_load_a += a;
    const int32_t both = b + a;
    car->load_shift += both / 8 + both / 4;
}
