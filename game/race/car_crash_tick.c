/* Physics model 4 (0040e050): a crashed car moved as its collision body,
 * against the road, its barriers and a tunnel roof, until it comes to rest.
 * The road frame is read with z the other way, as the body holds it. */
#include "game/race/car_crash.h"

#include "game/race/ai_drive.h"
#include "game/race/ai_lanes.h"
#include "game/race/car_progress.h"
#include "game/race/car_vertical.h"
#include "game/race/car_walls.h"
#include "game/race/crash_trail.h"
#include "game/race/rigid_body.h"

#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

enum {
    PHYSICS_DRIVING = 2,
    PHYSICS_COMPUTER = 3,
    PHYSICS_MODEL_5 = 5,          /* 0046fe34 */
    PHYSICS_MODEL_6 = 6,          /* 00470518 */
    FLAG_SMOKE = 2,               /* flags_175: the crash smoke runs (004591d4 draws the trail) */
    PHASE_RACING = 1,
    RPM_IDLE = 0x5dc,
    RPM_DROP = 200,
    RPM_RESTART = 700,
    /* the barriers: their distance from the centre in 1/8 units, plus a gap */
    BARRIER_UNIT = 0x2000,
    WALL_GAP = 0x10000,           /* 004b9088: no barrier on that side, or road property 5 */
    WALL_GAP_FAR = 0x34000,       /* 004b908c */
    PROPERTY_ROOF = 4,
    PROPERTY_OPEN = 5,
    ROOF_HEIGHT = 7,              /* times the up row */
    /* at rest */
    REST_SPEED = 0xcccc,          /* 004b9064 */
    REST_SPIN = 0x13333,          /* 004b9068 */
    LEVEL_SLACK = 0x8f5,          /* 004b906c */
    ONE = 0x10000,
    ON_SURFACE = 0x3333,          /* the deepest corner's distance: touching */
    STILL = 0x18000,
    UPRIGHT = 0xf70a,
    ON_SIDE = 0x8f5,
    FACING_BACK = 0xb333,
    ON_END = 0xe666,
    RETRY_TICKS = 0x3c,
    LONG_BODY = 0x30000,
    /* a player's crash */
    CRASH_STEP = 0x888,
    SMOKE_FIRST_STEP = 0x5555,
    SMOKE_SPEED = 0x80000,
    CAMERA_HOLD = 0x3c,
    CATCHING_SPEED = 0x90000,
    CATCH_NODES = 6,              /* per second of the coming car's speed */
    MESSAGE_TICKS = 0x1e,
    COUNT_SKIP_A = 2,             /* 00509d5c values that do not count a crash */
    COUNT_SKIP_B = 3,
    /* the sound */
    SCRAPE_SCALE = 0x1e,
    SCRAPE_MIN = 0x230000,
    SCRAPE_MAX = 0x2bc0000,
    TIPPED = 0xcccc,
    NOTE_LEVEL = 2,
    NOTE_TIPPED = 4,
    SOUND_KIND = 1,
    SOUND_SCALE = 0x14,           /* 00443fc0 */
    SOUND_GAP = 0x8000,
    SOUND_TICKS = 10,
    SOUND_HEAVY = 0x80000         /* body +0x60 at or above: no sound */
};

typedef struct {
    int32_t side[3], up[3], ahead[3], centre[3];
} CrashRoad;

static int32_t magnitude(int32_t v)
{
    return v > 0 ? v : -v;
}

static int32_t dot8(const int32_t a[3], const int32_t b[3])
{
    return (a[0] >> 8) * (b[0] >> 8) + (a[1] >> 8) * (b[1] >> 8) + (a[2] >> 8) * (b[2] >> 8);
}

static void read_road(const Car *car, CrashRoad *r)
{
    for (int i = 0; i < 3; i++) {
        r->side[i] = car->road_axes[i];
        r->up[i] = car->road_axes[3 + i];
        r->ahead[i] = car->road_axes[6 + i];
        r->centre[i] = car->road_centre[i];
    }
    r->side[2] = -r->side[2];
    r->up[2] = -r->up[2];
    r->ahead[2] = -r->ahead[2];
    r->centre[2] = -r->centre[2];
}

/* 0041b414 against one plane; the largest bounce so far is kept. */
static int32_t touch(RigidBody *b, const int32_t normal[3], const int32_t point[3], int32_t loudest, RbContact *last)
{
    rb_ground_contact(b, normal, point, last);
    return last->bounce > loudest ? last->bounce : loudest;
}

/* The barrier on the side the body is, the roof of a tunnel, the road.
 * Returns the hardest bounce; `last` keeps the road's contact. */
static int32_t meet_road(RigidBody *b, const RaceState *race, const Car *car, const CrashRoad *r, RbContact *last)
{
    const TriNode n = race_node(race, car->node);
    int32_t from[3], point[3], normal[3];
    for (int i = 0; i < 3; i++) from[i] = b->position[i] - r->centre[i];
    const int right = dot8(from, r->side) > 0;
    const int open = (right ? (n.flags[1] & 0xf) : (n.flags[1] >> 4)) == 0 || n.property == PROPERTY_OPEN;
    const int32_t gap = open ? WALL_GAP : WALL_GAP_FAR;
    const int32_t reach = (right ? n.right_barrier * BARRIER_UNIT + gap : -(n.left_barrier * BARRIER_UNIT) - gap) >> 8;
    for (int i = 0; i < 3; i++) {
        point[i] = reach * (r->side[i] >> 8) + r->centre[i];
        normal[i] = right ? -r->side[i] : r->side[i];
    }
    int32_t loudest = touch(b, normal, point, 0, last);
    if (n.property == PROPERTY_ROOF) {
        for (int i = 0; i < 3; i++) {
            point[i] = r->up[i] * ROOF_HEIGHT + r->centre[i];
            normal[i] = -r->up[i];
        }
        loudest = touch(b, normal, point, loudest, last);
    }
    return touch(b, r->up, r->centre, loudest, last);
}

/* A player's car leaves the controls as they are: turns from the body, no
 * pedals, neutral. */
static void controls_off(Car *car, const RigidBody *b)
{
    car_crash_read_turns(car, b->matrix);
    car->throttle = 0;
    car->brake = 0;
    car->rpm = car->rpm > RPM_DROP ? car->rpm - RPM_DROP : 0;
    car->local_velocity_z = 0;
    car->local_velocity_y = 0;
    car->local_velocity_x = 0;
    car->yaw_rate = 0;
    car->wheel_speed = 0;
    car->tyre_flags_rear = 0;
    car->tyre_flags_front = 0;
    car->engaged = 0;
    car->gear = -1;
    if (car->transmission != 0) car->transmission = 2;
    car->shift_timer = -1;
    car->saved_throttle = 0;
    car->stalled = 0;
    car->steer = 0;
}

/* Nearly still, slow turning, on the surface and level or on its side: the
 * motion halves. */
static void damp_at_rest(RigidBody *b, const CrashRoad *r, const RbContact *last)
{
    for (int i = 0; i < 3; i++)
        if (magnitude(b->velocity[i]) >= REST_SPEED) return;
    for (int i = 0; i < 3; i++)
        if (magnitude(b->spin[i]) >= REST_SPIN) return;
    if (last->depth >= ON_SURFACE) return;
    const int32_t level = dot8(&b->matrix[3], r->up);
    if (level <= ONE - LEVEL_SLACK && level >= LEVEL_SLACK - ONE && (level >= LEVEL_SLACK || level <= -LEVEL_SLACK))
        return;
    for (int i = 0; i < 3; i++) {
        b->velocity[i] = (b->velocity[i] >> 8) * 128;
        b->spin[i] = (b->spin[i] >> 8) * 128;
    }
}

static int all_still(const RigidBody *b)
{
    for (int i = 0; i < 3; i++)
        if (magnitude(b->velocity[i]) >= STILL || magnitude(b->spin[i]) >= STILL) return 0;
    return 1;
}

/* level: well up or down, or on its side */
static int lying(int32_t up)
{
    return up > UPRIGHT || up < -UPRIGHT || (up < ON_SIDE && up > -ON_SIDE);
}

static void settle(Car *car, RaceState *race, RigidBody *b)
{
    car_set_body(car, b);
    car_crash_settle(car, race);
    car_get_body(car, b);
}

/* A racer coming up behind within a second keeps the crash going a tick. */
static void watch_traffic(const Car *car, const RaceState *race, RigidBody *b)
{
    for (int i = 0; i < race->racers && i < RACE_PLAYERS; i++) {
        const Car *other = race->cars[i];
        if (i == car->player || other == NULL) continue;
        if (other->ai_speed <= CATCHING_SPEED || other->node > car->node) continue;
        const int32_t gap = (int32_t)((uint32_t)((car->node - other->node) * CATCH_NODES) << 16);
        if (fx_div(gap, other->ai_speed) < ONE) b->countdown = 1;
    }
}

/* The body set down upright on the road's axes, moved across to the middle
 * of the lanes. */
static void set_on_road(Car *car, const RaceState *race, RigidBody *b, const CrashRoad *r)
{
    for (int i = 0; i < 9; i++) b->matrix[i] = car->road_axes[i];
    b->matrix[2] = -b->matrix[2];
    b->matrix[5] = -b->matrix[5];
    b->matrix[6] = -b->matrix[6];
    b->matrix[7] = -b->matrix[7];
    int32_t from[3];
    for (int i = 0; i < 3; i++) from[i] = b->position[i] - r->centre[i];
    const TriNode n = race_node(race, car->node);
    const int32_t middle = (ai_lane_width(race, n.flags[0] & 0xf, n.right_verge) << 8) >> 1;
    const int32_t across = (middle - dot8(from, r->side)) >> 8;
    for (int i = 0; i < 3; i++) b->position[i] += (r->side[i] >> 8) * across;
}

static void restart_driving(Car *car, RaceState *race, const RigidBody *b)
{
    car_crash_read_turns(car, b->matrix);
    race->camera_hold = 0;
    crash_trail_clear(&race->trail);
    car->throttle = 0;
    car->brake = 0;
    car->rpm = RPM_RESTART;
    car->target_rpm = 0;
    car->local_velocity_z = car->local_velocity_y = car->local_velocity_x = 0;
    car->velocity_z = car->velocity_y = car->velocity_x = 0;
    car->yaw_rate = 0;
    car->wheel_speed = 0;
    car->tyre_flags_rear = 0;
    car->tyre_flags_front = 0;
    car->engaged = 0;
    car->gear = -1;
    car->shift_timer = -1;
    car->saved_throttle = 0;
    car->stalled = 0;
    car->steer = 0;
    car->flags_175 = (uint8_t)(car->flags_175 & ~FLAG_SMOKE);
    if (race->phase == PHASE_RACING) race->unknown_4b91c0 = MESSAGE_TICKS;
    if (car->transmission != 0) car->transmission = 2;
}

/* The crash is over: the cars around are checked again and the player's car
 * put back on the road, driving. */
static void back_on_road(Car *car, RaceState *race, RigidBody *b, const CrashRoad *r)
{
    car->crashed_hard = 0;
    car_set_body(car, b);
    ai_recheck_cars(race);
    car_get_body(car, b);
    set_on_road(car, race, b, r);
    restart_driving(car, race, b);
    car_set_body(car, b);
    car_crash_end(car, race, PHYSICS_DRIVING);
    car->use_matrix = 0;
    car_vertical(car, race, 0);
    car->yaw_rate = 0;
    car->local_velocity_z = car->local_velocity_y = car->local_velocity_x = 0;
    car->velocity_z = car->velocity_y = car->velocity_x = 0;
    car->speed = 0;
    if (race->phase == PHASE_RACING && race->unknown_509d5c != COUNT_SKIP_A && race->unknown_509d5c != COUNT_SKIP_B &&
        car->player < RACE_PLAYERS)
        race->players[car->player].crashes += 1;
    race->unknown_4c538c = 1;
}

/* Still crashing: once still and upright facing along the road it settles
 * by itself, else the count runs on. */
static void still_crashing(Car *car, RaceState *race, RigidBody *b, const CrashRoad *r)
{
    car->speed = magnitude(car->ai_speed);
    if (--b->countdown == 0) return;
    if (magnitude(b->velocity[1]) >= STILL || magnitude(b->spin[2]) >= STILL) return;
    if (magnitude(b->velocity[0]) < STILL && magnitude(b->velocity[2]) < STILL && magnitude(b->spin[0]) < STILL &&
        magnitude(b->spin[1]) < STILL) {
        b->countdown -= 1;
        const int32_t up = dot8(&b->matrix[3], r->up);
        const int32_t facing = dot8(&b->matrix[6], r->ahead);
        if (!lying(up)) return;
        if (up > UPRIGHT && -facing > FACING_BACK)
            settle(car, race, b);
        else if (b->countdown != 0)
            b->countdown -= 1;
        return;
    }
    if (dot8(&b->matrix[6], r->up) > ON_END && dot8(&b->matrix[3], r->up) > UPRIGHT) settle(car, race, b);
}

/* A player's hard crash: smoke, the camera held, and the end of it. Returns
 * 1 once the car is driving again. */
static int player_crash(Car *car, RaceState *race, RigidBody *b, const CrashRoad *r)
{
    if (car->flags_175 & FLAG_SMOKE) crash_trail_age(&race->trail, CRASH_STEP);
    const int32_t speed = magnitude(b->velocity[0]) + magnitude(b->velocity[1]) + magnitude(b->velocity[2]);
    car_set_body(car, b);
    /* crashed_hard is set on this path: the original's tests of it here always pass */
    if (car->flags_175 & FLAG_SMOKE) {
        crash_trail_drop(&race->trail, &race->trail_seed, car, CRASH_STEP);
    } else if (speed < SMOKE_SPEED) {
        car->flags_175 = (uint8_t)(car->flags_175 | FLAG_SMOKE);
        crash_trail_drop(&race->trail, &race->trail_seed, car, SMOKE_FIRST_STEP);
    }
    race->camera_hold = CAMERA_HOLD;
    if (b->countdown == 0) watch_traffic(car, race, b);
    if (b->countdown == 0) {
        back_on_road(car, race, b, r);
        return 1;
    }
    still_crashing(car, race, b, r);
    return 0;
}

/* A computer car (or a player's car in a light crash): handed to model 5
 * once upright or out of the players' sight (returns 1), else the count
 * starts again while it tumbles. */
static int computer_crash(Car *car, RaceState *race, RigidBody *b, const CrashRoad *r)
{
    const int32_t up = dot8(&b->matrix[3], r->up);
    if (b->countdown == 0 && (up > UPRIGHT || !ai_human_near(race, car))) {
        rm_set_up(b->matrix, r->up);
        if (car->flags_174 & AI_COP) ai_release_cops(race, 0);
        car_set_body(car, b);
        car_crash_end(car, race, PHYSICS_MODEL_5);
        car_get_body(car, b);
        return 1;
    }
    if (b->countdown != 0) {
        b->countdown -= 1;
        if (!all_still(b) || !lying(up)) {
            b->countdown = (b->countdown & 3) + RETRY_TICKS;
            if (car->flags_174 & AI_COP) ai_release_cops(race, 0);
        }
    }
    if (b->extents[2] > LONG_BODY && car->crashed_hard == 0) {
        rm_set_up(b->matrix, r->up);
        const int32_t turn = dot8(b->spin, r->up) >> 8;
        for (int i = 0; i < 3; i++) b->spin[i] = turn * (r->up[i] >> 8);
    }
    return 0;
}

/* The scrape along the road, and the crash's sound when it is louder than
 * the last one (0040d8e8 lets that fade). */
static void crash_sound(Car *car, RaceState *race, const RigidBody *b, const RbContact *last, int32_t loudest)
{
    if (last->depth >= ON_SURFACE) {
        car->skid_front = 0;
        return;
    }
    if (last->bounce > 0) {
        int32_t scrape = (magnitude(last->velocity[0]) + magnitude(last->velocity[2])) * SCRAPE_SCALE;
        if (scrape < SCRAPE_MIN) scrape = 0;
        if (scrape > SCRAPE_MAX) scrape = SCRAPE_MAX;
        car->skid_front = scrape;
    }
    const int32_t note = b->matrix[4] > TIPPED ? NOTE_TIPPED : NOTE_LEVEL;
    car->skid_rear = note << 8 | 1;
    if (race->crash_sound_level >= loudest) return;
    const int32_t at[3] = { car->x, car->y, car->z };
    int32_t angle, distance;
    race_sound_position(race, at, &angle, &distance);
    if (b->pdn_10 < SOUND_HEAVY && race->sounds.sound != NULL)
        race->sounds.sound(race->sounds.user, -1, note, SOUND_KIND, loudest * SOUND_SCALE, distance, angle);
    race->crash_sound_level = loudest + SOUND_GAP;
    race->crash_sound_ticks = SOUND_TICKS;
}

/* The car follows its body (z the other way, lifted off its base). */
static void follow_body(Car *car, RigidBody *b)
{
    const int32_t lift = car->unknown_0bc >> 8;
    car->x = b->position[0] - lift * (b->matrix[3] >> 8);
    car->y = b->position[1] - lift * (b->matrix[4] >> 8);
    car->z = -(b->position[2] - (b->matrix[5] >> 8) * lift);
    car->velocity_x = -b->velocity[0];
    car->velocity_y = b->velocity[1];
    car->velocity_z = -b->velocity[2];
    if ((b->countdown & 3) == 2 || b->countdown == 0) rb_orthonormalise(b->matrix);
    car_set_body(car, b);
    for (int i = 0; i < 9; i++) car->matrix[i] = b->matrix[i];
    car->matrix[2] = -car->matrix[2];
    car->matrix[5] = -car->matrix[5];
    car->matrix[6] = -car->matrix[6];
    car->matrix[7] = -car->matrix[7];
}

void car_crash_tick(Car *car, RaceState *race)
{
    /* +0x88 holds the car's own address */
    if (car->physics == PHYSICS_MODEL_6) {
        car_crash_end(car, race, PHYSICS_COMPUTER);
        return;
    }
    car->use_matrix = 1;
    if (car == race->second_car) car->rpm += (RPM_IDLE - car->rpm) / 8;
    if (car_progress(car, race)) ai_road_frame(car, race);
    CrashRoad r;
    read_road(car, &r);
    RigidBody b;
    car_get_body(car, &b);
    rb_step(&b);
    RbContact last = { 0, 0, { 0, 0, 0 } };
    const int32_t loudest = meet_road(&b, race, car, &r, &last);
    const int player = car->player >= 0 && car->player < race->humans;
    if (player) controls_off(car, &b);
    damp_at_rest(&b, &r, &last);
    if (player && car->crashed_hard != 0) {
        if (player_crash(car, race, &b, &r)) return;
    } else if (computer_crash(car, race, &b, &r)) {
        follow_body(car, &b);
        return;
    }
    crash_sound(car, race, &b, &last, loudest);
    follow_body(car, &b);
}
