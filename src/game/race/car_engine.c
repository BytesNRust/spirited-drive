#include "game/race/car_engine.h"

#include "game/race/car_specs_word.h"

enum {
    GEAR_NEUTRAL = -1,
    MIN_TARGET_RPM = 700,
    MAX_RPM = 11000,
    REV_THROTTLE = 13,        /* below this the free engine falls to idle */
    CATCH_UP_BAND = 500,
    HEAVY_THROTTLE = 0xdc
};

/* Out of gear or in the air: the revs follow the throttle alone. */
static void free_revs(Car *car, const CarSpecs *specs, int32_t limit)
{
    const int neutral = car->gear == GEAR_NEUTRAL;
    if (car->throttle < REV_THROTTLE) {
        const int32_t fall = neutral ? specs->rpm_fall / 2 : specs->rpm_fall;
        if (car->rpm > specs->idle_rpm) {
            car->rpm -= fall;
            if (car->rpm < specs->idle_rpm) car->rpm = specs->idle_rpm;
        } else {
            car->rpm += fall;
            if (car->rpm > specs->idle_rpm) car->rpm = specs->idle_rpm;
        }
        return;
    }
    const int32_t rise = car->throttle * specs->rpm_rise;
    car->rpm += (neutral ? rise / 2 : rise) >> 8;
    if (limit < car->rpm) car->rpm = limit;
}

/* In gear on the road: the revs move towards what the wheels turn. */
static void geared_revs(Car *car, const CarSpecs *specs, const RaceState *race)
{
    if (car->rpm > car->target_rpm) {
        const int32_t drop =
            (car->tyre_flags_rear & 2) && car->throttle > HEAVY_THROTTLE ? specs->rpm_drop / 8 : specs->rpm_drop;
        car->rpm -= drop;
        if (car->rpm < car->target_rpm) car->rpm = car->target_rpm;
        return;
    }
    if (car->target_rpm + CATCH_UP_BAND > car->rpm) {
        car->rpm += specs->rpm_catch_up;
    } else {
        car->rpm += specs->rpm_catch_up * race_gear_step(race, car->gear) / 256;
    }
    if (car->rpm > car->target_rpm) car->rpm = car->target_rpm;
}

void car_engine_revs(Car *car, const CarSpecs *specs, const RaceState *race)
{
    if (car->stalled != 0) car->throttle = 0;
    const int32_t ratio = car_gear_ratio(specs, car->gear);
    const int32_t wheel = ((car->wheel_speed >> 8) * (specs->wheel_rpm >> 8)) >> 8;
    car->target_rpm = ((ratio >> 8) * wheel) >> 16;
    if (car->target_rpm < MIN_TARGET_RPM) car->target_rpm = MIN_TARGET_RPM;
    const int32_t limit = (specs->unknown_0ac * car->throttle) >> 8;
    if (car->engaged == 0) {
        free_revs(car, specs, limit);
    } else {
        geared_revs(car, specs, race);
    }
    if (car->transmission != 0 && car->gear == 0 && car->rpm < specs->idle_rpm) car->rpm = specs->idle_rpm;
    if (car->rpm > MAX_RPM) {
        car->rpm = MAX_RPM;
        car->handbrake = 1;
    }
    if (car->target_rpm > MAX_RPM) car->target_rpm = MAX_RPM;
}

int32_t car_drag(Car *car, const CarSpecs *specs, const RaceState *race, int32_t speed, int with_wind)
{
    const Surface *surface = car->surface >= 0 && car->surface < RACE_SURFACES ? &race->surfaces[car->surface] : NULL;
    const Surface none = { 0, 0, 0, 0 };
    if (surface == NULL) surface = &none;
    car->off_road = surface->off_road != 0 ? 4 : 0;
    const int32_t rolling = specs->drag * surface->rolling / 256;
    const int32_t square = (speed >> 16) * (speed >> 16);
    int32_t drag = rolling * square;
    if (with_wind) drag += (car->wind_a + car->wind_b) * square;
    drag += surface->drag;
    int32_t roll = car->roll;
    if (roll > 0x800000) roll -= 0x1000000;
    if (roll <= 0) roll = -roll;
    if (race->track > 2 && roll > 0xa0000) drag /= 8;
    int32_t limit = car->ticks_per_second * speed;
    if (limit <= 0) limit = -limit;
    if (drag > limit) drag = limit;
    return speed >= 0 ? -drag : drag;
}
