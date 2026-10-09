#include "game/race/ai_lanes.h"

#include "game/race/car_progress.h"
#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

#include <string.h>

enum {
    ZONES = 2,
    ZONE_WORDS = 6,             /* the words of a zone record at ai.zones */
    ZONE_SLOT = 0,
    ZONE_KEEP_END = 1,
    ZONE_KEEP_START = 2,
    ZONE_VIEW_END = 4,
    ZONE_VIEW_START = 5,
    LANE_ROW_WORDS = 40,        /* ai.lane_width: a row of 0x50 bytes per lane count */
    VERGE_UNIT = 0x2000,
    COP_STAGE_SHIFT = 20,
    PLACE_AHEAD_SPREAD = 11,    /* 0..10 nodes beyond the view span */
    PLACE_BEHIND_SPREAD = 0xf,
    SLOW_LEADER = 0x10000,
    GRID_NODE = 0x20,           /* the start line's node; grid rows are behind it */
    GRID_NODE_A = 10,           /* nodes whose widths and kinds set the grid's sides */
    GRID_NODE_B = 0x14,
    OPPONENT_CAR_MODEL = 9,     /* entries[1].car: this model skips the grid */
    PHYSICS_AI = 6,             /* car +0x512 */
    SLIDE_OPPONENT = 0x64,
    SLIDE_TRAFFIC = 0xa0,
    BEHIND_FIRST_CAR = 0xc,     /* nodes behind the first car for a car placed behind */
    BEHIND_FIRST_CAR_COP = 0x12,
    PARAM_COUNTDOWN = 12,       /* ai.params */
    PARAM_FACTORS = 14,
    PARAM_SPEED = 18,
    PARAM_BIAS = 19,
    SETTLED_CLEAR = 0xefef
};

static const uint32_t FLAGS_PLAYER_CLEAR = 0xfffedbffu;  /* what ends a police chase on the first car */
static const uint32_t FLAGS_COP_CLEAR = 0xff87dbffu;
static const uint32_t FLAGS_RESPAWN_CLEAR = 0xfffd7fffu;

/* The flag bytes at +0x174..+0x177 as one word. */
static uint32_t flags_of(const Car *car)
{
    return (uint32_t)car->flags_174 | (uint32_t)car->flags_175 << 8 | (uint32_t)car->flags_176[0] << 16 |
           (uint32_t)car->flags_176[1] << 24;
}

static void set_flags(Car *car, uint32_t flags)
{
    car->flags_174 = (uint8_t)flags;
    car->flags_175 = (uint8_t)(flags >> 8);
    car->flags_176[0] = (uint8_t)(flags >> 16);
    car->flags_176[1] = (uint8_t)(flags >> 24);
}

static int32_t node_mask(const RaceState *race)
{
    return (int32_t)(race->tri->ring_nodes - 1u);
}

/* A word of the ai block by byte offset; 0 past its end (the original
 * reads whatever follows). */
static int32_t ai_word(const RaceState *race, size_t offset, size_t size)
{
    int16_t half = 0;
    int32_t whole = 0;
    if (offset + size > sizeof race->ai) return 0;
    if (size == 2) {
        memcpy(&half, (const uint8_t *)&race->ai + offset, 2);
        return half;
    }
    memcpy(&whole, (const uint8_t *)&race->ai + offset, 4);
    return whole;
}

/* ai.zones[zone * 6 + word] */
static int32_t zone_word(const RaceState *race, int32_t zone, int32_t word)
{
    const int64_t at = (int64_t)offsetof(AiState, zones) + ((int64_t)zone * ZONE_WORDS + word) * 2;
    return at < 0 ? 0 : ai_word(race, (size_t)at, 2);
}

/* The car the zone follows (its first word), or NULL for a slot that is not one. */
static Car *zone_car(const RaceState *race, int32_t zone)
{
    const int32_t slot = zone_word(race, zone, ZONE_SLOT);
    return slot >= 0 && slot < RACE_PLAYERS ? race->cars[slot] : NULL;
}

/* ai.lane_width[lanes * 40 + (verge * 0x2000 >> 16)], read as the original
 * does from 0052b336 (past the table when the index runs on). */
static int32_t lane_width(const RaceState *race, int32_t lanes, int32_t verge)
{
    const int32_t at = lanes * LANE_ROW_WORDS + ((verge * VERGE_UNIT) >> 16);
    const int64_t offset = (int64_t)offsetof(AiState, lane_width) + (int64_t)at * 2;
    return offset < 0 ? 0 : ai_word(race, (size_t)offset, 2);
}

/* The driver profile an ai.profiles word points at. */
static const DriverProfile *profile_of(const RaceState *race, int32_t slot)
{
    if (slot < 0 || slot >= RACE_PLAYERS) return NULL;
    const uint32_t handle = race->ai.profiles[slot];
    if (race->driver_profiles == NULL || handle < race->driver_profiles_base) return NULL;
    const uint32_t index = (handle - race->driver_profiles_base) / sizeof(DriverProfile);
    return index < (uint32_t)race->driver_profile_count ? &race->driver_profiles[index] : NULL;
}

/* The car a handle at +0x88 names: itself. */
static Car *handle_car(const RaceState *race, Car *car)
{
    Car *named = race_car_by_handle(race, car->unknown_088);
    return named != NULL ? named : car;
}

static int span_holds(const RaceState *race, int32_t node, int32_t end, int32_t start)
{
    const int32_t ring = race->segments * 4;
    int32_t length = end - start;
    if (length < 0) length += ring;
    int32_t from = node - start;
    if (node <= start) from += ring;
    return from < length;
}

int ai_in_keep_span(const RaceState *race, int32_t node, int32_t zone)
{
    return span_holds(race, node, zone_word(race, zone, ZONE_KEEP_END), zone_word(race, zone, ZONE_KEEP_START));
}

int ai_in_view_span(const RaceState *race, int32_t node, int32_t zone)
{
    return span_holds(race, node, zone_word(race, zone, ZONE_VIEW_END), zone_word(race, zone, ZONE_VIEW_START));
}

int ai_keep_zone(const RaceState *race, int32_t node)
{
    const int32_t masked = node & node_mask(race);
    for (int32_t zone = 0; zone < ZONES; zone++)
        if (ai_in_keep_span(race, masked, zone)) return zone + 1;
    return 0;
}

int ai_view_zone(const RaceState *race, int32_t node)
{
    const int32_t masked = node & node_mask(race);
    if (race->racers > 2) return 1;
    for (int32_t zone = 0; zone < ZONES; zone++)
        if (ai_in_view_span(race, masked, zone)) return zone + 1;
    return 0;
}

void ai_release_cops(RaceState *race, int32_t clear_flag)
{
    if (race->cops != 1) return;
    Car *player = race->cars[0], *cop = race->cars[2];
    if (clear_flag) cop->flags_176[0] &= 0xf7;
    cop->flags_175 &= 0xdb;
    set_flags(player, flags_of(player) & FLAGS_PLAYER_CLEAR);
    race->unknown_4c5cd8 = 0;
    set_flags(cop, flags_of(cop) & FLAGS_COP_CLEAR);
}

/* 0046aab0: how fast a car placed behind the zone's car goes, by that
 * car's own speed. */
int32_t ai_leader_speed(const Car *car, const RaceState *race)
{
    const Car *leader = zone_car(race, car->zone);
    const int32_t speed = leader != NULL ? leader->ai_speed : 0;
    if (speed < 0xdcccc) return 0x136666;
    if (speed < 0x1bcccc) return 0xde666;
    if (speed < 0x29b333) return 0xb1c28;
    if (speed < 0x378000) return 0x6e666;
    if (speed < 0x456666) return 0x428f5;
    return 0x2c51e;
}

int32_t ai_scale_speed(const Car *car, int32_t speed)
{
    int32_t scaled = fx_mul_round(speed, car->speed_factor);
    if (speed < 1) return scaled;
    if (scaled < speed - 0xd6666) scaled = speed - 0xd6666;
    return scaled < 0x8e666 ? 0x8e666 : scaled;
}

void ai_new_bias(Car *car, RaceState *race)
{
    const uint32_t pick = race_random(race) & 3;
    if (flags_of(car) & AI_RACER) {
        const DriverProfile *profile = profile_of(race, car->player);
        car->lane_bias = profile != NULL ? profile->lane_bias[pick] : 0;
    } else {
        car->lane_bias = race->ai.params[PARAM_BIAS + pick];
    }
}

void ai_road_frame(Car *car, RaceState *race)
{
    const TriNode here = race_node(race, car->node);
    const TriNode next = race_node(race, car->node + 1);
    int32_t point[3] = { here.position.x, here.position.y, here.position.z };
    int32_t ahead[3] = { next.position.x - here.position.x, next.position.y - here.position.y,
                         next.position.z - here.position.z };
    /* the node's own vector (the original has no name for these words) */
    const int32_t up[3] = { here.z_orientation * 2, here.slant_b * 2, here.x_orientation * 2 };
    rm_normalise(ahead);
    const int32_t u0 = up[0] >> 8, u1 = up[1] >> 8, u2 = up[2] >> 8;
    const int32_t a0 = ahead[0] >> 8, a1 = ahead[1] >> 8, a2 = ahead[2] >> 8;
    const int32_t axes[9] = { up[0], up[1], up[2],
                              a1 * u2 - u1 * a2, a2 * u0 - u2 * a0, a0 * u1 - a1 * u0,
                              ahead[0], ahead[1], ahead[2] };
    memcpy(car->road_centre, point, sizeof point);
    memcpy(car->road_axes, axes, sizeof axes);

    const Car *driven = handle_car(race, car);
    const int32_t slot = driven->index;
    if (slot < 0 || slot >= race->humans) {
        int32_t odds = race->default_profile_odds;
        if (race->humans <= slot && slot < race->racers) {
            const DriverProfile *profile = profile_of(race, slot);
            odds = profile != NULL ? profile->reset_odds : 0;
        }
        if (((uint32_t)odds * race_random(race)) >> 16 == 1) ai_new_bias(car, race);
    }
}

void ai_snap_to_road(Car *car, RaceState *race)
{
    ai_road_frame(car, race);
    const int32_t jitter = (int32_t)race_random(race);
    car->x = car->road_centre[0];
    car->y = car->road_centre[1];
    car->z = car->road_centre[2] + jitter;
    const int32_t side = car->ai_target_lateral >> 8;
    car->x += side * (car->road_axes[0] >> 8);
    car->y += side * (car->road_axes[1] >> 8);
    car->z += side * (car->road_axes[2] >> 8);
    if (car_progress(car, race)) ai_road_frame(car, race);

    int32_t axes[9], matrix[9];
    memcpy(axes, car->road_axes, sizeof axes);
    if (!(flags_of(car) & AI_ONCOMING)) {
        memcpy(matrix, axes, sizeof matrix);
    } else {  /* facing the other way: the first and last rows reversed */
        for (int i = 0; i < 9; i++) matrix[i] = (i < 3 || i >= 6) ? -axes[i] : axes[i];
    }
    if (car->steer != 0) {  /* TODO(0046e6e4): car +0x18 is zero when 0046f110 gets here, so this is never run */
        int32_t turn[9];
        rm_turn_y(turn, car->steer);
        rm_mul(matrix, turn, matrix);
    }
    memcpy(car->matrix, matrix, sizeof matrix);
}

void ai_respawn_flags(Car *car, RaceState *race)
{
    set_flags(car, flags_of(car) & FLAGS_RESPAWN_CLEAR);
    /* The original also tests whether the car is ahead of the first car and,
     * for the police car, where the road leads, but both outcomes do the same. */
    if (car->flags_175 & (AI_PLACE_REQUEST >> 8)) {
        car->flags_175 = (uint8_t)((car->flags_175 & 0xdb) | (AI_PLACE_BEHIND >> 8));
    } else if (car->flags_174 & AI_COP) {
        race->unknown_4c5cd8 = 0;
    }
    if ((car->flags_174 & AI_COP) && !(car->flags_175 & (AI_PLACE_BEHIND >> 8))) {
        Car *driven = handle_car(race, car);
        driven->flags_51a &= 0xfb;
    }
}

void ai_set_speeds(Car *car, RaceState *race, int32_t lane)
{
    uint32_t flags = flags_of(car);
    if (lane == 0 && !(flags & AI_RACER)) {
        const int32_t speed = ai_leader_speed(car, race);
        car->ai_speed = -speed;
        car->ai_target_speed = -speed;
        set_flags(car, (flags & ~(uint32_t)AI_SETTLED) | AI_ONCOMING);
        return;
    }
    const Car *leader = zone_car(race, car->zone);
    const int32_t leader_speed = leader != NULL ? leader->speed : 0;
    int32_t speed = (race->ai.params[PARAM_SPEED] >> 1) + (leader_speed >> 1);
    speed = ai_scale_speed(car, speed);
    car->ai_target_speed = speed;
    if (!(flags & AI_RACER)) {
        if ((flags & (AI_PLACE_BEHIND | AI_RACER)) == 0) {
            car->ai_speed = speed;
        } else {
            const int32_t first = race->cars[0]->ai_speed;
            car->ai_speed = first;
            car->ai_target_speed = first;
        }
    } else {
        car->ai_speed = 0;
    }
    set_flags(car, flags & (0xffff0000u | SETTLED_CLEAR));
}

void ai_choose_node(const Car *car, RaceState *race, int32_t *lane, int32_t *node)
{
    const uint32_t flags = flags_of(car);
    const int first_open = (race_node(race, 0).flags[0] >> 4) == 0;
    int32_t at;
    if ((flags & (AI_PLACE_BEHIND | AI_PLACE_REQUEST)) || ((flags & AI_COP) && first_open)) {
        *lane = 1;
        at = race->cars[0]->node - ((flags & AI_COP) ? BEHIND_FIRST_CAR_COP : BEHIND_FIRST_CAR);
    } else {
        if (flags & AI_COP) {
            *lane = 0;
        } else {
            *lane = race_random(race) > 0x7fff;
            if (first_open) *lane = 1;
            if (!(flags & AI_RACER)) {
                const int32_t from = flags & AI_ONCOMING ? 0 : 1;
                if (--race->ai.lane_cars[from] < 0) race->ai.lane_cars[from] = 0;
                if (race->ai.lane_cars[*lane] >= race->traffic_setting - 1 && !first_open) *lane = *lane == 0;
                race->ai.lane_cars[*lane]++;
            }
        }
        const Car *followed = zone_car(race, car->zone);
        const int32_t followed_speed = followed != NULL ? followed->speed : 0;
        if (followed_speed < SLOW_LEADER && *lane == 1 && !(flags & AI_RACER)) {
            at = zone_word(race, car->zone, ZONE_VIEW_START) - (int32_t)(race_random(race) & PLACE_BEHIND_SPREAD);
        } else {
            at = zone_word(race, car->zone, ZONE_VIEW_END) +
                 (int32_t)(race_random(race) * PLACE_AHEAD_SPREAD >> 16);
        }
    }
    if (at < 0) at = 0;
    *node = at & node_mask(race);
}

void ai_grid_slot(Car *car, const RaceState *race, int32_t *node)
{
    if (!(flags_of(car) & AI_RACER) || race->entries[1].car == OPPONENT_CAR_MODEL) return;
    const int32_t first_open = (race_node(race, 0).flags[0] >> 4) == 0;
    const TriNode a = race_node(race, GRID_NODE_A);
    const int32_t grid = car->player >= 0 && car->player < RACE_PLAYERS ? race->grid[car->player] : 0;
    const int32_t width = lane_width(race, a.flags[0] & 0xf, a.right_verge) << 8;
    const int32_t side = grid % 2;
    int32_t offset = car->lane_bias + (first_open ? width : 0) + side * width + (-width) / 2;
    const TriNode b = race_node(race, GRID_NODE_B);
    if ((b.flags[0] >> 4) == 2 && (b.flags[0] & 0xf) == 1 && side == 0) offset -= width;
    car->ai_target_lateral = offset;
    *node = race->racers > 2 ? GRID_NODE - grid * 2 : GRID_NODE - (grid / 2) * 4;
}

void ai_respawn(Car *car, RaceState *race, int32_t node, int32_t lane, int32_t offset, int32_t activate)
{
    if (node < 0) node = 0;
    car->node = node;
    car->progress = race->segments * (car->lap - 1) * 4 + car->node;
    car->speed_factor = race->ai.params[PARAM_FACTORS + (race_random(race) & 3)];
    ai_new_bias(car, race);
    car->coast_timer = 0;
    car->unknown_148 = 0;
    car->slide_timer = (flags_of(car) & AI_RACER) ? SLIDE_OPPONENT : SLIDE_TRAFFIC;
    car->slip_steer = 0;
    car->steer = 0;
    car->ai_countdown = 0;
    car->yaw_rate = 0;
    if ((flags_of(car) & AI_COP) && race->phase != 1 && race->camera_mode == 7) {
        race->camera_mode = 5;
        if (race->sounds.camera != NULL) race->sounds.camera(race->sounds.user, race->player, 5);
    }
    ai_respawn_flags(car, race);
    ai_set_speeds(car, race, lane);
    car->ai_target_lateral = offset;
    ai_snap_to_road(car, race);
    car->speed_factor = race->ai.params[PARAM_FACTORS + (race_random(race) & 3)];

    const uint32_t flags = flags_of(car);
    if (!activate || ((flags & AI_RACER) && race->entries[1].car != OPPONENT_CAR_MODEL) || (flags & AI_COP)) return;
    Car *driven = handle_car(race, car);
    if (driven->index >= 0 && driven->index < race->humans) return;
    car->unknown_148 = (int32_t)(((uint32_t)race->ai.params[PARAM_COUNTDOWN] * race_random(race) >> 16) + 1);
    driven->physics = PHYSICS_AI;
    if (!(flags & AI_RACER)) race->ai.traffic_placed++;
}

void ai_place_car(Car *car, RaceState *race, int32_t activate)
{
    if (race->cops == 1 && (flags_of(car) & AI_COP)) race->cars[2]->zone = race->cars[0]->zone;
    car->skid_front = 0;
    car->skid_rear = 0;
    if (car == race->cars[0]) set_flags(car, flags_of(car) & FLAGS_PLAYER_CLEAR);

    int32_t lane, node;
    ai_choose_node(car, race, &lane, &node);
    const TriNode at = race_node(race, node);
    const uint32_t row = at.flags[0] & 0xf;
    const int32_t width = lane_width(race, (int32_t)row, at.right_verge) << 8;
    const uint32_t r = race_random(race);
    if (lane == 0 && !(flags_of(car) & AI_RACER)) {
        const int32_t count = (int32_t)((r * (uint32_t)(at.flags[0] >> 4)) >> 16) + 1;
        car->ai_target_lateral = count * -width + width / 2;
    } else {
        const int32_t count = (int32_t)((r * row) >> 16) + 1;
        car->ai_target_lateral = width * count - width / 2;
    }
    car->ai_target_lateral += car->lane_bias;
    ai_grid_slot(car, race, &node);
    ai_respawn(car, race, node, lane, car->ai_target_lateral, activate);
}

void ai_find_spawn(Car *car, RaceState *race)
{
    if (race->racers >= 3) return;
    if (!(flags_of(car) & AI_COP)) {
        while (car->zone < ZONES) {
            ai_place_car(car, race, 0);
            if (!ai_view_zone(race, car->node)) break;
            car->zone++;
        }
        if (car->zone < ZONES) return;
        car->zone = 0;
        ai_place_car(car, race, 1);
    } else {
        const int32_t stage = ((int32_t)flags_of(car) >> COP_STAGE_SHIFT) + 1;
        if (stage >= 2) {
            ai_release_cops(race, 1);
            return;
        }
        car->flags_176[0] &= 0x8f;
        set_flags(car, flags_of(car) | (uint32_t)(stage * (1 << COP_STAGE_SHIFT)));
        race->cars[2]->zone = race->cars[0]->zone;
        ai_place_car(car, race, 0);
    }
}

int ai_lane_update(Car *car, RaceState *race)
{
    if (car->progress < 2) {
        if (!(flags_of(car) & AI_RACER)) car->zone = 0;
        ai_place_car(car, race, 1);
    } else if (race->racers < 3) {
        if (ai_keep_zone(race, car->node)) return 1;
        car->zone++;
        ai_find_spawn(car, race);
    }
    return 0;
}
