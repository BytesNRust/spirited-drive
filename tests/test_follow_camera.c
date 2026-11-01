/* port (port_follow_camera): the camera's follow of the car's heading (race_follow.h, race_camera.h's camera_faces_road) */
#include "game/race/race_camera.h"
#include "game/race/race_follow.h"
#include "game/race/race_keys.h"
#include "game/sys/fixed_math.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

static RaceState race;
static Car car, other;

static int32_t wrapped(int32_t a)
{
    a &= 0xffffff;
    return a > 0x800000 ? a - 0x1000000 : a;
}

/* with the flag the camera takes no road: a car pointing any way along the track faces it; only a car on its side or upside
 * down does not (the pitch and roll limit, 0x240000, stays) */
static void test_faces(void)
{
    race.port_follow_camera = 1;
    for (int32_t yaw = 0; yaw < 0x1000000; yaw += 0x100000) {
        car.yaw = yaw, car.roll = 0, car.pitch = 0;
        CHECK(camera_faces_road(&car, &race));
    }
    car.yaw = 0x800000;
    car.roll = 0x240000;
    CHECK(camera_faces_road(&car, &race));
    car.roll = 0x240001;
    CHECK(!camera_faces_road(&car, &race));
    car.roll = -0x240001;
    CHECK(!camera_faces_road(&car, &race));
    car.roll = 0, car.pitch = 0x240001;
    CHECK(!camera_faces_road(&car, &race));
    race.port_follow_camera = 0;
}

/* the spring starts at the heading of the car it follows, follows it round a spin and starts again for another car */
static void test_follow(void)
{
    RaceChase chase = { 0 };
    car.yaw = 0x123456, car.yaw_rate = 0, car.roll = 0, car.pitch = 0;
    CHECK_EQ_INT(race_follow_yaw(&chase, &car), 0x123456);
    CHECK(chase.port_follow_car == &car);
    car.yaw = 0x923456;   /* turned half a turn: the camera lags, then arrives */
    int32_t first = race_follow_yaw(&chase, &car);
    CHECK(abs(wrapped(first - 0x123456)) < 0x20000);   /* not at once */
    int32_t at = first;
    for (int i = 0; i < 100; i++) at = race_follow_yaw(&chase, &car);
    CHECK(abs(wrapped(at - car.yaw)) < 0x20000);
    other.yaw = 0x555555;   /* another car: started at its heading, not swung round from the first's */
    CHECK_EQ_INT(race_follow_yaw(&chase, &other), 0x555555);
    CHECK_EQ_INT(chase.port_follow_velocity, 0);
}

/* a car spinning at a steady rate is followed round: the camera keeps within a few degrees of the heading */
static void test_spin(void)
{
    RaceChase chase = { 0 };
    car.yaw = 0, car.yaw_rate = 0x600000;   /* a quarter turn a second: 0x600000 is a bit over a third of the top rate */
    int32_t yaw = race_follow_yaw(&chase, &car);
    int32_t worst = 0;
    for (int i = 0; i < 150; i++) {
        car.yaw = (car.yaw + car.yaw_rate / 30) & 0xffffff;
        yaw = race_follow_yaw(&chase, &car);
        if (i > 30 && abs(wrapped(yaw - car.yaw)) > worst) worst = abs(wrapped(yaw - car.yaw));
    }
    CHECK(worst < 0x100000);   /* within 25 degrees once started */
}

/* the direction of a yaw is the one the exe's road direction has: its angle is the yaw (angle_of in race_chase.c) */
static void test_direction(void)
{
    for (int32_t yaw = 0; yaw < 0x1000000; yaw += 0x71000) {
        int32_t dir[3];
        race_follow_direction(yaw, dir);
        CHECK_EQ_INT(dir[1], 0);
        const int32_t back = (int32_t)((uint32_t)fx_atan2(dir[0], dir[2]) << 8);
        CHECK(abs(wrapped(back - yaw)) < 0x4000);
    }
}

/* look back (port_look_back): while held the spring's target is the car's heading turned half a turn, and released it returns to it, both
 * ways through the same spring: no jump, and settled within a second and a half */
static void test_look_back(void)
{
    RaceChase chase = { 0 };
    car.yaw = 0x100000, car.yaw_rate = 0, car.roll = 0, car.pitch = 0;
    race_follow_yaw(&chase, &car);
    chase.port_look_back = 1;
    const int32_t first = race_follow_yaw(&chase, &car);
    CHECK(abs(wrapped(first - 0x100000)) < 0x70000);   /* the first tick has not jumped: the spring's top speed is 8.5 degrees a tick */
    int ticks = 0;
    int32_t at = first;
    while (ticks < 200 && abs(wrapped(at - (car.yaw + 0x800000))) > 0x38e4) at = race_follow_yaw(&chase, &car), ticks++;   /* within 1.4 degrees */
    CHECK(ticks < 45);   /* a second and a half at 30 ticks */
    printf("look back settles in %d ticks\n", ticks);
    chase.port_look_back = 0;
    ticks = 0;
    while (ticks < 200 && abs(wrapped(at - car.yaw)) > 0x38e4) at = race_follow_yaw(&chase, &car), ticks++;
    CHECK(ticks < 45);
    printf("and back in %d ticks\n", ticks);
    chase.port_look_back = 1;   /* the car turning while looked back: the camera stays behind it */
    car.yaw_rate = 0x600000;
    for (int i = 0; i < 150; i++) {
        car.yaw = (car.yaw + car.yaw_rate / 30) & 0xffffff;
        at = race_follow_yaw(&chase, &car);
    }
    CHECK(abs(wrapped(at - (car.yaw + 0x800000))) < 0x100000);
}

/* the look back is held only with the 360 camera, by the key or the pad's button; none, or another key, and it is not */
static void test_look_back_held(void)
{
    uint8_t keys[0x5c] = { 0 };
    CHECK_EQ_INT(race_look_back_held(1, keys, 0x5c, 0x30), 0);
    keys[0x30] = 1;
    CHECK_EQ_INT(race_look_back_held(1, keys, 0x5c, 0x30), 1);
    CHECK_EQ_INT(race_look_back_held(0, keys, 0x5c, 0x30), 0);   /* the 360 camera off: the key does nothing */
    CHECK_EQ_INT(race_look_back_held(1, keys, 0x5c, 0x31), 0);
    CHECK_EQ_INT(race_look_back_held(1, keys, 0x5c, 0), 0);      /* no key set */
    keys[0x30] = 0;
    keys[RACE_KEYS_PAD_LOOK_BACK] = 1;
    CHECK_EQ_INT(race_look_back_held(1, keys, 0x5c, 0), 1);      /* the pad's button */
    CHECK_EQ_INT(race_look_back_held(0, keys, 0x5c, 0), 0);
    CHECK_EQ_INT(race_look_back_held(1, keys, 0x5b, 0), 0);      /* a key table that stops before it */
}

int main(void)
{
    test_look_back_held();
    test_look_back();
    test_faces();
    test_follow();
    test_spin();
    test_direction();
    return TEST_REPORT();
}
