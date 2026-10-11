/* The computer cars' manager, run every 4 ticks (0046c490). */
#include "game/race/ai_manager.h"

#include "game/race/ai_lanes.h"
#include "game/race/ai_police.h"
#include "game/race/ai_zones.h"
#include "game/sys/fixed_math.h"

enum {
    SLOTS = 8,                   /* lane slots across the road: 0..3 left of the centre, 4..7 right */
    POLICE_CAR = 8,              /* flags_174 */
    RACER = 4,                   /* flags_174 */
    ONCOMING = 0x10,             /* flags_175 */
    STOPPED = 2,                 /* flags_176[0] */
    KEEPS_GAP = 0x404,           /* flags_174 bit 2 or flags_175 bit 2, read as one word */
    ACTIVE = 4,                  /* flags_51a */
    PHYSICS_CRASHED = 4,
    PHYSICS_GONE = 6,
    MANAGER_OFF = 4,             /* track_flags: the police car is not placed */
    WEIGHT_UNIT = 0x10000,
    START_PULL = 0x1b580000,     /* the middle lane early in a race, off the shoulder */
    START_PROGRESS = 200,
    HUMAN_PULL = 0x460000,       /* a racer near the human at a race's start (race type 1) */
    HUMAN_NODES = 0x3c,
    HUMAN_GAP = 5,
    TYPE1_LEFT = 0x30000,        /* race type 1: lanes left of the centre lose this */
    TYPE1_RIGHT = 0x50000,       /* race type 1, oncoming: lanes right of the centre */
    LANES_AHEAD = 0xb,           /* nodes ahead (and behind) the lane count is compared at */
    MERGE_PULL = 0x280000,       /* toward the lanes a lane count change keeps */
    SLOW_FACTOR = 0x10000,       /* speed_factor under this: a slow car */
    SHOULDER_RACER = -0xf0000,
    SHOULDER_OTHER = -0x260000,
    NARROW_SHOULDER = 0x640000,  /* the barrier closer than the car's half width */
    OFF_ROAD = 0x3e80000,
    SPIN_GAP = 10,
    SPIN_KICK = 0x190000,
    BEHIND_FAST = 0xa0000,       /* the oncoming car ahead faster than this */
    BEHIND_PULL = 0x1e0000,
    SOUND_FLOOR = -0x30000,
    SOUND_ODDS = 0x23,           /* in 256 */
    EARLY_PROGRESS = 0x41,
    EARLY_RATE = 0x1f80,
    VERGE_UNIT = 0x2000,
    HALF_STEP = 0x8000
};

/* The lane width at the node (lane count node +4 & 0xf, verge node +1),
 * times 0x100: one lane slot's width. */
static int32_t slot_width(const RaceState *race, const TriNode *node)
{
    return (int32_t)((uint32_t)ai_lane_width(race, node->flags[0] & 0xf, node->right_verge) << 8);
}

/* The lane slot (0..7) of a distance `p` from the road's centre, `unit` wide. */
static uint32_t slot_of(int32_t p, int32_t unit)
{
    if (p < 0) {
        if (-unit < p) return 3;
        if (unit * -2 < p) return 2;
        return unit * -3 < p;
    }
    if (p < unit) return 4;
    if (p < unit * 2) return 5;
    return (unit * 3 <= p) + 6;
}

/* The slot just past the road's outermost lane on either side. */
static int at_shoulder(const TriNode *node, int32_t slot)
{
    return slot == 3 - (node->flags[0] >> 4) || slot == (node->flags[0] & 0xf) + 4;
}

/* A slot outside the shoulders. */
static int beyond(const TriNode *node, int32_t slot)
{
    return slot < 3 - (node->flags[0] >> 4) || (node->flags[0] & 0xf) + 4 < slot;
}

static uint32_t lanes_at(const RaceState *race, int32_t node)
{
    return race_node(race, node).flags[0] & 0xf;
}

/* First pass: each car's road state (0046ab24), then the slots its body
 * covers: `covered` per place in by_node, `counts` per slot. */
static void cover_slots(RaceState *race, uint32_t covered[RACE_CARS], int32_t counts[SLOTS + 1])
{
    for (int32_t i = 0; i < race->car_count && i < RACE_CARS; i++) {  /* port guard: 9 places */
        Car *car = race->cars[race_slot(race->by_node[i])];  /* port guard */
        if (car->physics == PHYSICS_GONE) continue;
        ai_car_update(car, race);
        const TriNode node = race_node(race, car->node);
        const int32_t unit = slot_width(race, &node);
        const uint32_t left = slot_of(car->road_lateral - car->unknown_084 + HALF_STEP, unit);
        race->ai.last_lane_width = unit;
        const uint32_t right = slot_of(car->road_lateral + car->unknown_084 - HALF_STEP, unit);
        if (right == left) {
            covered[i] = 1u << left;
            counts[right]++;
        } else if (right == left + 1) {
            covered[i] = 3u << left;
            counts[right]++;
            counts[left]++;
        } else {
            counts[right]++;
            covered[i] = 7u << left;
            counts[left]++;
            counts[left + 1]++;
        }
    }
}

/* The steering wish's start: the middle lane early in a race, or near the
 * human in race type 1, unless the car is on a shoulder. */
static void start_wish(const Car *car, const RaceState *race, const TriNode *node, int32_t slot, int32_t wish[3])
{
    if (car->flags_175 & ONCOMING) {
        wish[0] = WEIGHT_UNIT;
        wish[1] = 0;
        wish[2] = -WEIGHT_UNIT;
        return;
    }
    wish[0] = -WEIGHT_UNIT;
    wish[1] = 0;
    wish[2] = 0;
    const int32_t grid = (uint32_t)car->player < RACE_PLAYERS ? race->grid[car->player] : 0;  /* port guard */
    if (race->race_type > 1 && car->progress < START_PROGRESS && car->progress < grid * 5 + 100 &&
        !at_shoulder(node, slot)) {
        wish[1] = START_PULL;
        return;
    }
    if (race->race_type == 1 && (car->flags_174 & RACER) && car->node < HUMAN_NODES) {
        int32_t gap = car->node - race->cars[0]->node;
        if (gap < 1) gap = -gap;
        if (gap < HUMAN_GAP && !at_shoulder(node, slot)) wish[1] = HUMAN_PULL;
    }
}

/* A lone car in its own half, in a lane, the lane count steady ahead: it
 * keeps its slot without looking around. */
static int keeps_slot(const Car *car, const RaceState *race, const TriNode *node, int32_t slot,
                      const int32_t counts[SLOTS + 1], const Car *middle)
{
    if (middle != NULL || (node->flags[0] & 0xf) != lanes_at(race, car->node + LANES_AHEAD) || counts[slot] != 1)
        return 0;
    const int oncoming = (car->flags_175 & ONCOMING) != 0;
    if (!((oncoming && slot < 4) || (!oncoming && slot > 3))) return 0;
    return !at_shoulder(node, slot) && !beyond(node, slot) &&
           ((car->flags_174 | car->flags_175 << 8) & KEEPS_GAP) == 0;
}

/* Lane count changes: lanes ending ahead push toward the lanes that stay,
 * lanes added behind a slow car pull it over. */
static void merge_wish(const Car *car, const RaceState *race, const TriNode *node, int32_t slot, int32_t wish[3])
{
    const int32_t inner = 3 - (node->flags[0] >> 4), lanes = node->flags[0] & 0xf, outer = lanes + 4;
    if (!(car->flags_175 & ONCOMING) && (uint32_t)lanes > lanes_at(race, car->node + LANES_AHEAD)) {
        if (slot + 1 == inner || slot + 1 == outer) wish[0] += MERGE_PULL;
        else if (slot + 2 == inner || slot + 2 == outer) wish[1] += MERGE_PULL;
        return;
    }
    if ((car->flags_175 & ONCOMING) || car->node <= LANES_AHEAD || car->speed_factor >= SLOW_FACTOR) return;
    if (lanes_at(race, car->node - LANES_AHEAD) < (uint32_t)lanes) {
        if (slot + 1 == inner || slot + 1 == outer) wish[1] += MERGE_PULL;
        else wish[2] += MERGE_PULL;
    }
}

/* The shoulders and beyond weigh against a lane. */
static void edge_wish(const Car *car, const TriNode *node, int32_t slot, int32_t wish[3])
{
    const int32_t shoulder = (car->flags_174 & RACER) ? SHOULDER_RACER : SHOULDER_OTHER;
    const int32_t left_room = node->left_barrier * VERGE_UNIT - node->left_verge * VERGE_UNIT;
    const int32_t right_room = node->right_barrier * VERGE_UNIT - node->right_verge * VERGE_UNIT;
    if (at_shoulder(node, slot - 1)) {
        wish[0] += shoulder;
        if (left_room < car->body_extent_a) wish[0] -= NARROW_SHOULDER;
    }
    if (at_shoulder(node, slot)) {
        wish[1] += shoulder;
        if (right_room < car->body_extent_a) wish[1] -= NARROW_SHOULDER;  /* the slot is never negative: the right side */
    }
    if (at_shoulder(node, slot + 1)) {
        wish[2] += shoulder;
        if (right_room < car->body_extent_a) wish[2] -= NARROW_SHOULDER;
    }
    if (beyond(node, slot - 1)) wish[0] -= OFF_ROAD;
    if (beyond(node, slot)) wish[1] -= OFF_ROAD;
    if (beyond(node, slot + 1)) wish[2] -= OFF_ROAD;
}

/* The lane the wish picks, and the distance from the road's centre to
 * steer for: its middle, within the barriers less the car's half width,
 * plus the car's own bias. */
static uint32_t steer_for(Car *car, const RaceState *race, const TriNode *node, uint32_t slot, const int32_t wish[3])
{
    uint32_t to = slot;
    int32_t best = wish[1];
    if (wish[1] < wish[0]) {
        to = slot - 1;
        best = wish[0];
    }
    if (best < wish[2]) to = slot + 1;
    const int32_t unit = slot_width(race, node);
    car->ai_target_lateral = (int32_t)(to - 4) * unit + unit / 2;
    const int32_t right = node->right_barrier * VERGE_UNIT - car->body_extent_a;
    if (right < car->ai_target_lateral) car->ai_target_lateral = right;
    const int32_t left = car->body_extent_a + node->left_barrier * -VERGE_UNIT;
    if (car->ai_target_lateral < left) car->ai_target_lateral = left;
    if (car->flags_175 & ONCOMING) car->ai_target_lateral += car->lane_bias;
    else car->ai_target_lateral -= car->lane_bias;
    return to;
}

/* Second pass, one car: its wish over the slot it is in and the two
 * beside it, the cars ahead, then the lane to steer for and the speed. */
static void drive_car(Car *car, RaceState *race, const uint32_t covered[RACE_CARS],
                      const int32_t counts[SLOTS + 1])
{
    const int seen = ai_view_zone(race, car->node);
    uint32_t found = 0;
    if (!(car->flags_51a & ACTIVE) || (car->index >= 0 && car->index < race->humans) ||
        car->physics == PHYSICS_CRASHED)
        return;
    const TriNode node = race_node(race, car->node);
    const int32_t unit = slot_width(race, &node);
    const uint32_t slot = slot_of(car->road_lateral, unit);
    race->ai.last_lane_width = unit;
    int32_t wish[3];
    start_wish(car, race, &node, (int32_t)slot, wish);
    const Car *ahead[3] = { NULL, NULL, NULL };
    if (car->flags_174 & POLICE_CAR) ai_police_stop(car, race, (int32_t)slot, wish);
    if (car->flags_174 & RACER) ai_finish_and_revs(car, race, (int32_t)slot, wish);
    if (keeps_slot(car, race, &node, (int32_t)slot, counts, ahead[1]) || (car->flags_176[0] & STOPPED)) {
        car->ai_target_lateral = (int32_t)(slot - 4) * unit + unit / 2;
        return;
    }
    if (race->race_type == 1) {
        const int32_t side = (car->flags_175 & ONCOMING) ? -TYPE1_RIGHT : -TYPE1_LEFT;
        for (int32_t k = 0; k < 3; k++) {
            const int32_t lane = (int32_t)slot - 1 + k;
            if ((car->flags_175 & ONCOMING) ? lane > 3 : lane < 4) wish[k] += side;
        }
    }
    merge_wish(car, race, &node, (int32_t)slot, wish);
    edge_wish(car, &node, (int32_t)slot, wish);
    int32_t first[3], second[3];
    for (int32_t k = 0; k < 3; k++) first[k] = second[k] = car->ai_target_speed;
    ai_cars_ahead(car, race, covered, (int32_t)slot, &found, first, second, ahead);
    if (race->humans == 1 && race->unknown_4c5cd4 != 0) {
        int32_t gap = car->progress - race->cars[0]->progress;
        if (gap < 0) gap = -gap;
        if (race->cars[0]->look_back == 1 && gap < SPIN_GAP) ai_spin_out(car, race, SPIN_KICK, SPIN_KICK, SPIN_KICK);
    }
    ai_weigh_lanes(car, first, second, wish);
    if ((car->flags_175 & ONCOMING) && ahead[1] != NULL && BEHIND_FAST < ahead[1]->ai_speed) wish[2] -= BEHIND_PULL;
    if (((car->flags_174 | car->flags_175 << 8) & KEEPS_GAP) == 0 && !(car->flags_174 & POLICE_CAR) &&
        (car->flags_175 & ONCOMING) && seen && SOUND_FLOOR < first[1]) {
        if ((race_random(race) & 0xff) < SOUND_ODDS && car->ai_speed != 0 && race->unknown_4b92d4 == 0)
            race_hold_car_sound(race, car->index, car->pdn_1c8);
    }
    const uint32_t to = steer_for(car, race, &node, slot, wish);
    if (race->ai.unknown_358 != 0 && to != slot)
        ai_hold_lane_speed(car, race, ahead[1], (int32_t)slot, (int32_t)to, first);
    if ((car->flags_174 & RACER) && car->progress < EARLY_PROGRESS)
        car->ai_target_speed = fx_mul_round(car->ai_target_speed, (EARLY_PROGRESS - car->progress) * EARLY_RATE + 0x10000);
}

void ai_manager_tick(RaceState *race)
{
    if (race->unknown_4c5cdc != 0) return;
    race->ai.unknown_364 += 4;
    ai_zone_spans(race);
    int32_t best = 0;
    for (int32_t i = 0; i < race->humans && i < RACE_PLAYERS; i++) {  /* port guard: 8 cars */
        const int32_t progress = race->cars[i]->progress;
        if (best < progress) {
            best = progress;
            race->ai.unknown_344 = i;
            race->ai.first_car = race->cars[i]->unknown_088;
        }
    }
    if (!(race->track_flags & MANAGER_OFF)) ai_place_police(race);
    int32_t counts[SLOTS + 1] = { 0 };          /* [8]: the original's counts reach one past, into a row it resets */
    uint32_t covered[RACE_CARS] = { 0 };     /* port guard: the original leaves gone cars' words unset; never read */
    cover_slots(race, covered, counts);
    ((Car *)race->leader)->flags_175 &= 0xf3;   /* the leader is one of race->cars */
    for (int32_t i = 0; i < race->car_count && i < RACE_CARS; i++) {
        Car *car = race->cars[race_slot(race->by_node[i])];  /* port guard */
        if (car->physics != PHYSICS_GONE) drive_car(car, race, covered, counts);
    }
}
