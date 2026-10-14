#include "game/race/car_held.h"

#include "game/race/ai_lanes.h"
#include "game/sys/fixed_math.h"

#include <string.h>

enum {
    PHYSICS_DRIVING = 2,
    GEAR_NEUTRAL = -1,
    AUTOMATIC_DRIVE = 2,
    PULLED_OVER = 1,          /* flags_176 */
    POLICE_SLOT = 2,          /* 00509e24 */
    STEER_MOST = 0x140000,
    HALF_TURN = 0x800000,
    TURN = 0x1000000,
    NOTICE_TICK_UP = -1000,   /* 004b92e4 while a notice is up */
    BARRIER_UNIT = 0x2000,    /* node barrier bytes to road units */
    ANGLE_UNIT = 0x400        /* node angles (0x4000 a turn) to car angles */
};

void race_stop_notice_first(RaceState *race)
{
    race->stop_notice[0] = 1;
    race->stop_notice_tick = NOTICE_TICK_UP;
}

void race_stop_notice_second(RaceState *race)
{
    race->stop_notice[1] = 1;
    race->stop_notice_tick = NOTICE_TICK_UP;
}

int32_t race_stop_notice_up(const RaceState *race)
{
    return race->stop_notice[0] != 0 || race->stop_notice[1] != 0;
}

void race_stop_notices_clear(RaceState *race)
{
    if (race->stop_notice[0] != 0 || race->stop_notice[1] != 0) race->stop_notice_tick = race->tick;
    race->stop_notice[0] = 0;
    race->stop_notice[1] = 0;
}

void car_held_tick(Car *car, RaceState *race)
{
    /* the body's +0x84 countdown; +0x88 holds the car's own address */
    car->unknown_148 -= 1;
    if (car->unknown_148 > 0) return;
    car->physics = PHYSICS_DRIVING;
    race_stop_notices_clear(race);
    car->engaged = 0;
    car->gear = GEAR_NEUTRAL;
    if (car->transmission != 0) car->transmission = AUTOMATIC_DRIVE;
    race->unknown_4c538c = 1;
}

void car_pulled_over(Car *car, const RaceState *race)
{
    if (!(car->flags_176[0] & PULLED_OVER)) return;
    const Car *police = race->cars[POLICE_SLOT];
    if (police == NULL) return;  /* port guard: the original reads cars[2] unchecked */
    /* 0042ebbc(dz, dx): the heading to the police car, less the car's own */
    int32_t turn = (int32_t)((uint32_t)((fx_atan2(police->x - car->x, police->z - car->z) << 8) - car->yaw) & 0xffffff);
    if (turn > HALF_TURN) turn -= TURN;
    car->steer = (car->steer >> 1) + (turn >> 1);
    if (car->steer > STEER_MOST)
        car->steer = STEER_MOST;
    else if (car->steer < -STEER_MOST)
        car->steer = -STEER_MOST;
    car->throttle = 0;
}

void car_beside_road(Car *car, RaceState *race, int32_t right)
{
    const int32_t x = car->x, y = car->y, z = car->z;
    const int32_t ax = car->road_axes[0], ay = car->road_axes[1], az = car->road_axes[2];
    const int32_t cx = car->road_centre[0], cy = car->road_centre[1], cz = car->road_centre[2];
    const TriNode n = race_node(race, car->node);
    const int32_t lanes = n.flags[0] & 0xf, w = ai_lane_width(race, lanes, n.right_verge);
    int32_t to;
    if (right == 0) {
        to = w * -0x100 * (n.flags[0] >> 4) - (w * 0x100) / 2;
        const int32_t edge = car->body_extent_a + n.left_barrier * -BARRIER_UNIT;
        if (to < edge) to = edge;
    } else {
        const int32_t unit = w * 0x100;
        to = unit / 2 + lanes * unit;
        const int32_t edge = n.right_barrier * BARRIER_UNIT - car->body_extent_a;
        if (edge < to) to = edge;
    }
    memcpy(car->matrix, car->road_axes, sizeof car->matrix);
    car->yaw_rate = 0;
    car->yaw = n.orientation * ANGLE_UNIT;
    int32_t pitch = n.slope * ANGLE_UNIT;
    if (pitch > HALF_TURN) pitch -= TURN;
    else if (pitch < -HALF_TURN) pitch += TURN;
    car->pitch = -pitch;
    car->local_velocity_z = 0;
    car->roll = n.slant_a * ANGLE_UNIT;
    car->velocity_x = car->velocity_y = car->velocity_z = car->local_velocity_x = 0;
    const int32_t d = (to - ((ax >> 8) * ((x - cx) >> 8) + (ay >> 8) * ((y - cy) >> 8) + ((z - cz) >> 8) * (az >> 8))) >> 8;
    car->x += (ax >> 8) * d;
    car->y += (ay >> 8) * d;
    car->z += d * (az >> 8);
}
