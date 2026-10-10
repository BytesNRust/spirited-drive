#include "game/race/ai_manager.h"

#include "game/race/ai_drive.h"
#include "game/sys/fixed_math.h"

enum {
    ONCOMING = 0x10,            /* flags_175: drives against the first car's direction */
    TARGET_FLOOR = -0x14ccc,
    TARGET_CAP = 0x14ccc,
    PHYSICS_CRASHED = 4,
    ROOM_PER_NODE = 0x60000,
    CAUTION = 0x13333
};

/* The weight a lane loses for `gap` (16.16) of speed, by `per` per 1/256. */
static int32_t weight(int32_t gap, int32_t per)
{
    return (gap >> 8) * per;
}

void ai_weigh_lanes(Car *car, const int32_t first[3], const int32_t second[3], int32_t wish[3])
{
    const int32_t target = car->ai_target_speed, speed = car->ai_speed;
    if (!(car->flags_175 & ONCOMING)) {
        for (int i = 0; i < 3; i++) {
            if (first[i] < target) wish[i] += weight(target - first[i], -0xcd);
            if (second[i] < target) wish[i] += weight(target - second[i], -0x80);
            if (first[i] < speed) wish[i] += weight(speed - first[i], -0x100);
            if (second[i] < speed) wish[i] += weight(speed - second[i], -0x1a);
        }
        if (first[1] < target) car->ai_target_speed = first[1];
        if (car->ai_target_speed < TARGET_FLOOR) car->ai_target_speed = TARGET_FLOOR;
    } else {
        for (int i = 0; i < 3; i++) {
            if (target < first[i]) wish[i] += weight(first[i] - target, -0xcd);
            if (target < second[i]) wish[i] += weight(second[i] - target, -0x80);
            if (speed < first[i]) wish[i] += weight(first[i] - speed, -0x100);
            if (speed < second[i]) wish[i] += weight(second[i] - speed, -0x1a);
        }
        if (target < first[1]) car->ai_target_speed = first[1];
        if (car->ai_target_speed > TARGET_CAP) car->ai_target_speed = TARGET_CAP;
    }
}

static int32_t magnitude(int32_t v)
{
    return v < 1 ? -v : v;
}

/* Whether the car can pull out past `ahead` before closing on it. */
static int has_room(const Car *car, const RaceState *race, const Car *ahead)
{
    int32_t nodes = car->progress - ahead->progress;
    if (nodes < 0) nodes += race->segments * 4;
    nodes = magnitude(nodes);
    const int32_t closing = magnitude(car->ai_speed - ahead->ai_speed);
    const int32_t sideways = magnitude(car->ai_target_lateral - car->road_lateral);
    const int32_t steer = magnitude(ai_lane_speed(car, race, 1, car->ai_speed));
    const int32_t needed = fx_mul_round(fx_mul_round(sideways, closing), CAUTION);
    return needed < fx_mul_round(nodes * ROOM_PER_NODE, steer);
}

void ai_hold_lane_speed(Car *car, const RaceState *race, const Car *ahead, int32_t from, int32_t to,
                        const int32_t row[3])
{
    int32_t speed = row[1];
    if (to != from && ahead != NULL && ahead->physics != PHYSICS_CRASHED && has_room(car, race, ahead))
        speed = from < to ? row[2] : row[0];
    if (!(car->flags_175 & ONCOMING)) {
        if (speed < car->ai_target_speed) car->ai_target_speed = speed;
        if (car->ai_target_speed < TARGET_FLOOR) car->ai_target_speed = TARGET_FLOOR;
    } else {
        if (speed > car->ai_target_speed) car->ai_target_speed = speed;
        if (car->ai_target_speed > TARGET_CAP) car->ai_target_speed = TARGET_CAP;
    }
}
