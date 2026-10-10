#include "game/race/car_setup.h"

#include "game/sys/fixed_math.h"

#include <limits.h>

enum {
    GRAVITY = 0x9cf5c,            /* 9.81 in 16.16 */
    INERTIA_FACTOR = 0x324
};

/* The original's two-step 16.16 division (it faults where this gives 0). */
static int32_t divide(int32_t a, int32_t b)
{
    if (b == 0 || (a == INT_MIN && b == -1)) return 0;
    return fx_div(a, b);
}

/* Weight on each axle: the share of the mass, times g, times the axle's
 * spring factor; downforce is scaled the same way. */
static void axle_weights(Car *car, const CarSpecs *specs)
{
    car->unknown_455 = fx_mul_round(specs->unknown_2dc, fx_mul_round(car->unknown_43d, GRAVITY));
    car->unknown_451 = fx_mul_round(specs->unknown_2e0, fx_mul_round(car->unknown_441, GRAVITY));
    car->axle_load_b = car->unknown_455;
    car->axle_load_a = car->unknown_451;
    car->wind_a = (int16_t)(car->downforce_a * 2);
    car->wind_b = (int16_t)(car->downforce_b * 2);
    car->downforce_a = (int16_t)(specs->unknown_2dc / 256 * car->downforce_a * 10 / 256);
    car->downforce_b = (int16_t)(specs->unknown_2e0 / 256 * car->downforce_b * 10 / 256);
}

/* The turning inertia and each axle's lever on it. */
static void turning(Car *car, const CarSpecs *specs)
{
    const int32_t length = specs->unknown_040;
    const int32_t inertia = fx_mul_round(fx_mul_round(length, length), INERTIA_FACTOR);
    car->unknown_459 = divide(inertia, length);
    car->unknown_45d = divide(fx_mul_round(inertia, specs->unknown_2d0), length);
    car->unknown_461 = divide(fx_mul_round(car->unknown_441, length), inertia);
    car->unknown_465 = divide(fx_mul_round(car->unknown_43d, length), inertia);
}

void car_setup_physics(Car *car, CarSpecs *specs, RaceState *race)
{
    race_surfaces_init(race);
    car->unknown_43d = fx_mul_round(specs->unknown_000, specs->mass);
    car->unknown_441 = fx_mul_round(specs->unknown_004, specs->mass);
    car->unknown_445 = fx_mul_round(specs->unknown_008, specs->unknown_00c);
    car->unknown_449 = fx_mul_round(specs->unknown_008, specs->unknown_010);
    specs->drag = fx_mul_round(specs->drag, specs->mass);
    car->unknown_44d = fx_mul_round(specs->unknown_024, specs->unknown_044);
    axle_weights(car, specs);
    turning(car, specs);
}
