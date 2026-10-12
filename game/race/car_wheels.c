#include "game/race/car_wheels.h"

#include "game/sys/fixed_math.h"

#include <stdint.h>

enum {
    TURNING = 0x1999,           /* +0x68 from which the wheels turn */
    ALTERNATE = 0xa0000,        /* below it frames 1 and 2 alternate each call */
    FAST = 0x1e0000,            /* above it frame 3 */
    MOVED = 0x1998,             /* |dx| + |dy| above it: the car moved */
    SLIDING = 1,                /* tyre flags (0042fb88) */
    PAST_GRIP = 2,
    FRAME_STILL = 0,
    FRAME_BLUR = 3,
    PHASE_4 = 4,                /* 004c5390 */
    HALF_TURN = 0x800000, TURN = 0x1000000,
    SIDE_FROM = 0xa0000, SIDE_TO = 0x760000,  /* the view angles a side's wheel is seen from */
    STEER_SHIFT = 32            /* +0x18 / 32: the weight */
};

/* The original's |v|: 0 - v for v < 1 (INT32_MIN stays). */
static int32_t magnitude(int32_t v)
{
    return v < 1 ? (int32_t)(0u - (uint32_t)v) : v;
}

static int32_t turning_frame(RaceState *race, int32_t speed)
{
    if (speed < TURNING) return FRAME_STILL;
    int32_t frame;
    if (race->wheel_tick == race->tick || race->phase == PHASE_4) {
        frame = 1;
    } else {
        frame = -race->wheel_counter;
        race->wheel_counter = frame + 1;
        if (speed < ALTERNATE) {
            if (race->wheel_counter != 0) race->wheel_toggle = 1 - race->wheel_toggle;
            frame = race->wheel_toggle + 1;
        } else {
            frame += 2;
        }
    }
    race->wheel_tick = race->tick;
    return frame;
}

static int32_t sliding_frame(uint8_t flags, int32_t frame)
{
    if (flags & SLIDING) return FRAME_STILL;
    return flags & PAST_GRIP ? FRAME_BLUR : frame;
}

/* Frame k's word; past the four, 0 (port guard: the original reads on in
 * the car record, which a 004c5810 other than 0 or 1 would make it do). */
static uint32_t frame_word(const Car *car, int32_t k)
{
    return (uint32_t)k < 4 ? car->wheel_frames[k] : 0;
}

void car_wheel_frames(Car *car, RaceState *race, CarBindTexture bind, void *user)
{
    const int32_t speed = magnitude(car->local_velocity_z);
    const int32_t frame = speed <= FAST ? turning_frame(race, speed) : FRAME_BLUR;
    const int32_t dx = (int32_t)((uint32_t)race->wheel_last_x - (uint32_t)car->x);
    const int32_t dy = (int32_t)((uint32_t)race->wheel_last_y - (uint32_t)car->y);
    int32_t front = frame;
    if (car->model == 1 || car->model == 10) {
        front = sliding_frame(car->tyre_flags_front, frame);
    } else if (!(car->tyre_flags_front & SLIDING) &&
               (int32_t)((uint32_t)magnitude(dy) + (uint32_t)magnitude(dx)) > MOVED) {
        if (car->tyre_flags_front & PAST_GRIP) front = FRAME_BLUR;
    } else {
        front = FRAME_STILL;
    }
    const int32_t rear = sliding_frame(car->tyre_flags_rear, frame);
    race->wheel_last_x = car->x;
    race->wheel_last_y = car->y;
    if (front != car->front_frame) {
        bind(user, car->body_object, car->front_tyres, frame_word(car, front));
        car->front_frame = front;
    }
    if (rear != car->rear_frame) {
        bind(user, car->body_object, car->rear_tyres, frame_word(car, rear));
        car->rear_frame = rear;
    }
}

/* A 24-bit angle above a half turn, one turn less. */
static int32_t signed_angle(int32_t a)
{
    return a > HALF_TURN ? a - TURN : a;
}

int32_t car_view_angle(const Car *car, const RaceState *race)
{
    const int32_t dx = (int32_t)((uint32_t)car->x - (uint32_t)race->chase.position[0]) >> 8;
    const int32_t dz = (int32_t)((uint32_t)car->z - (uint32_t)race->chase.position[2]) >> 8;
    const int32_t seen = signed_angle((int32_t)((uint32_t)fx_atan2(dx, dz) << 8));
    uint32_t d = (uint32_t)seen - (uint32_t)signed_angle(car->heading);
    if ((int32_t)d < 0) d += TURN;
    return signed_angle((int32_t)(d & (TURN - 1)));
}

void car_steer_wheels(const Car *car, const RaceState *race, CarMorphPart morph, void *user)
{
    const int32_t weight = car->steer / STEER_SHIFT;
    const int32_t seen = car_view_angle(car, race);
    const int32_t left = seen < SIDE_FROM || SIDE_TO < seen ? 0 : weight;
    const int32_t right = -SIDE_FROM < seen || seen < -SIDE_TO ? 0 : weight;
    morph(user, car->body_object, car->left_turn, -left);
    morph(user, car->body_object, car->right_turn, -right);
}
