/* The race camera's choice of view (race_camera.h). */
#include "game/race/race_camera.h"

#include "game/race/race_chase.h"
#include "game/gfx/x86_int.h"

#include <stdint.h>

enum {
    HALF_TURN = 0x800000,         /* 24-bit angles */
    FULL_TURN = 0x1000000,
    HEADING_SCALE = 0x400,        /* node heading (0x4000 a turn) to the car's angles */
    HEADING_OFF = 0x1e0000,
    TILT_OFF = 0x240000,
    FORCE_SHIFT = 13,
    FORCE_MAX = 100,
    CAMERA_HOLD = 0x3c,
    KIND_OFF_ROAD = 13,
    KIND_IN_CAR = 1,              /* kinds 0..1 sit in the car, 2..3 just behind it */
    KIND_NEAR = 3,
    KIND_DRIVER = 10,             /* 0 and 1 taken back as 10 and 11 */
    KIND_DRIVER_2 = 11
};

static int32_t magnitude(int32_t v)
{
    return v < 1 ? -v : v;
}

int car_faces_road(const Car *car, const RaceState *race)
{
    const TriNode n = race_node(race, car->node);
    int32_t off = car->yaw + n.orientation * -HEADING_SCALE;
    if (off > HALF_TURN) off -= FULL_TURN;
    else if (off < -HALF_TURN) off += FULL_TURN;
    if (off >= HEADING_OFF || off <= -HEADING_OFF) return 0;
    return magnitude(car->roll) <= TILT_OFF && magnitude(car->pitch) <= TILT_OFF;
}

void race_send_force(const Car *car, RaceState *race)
{
    if (race->force_off != 0) return;
    int32_t x = car->lateral_force >> FORCE_SHIFT, y = car->along_force >> FORCE_SHIFT;
    if (x > FORCE_MAX) x = FORCE_MAX;
    else if (x < -FORCE_MAX) x = -FORCE_MAX;
    if (y > FORCE_MAX || y < -FORCE_MAX) y = FORCE_MAX;  /* the original's: a strong backward force reads +100 */
    if (x == race->force_x && y == race->force_y) return;
    race->force_x = x;
    race->force_y = y;
    if (race->sounds.force != NULL) race->sounds.force(race->sounds.user, x, y);
}

void race_camera_pick(RaceState *race, const Car *car)
{
    if (race->camera_hold != 0) race->camera_hold = (int32_t)((uint32_t)race->camera_hold - 1u);
    RaceChase *chase = &race->chase;
    if (chase->kind == chase->next_kind && chase->kind > KIND_NEAR && chase->kind != KIND_DRIVER) return;
    if (car == NULL) return;  /* the original faults: cars[viewed] is never empty */
    int32_t kind;
    if (!car_faces_road(car, race)) {
        race->camera_hold = CAMERA_HOLD;
        if (chase->kind > KIND_NEAR && chase->kind != KIND_DRIVER && chase->kind != KIND_DRIVER_2) return;
        kind = KIND_OFF_ROAD;
    } else {
        if (chase->kind == chase->next_kind || race->camera_hold != 0) return;
        kind = chase->next_kind == 0 ? KIND_DRIVER : chase->next_kind == KIND_IN_CAR ? KIND_DRIVER_2 : chase->next_kind;
    }
    race_chase_take(race, chase, kind);
}

enum {
    SHAKE_X, SHAKE_Y,
    SHAKE_SCALE = 0x23d70, SHAKE_SHIFT = 16,
    SURFACE_NONE = 0, SURFACE_SOFT = 1, SURFACE_HARD = 2, SURFACE_BOTH = 3,
    SOFT_DROP = 2, Y_DROP = 4, BOTH_LEVEL = 1
};

/* 0043ce88's speed steps: below `upto`, the level and the strength */
static const struct {
    int32_t upto, level, strength;
} SHAKES[] = { { 5, 5, 0xf }, { 10, 6, 0x10 }, { 15, 7, 0x11 }, { 20, 8, 0x12 }, { 30, 9, 0x13 },
               { 40, 10, 0x14 }, { 80, 11, 0x15 }, { 150, 12, 0x16 }, { INT32_MAX, 13, 0x18 } };

static void shake_clear(RaceState *race, int32_t axis)
{
    int32_t *on = axis == SHAKE_X ? &race->vibration_x_on : &race->vibration_y_on;
    if (*on == 0) return;
    if (race->sounds.vibration_clear != NULL) race->sounds.vibration_clear(race->sounds.user, axis);
    *on = 0;
}

static int shake_due(const RaceState *race, int32_t on, int32_t level, int32_t strength)
{
    return on == 0 || level != race->vibration_level || strength != race->vibration_strength;
}

static void shake(RaceState *race, int32_t axis, int32_t a, int32_t b, int32_t c)
{
    *(axis == SHAKE_X ? &race->vibration_x_on : &race->vibration_y_on) = 1;
    if (race->sounds.vibration != NULL) race->sounds.vibration(race->sounds.user, axis, a, b, c);
}

static void keep_shake(RaceState *race, int32_t level, int32_t strength)
{
    race->vibration_strength = strength;
    race->vibration_level = level;
}

void race_send_vibration(const Car *car, RaceState *race)
{
    int32_t speed = race->viewed < race->humans ? car->local_velocity_z : car->ai_speed;
    if (speed <= 0) speed = x_sub(0, speed);
    const int64_t product = (int64_t)speed * SHAKE_SCALE;
    const int32_t rounded = (int32_t)(uint32_t)((uint64_t)(product >> SHAKE_SHIFT) + (uint64_t)(product >> (SHAKE_SHIFT - 1) & 1));
    const int32_t step = rounded >> SHAKE_SHIFT;
    if (step == 0) {
        shake_clear(race, SHAKE_X);
        shake_clear(race, SHAKE_Y);
        return;
    }
    size_t k = 0;
    while (step >= SHAKES[k].upto) k++;
    int32_t level = SHAKES[k].level;
    const int32_t strength = SHAKES[k].strength;
    switch (car->surface) {
    case SURFACE_SOFT:
        if (!shake_due(race, race->vibration_x_on, level, strength)) break;
        shake(race, SHAKE_X, strength - SOFT_DROP, strength - SOFT_DROP, level - SOFT_DROP);
        keep_shake(race, level, strength);
        return;
    case SURFACE_HARD:
        if (!shake_due(race, race->vibration_x_on, level, strength)) break;
        shake(race, SHAKE_X, strength, strength, level);
        keep_shake(race, level, strength);
        return;
    case SURFACE_BOTH:
        if (shake_due(race, race->vibration_x_on, level, strength)) {
            shake(race, SHAKE_X, strength, strength, BOTH_LEVEL);
            level = BOTH_LEVEL;
        }
        if (!shake_due(race, race->vibration_y_on, level, strength)) break;
        shake(race, SHAKE_Y, 0, strength - Y_DROP, level);
        keep_shake(race, level, strength);
        return;
    default:  /* 0 and any other (an unsigned test) */
        shake_clear(race, SHAKE_X);
        shake_clear(race, SHAKE_Y);
        break;
    }
    keep_shake(race, level, strength);
}

void race_forces_enable(RaceState *race)
{
    if (race->effects != 0 && race->sounds.forces_enable != NULL) race->sounds.forces_enable(race->sounds.user);
}

void race_forces_disable(RaceState *race)
{
    if (race->effects != 0 && race->sounds.forces_disable != NULL) race->sounds.forces_disable(race->sounds.user);
}

void race_forces_clear(RaceState *race)
{
    if (race->effects == 0) return;
    if (race->sounds.forces_clear != NULL) race->sounds.forces_clear(race->sounds.user);
    race->force_off = 0;
}
