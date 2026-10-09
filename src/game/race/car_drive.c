#include "game/race/car_drive.h"

#include "game/race/car_specs_word.h"

enum {
    FULL_THROTTLE = 0xff,
    LAUNCH_RPM = 0x4b0,           /* over the throttle's rev range */
    LAUNCH_FULL_TICKS = 200,
    LAUNCH_TICKS = 400,
    LAUNCH_EARLIEST = 100,
    MAX_DRIVE = 0x70000,
    SPIN_SLIP = 0x30000,
    SPIN_THROTTLE = 0xfa,
    BOOST_THROTTLE = 0xf0,
    SPIN_BAND = 500,
    ARCADE_BOOST_SPEED = 0x190000
};

int32_t car_torque(const CarSpecs *specs, int32_t rpm)
{
    const int32_t rounded = rpm - (rpm + 100) % 200 + 100;
    int32_t point = (rounded - specs->torque_start_rpm) / 200;
    if (point <= 0) point = 0;
    if (point >= specs->torque_points) point = specs->torque_points - 1;
    return car_specs_word(specs, (int32_t)offsetof(CarSpecs, torque_curve) + point * 8);
}

/* The wheels' speed at these revs in the current gear (0 where the
 * original would divide by zero). */
static int32_t wheel_speed_at(const Car *car, const CarSpecs *specs, int32_t rpm)
{
    const int32_t per = ((specs->wheel_rpm >> 8) * (car_gear_ratio(specs, car->gear) >> 8)) >> 16;
    return per == 0 ? 0 : rpm * 0x10000 / per;
}

static int32_t abs32(int32_t v)
{
    return v > 0 ? v : -v;
}

/* The player's revved start: holding the throttle keys before the lights
 * gives full throttle for 200 ticks of the start clock. Returns 1 while the
 * engine is held at full throttle. */
static int launch(Car *car, const CarSpecs *specs, RaceState *race)
{
    if (race->player == car->player && race->playback == 0 && race->start_input != 0) {
        const uint8_t *k = race->start_keys;
        if (k[0] != 0 && (k[1] != 0 || k[2] != 0 || k[3] != 0) && race->launch == 0) {
            race->launch = 1;
            race->launch_full_until = race->start_clock + LAUNCH_FULL_TICKS;
            car->rpm = specs->unknown_0ac + LAUNCH_RPM;
            race->launch_end = race->start_clock + LAUNCH_TICKS;
        }
        if (race->launch == 0) return 0;
        if (race->start_clock < LAUNCH_EARLIEST) race->launch = 0;
        if (race->start_clock <= race->launch_full_until) {
            car->throttle = FULL_THROTTLE;
            return 1;
        }
        if (race->start_clock > race->launch_end) race->launch = 0;
        return 0;
    }
    if (car->player < race->humans && car->player >= 0 && car->player < RACE_PLAYERS &&
        race->launch_ready[car->player] != 0) {
        car->throttle = FULL_THROTTLE;
        car->rpm = specs->unknown_0ac + LAUNCH_RPM;
        return 1;
    }
    return 0;
}

/* Over the throttle's revs: the engine holds the car back, never more than
 * 16 times its speed. */
static int32_t over_revving(const Car *car, const CarSpecs *specs, int32_t limit)
{
    int32_t f = abs32((car->rpm - limit) * specs->engine_brake);
    f = abs32((f >> 8) * (car_gear_ratio(specs, car->gear) >> 8));
    if (f > abs32(car->local_velocity_z) * 16) return -(car->local_velocity_z * 16);
    if (car->local_velocity_z > 0) return -f;
    return car->local_velocity_z < 0 ? f : 0;
}

/* The torque through the gears, or the slip itself when the wheels spin up
 * from low gears. */
static int32_t pulling(Car *car, const CarSpecs *specs, RaceState *race)
{
    const int32_t ratio = car_gear_ratio(specs, car->gear) / 256;
    int32_t f = car_torque(specs, car->rpm) / 256 * ratio;
    f = car_specs_word(specs, (int32_t)offsetof(CarSpecs, drive_ratio) + car->gear * 4) * f / 256;
    if (f > MAX_DRIVE) f = MAX_DRIVE;
    const int32_t slip = wheel_speed_at(car, specs, car->rpm) - car->local_velocity_z;
    if (abs32(slip) > SPIN_SLIP && car->throttle > SPIN_THROTTLE && car->gear < 1) {
        race->wheelspin = 1;
        return slip + slip / 4;
    }
    return (f * car->throttle) >> 8;
}

/* The wheels spin faster than the car moves; with the arcade boost the revs
 * climb too. */
static void spin_wheels(Car *car, const CarSpecs *specs, int boosted)
{
    car->wheel_speed = wheel_speed_at(car, specs, car->rpm);
    if (boosted && car->rpm + 200 < specs->unknown_0ac) {
        car->rpm += 150;
        car->wheel_speed = wheel_speed_at(car, specs, car->rpm - 200);
    }
    const int32_t v = car->local_velocity_z;
    if (v > 0) {
        if (v > car->wheel_speed) car->wheel_speed = v;
    } else if (v < car->wheel_speed) {
        car->wheel_speed = v;
    }
}

/* With grip the wheels settle back to the car's speed. */
static void settle_wheels(Car *car, const RaceState *race)
{
    const int32_t v = car->local_velocity_z;
    if (car->wheel_speed > v && car->wheel_speed > 0) {
        car->wheel_speed -= race_gear_step(race, car->gear) * 15 * 0x20000 / 256;
        if (car->wheel_speed < v) car->wheel_speed = v;
    } else {
        car->wheel_speed = v;
    }
}

int32_t car_drive_force(Car *car, const CarSpecs *specs, RaceState *race)
{
    int32_t force = 0;
    int boost = 0;
    if (car->engaged != 0) {
        boost = launch(car, specs, race);
        if (car->target_rpm > car->rpm) {
            /* the wheels turn the engine faster: engine braking */
            const int32_t brake = specs->engine_brake * -((car->target_rpm - car->rpm) * 8);
            force = (brake >> 8) * (car_gear_ratio(specs, car->gear) >> 8);
        } else {
            const int32_t limit = (specs->unknown_0ac * car->throttle) >> 8;
            if (car->rpm > limit && !boost) {
                force = over_revving(car, specs, limit);
            } else {
                force = pulling(car, specs, race);
                if (boost) force *= 4;
            }
        }
    }
    force = force * (specs->force_scale / 64) / 4;
    int boosted = 0;
    const Surface *surface = car->surface >= 0 && car->surface < RACE_SURFACES ? &race->surfaces[car->surface] : NULL;
    if (race->arcade && surface != NULL && surface->off_road != 0 && abs32(force) > car->axle_load_a &&
        car->rpm < specs->unknown_0ac && car->throttle > BOOST_THROTTLE) {
        boosted = 1;
        force *= 4;
    }
    const int spinning = car->throttle > BOOST_THROTTLE && abs32(force) > car->axle_load_a - car->unknown_40d &&
                         specs->shift_rpm[4] - SPIN_BAND > car->rpm && specs->unknown_018 == 0;
    if (spinning || boosted) {
        spin_wheels(car, specs, boosted);
    } else {
        settle_wheels(car, race);
    }
    if (race->arcade && car->throttle > SPIN_THROTTLE && car->gear > 0 && car->local_velocity_z < ARCADE_BOOST_SPEED)
        force += force;
    return force;
}
