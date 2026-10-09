#include "game/race/car_crash.h"

#include "game/race/rigid_body.h"

#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

#include <string.h>

enum {
    PHYSICS_DRIVING = 2,
    PHYSICS_CRASH = 4,
    GRAVITY = -0x9cf5c,
    SPIN_FROM_YAW_RATE = 0x648,
    SLIDE_TICKS = 0x3c,
    HIT_TICKS = 300,
    NOTE_CRASH = 0x5c,
    FLAG_DASHBOARD = 2,           /* flags_175 */
    YAW_RATE_FROM_SPIN = 0x28be63,
    /* 004b9078..004b9084 */
    KICK_SPIN_KEPT = 0x5999,
    KICK_SPIN_SIDE = -0x3d999,
    KICK_SPIN_ALONG = -0x1d999,
    KICK_UP = 0x5b333
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

void car_crash_end(Car *car, RaceState *race, int32_t physics)
{
    Car *heard = race->cars[race->viewed & (RACE_PLAYERS - 1)];
    if (car == heard) {
        car->unknown_4be[0] = -1;
        car->unknown_4be[1] = -1;
        if (car->physics == PHYSICS_CRASH && race->sounds.start_engine != NULL)
            race->sounds.start_engine(race->sounds.user);
    }
    car->physics = physics;
    const int32_t lift = car->unknown_0bc >> 8;
    car->x = car->body_position[0] - lift * (car->body_matrix[3] >> 8);
    car->y = car->body_position[1] - lift * (car->body_matrix[4] >> 8);
    car->z = -(car->body_position[2] - (car->body_matrix[5] >> 8) * lift);
    car->velocity_x = -car->body_velocity[0];
    car->velocity_y = car->body_velocity[1];
    car->velocity_z = -car->body_velocity[2];
    car->yaw_rate += (-car->yaw_rate - fx_mul_round(car->body_spin[1], YAW_RATE_FROM_SPIN)) >> 1;
    car->ai_countdown = 0;
    car->skid_front = 0;
    car->skid_rear = 0;
    /* 004302a0 for the humans' cars does nothing */
}

/* A turn kept within -0x400000..0xbfffff. */
static int32_t settled_turn(int32_t turn)
{
    return turn > 0xbfffff ? turn - 0x1000000 : turn;
}

void car_crash_settle(Car *car, RaceState *race)
{
    int32_t m[9];
    memcpy(m, car->body_matrix, sizeof m);
    Car *heard = race->cars[race->viewed & (RACE_PLAYERS - 1)];
    if (car == heard) {
        car->unknown_4be[0] = -1;
        car->unknown_4be[1] = -1;
        if (car->physics == PHYSICS_CRASH && race->sounds.start_engine != NULL)
            race->sounds.start_engine(race->sounds.user);
    }
    /* +0x88 holds the car's own address */
    car_crash_end(car, race, PHYSICS_DRIVING);
    car->crashed_hard = 0;
    int32_t pitch, yaw, roll;
    rb_angles(m, &pitch, &roll, &yaw);
    car->pitch = settled_turn(-pitch & 0xffffff);
    car->roll = settled_turn(roll & 0xffffff);
    car->yaw = -yaw & 0xffffff;
    race->camera_hold = 0;
    if (race->sounds.skids_reset != NULL) race->sounds.skids_reset(race->sounds.user);
    car->use_matrix = 0;
    car->flags_175 = (uint8_t)(car->flags_175 & ~FLAG_DASHBOARD);
    if (car->transmission != 0) car->transmission = 2;
}

int32_t car_body_road_speed(const Car *car)
{
    return (car->road_axes[6] >> 8) * (car->body_velocity[0] >> 8) +
           (car->road_axes[7] >> 8) * (car->body_velocity[1] >> 8) +
           (-car->body_velocity[2] >> 8) * (car->road_axes[8] >> 8);
}

void car_crash_kick(Car *car, RaceState *race)
{
    const int32_t kept = KICK_SPIN_KEPT >> 8, side = KICK_SPIN_SIDE >> 8, along = KICK_SPIN_ALONG >> 8;
    for (int i = 0; i < 3; i++) car->body_spin[i] = (car->body_spin[i] >> 8) * kept;
    for (int i = 0; i < 3; i++) car->body_spin[i] += side * (car->road_axes[i] >> 8);
    for (int i = 0; i < 3; i++) car->body_spin[i] += along * (car->road_axes[6 + i] >> 8);
    const int32_t up = KICK_UP >> 8;
    const int32_t kick = up * (car->body_matrix[5] >> 8);
    car->body_velocity[0] += up * (car->body_matrix[3] >> 8);
    car->body_velocity[1] += up * (car->body_matrix[4] >> 8);
    car->body_velocity[2] += kick;
    if (race->viewed == car->index && race->effects != 0 && race->sounds.buffet != NULL)
        race->sounds.buffet(race->sounds.user, car->body_velocity[2], kick);
}
