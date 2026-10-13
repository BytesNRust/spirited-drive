/* The replay's seek to a minute (race_replay_seek.h). */
#include "game/race/race_replay_seek.h"

#include "game/race/car_scenery.h"
#include "game/race/race_chase.h"
#include "game/race/race_sound.h"

#include <string.h>

enum {
    MINUTE_TICKS = 0x708,
    EVEN_TICK = 1, FOURTH_TICK = 4, EIGHTH_TICK = 0x10,
    BY_NODE_AT = 0, BY_PROGRESS_AT = 0x24, OTHERS_AT = 0x50, COUNTDOWN_AT = 0x54, UNKNOWN_4C5CD8_AT = 0x58,
    ZONES_AT = 0x5c, POLICE_STOPS_AT = 0x74, UNKNOWN_4C5FA0_AT = 0x78, SEED_AT = 0x7c, POLICE_NODE_AT = 0x80,
    LANE_CARS_AT = 0x84, TRAFFIC_AT = 0x8c, UNKNOWN_509E10_AT = 0x90, UNKNOWN_4B9624_AT = 0x98,
    UNKNOWN_33C_AT = 0x99, PASSED_AT = 0x9a, RIVAL_PASSED_AT = 0x9e,
    RACING = 1, SEEKING = 2,
    KIND_SLOT2 = 7, KIND_OTHER = 6, KIND_FOLLOW = 5, AHEAD_AFTER = 0, AHEAD_UPTO = 5, SLOT2 = 2,
    PHYSICS_COUNTDOWN = 1, PHYSICS_DRIVING = 2
};

static int32_t raw32(const uint8_t *b)
{
    return (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
}

static void restore_state(RaceState *race, const uint8_t *m)
{
    for (int32_t i = 0; i < race->car_count && i < RACE_CARS; i++) {  /* port guard: RACE_CARS */
        race->by_node[i] = raw32(m + BY_NODE_AT + 4 * i);
        race->by_progress[i] = raw32(m + BY_PROGRESS_AT + 4 * i);
    }
    race->random_seed = (uint32_t)raw32(m + SEED_AT);
    race->ai.unknown_364 = raw32(m + POLICE_STOPS_AT);
    race->ai.unknown_33c = m[UNKNOWN_33C_AT];
    race->unknown_4c5fa0 = raw32(m + UNKNOWN_4C5FA0_AT);
    race->unknown_4c5cd8 = raw32(m + UNKNOWN_4C5CD8_AT);
    race->police_node = raw32(m + POLICE_NODE_AT);
    race->ai.lane_cars[0] = raw32(m + LANE_CARS_AT);
    race->ai.lane_cars[1] = raw32(m + LANE_CARS_AT + 4);
    race->ai.traffic_placed = raw32(m + TRAFFIC_AT);
    race->unknown_509e10 = raw32(m + UNKNOWN_509E10_AT);
    race->unknown_4b9624 = m[UNKNOWN_4B9624_AT] & 1;
    memcpy(race->ai.zones, m + ZONES_AT, sizeof race->ai.zones);
    for (int32_t i = 0; i < race->car_count && i < RACE_CARS; i++) {
        if (race->cars[i] == NULL) continue;  /* port guard */
        race->cars[i]->scenery_index = 0;
        race->cars[i]->scenery_node = 0;
    }
    race->manager_countdown = raw32(m + COUNTDOWN_AT);
    race->splits.passed = raw32(m + PASSED_AT);
    race->splits.rival_passed = raw32(m + RIVAL_PASSED_AT);
}

static void follow(RaceState *race)
{
    int32_t kind = race->chase.next_kind, slot = race->player;
    if (kind == KIND_SLOT2) {
        if (race_chase_ahead_of_slot2(race, AHEAD_AFTER, AHEAD_UPTO)) slot = SLOT2;
        else kind = race->chase.next_kind = KIND_FOLLOW;
    } else if (kind == KIND_OTHER) {
        if (race_chase_ahead_of_other(race, AHEAD_AFTER, AHEAD_UPTO)) slot = race->player == 0;
        else kind = race->chase.next_kind = KIND_FOLLOW;
    }
    if (race->sounds.camera != NULL) race->sounds.camera(race->sounds.user, slot, kind);
}

void race_replay_seek(RaceState *race, const RaceReplaySeekOps *ops, int32_t clock, int32_t mark)
{
    int32_t minute;
    if (mark < 0) {
        minute = clock / MINUTE_TICKS;
    } else {
        if (mark >= REPLAY_MARK_COUNT) return;  /* port guard: the original reads 00512f84 on */
        race->tick = race->replay_mark_ticks[mark];
        minute = mark + REPLAY_MARK_FIRST;
        uint8_t flags = (race->tick & 1) == 0 ? EVEN_TICK : 0;  /* a dword write: 004fcdb1..3 are read nowhere */
        if ((race->tick & 3) == 0) flags |= FOURTH_TICK;
        if ((race->tick & 7) == 0) flags |= EIGHTH_TICK;
        race->tick_flags = flags;
    }
    if (minute < 0 || minute >= REPLAY_MARKS) return;  /* port guard: the original reads 005120d8 unchecked */
    race_scenery_restore(race);
    race_scenery_rewind(race, race->tick / MINUTE_TICKS);
    /* 0045aab0 (here and below) is a bare ret */
    const uint8_t *m = race->replay_marks + minute * REPLAY_MARK;
    const int32_t others = raw32(m + OTHERS_AT);
    restore_state(race, m);
    for (int32_t i = 0; i < race->humans; i++) {
        if (mark >= 0) race->unknown_51372c = 1;
        ops->restore_car(ops->ctx, 1, minute, i);
    }
    for (int32_t i = 0; i < others; i++) ops->restore_car(ops->ctx, 0, minute, i);
    if (race->unknown_51372c != 0) race->unknown_51372c = 0;
    if (mark >= 0) {
        if (race->phase == RACING) race_scenery_keep(race);
        race->phase = SEEKING;
        race->unknown_513728 = 1;
    }
    if (race->phase != RACING) follow(race);
    if (race->unknown_513734 == 0) return;
    race->unknown_513734 = 0;
    race_sound_start_voices(race);
    const Car *car = (uint32_t)race->player < RACE_CARS ? race->cars[race->player] : NULL;  /* port guard */
    const int32_t physics = car != NULL ? car->physics : 0;
    if (physics == PHYSICS_DRIVING || physics == PHYSICS_COUNTDOWN) {
        if (race->sounds.start_engine != NULL) race->sounds.start_engine(race->sounds.user);
    } else if (race->sounds.stop_engine != NULL) {
        race->sounds.stop_engine(race->sounds.user);
    }
}
