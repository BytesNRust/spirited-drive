#include "game/race/car_vertical.h"

#include "game/race/car_road.h"
#include "game/race/track_gravity.h"
#include "game/sys/fixed_math.h"

#include <string.h>

enum {
    GRAVITY = 0x9cf5c,          /* per second at tick_scale 0x10000, >> 14 */
    LANDING_GAP = 0x40,         /* this far above the road still counts as on it */
    LONG_DROP = 0x4000,         /* higher than this above the road is a jump */
    LONG_JUMP_TICKS = 10,
    VERY_LONG_JUMP_TICKS = 0x14,
    LANDING_NOTE = 0x54,
    LANDING_SHAKE = 5
};

static int32_t abs32(int32_t v)
{
    return v > 0 ? v : -v;
}

/* 00428aec: pitch and roll move a quarter of the way to the road's slope
 * under the car, lifted at the front by front_lift and at the side by
 * side_lift. */
static void ease_to_road(Car *car, int32_t front_lift, int32_t side_lift)
{
    const int32_t pitch = fx_atan2_10(-(car->road_forward[1] - front_lift), car->half_length) << 14;
    const int32_t roll = fx_atan2_10(-car->road_side[1] - side_lift, car->half_width) << 14;
    const int32_t pitch_step = abs32((pitch - car->pitch) / 4), roll_step = abs32((roll - car->roll) / 4);
    const int32_t dp = car->pitch - pitch;
    if (dp > pitch_step) {
        car->pitch -= pitch_step;
    } else if (dp < -pitch_step) {
        car->pitch += pitch_step;
    }
    const int32_t dr = car->roll - roll;
    if (dr > roll_step) {
        car->roll -= roll_step;
    } else if (dr < -roll_step) {
        car->roll += roll_step;
    }
}

/* Landing: the road slope becomes the car's attitude speed, and long jumps
 * add their fall. Returns the new vertical speed. */
static int32_t landing_speed(Car *car, const RaceState *race, int32_t gravity, int32_t *pitch, int32_t *roll)
{
    *pitch = fx_atan2_10(-car->road_forward[1], car->half_length) << 14;
    *roll = fx_atan2_10(-car->road_side[1], car->half_width);
    const int32_t from_pitch = (fx_sin((uint32_t)(*pitch >> 14)) >> 8) * (car->local_velocity_z >> 8);
    *roll <<= 14;
    const int32_t from_roll = (fx_sin((uint32_t)(*roll >> 14)) >> 8) * (car->local_velocity_x >> 8);
    int32_t vy = -from_pitch - from_roll;
    if (car->airborne_ticks > VERY_LONG_JUMP_TICKS) {
        const int slow = (race->track_flags & TRACK_FLAG_LOW_GRAVITY) && strcmp(race->track_name, "tr6") != 0;
        vy += (slow ? car->airborne_ticks / 6 : car->airborne_ticks / 4) * gravity;
    }
    return vy;
}

/* On the road (or just landed): settle the attitude, count down the landing
 * and sound it. */
static void touch_down(Car *car, RaceState *race, int32_t gravity)
{
    ease_to_road(car, 0, 0);
    int32_t pitch, roll;
    car->velocity_y = landing_speed(car, race, gravity, &pitch, &roll);
    if (car->airborne_ticks > LONG_JUMP_TICKS) {
        race_note(race, LANDING_NOTE);
        car->airborne_ticks = -2;
        car->pitch = pitch;
        car->roll = roll;
        if (car->player == race->player) race->landing_shake = LANDING_SHAKE;
        int32_t bounce = car->airborne_ticks - 7;
        if (bounce > 7) bounce = 7;
        car->suspension_x_index = 0x1d;
        car->suspension_x = -(bounce * 0x10000);
    }
    if (car->airborne_ticks < 0) {
        car->airborne_ticks++;
    } else {
        car->airborne_ticks = 0;
    }
    if (car->airborne_ticks == -1 && race->player == car->player && race->sounds.sound != NULL)
        race->sounds.sound(race->sounds.user, -1, 4, 1, abs32(car->velocity_y), 1, 0);
}

/* 00428bd8 */
void car_vertical(Car *car, RaceState *race, int on_road)
{
    car_road_contact(car, race, 0);
    race->last_node = car->node;
    const int32_t gravity = ((car->tick_scale >> 2) * GRAVITY) >> 14;
    const int32_t road_y = car->road_point[1];
    if (race->port_lunar_gravity) {   /* port: the moon's pull, a third of the unit and its half */
        car->velocity_y -= track_gravity(1, gravity + gravity / 2);
    } else {
        car->velocity_y -= gravity;
        if (!(race->track_flags & TRACK_FLAG_LOW_GRAVITY)) car->velocity_y -= gravity / 2;
    }
    const int32_t fallen = (((car->velocity_y / 4 + car->velocity_y) >> 12) * (car->tick_scale >> 4)) + car->y;
    if (!on_road) {
        car->y = road_y;
        car->velocity_y = 0;
        car->unknown_3e1 = 0;
        car->unknown_3d9 = 0;
        return;
    }
    int32_t y;
    if (fallen - LANDING_GAP > road_y) {
        y = fallen;
        if (fallen - road_y < LONG_DROP) {
            ease_to_road(car, 0, 0);
            if (car->on_ground == 0 && car->gear != -1) car->engaged = 1;
            car->on_ground = 1;
        } else {
            car->engaged = 0;
            car->on_ground = 0;
            const int32_t t = ++car->airborne_ticks;
            /* in the air the nose drops and the car rolls back level */
            ease_to_road(car, 0x3d7 * t, car->roll < 0 ? 0x28f * t : -0x28f * t);
        }
    } else {
        y = road_y;
        touch_down(car, race, gravity);
        if (car->on_ground == 0 && car->gear != -1) car->engaged = 1;
        car->on_ground = 1;
    }
    car->unknown_3dd = 0;
    car->unknown_3e1 = (fx_sin((uint32_t)(car->pitch >> 14)) >> 8) * 0x9cf;
    car->unknown_3d9 = car->unknown_3dd;
    if (car->velocity_y > 0) {
        car->unknown_3e1 = fx_mul_round(car->unknown_3e1, 0x2666);
    } else if (car->velocity_y < 0) {
        car->unknown_3e1 = fx_mul_round(car->unknown_3e1, 0x10000);
    }
    if (race->landing_shake != 0) race->landing_shake--;
    car->y = y;
}
