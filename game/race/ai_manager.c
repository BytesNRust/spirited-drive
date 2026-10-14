#include "game/race/ai_manager.h"

#include "game/gfx/x86_int.h"

#include "game/race/ai_drive.h"
#include "game/race/ai_lanes.h"
#include "game/race/car_control.h"
#include "game/race/car_crash.h"
#include "game/sys/rigid_math.h"
#include "game/sys/fixed_math.h"

#include <string.h>

enum {
    ONCOMING = 0x10,            /* flags_175: drives against the first car's direction */
    TARGET_FLOOR = -0x14ccc,
    TARGET_CAP = 0x14ccc,
    PHYSICS_CRASHED = 4,
    PHYSICS_GONE = 6,
    ROOM_PER_NODE = 0x60000,
    CAUTION = 0x13333,
    RACER = 4,                  /* flags_174 */
    KEEPS_GAP = 4,              /* flags_174 and flags_175: the gap to a car ahead is at least 1 */
    ACTIVE = 4,                 /* flags_51a */
    REACH = 0x1b,               /* nodes ahead a car counts */
    BESIDE_AHEAD = 0x50000,     /* how far along the road a car on the next node must be */
    FIRST_PER_NODE = 0x50000,
    SECOND_PER_NODE = 0x1b6db,
    FAST = 0x140000,
    PULLING_OUT = 0xa0000,
    CLOSING_FACTOR = 0x50000,
    CLOSING_SCALE = 0x20000,
    CAR_SOUND_BASE = 5,          /* 004bc764 */
    CAR_SOUND_SPREAD = 15,       /* 004bc768 */
    POLICE_SWITCH = 13,          /* ai params: 0 keeps the police out */
    POLICE_AHEAD = 0x32,         /* nodes ahead of the first car the police car is placed */
    POLICE_SPOT = 1,             /* node +0x1a: the police car may be placed here */
    POLICE_FAR_SIDE = 2,         /* node +0x1a: at the left barrier */
    POLICE_STAYS = 4             /* node +0x1a: leaves +0x176 bit 1 clear */
};

enum {
    BLOCK_PROGRESS = 0x86,       /* progress past which a car blocks humans */
    BLOCK_PULL = 7 << 16,        /* what the wish gains toward the human's side */
    HORN_PACE = 0x35555,         /* about 0.83 node lengths (6.0) a tick */
    HORN_SPEED = 0xf0000,
    HUMAN_HELD = 1               /* physics model 1 */
};

enum {
    FINISH_PULL = 100 << 16,     /* past the finish, toward the grid place's side */
    FINISH_LEAD = 10,            /* nodes before its place the pull starts */
    PAST_FINISH = 2,             /* car_race_window */
    GEARS = 5,
    SHIFT_WINDOW = 0x8000,
    COAST_REVS = 3000,
    REDLINE_MARGIN = 200
};

static const int32_t COAST_TICKS[8] = { 3, 3, 4, 5, 4, 5, 4, 3 };  /* 004c5d00, by model */

enum {
    TRI_SPEED_TABLE = 0x15b0c,   /* 3 bytes per segment, the first the speed limit in units of 0x10000 */
    NO_PROFILE = 4,              /* track_flags: the profiles' scales are left out */
    PACE_BY_GAP_RACE = 23,       /* ai.params rows by the gap, race type 1 */
    PACE_BY_GAP_OTHER = 44,
    GAP_STEPS = 20,
    PACE_BASE = 0xcccc,
    CROWD_FAST = 0x13333,        /* the pace bounds with a human nearby */
    CROWD_SLOW = 0xcccc,
    EARLY_PROGRESS = 0x41,
    EARLY_BOOST = 0x3f0,         /* per node of progress before EARLY_PROGRESS */
    FIRST_CRAWLING = 0x20000,
    PHYSICS_FIRST_WATCH = 2
};

static const int32_t MODEL_PACE[8] = { 60293, 65536, 60948, 60948, 65536, 62259, 58982, 58982 };  /* 004c5ce0 */

enum {
    TRAFFIC_PACE = 18,           /* ai.params: averaged with the first car's speed for traffic */
    PACK_LEAD = 6,               /* ai.params: what racers and police may run above the first car */
    COP_CAR = 8,                 /* flags_174 */
    COP_CHASING = 4,             /* flags_175 */
    STOPS_LIMIT = 900,           /* ai.unknown_364 */
    MODEL_SLEEPING = 3,          /* physics: the speed is left alone */
    STOPPED = 2,                 /* flags_176[0] */
    FLAGS_KEPT = 0xbe1c,         /* flags_174/175 as a word */
    FLAGS_SET = 0x1e0
};

static const int32_t POLICE_ODDS[4] = { 32, 64, 96, 128 };  /* 004c5d30, in 128 */

/* The weight a lane loses for `gap` (16.16) of speed, by `per` per 1/256. */
static int32_t weight(int32_t gap, int32_t per)
{
    return (gap >> 8) * per;
}

void ai_weigh_lanes(Car *car, const int32_t first[3], const int32_t second[3], int32_t wish[3])
{
    const int32_t target = car->ai_target_speed, speed = car->ai_speed;
    if (!(car->flags_175 & ONCOMING)) {
        for (int i = 0; i < 3; i++) {
            if (first[i] < target) wish[i] += weight(target - first[i], -0xcd);
            if (second[i] < target) wish[i] += weight(target - second[i], -0x80);
            if (first[i] < speed) wish[i] += weight(speed - first[i], -0x100);
            if (second[i] < speed) wish[i] += weight(speed - second[i], -0x1a);
        }
        if (first[1] < target) car->ai_target_speed = first[1];
        if (car->ai_target_speed < TARGET_FLOOR) car->ai_target_speed = TARGET_FLOOR;
    } else {
        for (int i = 0; i < 3; i++) {
            if (target < first[i]) wish[i] += weight(first[i] - target, -0xcd);
            if (target < second[i]) wish[i] += weight(second[i] - target, -0x80);
            if (speed < first[i]) wish[i] += weight(first[i] - speed, -0x100);
            if (speed < second[i]) wish[i] += weight(second[i] - speed, -0x1a);
        }
        if (target < first[1]) car->ai_target_speed = first[1];
        if (car->ai_target_speed > TARGET_CAP) car->ai_target_speed = TARGET_CAP;
    }
}

static int32_t magnitude(int32_t v)
{
    return v < 1 ? -v : v;
}

/* Whether the car can pull out past `ahead` before closing on it. */
static int has_room(const Car *car, const RaceState *race, const Car *ahead)
{
    int32_t nodes = car->progress - ahead->progress;
    if (nodes < 0) nodes += race->segments * 4;
    nodes = magnitude(nodes);
    const int32_t closing = magnitude(car->ai_speed - ahead->ai_speed);
    const int32_t sideways = magnitude(car->ai_target_lateral - car->road_lateral);
    const int32_t steer = magnitude(ai_lane_speed(car, race, 1, car->ai_speed));
    const int32_t needed = fx_mul_round(fx_mul_round(sideways, closing), CAUTION);
    return needed < fx_mul_round(nodes * ROOM_PER_NODE, steer);
}

void ai_hold_lane_speed(Car *car, const RaceState *race, const Car *ahead, int32_t from, int32_t to,
                        const int32_t row[3])
{
    int32_t speed = row[1];
    if (to != from && ahead != NULL && ahead->physics != PHYSICS_CRASHED && has_room(car, race, ahead))
        speed = from < to ? row[2] : row[0];
    if (!(car->flags_175 & ONCOMING)) {
        if (speed < car->ai_target_speed) car->ai_target_speed = speed;
        if (car->ai_target_speed < TARGET_FLOOR) car->ai_target_speed = TARGET_FLOOR;
    } else {
        if (speed > car->ai_target_speed) car->ai_target_speed = speed;
        if (car->ai_target_speed > TARGET_CAP) car->ai_target_speed = TARGET_CAP;
    }
}

/* The car in place `at` of the road's order, or NULL past the table. */
static const Car *car_at(const RaceState *race, int32_t at)
{
    if (at < 0 || at >= RACE_CARS) return NULL;  /* port guard */
    const int32_t slot = race->by_node[at];
    if (slot < 0 || slot >= RACE_CARS) return NULL;  /* port guard */
    return race->cars[slot];
}

/* How far `other` is ahead of the car along the road's forward row. */
static int32_t along_road(const Car *car, const Car *other)
{
    return (car->road_axes[6] >> 8) * ((other->x - car->x) >> 8) +
           ((other->z - car->z) >> 8) * (car->road_axes[8] >> 8);
}

/* Slot `bit` gets `other` when it is in `occupied` and not yet found. */
static int take(const Car *other, uint32_t occupied, uint32_t bit, int32_t gap, uint32_t *found, int32_t *first,
                int32_t *second, const Car **ahead)
{
    if (!(occupied & bit) || (*found & bit)) return 0;
    *second = other->ai_speed + gap * SECOND_PER_NODE;
    *first = other->ai_speed + gap * FIRST_PER_NODE;
    *found |= bit;
    *ahead = other;
    return 1;
}

static int take_lanes(const Car *other, uint32_t occupied, int32_t lane, int32_t gap, uint32_t *found,
                      int32_t first[3], int32_t second[3], const Car *ahead[3])
{
    int taken = take(other, occupied, 1u << (lane & 31), gap, found, &first[1], &second[1], &ahead[1]);
    if (lane > 0) taken += take(other, occupied, 1u << ((lane - 1) & 31), gap, found, &first[0], &second[0], &ahead[0]);
    if (lane < 7) taken += take(other, occupied, 1u << ((lane + 1) & 31), gap, found, &first[2], &second[2], &ahead[2]);
    return taken;
}

static int counts(const Car *car, const Car *other)
{
    return other != NULL && other != car && (other->flags_51a & ACTIVE) && other->physics != PHYSICS_GONE;
}

static void halve(int32_t first[3], int32_t second[3], int i)
{
    first[i] >>= 1;
    second[i] >>= 1;
}

static int in_traffic(const Car *car, const RaceState *race)
{
    const int32_t from = race->racers + race->cops;
    return from <= car->index && car->index < from + race->traffic_setting;
}

static int32_t reach_of(const Car *car, const RaceState *race, const Car *other)
{
    if (!(car->flags_174 & RACER) || !(other->flags_175 & ONCOMING)) return REACH;
    const DriverProfile *profile = ai_driver_profile(race, car->index);
    return profile != NULL ? profile->oncoming_reach : REACH;  /* port guard */
}

void ai_cars_ahead(const Car *car, const RaceState *race, const uint32_t occupied[RACE_CARS], int32_t lane,
                   uint32_t *found, int32_t first[3], int32_t second[3], const Car *ahead[3])
{
    const int32_t count = race->car_count;
    int32_t at = 0;  /* the original leaves it unset when the car is not in the order */
    for (int32_t i = 0; i < count; i++)
        if (car_at(race, i) == car) at = i;
    const Car *last = NULL;  /* the last car looked at */
    int taken = 0;
    if (!(car->flags_175 & ONCOMING)) {
        for (int32_t k = 1, place = at + 1; k < count && taken != 3; k++, place++) {
            const int32_t i = place % count;
            const Car *other = last = car_at(race, i);
            if (!counts(car, other)) continue;
            int32_t gap = other->node - car->node;
            if (magnitude(gap) < 2 && along_road(car, other) < BESIDE_AHEAD) continue;
            if (gap < 0 && other->progress - car->progress > 0) gap = -gap;
            if (gap <= 0 || gap >= reach_of(car, race, other)) continue;
            gap -= 2;
            if (((car->flags_174 & KEEPS_GAP) || (car->flags_175 & KEEPS_GAP)) && gap < 1) gap = 1;
            taken += take_lanes(other, occupied[i], lane, gap, found, first, second, ahead);
        }
        if (in_traffic(car, race) && ahead[1] != NULL && ahead[1]->node - car->node < 3) {
            halve(first, second, 1);
            if (car->ai_target_speed <= ahead[1]->ai_speed) {
                halve(first, second, 0);
                halve(first, second, 2);
            }
        }
        if (car->index >= 0 && car->index < race->racers && ahead[1] != NULL && ahead[1]->node - car->node < 2 &&
            last != NULL && last->ai_speed > FAST) {  /* port guard: last */
            int32_t speed = last->ai_speed - (last->ai_speed >> 3);
            if (speed < 0) speed = 0x10000;
            first[1] = second[1] = speed;
        }
        const TriNode node = race_node(race, car->node);
        if (lane + 1 == (node.flags[0] & 0xf) + 4)
            halve(first, second, 2);
        else if (lane - 1 == 3 - (node.flags[0] >> 4))
            halve(first, second, 0);
    } else {
        for (int32_t place = count + at; place > at && taken != 3; place--) {
            const int32_t i = place % count;
            const Car *other = car_at(race, i);
            if (!counts(car, other)) continue;
            int32_t gap = other->node - car->node;
            if (gap < 0 && other->progress - car->progress > 0) gap = -gap;
            if (gap > 0 || -gap >= REACH) continue;
            gap += 2;
            taken += take_lanes(other, occupied[i], lane, gap, found, first, second, ahead);
        }
        if (ahead[1] != NULL && in_traffic(car, race) && car->node - ahead[1]->node < 3) {
            halve(first, second, 1);
            if (car->ai_target_speed >= ahead[1]->ai_speed) {
                halve(first, second, 0);
                halve(first, second, 2);
            }
        }
    }
    const Car *racer = ahead[1];
    if (car->ai_speed > PULLING_OUT && racer != NULL && racer->index >= 0 && racer->index < race->racers &&
        first[1] < first[0] && first[1] < first[2]) {
        const int32_t nodes = magnitude(car->node - racer->node);
        const int32_t closing = car->ai_speed - racer->ai_speed;
        if (nodes > 2 && nodes < 0xf && closing > 0) {
            const int32_t steer = magnitude(ai_lane_speed(car, race, 1, car->ai_speed));
            const int32_t needed = fx_mul_round(fx_mul_round(closing, CLOSING_FACTOR), CLOSING_SCALE);
            if (needed > fx_mul_round(nodes * ROOM_PER_NODE, steer)) first[1] = second[1] = 0;
        }
    }
}

void race_hold_car_sound(RaceState *race, int32_t slot, int32_t kind)
{
    if (race->car_sound_ticks != 0) return;
    if (slot >= 0 && slot < race->racers) {
        race->car_sound_slot = 1;
        race->car_sound_kind = 1;
    } else {
        race->car_sound_slot = slot;
        race->car_sound_kind = kind;
    }
    race->car_sound_ticks = race_rand_below(race, CAR_SOUND_SPREAD) + CAR_SOUND_BASE;
}

void car_push_spin(Car *car, RaceState *race, int32_t up, int32_t along, int32_t side)
{
    car_crash_hit(car, race);
    int32_t gain[3], loss[3], turn[3];
    for (int i = 0; i < 3; i++) {
        gain[i] = fx_mul_round(side, car->body_matrix[i]);
        loss[i] = fx_mul_round(up, car->body_matrix[3 + i]);
        turn[i] = fx_mul_round(along, car->body_matrix[6 + i]);
    }
    for (int i = 0; i < 3; i++) car->body_spin[i] = x_add(car->body_spin[i], gain[i]);
    for (int i = 0; i < 3; i++) car->body_spin[i] = x_add(car->body_spin[i], turn[i]);
    for (int i = 0; i < 3; i++) car->body_spin[i] = x_sub(car->body_spin[i], loss[i]);
    if (car->flags_174 & RACER) ai_release_cops(race, 0);
}

void ai_spin_out(Car *car, RaceState *race, int32_t up, int32_t along, int32_t side)
{
    car_push_spin(car, race, up, along, side);
    car_crash_kick(car, race);
}

static void place_police_car(RaceState *race, Car *cop, const Car *first, int32_t at, const TriNode *spot)
{
    cop->flags_176[0] &= 0xf7;
    const int32_t offset = (spot->unknown_1a & POLICE_FAR_SIDE) ? cop->body_extent_a - (spot->left_barrier << 13)
                                                                : (spot->right_barrier << 13) - cop->body_extent_a;
    const TriNode start = race_node(race, 0);
    const int32_t lane = (start.flags[0] >> 4) == 0 || !(spot->unknown_1a & POLICE_FAR_SIDE) ? 1 : 0;
    cop->flags_176[0] &= 0x8f;
    cop->zone = first->zone;
    cop->flags_175 &= 0xdb;
    cop->flags_176[0] &= 0xf7;
    race->unknown_4c5cd8 = 0;
    ai_respawn(cop, race, at, lane, offset, 0);
    cop->flags_51a |= ACTIVE;
    if (!(spot->unknown_1a & POLICE_STAYS)) cop->flags_176[0] |= 2;
}

void ai_place_police(RaceState *race)
{
    if (race->ai.params[POLICE_SWITCH] == 0 || race->unknown_4fb7d4 == 1) return;
    const Car *first = race->cars[0];
    const int32_t node = first->node;
    if (node > race->loop_nodes) ai_release_cops(race, 1);
    if (node <= race->police_node) return;
    Car *cop = race->cars[2];
    if (!(cop->flags_51a & ACTIVE)) {
        for (int32_t at = race->police_node + POLICE_AHEAD; at <= node + POLICE_AHEAD - 1; at++) {
            if (at >= (int32_t)race->tri->node_count) continue;
            const TriNode spot = race_node(race, at);
            if (!(spot.unknown_1a & POLICE_SPOT)) continue;
            const uint32_t chance = race_random(race);
            if ((int32_t)((chance & 0x7f00) << 8) <= POLICE_ODDS[(spot.unknown_1a & 0x18) >> 3] << 16)
                place_police_car(race, cop, first, at, &spot);
            break;
        }
    }
    race->police_node = node;
}

int32_t ai_road_speed(const Car *car)
{
    return (-car->velocity_x >> 8) * (car->road_axes[6] >> 8) + (car->road_axes[7] >> 8) * (car->velocity_y >> 8) +
           (car->road_axes[8] >> 8) * (car->velocity_z >> 8);
}

/* The horn's count on the car's body: up while it closes on `human` in its
 * lane faster than the gap allows, sounding once a draw is under it. */
static void horn_closing(Car *car, RaceState *race, const Car *human)
{
    const Car *second = race->second_car;
    const uint32_t speed = (uint32_t)magnitude(second != NULL ? second->ai_speed : 0);  /* port guard */
    const uint32_t limit = (uint32_t)fx_mul_halves(HORN_PACE, (uint32_t)(human->node - car->node) * 6u << 16);
    if (speed <= limit) return;
    const int32_t draw = (int32_t)(race_random(race) * 10 >> 16);
    car->unknown_148++;
    if (draw >= car->unknown_148) return;
    race_hold_car_sound(race, car->index, 0);
    car->unknown_148 = 0;
}

void ai_watch_humans(Car *car, RaceState *race, int32_t lane, int32_t wish[3], int32_t node, int32_t progress)
{
    for (int32_t i = 0; i < race->humans && i < RACE_PLAYERS; i++) {  /* port guard */
        const Car *human = race->cars[i];
        const TriNode at = race_node(race, human->node);
        const int32_t slot = ai_lane_slot(race, human, &at);
        const DriverProfile *profile = ai_driver_profile(race, car->index);
        if (human->node - 1 <= node && profile != NULL && node < human->node + profile->block_reach &&
            progress > BLOCK_PROGRESS && -along_road(car, human) >= profile->block_gap)
            wish[slot < lane ? 0 : slot == lane ? 1 : 2] += BLOCK_PULL;
        if (car->node < human->node && slot == lane && race->circuit == 0)
            horn_closing(car, race, human);
        else
            car->unknown_148 = 0;
        if ((human->node - car->node == 5 || race->cars[0]->node - car->node == 2) && car->ai_speed > HORN_SPEED &&
            human->physics == HUMAN_HELD)
            race_hold_car_sound(race, car->index, 0);
    }
}

void ai_finish_and_revs(Car *car, RaceState *race, int32_t lane, int32_t wish[3])
{
    const int32_t node = car->node, progress = car->progress;
    if (car_race_window(car, race) == PAST_FINISH) {
        const TriNode at = race_node(race, car->node);
        if (lane != 3 - (at.flags[0] >> 4) && lane != (at.flags[0] & 0xf) + 4) {
            const int32_t past = car_past_finish(car, race);
            const int32_t place = car->player >= 0 && car->player < RACE_PLAYERS ? race->grid[car->player] : 0;  /* port guard */
            if (past + FINISH_LEAD >= place / 2 * 2) wish[place % 2 != 0 ? 2 : 0] += FINISH_PULL;
        }
    }
    int32_t gear = 0;
    for (; gear < GEARS; gear++) {
        const int32_t over = car->ai_speed - car->pdn_1ac[gear];
        if (over > 0 && over < SHIFT_WINDOW && car->ai_speed >> 16 != 0 && car->coast_timer == 0)
            car->coast_timer = car->model >= 0 && car->model < 8 ? COAST_TICKS[car->model] : 0;  /* port guard */
        if (car->ai_speed < car->pdn_1ac[gear]) break;
    }
    int32_t revs = COAST_REVS;
    if (car->coast_timer < 1) {
        revs = fx_mul_round(fx_mul_round(car->ai_speed, car->pdn_1ac_inverse[gear]), car->pdn_1c4);
        revs = (revs >> 17) + (revs >> 18);
    }
    car->rpm += (revs - car->rpm) / 8;
    const int32_t redline = car->pdn_1c4 >> 16;
    if (redline - REDLINE_MARGIN < car->rpm) car->rpm = redline;
    ai_watch_humans(car, race, lane, wish, node, progress);
}

/* The track's speed byte for the node's segment, as 16.16. */
static int32_t segment_speed(const RaceState *race, int32_t node)
{
    const size_t at = TRI_SPEED_TABLE + 3 * (size_t)((uint32_t)node >> 2);
    return at < race->tri->size ? (int32_t)((uint32_t)race->tri->data[at] << 16) : 0;  /* port guard */
}

static int32_t traffic_pace(const Car *car, const RaceState *race)
{
    const Car *lead = race->cars[0];
    int32_t speed = segment_speed(race, car->node);
    if (lead->model >= 0 && lead->model <= 7) speed = fx_mul_round(speed, MODEL_PACE[lead->model]);
    return lead->crashed_hard != 0 ? 0 : speed;
}

/* The pace through the bend at the car's node, left in ai.bend and ai.curve_pace. */
static int32_t bend_pace(const Car *car, RaceState *race, const DriverProfile *profile)
{
    const TriNode here = race_node(race, car->node), before = race_node(race, car->node - 1);
    int32_t bend = magnitude(here.orientation * 0x400 - before.orientation * 0x400);
    race->ai.bend = bend;
    if (bend > 0x800000) bend = 0x1000000 - bend;
    const int32_t t = fx_mul_round(car->pdn_mass - bend, car->pdn_inverse_mass);
    race->ai.curve_pace = (t >> 4) + (t >> 3) + (t >> 6);
    if (profile != NULL) race->ai.curve_pace = fx_mul_round(race->ai.curve_pace, profile->bend_scale);
    return race->ai.curve_pace;
}

int32_t ai_target_pace(Car *car, RaceState *race)
{
    if (!(car->flags_174 & RACER)) return traffic_pace(car, race);
    const Car *first = race_car_by_handle(race, race->ai.first_car);
    if (first == NULL) return 0;  /* port guard */
    const int32_t progress = car->progress, first_progress = first->progress;
    const int32_t limit = segment_speed(race, car->node);
    const DriverProfile *profile = race->track_flags & NO_PROFILE ? NULL : ai_driver_profile(race, car->player);
    const int32_t mass = profile != NULL ? fx_mul_round(profile->mass_scale, car->pdn_18) : car->pdn_18;
    const int32_t curve = bend_pace(car, race, profile);
    const int32_t step = (progress - first_progress) / 10 + 10;
    race->ai.gap_step = step < 0 ? 0 : step > GAP_STEPS ? GAP_STEPS : step;
    const int32_t own = profile != NULL ? profile->pace_by_gap[race->ai.gap_step] : 0x10000;
    const int32_t row = race->race_type == 1 ? PACE_BY_GAP_RACE : PACE_BY_GAP_OTHER;
    int32_t scale = fx_mul_round(race->ai.params[row + race->ai.gap_step], own);
    if ((first->speed < FIRST_CRAWLING && first->physics == PHYSICS_FIRST_WATCH) ||
        (progress < first_progress && first->physics == PHYSICS_CRASHED && scale > 0x10000))
        scale = 0x10000;
    int32_t pace = fx_mul_round(mass, fx_mul_round(curve + PACE_BASE, scale));
    if (pace > CROWD_FAST && ai_human_near(race, car)) pace = CROWD_FAST;
    if (pace < CROWD_SLOW && ai_human_near(race, car)) pace = CROWD_SLOW;
    if (car->progress < EARLY_PROGRESS) pace = fx_mul_round(pace, (EARLY_PROGRESS - car->progress) * EARLY_BOOST + 0x10000);
    int32_t speed = fx_mul_round(limit, pace);
    if (car_race_window(car, race) == PAST_FINISH) {
        const int32_t place = car->player >= 0 && car->player < RACE_PLAYERS ? race->grid[car->player] : 0;  /* port guard */
        if (car_past_finish(car, race) >= place / 2 * 2) speed = 0;
    }
    return speed;
}

/* The node's point, read at the unmasked index as the original does. */
static TriVec32 node_point(const RaceState *race, int32_t node)
{
    TriNode n;
    if (tri_node(race->tri, (uint32_t)node, &n) != TRI_OK) memset(&n, 0, sizeof n);  /* port guard */
    return n.position;
}

/* Byte `k` of the track's speed table for the node's segment, as 16.16. */
static int32_t segment_byte(const RaceState *race, int32_t node, int k)
{
    const size_t at = TRI_SPEED_TABLE + 3 * (size_t)((uint32_t)node >> 2) + (size_t)k;
    return at < race->tri->size ? (int32_t)((uint32_t)race->tri->data[at] << 16) : 0;  /* port guard */
}

static int32_t across(const Car *car, const int32_t *v)
{
    return (car->road_axes[0] >> 8) * (v[0] >> 8) + (v[1] >> 8) * (car->road_axes[1] >> 8) +
           (v[2] >> 8) * (car->road_axes[2] >> 8);
}

static void place_on_road(Car *car, RaceState *race)
{
    const TriVec32 at = node_point(race, car->node);
    const int32_t d[3] = { car->x - at.x, car->y - at.y, car->z - at.z };
    if (car->use_matrix == 0) {
        ai_road_frame(car, race);
        rm_turn_xzy(car->matrix, car->pitch, car->roll, car->yaw);
    }
    car->road_lateral = (car->road_axes[1] >> 8) * (d[1] >> 8) + (car->road_axes[0] >> 8) * (d[0] >> 8) +
                        (d[2] >> 8) * (car->road_axes[2] >> 8);
    const int32_t a = magnitude(across(car, &car->matrix[0])), b = magnitude(across(car, &car->matrix[3])),
                  c = magnitude(across(car, &car->matrix[6]));
    car->unknown_084 = (car->body_extent_a >> 8) * (a >> 8) + (car->body_extent_b >> 8) * (b >> 8) +
                       (c >> 8) * (car->body_extent_c >> 8);
}

static void aim_speed(Car *car, RaceState *race, const Car *first, int32_t leader_progress)
{
    if (car->flags_175 & ONCOMING) {
        car->ai_target_speed = ai_scale_speed(car, -ai_leader_speed(car, race));
        return;
    }
    if (car->flags_174 & RACER) {
        car->ai_target_speed = first->ai_speed;
    } else {
        car->ai_target_speed = (race->ai.params[TRAFFIC_PACE] >> 1) + (first->ai_speed >> 1);
        car->ai_target_speed = ai_scale_speed(car, car->ai_target_speed);
    }
    if ((car->flags_174 & KEEPS_GAP) || (car->flags_175 & KEEPS_GAP)) {
        const int32_t lead = first->ai_speed + race->ai.params[PACK_LEAD];
        if (lead > car->ai_target_speed) car->ai_target_speed = lead;
        if ((car->flags_174 & COP_CAR) && (car->flags_175 & COP_CHASING)) {
            const int32_t ahead = car->progress - leader_progress;
            const Car *player = race->cars[0];
            if (ahead > 3 || (ahead >= -2 && race->ai.unknown_364 < STOPS_LIMIT &&
                              segment_byte(race, car->node, 2) < player->ai_speed))
                car->ai_target_speed = player->speed;
        }
    } else {
        const int32_t cap = ai_scale_speed(car, segment_byte(race, car->node, 2));
        if (cap < car->ai_target_speed) car->ai_target_speed = cap;
    }
    const int32_t pace = ai_target_pace(car, race);
    if (pace < car->ai_target_speed || (car->flags_174 & RACER)) car->ai_target_speed = pace;
}

void ai_car_update(Car *car, RaceState *race)
{
    const int32_t leader_progress = race->leader != NULL ? race->leader->progress : 0;  /* port guard */
    place_on_road(car, race);
    const Car *first = race_car_by_handle(race, race->ai.first_car);
    if (!(car->index >= 0 && car->index < race->humans) && first != NULL)  /* port guard: first */
        aim_speed(car, race, first, leader_progress);
    if (car->physics != MODEL_SLEEPING)
        car->ai_speed = car->physics == PHYSICS_CRASHED ? car_body_road_speed(car) : ai_road_speed(car);
    if (car->flags_176[0] & STOPPED) car->ai_target_speed = 0;
    const uint32_t flags = ((uint32_t)car->flags_174 | (uint32_t)car->flags_175 << 8) & FLAGS_KEPT;
    car->flags_174 = (uint8_t)(flags | FLAGS_SET);
    car->flags_175 = (uint8_t)((flags | FLAGS_SET) >> 8);
}
