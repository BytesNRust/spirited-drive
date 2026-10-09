#include "game/race/car_crash.h"

#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

#include <string.h>

enum {
    PHYSICS_CRASH = 4,
    GRAVITY = -0x9cf5c,
    SPIN_FROM_YAW_RATE = 0x648,
    SLIDE_TICKS = 0x3c,
    HIT_TICKS = 300,
    NOTE_CRASH = 0x5c,
    FLAG_DASHBOARD = 2            /* flags_175 */
};

/* 0040dc20: the body takes the car's place, speed and turn; its z axis
 * runs the other way. */
static void hand_to_body(Car *car)
{
    car->body_position[0] = car->x;
    car->body_position[1] = car->y;
    car->body_position[2] = -car->z;
    car->body_velocity[0] = -car->velocity_x;
    car->body_velocity[1] = car->velocity_y;
    car->body_velocity[2] = -car->velocity_z;
    car->body_force[0] = 0;
    car->body_force[1] = GRAVITY;
    car->body_force[2] = 0;
    car->body_spin[0] = 0;
    car->body_spin[1] = -fx_mul_round(car->yaw_rate, SPIN_FROM_YAW_RATE);
    car->body_spin[2] = 0;
    int32_t m[9];
    if (car->use_matrix == 0) {
        rm_turn_xzy(m, -car->pitch, car->roll, -car->yaw);
    } else {
        memcpy(m, car->matrix, sizeof m);
        m[2] = -m[2];
        m[5] = -m[5];
        m[6] = -m[6];
        m[7] = -m[7];
    }
    memcpy(car->body_matrix, m, sizeof m);
    const int32_t lift = car->unknown_0bc >> 8;
    car->body_position[0] += (m[3] >> 8) * lift;
    car->body_position[1] += lift * (m[4] >> 8);
    car->body_position[2] += (m[5] >> 8) * lift;
}

/* The car the camera and the sounds follow (either way of the original's
 * test it is the viewed one). */
static const Car *followed(const RaceState *race)
{
    return race->cars[race->viewed & (RACE_PLAYERS - 1)];
}

void car_crash_slide(Car *car, RaceState *race)
{
    const Car *heard = followed(race);
    hand_to_body(car);
    car->physics = PHYSICS_CRASH;
    car->ai_countdown = SLIDE_TICKS;
    if (car == heard && race->sounds.stop_engine != NULL) race->sounds.stop_engine(race->sounds.user);
}

void car_crash_hit(Car *car, RaceState *race)
{
    const Car *heard = followed(race);
    hand_to_body(car);
    car->crashed_hard = 1;
    car->physics = PHYSICS_CRASH;
    if (race->sounds.skids_reset != NULL) race->sounds.skids_reset(race->sounds.user);
    car->flags_175 = (uint8_t)(car->flags_175 & ~FLAG_DASHBOARD);
    car->ai_countdown = HIT_TICKS;
    race_note(race, NOTE_CRASH);
    if (car == heard && race->sounds.stop_engine != NULL) race->sounds.stop_engine(race->sounds.user);
}
