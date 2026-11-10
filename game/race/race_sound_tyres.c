/* The race's tyre sounds, and the speed level and the angle a car is heard at (race_sound.h). */
#include "game/race/race_sound_private.h"
#include "game/race/rally_gravel.h"
#include "game/race/car_walls.h"

enum { SPEED_LEVEL_TOP = 0x40 };

enum {
    TYRE_FRONT_CHANNEL = 4, TYRE_REAR_CHANNEL = 12, TYRE_SOUND = 4, TYRE_ON_ROAD = 1, TYRE_OFF_ROAD = 8,
    TYRE_FADE_FIRST = 0x26, TYRE_FADE_LAST = 0x2e, TYRE_LATERAL_SCALE = 6, TYRE_TURN = 0x1000000, TYRE_REAR_ANGLE = 0x800000,
    IN_CAR_KINDS = 1
};

static int tyre_heard(const RaceState *race, int32_t channel, int32_t skid)
{
    const int32_t sound = race->voices[channel].sound;
    return skid > 0 || (sound <= TYRE_FADE_LAST && sound >= TYRE_FADE_FIRST);
}

void race_sound_tyres(RaceState *race)
{
    Car *car = race->cars[race_slot(race->viewed)];  /* 0052b316 / 004fbb08 / 00513e6c pick the same car */
    if (car == NULL) return;  /* port guard */
    int32_t distance = -1, angle, rear_distance, rear_angle;
    if (race->chase.kind <= IN_CAR_KINDS) {
        angle = x_mul(x_sub(0, car->lateral_force), TYRE_LATERAL_SCALE);
        if (angle < 0) angle = x_add(angle, TYRE_TURN);
        rear_angle = TYRE_REAR_ANGLE;
        distance = rear_distance = 0;
    } else {
        const int32_t at[3] = { car->x, car->y, car->z };
        race_sound_position(race, at, &angle, &distance);
        rear_distance = distance, rear_angle = angle;
    }
    int32_t sound, byte;
    if (race->unknown_4c5f98 >= 0) {
        sound = race->unknown_4c5f98, byte = race->unknown_4c5f9c;
    } else {
        sound = TYRE_SOUND;
        byte = race->surfaces[(uint32_t)car->surface < RACE_SURFACES ? car->surface : 0].off_road == 0  /* port guard */
            ? TYRE_ON_ROAD : TYRE_OFF_ROAD;
    }
    int32_t front = car->skid_front, rear = car->skid_rear;
    if (race->port_rally_gravel != 0 && race->port_rally_gravel != RALLY_GRAVEL_SNOW && race->port_rally_shoulder_sound != 0 && car->node_flag_4aa == 0 && car->node_flag_4c6 == 0) {   /* port (snow crunch): the snow's roll is its own voice, not the off-road tyre sound */
        /* port (rally_gravel_shoulder): the rolling rally gravel scrubs as the shoulder does: the same tyre sound, its off-road variant, fed by speed */
        const int on_ground = car->airborne_ticks == 0;
        front = rally_gravel_tyre_skid(race->port_rally_gravel, car->speed, front, on_ground);
        rear = rally_gravel_tyre_skid(race->port_rally_gravel, car->speed, rear, on_ground);
        if (on_ground && (front != car->skid_front || rear != car->skid_rear)) byte = TYRE_OFF_ROAD;
        /* the rear tyre's volume is the distance's alone (race_sound_play: only channel 4's, the front's, is scaled by the skid, which the floor already halved), so the rolling rear
         * (its skid raised to the floor) is farther by the distance of half the loudness; a real skid keeps its own */
        if (on_ground && rear != car->skid_rear) rear_distance = rally_gravel_roll_distance(rear_distance);
    }
    if (race->unknown_4bc6f0 != 0) {
        if (tyre_heard(race, TYRE_FRONT_CHANNEL, front))
            race_sound_effect(race, TYRE_FRONT_CHANNEL, sound, byte, front, x_add(distance, 1), angle);
        if (tyre_heard(race, TYRE_REAR_CHANNEL, rear))
            race_sound_effect(race, TYRE_REAR_CHANNEL, sound, byte, rear, x_add(rear_distance, 1), rear_angle);
    } else {
        race->car_sound_ticks = 0;
    }
    if (car->horn == 0 || race->viewed < 0 || race->viewed >= race->humans) {
        race_sound_signal_off(race);
        return;
    }
    if (distance < 0) {
        const int32_t at[3] = { car->x, car->y, car->z };
        race_sound_position(race, at, &angle, &distance);
    }
    race_sound_signal(race, distance, angle);
}

int32_t race_sound_speed_level(const Car *car, const Car *followed)
{
    const int32_t apart = estimate(magnitude(x_sub(followed->velocity_z, car->velocity_z)),
                                   magnitude(x_sub(followed->velocity_x, car->velocity_x)));
    const int32_t own = x_shl(estimate(magnitude(car->velocity_z), magnitude(car->velocity_x)), 1);
    const int32_t level = x_sar(own > apart ? own : apart, 16);
    return level > SPEED_LEVEL_TOP ? SPEED_LEVEL_TOP : level;
}

enum { ANGLE_TURN = 0x1000000, ANGLE_FINE = 8 };

void race_sound_angle(RaceState *race, const Car *car, int32_t *angle)
{
    const int32_t dx = x_sar(x_sub(car->x, race->chase.position[0]), ANGLE_FINE);
    const int32_t dz = x_sar(x_sub(car->z, race->chase.position[2]), ANGLE_FINE);
    if (race->chase.angles[1] < 0) race->chase.angles[1] = x_add(race->chase.angles[1], ANGLE_TURN);
    *angle = x_sub(race->chase.angles[1], x_shl(fx_atan2(dx, dz), ANGLE_FINE));
    if (*angle < 0) *angle = x_add(*angle, ANGLE_TURN);
    if (*angle > ANGLE_TURN) *angle = x_sub(*angle, ANGLE_TURN);
}
