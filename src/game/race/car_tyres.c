#include "game/race/car_tyres.h"

#include "game/sys/fixed_math.h"

enum {
    LOCKING_BRAKE = 0xf5,
    SLIP_TABLE_END = 0x1ffffe,
    DRIFT_SLIP = 0xf0000,
    SPIN_SLIP_ANGLE = 0x1e0000,
    MAX_SKID = 0x960000,
    TYRE_SLIDING = 1,
    TYRE_DRIVEN_PAST_GRIP = 2,
    TYRE_DRIFTING = 4
};

static int32_t abs32(int32_t v)
{
    return v > 0 ? v : -v;
}

/* Two forces' combined size, as the original estimates it: the larger plus
 * a quarter of the smaller. */
static int32_t combined(int32_t a, int32_t b)
{
    return a > b ? a + (b >> 2) : b + (a >> 2);
}

/* 0042ebbc */
static int32_t slip_angle(int32_t lateral, int32_t along)
{
    return fx_atan2(lateral, along) << 8;
}

/* 0042f93c: forces past the limit are scaled back inside it (to 5/8 of it),
 * and how far past goes to *skid. A sum under 256 would fault in the
 * original; here it leaves the forces alone. */
static void friction_circle(int32_t *lateral, int32_t *along, int32_t limit, int32_t *skid)
{
    const int32_t size = combined(abs32(*lateral), abs32(*along));
    if (size <= limit) return;
    const int32_t inside = limit - limit * 3 / 8;
    const int32_t per = size / 256;
    if (per == 0) return;
    const int32_t k = inside / per;
    *lateral = *lateral * k / 256;
    *along = *along * k / 256;
    *skid = size - inside;
}

/* 0042f9dc: the tyre's grip limit on the two forces. Traction control first
 * takes back drive force; past the limit both are scaled, or (without ABS
 * under hard braking, engine intact) clamped. rear: the rear axle, whose
 * spin feeds unknown_40d. */
static void grip_limit(Car *car, int32_t *lateral, int32_t *along, int32_t limit, int32_t *skid, int rear)
{
    int32_t spin = 0;
    const int32_t a = abs32(*lateral);
    int32_t b = abs32(*along);
    int32_t size = b < a ? a + (b >> 2) : b + (a >> 2);
    if (car->traction_active != 0) {
        if (size > limit) {
            const int32_t over = size - limit;
            if (b > over) {
                b -= over;
                *along = *along > 0 ? b : -b;
            } else {
                *along = 0;
                b = 0;
            }
        }
        size = b < a ? a + (b >> 2) : b + (a >> 2);
    }
    if (size > limit) {
        *skid = size - limit;
        int32_t capped = size;
        if (capped > limit * 18 / 16) capped = limit * 18 / 16;
        spin = capped - limit;
        int32_t room = limit;
        if (spin > 0) room -= spin;
        const int32_t per = size / 256;
        const int32_t k = per == 0 ? 0 : room / per;
        const int clamp = !((car->brake > 0x96 && car->anti_lock == 0) || car->blown != 0);
        if (!clamp) {
            *lateral = *lateral * k / 256;
            *along = *along * k / 256;
        } else {
            if (limit < *lateral) {
                *lateral = limit;
            } else if (-limit > *lateral) {
                *lateral = -limit;
            }
            if (car->throttle >= 0x32) {
                *along = *along * k / 256;
            } else if (limit < *along) {
                *along = limit;
            } else if (-limit > *along) {
                *along = -limit;
            }
        }
    }
    if (rear && ((*along > 0 && car->local_velocity_z > 0) || (*along < 0 && car->local_velocity_z < 0)))
        car->unknown_40d = spin;
}

/* The full model: the grip table by slip angle, drift and spin, traction. */
static void tyre_model(Car *car, const CarSpecs *specs, RaceState *race, int32_t *lateral, int32_t *along,
                       int32_t slip, int32_t load, int32_t drive, int32_t brake, int axle, uint8_t *flags,
                       int32_t *skid)
{
    int32_t excess = 0;
    const int32_t index = (slip > SLIP_TABLE_END ? SLIP_TABLE_END : slip) >> 12;
    const uint8_t *table = axle == 1 ? specs->grip_front : specs->grip_rear;
    const int32_t grip = fx_mul_round(table[index] << 9, load);
    if (abs32(*lateral) > grip) {
        if (abs32(*lateral) > grip * 8 && slip > DRIFT_SLIP) {
            if (slip > SPIN_SLIP_ANGLE) {
                *flags |= TYRE_SLIDING;
                excess = abs32(*lateral) - grip * 8;
            } else {
                excess = (abs32(*lateral) - grip * 8) / 4;
                *flags |= TYRE_DRIFTING;
            }
        }
        *lateral = *lateral > 0 ? grip : -grip;
    }
    if (*along <= 0) brake = -brake;
    const int32_t push = drive + brake;
    if (abs32(brake) < abs32(drive)) {
        *along = push;
        if (abs32(*lateral) + abs32(push) > load) {
            *flags = TYRE_DRIVEN_PAST_GRIP;
            if (abs32(push) > load && race->wheelspin != 0 && car->traction_control == 0) *lateral /= 8;
        }
        grip_limit(car, lateral, along, load, skid, axle != 1);
        if (*skid != 0) *skid *= 8;
        if (excess > *skid) *skid = excess;
    } else {
        if (abs32(push) < abs32(*along)) *along = *along > 0 ? abs32(push) : -abs32(push);
        grip_limit(car, lateral, along, load, skid, axle != 1);
        *skid |= excess;
        if (car->anti_lock_active != 0) *skid = 0;
    }
}

void car_tyre_forces(Car *car, const CarSpecs *specs, RaceState *race, int32_t *out_lateral, int32_t *out_along,
                     int32_t lateral_in, int32_t along_in, int32_t steer, int32_t drive, int32_t brake, int axle)
{
    if (car->on_ground == 0) {
        *out_along = 0;
        *out_lateral = 0;
        return;
    }
    int32_t lateral, along, load, skid;
    uint8_t flags = 0;
    if (axle == 1) {
        fx_rotate(lateral_in, along_in, -steer, &lateral, &along);
        load = car->axle_load_b;
        skid = car->skid_front;
    } else {
        lateral = lateral_in;
        along = along_in;
        load = car->axle_load_a;
        skid = car->skid_rear;
    }
    race->tyre_force_sum = *out_along + *out_lateral;
    int32_t slip = slip_angle(abs32(lateral), abs32(along));
    if (slip > specs->max_slip) {
        slip = specs->max_slip;
        flags |= TYRE_SLIDING;
    }
    const int locked = car->brake > LOCKING_BRAKE && car->anti_lock == 0;
    const int heavy = load * 5 / 4 < abs32(brake) && !(car->anti_lock != 0 && car->blown == 0);
    if (locked || heavy) {
        if (abs32(along) > load) flags |= TYRE_SLIDING;
        friction_circle(&lateral, &along, load, &skid);
    } else {
        tyre_model(car, specs, race, &lateral, &along, slip, load, drive, brake, axle, &flags, &skid);
    }
    if (skid > MAX_SKID) skid = MAX_SKID;
    if (axle == 1) {
        car->tyre_flags_front = flags;
        car->skid_front = skid;
    } else {
        car->tyre_flags_rear = flags;
        car->skid_rear = skid;
    }
    if (steer != 0) {
        fx_rotate(lateral, along, steer, out_lateral, out_along);
    } else {
        *out_lateral = lateral;
        *out_along = along;
    }
}
