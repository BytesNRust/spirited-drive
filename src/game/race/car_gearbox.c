#include "game/race/car_gearbox.h"

#include "game/race/car_specs_word.h"

/* The engine revs the car's speed gives in a gear (car +0x68 through the
 * wheels and the gear ratio), as 004098a0 computes them. */
static int32_t revs_in(const Car *car, const CarSpecs *specs, int32_t gear)
{
    const int32_t ratio = car_gear_ratio(specs, gear);
    return ((((car->local_velocity_z >> 8) * (specs->wheel_rpm >> 8)) >> 8) * (ratio >> 8)) >> 16;
}

static int32_t shift_rpm(const CarSpecs *specs, int32_t slot)
{
    return car_specs_word(specs, (int32_t)offsetof(CarSpecs, shift_rpm) + slot * 4);
}

void car_auto_shift(Car *car, const CarSpecs *specs)
{
    const int32_t gear = car->gear;
    if (gear + 3 != specs->gear_limit && revs_in(car, specs, gear) > shift_rpm(specs, gear + 3)) {
        car->target_rpm = revs_in(car, specs, gear + 1);
        car->gear = gear + 1;
        return;
    }
    if (gear == 0) return;
    const int32_t lower = revs_in(car, specs, gear - 1);
    if (lower < shift_rpm(specs, gear + 2) * 15 / 16) {
        car->target_rpm = lower;
        car->gear = gear - 1;
    }
}

enum { SLOW_SHIFT_CAR = 4 };

void car_gear_change(Car *car, const CarSpecs *specs, const RaceState *race)
{
    if (car->shift_timer > 0) car->shift_timer--;
    if (car->shift_timer == 0) {
        car->engaged = 1;
        car->shift_timer = -1;
        car->stalled = 0;
        car->throttle = car->saved_throttle;
    }
    if (car->shift_timer >= 0) return;
    const int32_t gear = car->gear;
    car_auto_shift(car, specs);
    if (gear == car->gear) return;
    car->saved_throttle = car->throttle;
    car->stalled = 1;
    car->engaged = 0;
    const int slow = car->unknown_48a == SLOW_SHIFT_CAR && (race->tick & 0x31) == 0x10;
    car->shift_timer = specs->shift_ticks + (slow ? 3 : 0);
}

void car_gear_sound(const Car *car, const RaceState *race)
{
    if (car->gear == -1 || race->sounds.tone == NULL) return;
    const int32_t down = car->gear < car->last_gear ? 0xb4 : 0;
    int32_t pitch;
    if (car->rpm < 0x1000) {
        pitch = 0xc;
    } else if (car->rpm < 0x1300) {
        pitch = 0x16;
    } else if (car->rpm < 0x1700) {
        pitch = 0x20;
    } else {
        pitch = 0x2a;
    }
    race->sounds.tone(race->sounds.user, pitch, down, 0x50);
}
