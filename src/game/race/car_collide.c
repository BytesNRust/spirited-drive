#include "game/race/car_collide.h"

#include "game/race/ai_lanes.h"
#include "game/race/car_crash.h"
#include "game/race/rigid_body.h"

enum {
    PHYSICS_CRASH = 4,
    HIT_HARD = 0xd0000,          /* 004b9070 */
    HIT_HARD_PLAYER = 0x100000,  /* 004b9074: when a player's car is one of them */
    MOVING_ON = 0x100000,        /* below this road speed a light hit makes no sound */
    TUMBLING = 0x8000,
    CRASH_TICKS = 300,
    POLICE_SLOT = 2              /* 00509e24 */
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
