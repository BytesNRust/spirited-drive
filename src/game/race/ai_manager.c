#include "game/race/ai_manager.h"

#include "game/race/ai_drive.h"
#include "game/race/ai_lanes.h"
#include "game/sys/fixed_math.h"

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
    CAR_SOUND_SPREAD = 15        /* 004bc768 */
};

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
    if (at < 0 || at >= RACE_PLAYERS) return NULL;  /* port guard */
    const int32_t slot = race->by_node[at];
    if (slot < 0 || slot >= RACE_PLAYERS) return NULL;  /* port guard */
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

void ai_cars_ahead(const Car *car, const RaceState *race, const uint32_t occupied[RACE_PLAYERS], int32_t lane,
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
