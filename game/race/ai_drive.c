#include "game/race/ai_drive.h"

#include "game/race/ai_lanes.h"

#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

#include <string.h>

enum { EDGE_DRAG = 0x14000, EDGE_BACK = 0x10000 };  /* 0046d5c8 */

enum {
    CAR_RACER = 4,                /* flags_174: a racer; 8 a police car; neither: traffic */
    CAR_COP = 8,
    CAR_BACKWARDS = 0x10,         /* flags_175: drives against the road's direction */
    INTERVAL_TRAFFIC = 8,         /* ticks between two rounds, and the lane acceleration (8.8 of a unit) */
    INTERVAL_RACER = 4,
    INTERVAL_COP = 6,
    LANE_ACCEL_TRAFFIC = 0x2000,
    LANE_ACCEL_RACER = 0x4000,
    LANE_ACCEL_COP = 0x2aaa,
    SLIDE_FULL_RACER = 0x64,
    SLIDE_FULL_OTHER = 0xa0,
    PHYSICS_COP_WATCH = 2,        /* car +0x512 of the first car: the models that switch the catch-up off or on */
    PHYSICS_DRIVING = 3,
    PHYSICS_NO_CATCH_UP = 4,
    FIRST_BRAKE_TICK = 0x136,     /* no acceleration before this tick */
    LEAD_SLOW = 0x20000,
    CATCH_UP_RACE = 65,           /* ai.params: the scales by gap, race type 1 */
    CATCH_UP_OTHER = 86,
    CATCH_UP_STEPS = 20,
    BRAKE_SPEED_GAP = 0xa0000,
    BRAKE_LIGHT = 0x11,
    BRAKE_RELEASE = 4,
    SLIP_MIN = 0x60000,
    SLIP_MIN_SPEED = 0x70000,
    SLIP_DRAG_RACER = 0xccc,
    SLIP_DRAG_OTHER = 0x1745,
    SLIDE_GAP = 0x20000,
    SLIDE_GAP_LANE = 0x80000,
    LATERAL_STEP = 0xb333,
    TABLE_LAST = 99,
    TABLE_FIRST_SPEED = 0x640000,
    VERGE_UNIT = 0x2000,
    COAST_GRACE = -20,
    HEIGHT_LIMIT = 0x666,
    FLY_PROGRESS = 0x64,
    FLY_SPEED = 0x1b0000,
    TRAFFIC_LIMIT = 0x33333,
    TRAFFIC_RATE = 0x4000,
    WINDOW_BEHIND = 0x14,
    WINDOW_AHEAD = 0x36,
    LANE_SLOT_RIGHT = 4,          /* slot of the first lane right of the centre */
    LANE_ROW = 40,                /* words per row of ai.lane_width */
    TOP_SPEED_SHIFT = 16,
    TRI_SPEED_TABLE = 0x15b0c     /* 3 bytes per segment, the first the speed limit in units of 0x10000 */
};


static int32_t abs32(int32_t v)
{
    return v > 0 ? v : -v;
}

static int32_t mul32(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

static int is_racer(const Car *car)
{
    return (car->flags_174 & CAR_RACER) != 0;
}

int32_t ai_lane_speed(const Car *car, const RaceState *race, int32_t error, int32_t speed)
{
    int32_t limit = TRAFFIC_LIMIT, rate = TRAFFIC_RATE;
    if (is_racer(car)) {
        const DriverProfile *profile = ai_driver_profile(race, car->player);
        if (profile != NULL) {
            limit = profile->lane_speed_limit;
            rate = profile->lane_speed_rate;
        }
    }
    const int32_t v = fx_mul_round(rate, speed);
    if (v < limit) return error < 1 ? -v : v;
    return error < 1 ? -limit : limit;
}

int ai_in_window(const RaceState *race, const Car *human, const Car *other)
{
    const int32_t mask = (int32_t)(race->tri->ring_nodes - 1u), ring = race->segments * 4;
    const int32_t from = (human->node - WINDOW_BEHIND) & mask;
    int32_t length = ((human->node + WINDOW_AHEAD) & mask) - from;
    if (length < 0) length += ring;
    const int32_t at = from < other->node ? other->node - from : ring + (other->node - from);
    return at < length;
}

int ai_human_near(const RaceState *race, const Car *car)
{
    for (int32_t i = 0; i < race->humans; i++)
        if (ai_in_window(race, race->cars[i], car)) return 1;
    return 0;
}

/* The word of ai.lane_width at [row][column], read as the original does past the table. */
static int32_t lane_width_at(const RaceState *race, uint32_t row, uint32_t column)
{
    const size_t at = offsetof(AiState, lane_width) + 2 * (size_t)(row * LANE_ROW + column);
    int16_t w = 0;
    if (at + sizeof w <= sizeof race->ai) memcpy(&w, (const uint8_t *)&race->ai + at, sizeof w);
    return w;
}

/* 0046f3e8, twice inlined: which of the 8 slots across the road the car's
 * sideways position falls in (0..3 left of the centre, 4..7 right), by the
 * lane width at its node. Also leaves the width in ai.last_lane_width. */
int32_t ai_lane_slot(RaceState *race, const Car *car, const TriNode *node)
{
    const int32_t w = lane_width_at(race, node->flags[0] & 0xf, node->right_verge >> 3);
    const int32_t unit = (int32_t)((uint32_t)w << 8), p = car->road_lateral;
    race->ai.last_lane_width = unit;
    if (p < 0) {
        if (-unit < p) return 3;
        if (-(int32_t)((uint32_t)w << 9) < p) return 2;
        return mul32(unit, 3) * -1 < p;
    }
    if (p < unit) return 4;
    if ((int32_t)((uint32_t)w << 9) > p) return 5;
    return (mul32(unit, 3) <= p) + 6;
}

/* The top speed table byte of the track at the car's segment, as 16.16. */
static int32_t track_speed_limit(const RaceState *race, const Car *car)
{
    const size_t at = TRI_SPEED_TABLE + 3 * (size_t)((uint32_t)car->node >> 2);
    return at < race->tri->size ? (int32_t)((uint32_t)race->tri->data[at] << 16) : 0;
}

/* The share of the speed table's step that the first car's lead allows (the
 * catch-up scale by the gap in progress), or 0x10000. */
static int32_t catch_up_scale(const RaceState *race, const Car *car, const Car *first)
{
    int32_t step = (car->progress - first->progress) / 10 + 10;
    step = step < 0 ? 0 : step > CATCH_UP_STEPS ? CATCH_UP_STEPS : step;
    return race->ai.params[(race->race_type == 1 ? CATCH_UP_RACE : CATCH_UP_OTHER) + step];
}

/* The speed along the road moved towards the target (the cars drive on
 * with the wheels' turn); `up` is the original's direction flag. */
static void move_speed(Car *car, const RaceState *race, int32_t brake_step, int32_t accel, int32_t previous,
                       int may_accelerate, int32_t racer)
{
    if (!(car->flags_175 & CAR_BACKWARDS)) {
        if (car->ai_target_speed < car->ai_speed) {
            const int32_t speed = car->ai_speed - brake_step;
            car->ai_speed = speed > car->ai_target_speed ? speed : car->ai_target_speed;
            const Car *self = race_car_by_handle(race, car->unknown_088);
            if (racer && self != NULL && self->physics == PHYSICS_DRIVING &&
                car->ai_target_speed < car->ai_speed - BRAKE_SPEED_GAP)
                car->brake = BRAKE_LIGHT;
        } else if (previous == 0) {
            car->ai_speed -= brake_step;
        } else if (may_accelerate) {
            car->ai_speed += accel;
            if (car->ai_target_speed < car->ai_speed) car->ai_speed = car->ai_target_speed;
            if (racer) {
                car->brake -= BRAKE_RELEASE;
                if (car->brake < 0) car->brake = 0;
            }
        }
    } else if (car->ai_speed < car->ai_target_speed) {
        car->ai_speed += brake_step;
        if (car->ai_speed > car->ai_target_speed) car->ai_speed = car->ai_target_speed;
    } else if (previous == 0) {
        car->ai_speed += brake_step;
    } else if (may_accelerate) {
        car->ai_speed -= accel;
        if (car->ai_speed < car->ai_target_speed) car->ai_speed = car->ai_target_speed;
    }
}

/* The slide resistance: the speed falls by `drag` towards 0. */
static void slide_drag(Car *car, int32_t drag)
{
    if (drag == 0) return;
    if (abs32(car->ai_speed) < drag) {
        car->ai_speed = 0;
    } else if (car->ai_speed < 0) {
        car->ai_speed += drag;
    } else if (car->ai_speed > 0) {
        car->ai_speed -= drag;
    }
}

/* The sideways speed eased towards `want` (0046f3e8). */
static void ease_lateral(Car *car, int32_t want, int32_t elapsed)
{
    int32_t v = car->local_velocity_x;
    if (want > v) {
        const int32_t step = v > 0 ? abs32(want) >> 4 : LATERAL_STEP;
        v = (int32_t)((uint32_t)v + (uint32_t)mul32(step, elapsed));
        if (want < v) v = want;
    } else if (want < v) {
        const int32_t step = v < 0 ? abs32(want) >> 4 : LATERAL_STEP;
        v = (int32_t)((uint32_t)v - (uint32_t)mul32(step, elapsed));
        if (want > v) v = want;
    }
    car->local_velocity_x = v;
}

/* The factor of the sideways speed that becomes slip_steer, by speed / 5. */
static int32_t slip_factor(int32_t speed)
{
    static const int32_t FACTOR[12] = { 0x10000, 0x18ccc, 0x15999, 0x15999, 0x11999, 0x11999,
                                        0xcccc, 0xcccc, 0x8000, 0x8000, 0x4ccc, 0x4ccc };
    const int32_t k = (speed >> 16) / 5;
    return (uint32_t)k < 12 ? FACTOR[k] : 0;
}

void ai_drive(Car *car, RaceState *race, int32_t countdown)
{
    const int racer = is_racer(car);
    const Car *first = race_car_by_handle(race, race->ai.first_car);
    if (first == NULL) return;  /* the original would read through a wild pointer */
    int32_t bucket = abs32(car->ai_speed >> 16) + 1;
    if (bucket > TABLE_LAST) bucket = TABLE_LAST;
    int32_t accel = car->ai_speed_table[bucket];
    int32_t previous = bucket == 0 ? TABLE_FIRST_SPEED : car->ai_speed_table[bucket - 1];

    if (racer && !((first->speed < LEAD_SLOW && first->physics == PHYSICS_COP_WATCH) ||
                   first->physics == PHYSICS_NO_CATCH_UP)) {
        const int32_t scale = catch_up_scale(race, car, first);
        accel = fx_mul_round(accel, scale);
        previous = fx_mul_round(previous, scale);
    }
    int32_t brake_step = racer ? race->ai.racer_decel : 0xb333;
    if (car->on_ground == 0) {
        accel = 0;
        brake_step = 0;
    }
    if (race->tick < FIRST_BRAKE_TICK) accel = 0;

    int32_t interval = INTERVAL_TRAFFIC, lane_accel = LANE_ACCEL_TRAFFIC;
    if (car->flags_174 & CAR_COP) {
        interval = INTERVAL_COP, lane_accel = LANE_ACCEL_COP;
    } else if (racer) {
        interval = INTERVAL_RACER, lane_accel = LANE_ACCEL_RACER;
    }
    const int32_t elapsed = interval - countdown;

    int32_t drag = abs32(car->steer - car->slip_steer);
    if (drag <= SLIP_MIN || car->ai_speed <= SLIP_MIN_SPEED) {
        drag = 0;
    } else {
        drag = (drag >> 16) * (racer ? SLIP_DRAG_RACER : SLIP_DRAG_OTHER);
    }

    const TriNode here = race_node(race, car->node);
    int may_accelerate = 1;
    if (ai_view_zone(race, car->node) != 0) {
        const int32_t left = -(int32_t)((uint32_t)here.left_barrier << 13);
        const int32_t right = (int32_t)((uint32_t)here.right_barrier << 13);
        may_accelerate = car->road_lateral >= left && right >= car->road_lateral && car->coast_timer <= 0;
    }
    if (car->coast_timer > 0) {
        car->coast_timer--;
        if (car->coast_timer == 0) car->coast_timer = COAST_GRACE;
    }
    if (car->coast_timer < 0) car->coast_timer++;

    move_speed(car, race, mul32(brake_step, elapsed), mul32(accel, elapsed), previous, may_accelerate, racer);
    slide_drag(car, drag);

    if ((uint32_t)car->player < RACE_PLAYERS) {
        const int32_t top = race->players[car->player].top_speed;
        if (top < car->ai_speed && track_speed_limit(race, car) >= car->ai_speed)
            race->players[car->player].top_speed = car->ai_speed;
    }

    /* the road frame's side row times the sideways speed, and the direction of travel */
    const int32_t side[3] = { car->road_axes[0], car->road_axes[1], car->road_axes[2] };
    const int32_t full = racer ? SLIDE_FULL_RACER : SLIDE_FULL_OTHER;
    int32_t dir[3];
    if (car->slide_timer == 0 || car->slide_timer == full) {
        if (abs32(car->steer - car->slip_steer) < SLIDE_GAP) {
            memcpy(dir, &car->road_axes[6], sizeof dir);
        } else {
            for (int i = 0; i < 3; i++) dir[i] = car->matrix[6 + i];
            if (car->flags_175 & CAR_BACKWARDS)
                for (int i = 0; i < 3; i++) dir[i] = -dir[i];
        }
    } else {
        memcpy(dir, &car->road_axes[6], sizeof dir);
    }
    for (int i = 0; i < 3; i++) dir[i] = mul32(car->ai_speed >> 8, dir[i] >> 8);
    const int32_t speed = abs32(car->ai_speed);

    /* the lane: steer for the target distance from the centre */
    const int32_t error = car->ai_target_lateral - car->road_lateral;
    const int32_t slot = ai_lane_slot(race, car, &here);
    const int32_t lanes = here.flags[0] & 0xf;
    int steer_lane = slot == 3 - (here.flags[0] >> 4 & 0xf) || slot == lanes + LANE_SLOT_RIGHT;
    if (!steer_lane && error != 0 && abs32(car->steer - car->slip_steer) < SLIDE_GAP_LANE) steer_lane = 1;
    if (steer_lane) {
        int32_t want = ai_lane_speed(car, race, error, speed);
        if (abs32(error) < (abs32(mul32(want, interval)) >> 5))
            want = ((lane_accel >> 8) * (error >> 8)) << 5;
        ease_lateral(car, want, elapsed);
        for (int i = 0; i < 3; i++) dir[i] += fx_mul_round(side[i], car->local_velocity_x);
        car->slip_steer = car->flags_175 & CAR_BACKWARDS ? 0 : fx_mul_round(car->local_velocity_x, slip_factor(speed));
    }

    /* pull the car onto the road plane, unless a human is near and it is flying */
    const int32_t diff[3] = { car->x - car->road_centre[0], car->y - car->road_centre[1],
                              car->z - car->road_centre[2] };
    const int32_t height = rm_dot(diff, &car->road_axes[3]);
    if (height > HEIGHT_LIMIT && ai_human_near(race, car) && car->progress > FLY_PROGRESS && racer &&
        car->ai_speed > FLY_SPEED) {
        car->on_ground = 0;
    } else {
        if (car->on_ground == 0) car->on_ground = 1;
        car->x -= (car->road_axes[3] >> 8) * (height >> 8);
        car->y -= (car->road_axes[4] >> 8) * (height >> 8);
        car->z -= (car->road_axes[5] >> 8) * (height >> 8);
    }

    if (car->on_ground != 0) {
        if (car->slide_timer == 0 || car->slide_timer == full) {
            car->velocity_x = -dir[0];
            car->velocity_y = dir[1];
            car->velocity_z = dir[2];
        } else {
            car->velocity_y = dir[1];
        }
    }
    car->ai_countdown = interval;
}

void ai_keep_off_edge(Car *car)
{
    const int32_t side_x = -car->road_axes[0], side_z = car->road_axes[2];
    int32_t vx = car->velocity_x, vz = car->velocity_z;
    int32_t out = fx_mul_round((side_x >> 8) * (vx >> 8) + (vz >> 8) * (side_z >> 8), EDGE_DRAG);
    if ((car->road_lateral > 0 && out < 0) || (car->road_lateral < 0 && out > 0)) out = 0;
    vz -= fx_mul_round(out, side_z);
    vx -= fx_mul_round(out, side_x);
    const int32_t along = (side_z >> 8) * (vz >> 8) + (side_x >> 8) * (vx >> 8);
    if (car->road_lateral > 0 && along > -EDGE_BACK) {
        vx -= side_x;
        vz -= side_z;
    } else if (car->road_lateral < 0 && along < EDGE_BACK) {
        vx += side_x;
        vz += side_z;
    }
    car->velocity_x = vx;
    car->velocity_z = vz;
}
