/* The replay's seek to a minute (race_replay_seek.h). */
#include "game/race/race_replay_seek.h"

#include "game/race/car_scenery.h"
#include "game/race/race_chase.h"
#include "game/race/race_sound.h"
#include "game/gfx/x86_int.h"

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

static void put32(uint8_t *b, int32_t v)
{
    for (int k = 0; k < 4; k++) b[k] = (uint8_t)((uint32_t)v >> (8 * k));
}

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

/* 00443820, then 00444394 for the player's car in physics 1 or 2, else 0044435c */
static void restart_sounds(RaceState *race)
{
    race_sound_start_voices(race);
    const Car *car = (uint32_t)race->player < RACE_CARS ? race->cars[race->player] : NULL;  /* port guard */
    const int32_t physics = car != NULL ? car->physics : 0;
    if (physics == PHYSICS_DRIVING || physics == PHYSICS_COUNTDOWN) {
        if (race->sounds.start_engine != NULL) race->sounds.start_engine(race->sounds.user);
    } else if (race->sounds.stop_engine != NULL) {
        race->sounds.stop_engine(race->sounds.user);
    }
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
    restart_sounds(race);
}

static void keep_state(const RaceState *race, uint8_t *m)
{
    for (int32_t i = 0; i < race->car_count && i < RACE_CARS; i++) {  /* port guard: RACE_CARS */
        put32(m + BY_NODE_AT + 4 * i, race->by_node[i]);
        put32(m + BY_PROGRESS_AT + 4 * i, race->by_progress[i]);
    }
    put32(m + COUNTDOWN_AT, race->manager_countdown);
    put32(m + SEED_AT, (int32_t)race->random_seed);
    put32(m + POLICE_NODE_AT, race->police_node);
    put32(m + LANE_CARS_AT, race->ai.lane_cars[0]);
    put32(m + LANE_CARS_AT + 4, race->ai.lane_cars[1]);
    put32(m + TRAFFIC_AT, race->ai.traffic_placed);
    put32(m + UNKNOWN_509E10_AT, race->unknown_509e10);
    m[UNKNOWN_4B9624_AT] = (uint8_t)race->unknown_4b9624;
    put32(m + UNKNOWN_4C5CD8_AT, race->unknown_4c5cd8);
    put32(m + POLICE_STOPS_AT, race->ai.unknown_364);
    m[UNKNOWN_33C_AT] = (uint8_t)race->ai.unknown_33c;
    put32(m + UNKNOWN_4C5FA0_AT, race->unknown_4c5fa0);
    memcpy(m + ZONES_AT, race->ai.zones, sizeof race->ai.zones);
    put32(m + PASSED_AT, race->splits.passed);
    put32(m + RIVAL_PASSED_AT, race->splits.rival_passed);
}

static void mark(RaceState *race, const RaceReplayTickOps *ops, int32_t minute)
{
    /* 0045aab0, a bare ret, first */
    race_scenery_keep(race);
    race->replay_mark_countdown = REPLAY_MARK_EVERY;
    uint8_t *m = race->replay_marks + minute * REPLAY_MARK;
    keep_state(race, m);
    ops->record(ops->ctx);
    int32_t others = 0;
    for (int32_t i = 0; i < race->car_count; i++) {
        ops->save_car(ops->ctx, i, others);
        const Car *car = i < RACE_CARS ? race->cars[i] : NULL;  /* port guard */
        if (car != NULL && car->index >= race->humans) others++;
    }
    put32(m + OTHERS_AT, others);
    ops->seek(ops->ctx, race->clock, -1);
}

void race_replay_tick(RaceState *race, const RaceReplayTickOps *ops)
{
    ops->input_tick(ops->ctx, race->input_clock);
    if (race->unknown_4fbb0c == 0 || race->playback == 1 || (race->playback != 0 && race->dropped != 0)) return;
    if (race->tick < REPLAY_INPUT_TICKS)
        for (int32_t i = 0; i < race->humans && i < RACE_PLAYERS; i++)  /* port guard: RACE_PLAYERS */
            race->inputs[i] = REPLAY_INPUT_WAIT;
    const int32_t minute = race->clock / MINUTE_TICKS;
    if (race->replay_mark_countdown == 0 && minute >= 0 && minute < REPLAY_MARKS)  /* port guard: the minute */
        mark(race, ops, minute);
    else
        ops->record(ops->ctx);
    race->replay_mark_countdown = x_sub(race->replay_mark_countdown, 1);
}

enum {
    PICK_TENTHS = 10, PICK_SLOT2_FROM = 6, PICK_LOW = 0xd6, PICK_HIGH = 0xf7, PICK_FAR_FROM = 8, KIND_FAR = 2,
    PICK_MID_LOW = 0xdb, PICK_MID_HIGH = 0xe3, PICK_MID_KINDS = 5, PICK_KINDS = 4
};

void race_replay_camera(RaceState *race, int32_t word)
{
    const int behind2 = race_chase_ahead_of_slot2(race, AHEAD_AFTER, AHEAD_UPTO);
    const int behind_other = race_chase_ahead_of_other(race, AHEAD_AFTER, AHEAD_UPTO);
    int32_t kind, slot = race->player;
    if (!behind2 && !behind_other) {
        if (word >= PICK_LOW && word <= PICK_HIGH && race_rand_below(race, PICK_TENTHS) >= PICK_FAR_FROM)
            kind = KIND_FAR;
        else if (word >= PICK_MID_LOW && word <= PICK_MID_HIGH)
            kind = race_rand_below(race, PICK_MID_KINDS);
        else
            kind = race_rand_below(race, PICK_KINDS) + 1;
    } else if (!behind2 || !behind_other) {
        kind = behind2 ? KIND_SLOT2 : KIND_OTHER;
    } else {
        kind = race_rand_below(race, PICK_TENTHS) < PICK_SLOT2_FROM ? KIND_OTHER : KIND_SLOT2;
    }
    if (kind == KIND_SLOT2) slot = SLOT2;
    else if (kind == KIND_OTHER) slot = race->player == 0;
    race->chase.next_kind = kind;
    if (race->sounds.camera != NULL) race->sounds.camera(race->sounds.user, slot, kind);
    if (race->unknown_512f78 >= 0 && kind > 0) (void)race_rand_below(race, PICK_TENTHS);
    race->camera_roll_add = 0;
}

enum { CUT_ROWS = 39, CUT_CLOCK = -1, CYCLE_TICK = 0xfc, TICK_BYTE = 0xff };

static void after_seek(RaceState *race, const RaceReplayStepOps *ops)
{
    race->unknown_4c538c = 0;
    if (race->sounds.camera != NULL) race->sounds.camera(race->sounds.user, race->player, race->chase.next_kind);
    const int32_t row = race->unknown_512f78;
    if (row == CUT_CLOCK) ops->camera(ops->ctx, race->clock);  /* row -1 is the clock (00512d90) */
    else if (row >= 0 && row < CUT_ROWS) ops->camera(ops->ctx, race->unknown_512d9c[row][0]);  /* port guard: the rows */
    race->unknown_4c5370 = 0;
    race->unknown_513728 = 0;
    if (race->unknown_512f7c != 0) {
        race->unknown_512f7c = 0;
        if (race->unknown_4fbb0c != 0 && race->playback != 1) race->replay_mark_ticks[0] = race->tick;
    }
    restart_sounds(race);
}

void race_replay_step(RaceState *race, const RaceReplayStepOps *ops)
{
    if (race->clock % MINUTE_TICKS == 0) ops->seek(ops->ctx, race->clock, -1);
    ops->play(ops->ctx);
    if (race->unknown_513728 != 0) after_seek(race, ops);
    if (race->unknown_51371c != 0 && (race->tick & TICK_BYTE) == CYCLE_TICK) ops->cycle(ops->ctx);
}

enum { CLOCK_WRAP = 0x10 * MINUTE_TICKS, CATCHING_UP = 3, NOTICE_TICK_START = -1000 };

void race_replay_jump(RaceState *race, RaceReplayJumpWords *w, const RaceReplayJumpOps *ops, int32_t clock, int32_t until)
{
    w->until = until > w->end_clock ? w->end_clock : until;
    const int on_minute = clock % CLOCK_WRAP % MINUTE_TICKS == 0;
    race->tick = on_minute ? clock : clock / MINUTE_TICKS * MINUTE_TICKS;
    if (!on_minute) w->target = clock;
    if (race->phase == RACING) {
        ops->keep_scenery(ops->ctx);
        w->saved_timer = w->timer;
        w->saved_input_clock = race->input_clock;
        w->saved_due = w->due;
    }
    race->phase = on_minute ? SEEKING : CATCHING_UP;
    ops->channels_stop(ops->ctx);
    race->unknown_513728 = on_minute;
    w->loading = 0;
    race->clock = race->tick % CLOCK_WRAP;
    uint8_t flags = (race->tick & 1) == 0 ? EVEN_TICK : 0;
    if ((race->tick & 3) == 0) flags |= FOURTH_TICK;
    if ((race->tick & 7) == 0) flags |= EIGHTH_TICK;
    race->tick_flags = flags;
    race->unknown_51372c = 1;
    ops->trail_clear(ops->ctx);
    ops->notices_off(ops->ctx);
    race->unknown_4b91c0 = 0;
    w->unknown_4b91c4 = 0;
    w->jump_clock = clock;
    race->stop_notice_tick = NOTICE_TICK_START;
    race->unknown_4c5370 = 1;
    ops->marks_back(ops->ctx);
    race->unknown_513734 = 1;
    race->splits.gap = 0;
    race->splits.gap_shown = 0;
    ops->road_yaw(ops->ctx);
}
