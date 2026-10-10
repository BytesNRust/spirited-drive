/* Physics model 3 (0046d79c): a computer car's tick. */
#include "game/race/ai_drive.h"

#include "game/race/ai_lanes.h"
#include "game/race/car_crash.h"
#include "game/race/car_progress.h"

#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

enum {
    RACER = 0x4,                  /* flags_174 */
    PLACE_BEHIND = 0x4,           /* flags_175: AI_PLACE_BEHIND >> 8 */
    ONCOMING = 0x10,              /* flags_175 */
    NO_SLIDE = 0x2,               /* flags_176 */
    STARTED = 2,                  /* progress from which a racer keeps its zone */
    SKID_DRIFT = 0x408,           /* skid_rear while drifting near the edge, else 0x401 */
    SKID_TURN = 0x401,
    SKID_FROM = 0xa0000,          /* |steer - slip_steer| */
    SKID_SPEED = 0x90000,
    SKID_SCALE = 20,
    SLIDE_FROM = 0x160000,        /* |steer|: the car slides out of control (0040db0c) */
    SLIDE_FROM_RACER = 0x200000,
    NOTE_COP_SLIDES = 0x52,
    SLIDE_TICKS = 0xa0,
    SLIDE_TICKS_RACER = 100,
    STEADY_STEER = 0xf0000,
    SETTLED_PROGRESS = 0x96,      /* below: no turning yet */
    TURN_LIMIT = 0x12c0000,
    TURN_STEP = 0x444,            /* yaw rate to steer per tick */
    TURN_EASE = 0x13333,
    TURN_EASE_RACER = 0x23333,
    TURN_BACK_SPEED = 0x30000,
    TURN_BACK = 9,
    TURN_SNAP = 0x30000,
    TURN_SETTLE = 0x20000,
    EDGE_SLOW_PROGRESS = 0x46,
    BARRIER_UNIT = 0x2000,
    STOP_BIAS = -0x40000,         /* lane_bias below: the car stops at the barrier */
    GRAVITY_STEP = 0x6838
};

static int32_t magnitude(int32_t v)
{
    return v > 0 ? v : -v;
}

/* The matrix from the road's axes (rows 0 and 2 turned round for oncoming
 * traffic), turned by the steer. */
static void face_road(Car *car)
{
    for (int i = 0; i < 9; i++) car->matrix[i] = car->road_axes[i];
    if (car->flags_175 & ONCOMING) {
        static const int flip[6] = { 0, 1, 2, 6, 7, 8 };
        for (int i = 0; i < 6; i++) car->matrix[flip[i]] = -car->road_axes[flip[i]];
    }
    if (car->steer != 0) {
        int32_t m[9], turn[9];
        for (int i = 0; i < 9; i++) m[i] = car->matrix[i];
        rm_turn_y(turn, car->steer);
        rm_mul(m, turn, m);
        for (int i = 0; i < 9; i++) car->matrix[i] = m[i];
    }
    car->ai_countdown = 0;
}

/* in the outermost lane on either side, or past them */
static int at_edge(int32_t slot, const TriNode *node, int or_past)
{
    const int32_t left = 3 - (node->flags[0] >> 4), right = (node->flags[0] & 0xf) + 4;
    return or_past ? slot <= left || slot >= right : slot == left || slot == right;
}

/* A skid sound for a hard turn at speed; past the slide limit the car goes
 * out of control (0040db0c). */
static void skid(Car *car, RaceState *race)
{
    car->skid_rear = 0;
    car->skid_front = 0;
    if (!ai_view_zone(race, car->node)) {
        car->steer = 0;
        car->slip_steer = 0;
        car->yaw_rate = 0;
    }
    const int32_t turn = magnitude(car->steer - car->slip_steer);
    if (turn > SKID_FROM && magnitude(car->ai_speed) > SKID_SPEED) {
        const TriNode node = race_node(race, car->node);
        car->skid_rear = at_edge(ai_lane_slot(race, car, &node), &node, 1) ? SKID_DRIFT : SKID_TURN;
        car->skid_front = turn * SKID_SCALE;
    }
    const int32_t limit = (car->flags_174 & RACER) || (car->flags_175 & PLACE_BEHIND) ? SLIDE_FROM_RACER : SLIDE_FROM;
    if (magnitude(car->steer) <= limit || (car->flags_176[0] & NO_SLIDE)) return;
    car_crash_slide(car, race);
    if (!(car->flags_174 & AI_COP)) return;
    if (ai_view_zone(race, car->node)) race_note(race, NOTE_COP_SLIDES);
    ai_release_cops(race, 0);
}

static void yaw_turn(Car *car)
{
    if (magnitude(car->yaw_rate) > TURN_LIMIT || magnitude(car->steer) > TURN_LIMIT) {
        car->steer = 0;
        car->yaw_rate = 0;
    }
    const int32_t step = fx_mul_round(car->yaw_rate, TURN_STEP);
    car->steer += step;
    int32_t m[9], turn[9];
    for (int i = 0; i < 9; i++) m[i] = car->matrix[i];
    rm_turn_y(turn, step);
    rm_mul(m, turn, m);
    for (int i = 0; i < 9; i++) car->matrix[i] = m[i];
}

/* The steer eased toward the slip steer by the yaw rate. */
static void ease_steer(Car *car, int racer)
{
    const int32_t ease = racer ? TURN_EASE_RACER : TURN_EASE;
    if (car->steer < car->slip_steer) {
        if (car->yaw_rate < 0) {
            car->yaw_rate += ease;
        } else if (magnitude(car->ai_speed) > TURN_BACK_SPEED) {
            car->yaw_rate = -(car->steer * TURN_BACK);
            if (car->slip_steer - TURN_SNAP < car->steer) {
                car->yaw_rate = 0;
                car->steer = car->slip_steer;
            }
        }
    } else if (car->steer > car->slip_steer) {
        if (car->yaw_rate > 0) {
            car->yaw_rate -= ease;
        } else if (magnitude(car->ai_speed) > TURN_BACK_SPEED) {
            car->yaw_rate = -(car->steer * TURN_BACK);
            if (car->slip_steer + TURN_SNAP > car->steer) {
                car->yaw_rate = 0;
                car->steer = car->slip_steer;
            }
        }
    } else if (magnitude(car->steer) < magnitude(car->slip_steer) + TURN_SETTLE) {
        car->yaw_rate = 0;
        car->steer = car->slip_steer;
    }
}

/* Past a barrier: stopped (a car biased far left) or kept off the edge;
 * out of sight it just stops turning. */
static void at_barrier(Car *car, RaceState *race)
{
    const TriNode node = race_node(race, car->node);
    const int32_t lateral = car->road_lateral;
    if (!(lateral < -(int32_t)(node.left_barrier * BARRIER_UNIT) || lateral > (int32_t)(node.right_barrier * BARRIER_UNIT)))
        return;
    if (!ai_view_zone(race, car->node)) {
        car->slip_steer = 0;
        car->steer = 0;
    } else if (car->lane_bias < STOP_BIAS) {
        car->ai_target_speed = 0;
        car->ai_speed = 0;
        car->velocity_x = car->velocity_y = car->velocity_z = 0;
    } else {
        ai_keep_off_edge(car);
    }
}

/* the step along the speed: 1/32 + 1/128 + 1/512 + 1/2048 of it, x the other way */
static int32_t step_of(int32_t v)
{
    return (v >> 5) + (v >> 7) + (v >> 9) + (v >> 11);
}

void ai_model_tick(Car *car, RaceState *race)
{
    const int racer = (car->flags_174 & RACER) != 0;
    const int32_t countdown = car->ai_countdown;
    if (racer) {
        const int32_t vx = magnitude(car->velocity_x), vz = magnitude(car->velocity_z);
        car->speed = vz < vx ? vx + (vz >> 2) : vz + (vx >> 2);
    }
    car->use_matrix = 1;
    if (!racer || car->progress < STARTED) ai_lane_update(car, race);
    if (car_progress(car, race)) {
        ai_road_frame(car, race);
        face_road(car);
    }
    if (car->steer == 0) {
        car->skid_rear = 0;
        car->skid_front = 0;
    } else {
        skid(car, race);
    }
    if (car->steer == car->slip_steer || magnitude(car->steer) >= STEADY_STEER || car->on_ground == 0)
        car->slide_timer = racer ? SLIDE_TICKS_RACER : SLIDE_TICKS;
    else if (car->slide_timer != 0)
        car->slide_timer -= 1;
    if (car->progress < SETTLED_PROGRESS) {
        car->yaw_rate = 0;
        car->steer = 0;
        car->slip_steer = 0;
    }
    if (car->yaw_rate != 0) yaw_turn(car);
    ease_steer(car, racer);
    if (car->ai_countdown == 0) ai_drive(car, race, countdown);
    car->ai_countdown -= 1;
    if (race->circuit == 0 && ai_view_zone(race, car->node)) {
        const TriNode node = race_node(race, car->node);
        if (at_edge(ai_lane_slot(race, car, &node), &node, 0) && car->progress > EDGE_SLOW_PROGRESS)
            car->ai_speed = (car->ai_speed >> 8) * 0xff;
    }
    at_barrier(car, race);
    if (car->on_ground == 0) car->velocity_y -= GRAVITY_STEP;
    car->x -= step_of(car->velocity_x);
    car->y += step_of(car->velocity_y);
    car->z += step_of(car->velocity_z);
    if (car->slide_timer != 0 && car->slide_timer != (racer ? SLIDE_TICKS_RACER : SLIDE_TICKS)) {
        const int32_t from[3] = { car->x - car->road_centre[0], car->y - car->road_centre[1], car->z - car->road_centre[2] };
        const int32_t up[3] = { car->road_axes[3], car->road_axes[4], car->road_axes[5] };
        const int32_t height = rm_dot(from, up) >> 8;
        car->x -= (up[0] >> 8) * height;
        car->y -= (up[1] >> 8) * height;
        car->z -= height * (up[2] >> 8);
    }
}
