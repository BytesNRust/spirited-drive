#include "game/race/ai_setup.h"

#include "game/race/ai_lanes.h"
#include "game/race/ai_profiles.h"
#include "game/race/ai_zones.h"

#include <string.h>

#include "game/sys/fixed_math.h"

enum {
    TRACK_FLAG_4 = 4,
    GRID_LANE_NODE = 10,    /* whose lanes give the gap between the two columns */
    GRID_NARROW_NODE = 20,  /* 2 lanes left and 1 right: the left column moves a lane over */
    GRID_FRONT = 0x20,      /* the front row's node */
    PAIRED_RACERS = 2,      /* up to this, two cars a row */
    OPEN_ROAD_TRACK = 2     /* above: the cars take the node's x and heading */
};

enum {
    LANE_WIDTHS_AT = 8,     /* ai byte offset of the lane width words, 40 a row */
    LANE_ROW = 40,
    LANE_ROWS = 4,          /* row 0 zeroed, rows 1..3 i / row (the last 4 words of row 3 run into ai +0x140) */
    START_UNKNOWN_160 = 80000,
    RACER_DECEL = 0x5999,
    PARAM_COPS_AT = 13      /* zeroed for race type 0 and for type 1 with laps */
};

enum { PARAMS = 107, TICKS_PER_SECOND = 60, PARAM_SCALED = 18, PARAM_SCALE = 0x471c };

/* 00456410's defaults, in the order of the words */
static const int32_t DEFAULT_PARAMS[PARAMS] = {
    0x14, 0x48000, 0xc, 0x0, 0x14, 0x2b0000, 0x140000, 0x190,
    0x2, 0x1, 0x1, 0x4, 0x1f4, 0x4, 0x10000, 0xe666,
    0xb333, 0xcccc, 0x960000, 0x0, 0x0, 0x0, 0x0, 0x20000,
    0x20000, 0x20000, 0x20000, 0x1cccc, 0x1cccc, 0x19999, 0x16666, 0x13333,
    0x11999, 0x10000, 0xf851, 0xeb85, 0xe666, 0xd999, 0xcccc, 0xc000,
    0xb333, 0xa666, 0xa666, 0x9999, 0x20000, 0x20000, 0x20000, 0x20000,
    0x1cccc, 0x16666, 0x14ccc, 0x13333, 0x11999, 0x10000, 0x10000, 0x10000,
    0xf333, 0xe666, 0xd999, 0xcccc, 0xc000, 0xc000, 0xb333, 0xb333,
    0x9999, 0x90000, 0x30000, 0x20000, 0x20000, 0x18000, 0x18000, 0x18000,
    0x18000, 0x13333, 0x10000, 0x10000, 0xcccc, 0xcccc, 0x10000, 0x20000,
    0x60000, 0x90000, 0x90000, 0x90000, 0x90000, 0x90000, 0x90000, 0x30000,
    0x20000, 0x20000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000,
    0x10000, 0x10000, 0x50000, 0x90000, 0x90000, 0x90000, 0x90000, 0x90000,
    0x90000, 0x90000, 0x90000,
};

/* 00470238 by the first car's model (type 3): 0, 6, 7 -> 2; 1, 4 -> 0; 2, 3, 5 -> 1 */
static const int8_t ROW_BY_MODEL[8] = { 2, 0, 1, 1, 0, 1, 2, 2 };

int32_t ai_profile_row(const RaceState *race, int32_t fallback)
{
    switch ((uint32_t)race->race_type) {
    case 0:
        return 7;
    case 1:
        return race->track_flags & TRACK_FLAG_4 ? 6 : 5;
    case 2:
        if (race->cars[2] == NULL || race->cars[3] == NULL) return fallback & 0xff;  /* port guard */
        return race->cars[2]->model == race->cars[3]->model ? 4 : 3;
    case 3:
        if (race->cars[0] == NULL) return fallback & 0xff;  /* port guard */
        if ((uint32_t)race->cars[0]->model <= 7) return ROW_BY_MODEL[race->cars[0]->model];
        return fallback & 0xff;
    default:
        return fallback & 0xff;
    }
}

int32_t ai_driver_letter(int32_t place, int32_t row)
{
    const uint32_t at = (uint32_t)place + (uint32_t)row * 8;
    if (at >= AI_DRIVER_DATA_SIZE) return -1;  /* port guard: the original reads past the table */
    return AI_DRIVER_DATA[at] - 'A';
}

uint32_t ai_profile_handle(const RaceState *race, int32_t index)
{
    return race->driver_profiles_base + (uint32_t)index * AI_DRIVER_PROFILE_SIZE;
}

void ai_use_game_profiles(RaceState *race)
{
    race->driver_profiles = (const DriverProfile *)(const void *)(AI_DRIVER_DATA + AI_DRIVER_LETTERS);
    race->driver_profiles_base = AI_DRIVER_PROFILES_ADDR;
    race->driver_profile_count = AI_DRIVER_PROFILES;
}

void ai_grid_place(Car *car, RaceState *race)
{
    const TriNode lanes = race_node(race, GRID_LANE_NODE);
    const int32_t gap = ai_lane_width(race, lanes.flags[0] & 0xf, lanes.right_verge) << 8;
    const int32_t place = (uint32_t)car->player < RACE_PLAYERS ? race->grid[car->player] : 0;  /* port guard */
    car->node = GRID_FRONT - (race->racers > PAIRED_RACERS ? place * 2 : place / 2 * 4);
    car->x = -gap / 2 + place % 2 * gap;
    if (race->track > OPEN_ROAD_TRACK) {
        const TriNode at = race_node(race, car->node);
        car->x += at.position.x;
        car->yaw = (int32_t)((uint32_t)(int32_t)at.orientation << 10);
    }
    const TriNode narrow = race_node(race, GRID_NARROW_NODE);
    if (narrow.flags[0] >> 4 == 2 && (narrow.flags[0] & 0xf) == 1 && place % 2 == 0) car->x -= gap;
    car->lateral = 0;
    car->z = race_node(race, car->node).position.z;
}

void ai_default_params(int32_t params[107])
{
    for (int i = 0; i < PARAMS; i++) params[i] = DEFAULT_PARAMS[i];
    params[9] *= TICKS_PER_SECOND;
    params[10] *= TICKS_PER_SECOND;
    params[PARAM_SCALED] = fx_mul_round(params[PARAM_SCALED], PARAM_SCALE);
}

static void set_lane_width(RaceState *race, int32_t at, int32_t value)
{
    const int16_t word = (int16_t)value;
    memcpy((uint8_t *)&race->ai + LANE_WIDTHS_AT + 2 * at, &word, sizeof word);
}

static void lane_widths(RaceState *race)
{
    for (int32_t i = 0; i < LANE_ROW; i++) set_lane_width(race, i, 0);
    for (int32_t row = 1; row < LANE_ROWS; row++)
        for (int32_t i = 0; i < LANE_ROW; i++)
            set_lane_width(race, row * LANE_ROW + i, fx_div(i << 16, row << 16) >> 8);
}

static void opponents(RaceState *race, int32_t row)
{
    for (int32_t i = 0; i < race->car_count && i < RACE_PLAYERS; i++) race->ai.profiles[i] = 0;
    const int32_t last = race->racers < RACE_PLAYERS ? race->racers : RACE_PLAYERS;  /* port guard */
    if (race->track_variant == 0)
        for (int32_t i = race->humans; i < last; i++) race->driver_letters[i] = ai_driver_letter(race->grid[i], row);
    for (int32_t i = race->humans; i < last; i++) race->ai.profiles[i] = ai_profile_handle(race, race->driver_letters[i]);
}

void ai_race_setup(RaceState *race, int32_t fallback_row)
{
    ai_use_game_profiles(race);
    const int32_t row = ai_profile_row(race, fallback_row);
    race->ai.unknown_360 = (int32_t)(race->track_flags >> 16 & 1);
    opponents(race, row);
    lane_widths(race);
    race->ai.first_car = race->cars[0] != NULL ? race->cars[0]->unknown_088 : 0;
    race->ai.unknown_344 = 0;
    ai_zones_init(race);
    race->ai.lane_cars[0] = 0;
    race->ai.traffic_placed = 0;
    race->ai.unknown_33c = 0;
    race->ai.unknown_160 = START_UNKNOWN_160;
    race->unknown_4c5cd8 = 0;
    race->unknown_4c5cdc = 0;
    race->unknown_4c5d40 = 0;
    race->ai.unknown_358 = 1;
    race->ai.lane_cars[1] = race->traffic_setting;
    race->ai.unknown_168 = race->unknown_4fb7f0;
    ai_default_params(race->ai.params);
    race->ai.racer_decel = RACER_DECEL;
    if (race->race_type == 0 || (race->race_type == 1 && race->circuit != 0)) race->ai.params[PARAM_COPS_AT] = 0;
    for (int32_t i = 0; i < race->humans && i < RACE_PLAYERS; i++)
        if (race->cars[i] != NULL) ai_grid_place(race->cars[i], race);
    race_sort_cars(race);
}
