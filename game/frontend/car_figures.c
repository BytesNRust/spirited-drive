/* See car_figures.h. */
#include "game/frontend/car_figures.h"

enum { WEIGHT_AT = 0x008, GEAR_LIMIT_AT = 0x058, SHIFT_AT = 0x288, SHIFTS = 11, POINTS_AT = 0x088, START_AT = 0x0b4, CURVE_AT = 0x0b8, POINT_STRIDE = 8, RPM_STEP = 200, HP_DIVISOR = 7121 };

static int32_t word(const uint8_t *r, int at)
{
    return (int32_t)((uint32_t)r[at] | (uint32_t)r[at + 1] << 8 | (uint32_t)r[at + 2] << 16 | (uint32_t)r[at + 3] << 24);
}

/* the revs it changes up at, the highest of its forward gears' (004098a0: up above shift [gear + 3], the gears 0 .. gear_limit - 3); no
 * limit when the gear count is out of range */
static int32_t rev_limit(const uint8_t *r)
{
    const int32_t gears = word(r, GEAR_LIMIT_AT);
    if (gears < 4 || gears > SHIFTS) return INT32_MAX;
    int32_t limit = 0;
    for (int32_t g = 3; g < gears; g++)
        if (word(r, SHIFT_AT + 4 * g) > limit) limit = word(r, SHIFT_AT + 4 * g);
    return limit > 0 ? limit : INT32_MAX;
}

int car_figures_pbs(const uint8_t record[PBS_SIZE], int *hp, int *kg)
{
    const int32_t points = word(record, POINTS_AT), start = word(record, START_AT), limit = rev_limit(record);
    int64_t best = 0;
    for (int32_t i = 0; i < points && CURVE_AT + POINT_STRIDE * (i + 1) <= PBS_SIZE; i++) {
        const int32_t torque = word(record, CURVE_AT + POINT_STRIDE * i), label = word(record, CURVE_AT + POINT_STRIDE * i + 4);
        if (label != start + RPM_STEP * (i + 1)) break;   /* past the curve the record holds other fields */
        const int32_t rpm = start + RPM_STEP * i + RPM_STEP / 2;
        if (rpm > limit) break;   /* never reached: it changes up first */
        const int64_t power = (int64_t)torque * rpm;
        if (power > best) best = power;
    }
    const int32_t weight = (int32_t)(((int64_t)word(record, WEIGHT_AT) + 0x8000) >> 16);
    if (best <= 0 || weight < 1 || weight > 9999) return 0;
    *hp = (int)((best + HP_DIVISOR / 2) / HP_DIVISOR);
    *kg = weight;
    return 1;
}
