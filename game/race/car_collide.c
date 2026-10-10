#include "game/race/car_collide.h"

#include "game/race/ai_lanes.h"
#include "game/race/car_crash.h"
#include "game/race/car_walls.h"
#include "game/race/rigid_body.h"

enum {
    PHYSICS_CRASH = 4,
    HIT_HARD = 0xd0000,          /* 004b9070 */
    HIT_HARD_PLAYER = 0x100000,  /* 004b9074: when a player's car is one of them */
    MOVING_ON = 0x100000,        /* below this road speed a light hit makes no sound */
    TUMBLING = 0x8000,
    CRASH_TICKS = 300,
    POLICE_SLOT = 2,             /* 00509e24 */
    NOTE_HIT = 2,                /* 00443fc0's sound and kind for two cars hitting */
    SOUND_KIND = 2,
    SOUND_SCALE = 0x14,
    SOUND_GAP = 0x8000,
    SOUND_TICKS = 10,
    SHAKE_MOST = 0xa0000,
    SHAKE_INDEX = 0x1c
};

/* 004b909c, by: slot 1's car (2), a player's car (4), a hard hit (8), the
 * police car (1) */
static const int32_t HIT_NOTES[16] = { 0x14, 0x1e, 0x20, 0x22, 0x3c, 0x46, 0x50, 0,
                                       0x25, 0x26, 0x26, 0x27, 0x5c, 0x5f, 0x62, 0 };

static int32_t magnitude(int32_t v)
{
    return v > 0 ? v : -v;
}

static int player_car(const Car *car, const RaceState *race)
{
    return car->index >= 0 && car->index < race->humans;
}

/* the followed car's engine sounds stop as it crashes */
static void crash(Car *car, const Car *heard, RaceState *race)
{
    if (car == heard && heard->physics != PHYSICS_CRASH && race->sounds.stop_engine != NULL)
        race->sounds.stop_engine(race->sounds.user);
}

static void tumble_on(Car *car)
{
    if (magnitude(car->body_velocity[2]) > TUMBLING || magnitude(car->body_velocity[0]) > TUMBLING ||
        magnitude(car->body_velocity[1]) > TUMBLING)
        car->ai_countdown = CRASH_TICKS;
}

static void first_hit(Car *car, RaceState *race)
{
    ai_road_frame(car, race);
    car->crashed_hard = 1;
    car_crash_kick(car, race);
}

int car_cars_collide(Car *a, Car *b, RaceState *race)
{
    const Car *heard = race->cars[race->viewed & (RACE_PLAYERS - 1)];
    RigidBody body_a, body_b;
    car_get_body(a, &body_a);
    car_get_body(b, &body_b);
    int32_t point[3], normal[3];
    const int touched = rb_bodies_separate(&body_a, &body_b, point, normal) &&
                        rb_bodies_collide(race, &body_a, &body_b, a->index, b->index, point, normal, &race->car_impulse);
    car_set_body(a, &body_a);
    car_set_body(b, &body_b);
    if (!touched) return 0;
    const int player_a = player_car(a, race), player_b = player_car(b, race);
    const int player = player_a || player_b;
    const int32_t hard_from = player ? HIT_HARD_PLAYER : HIT_HARD;
    const int hard = hard_from < race->car_impulse;
    const int second = a == race->second_car || b == race->second_car;
    const int police = a == race->cars[POLICE_SLOT] || b == race->cars[POLICE_SLOT];
    const int index = second * 2 + player * 4 + police;
    if (!hard && !(player_a && a->crashed_hard == 1) && !(player_b && b->crashed_hard == 1)) {
        if (a->physics != PHYSICS_CRASH) car_crash_end(a, race, a->physics);
        /* the original tests a's model again here */
        if (a->physics != PHYSICS_CRASH) car_crash_end(b, race, b->physics);
        a->ai_speed = car_body_road_speed(a);
        b->ai_speed = car_body_road_speed(b);
        if (magnitude(a->ai_speed) <= MOVING_ON || magnitude(b->ai_speed) <= MOVING_ON) return 1;
        race_note(race, HIT_NOTES[index]);
        return 1;
    }
    race_note(race, HIT_NOTES[index + 8]);
    if (a == heard) crash(a, heard, race);
    if (b == heard) crash(b, heard, race);
    a->physics = PHYSICS_CRASH;
    b->physics = PHYSICS_CRASH;
    tumble_on(a);
    tumble_on(b);
    if (player_a && a->crashed_hard == 0) first_hit(a, race);
    if (player_b && b->crashed_hard == 0) first_hit(b, race);
    return 1;
}

/* Both suspensions shaken by the push: opposite ways along z, the same way
 * along x, each way picked at random. */
static void shake(Car *a, Car *b, RaceState *race, int32_t push)
{
    int32_t amount = push > SHAKE_MOST ? SHAKE_MOST : push;
    if (race_rand_below(race, 10) > 5) amount = -amount;
    a->suspension_z = amount;
    b->suspension_z = -amount;
    if (race_rand_below(race, 10) > 5) amount = -amount;
    a->suspension_x = amount;
    b->suspension_x = amount;
    a->suspension_z_index = SHAKE_INDEX;
    b->suspension_z_index = SHAKE_INDEX;
    a->suspension_x_index = SHAKE_INDEX;
    b->suspension_x_index = SHAKE_INDEX;
}

int car_cars_hit(Car *a, Car *b, RaceState *race)
{
    if (a->physics != PHYSICS_CRASH) car_hand_to_body(a);
    if (b->physics != PHYSICS_CRASH) car_hand_to_body(b);
    if (!car_cars_collide(a, b, race)) return 0;
    if (race->crash_sound_level > race->car_impulse) return 1;
    const Car *heard = a != race->cars[race->viewed & (RACE_PLAYERS - 1)] ? a : b;
    const int32_t at[3] = { heard->x, heard->y, heard->z };
    int32_t angle, distance;
    race_sound_position(race, at, &angle, &distance);
    if (race->sounds.sound != NULL)
        race->sounds.sound(race->sounds.user, -1, NOTE_HIT, SOUND_KIND, race->car_impulse * SOUND_SCALE, distance, angle);
    race->crash_sound_level = race->car_impulse + SOUND_GAP;
    race->crash_sound_ticks = SOUND_TICKS;
    shake(a, b, race, race->car_impulse);
    return 1;
}
