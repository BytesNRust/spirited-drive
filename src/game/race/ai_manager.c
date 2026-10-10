#include "game/race/ai_manager.h"

enum {
    ONCOMING = 0x10,            /* flags_175: drives against the first car's direction */
    TARGET_FLOOR = -0x14ccc,
    TARGET_CAP = 0x14ccc
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
