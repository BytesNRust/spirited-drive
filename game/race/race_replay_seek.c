/* The replay's seek to a minute (race_replay_seek.h). */
#include "game/race/race_replay_seek.h"

#include "game/race/car_scenery.h"
#include "game/race/race_chase.h"
#include "game/race/race_replay_cuts.h"
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

enum { CUT_CLOCK = -1, CYCLE_TICK = 0xfc, TICK_BYTE = 0xff };

static void after_seek(RaceState *race, const RaceReplayStepOps *ops)
{
    race->unknown_4c538c = 0;
    if (race->sounds.camera != NULL) race->sounds.camera(race->sounds.user, race->player, race->chase.next_kind);
    const int32_t row = race->unknown_512f78;
    if (row == CUT_CLOCK) ops->camera(ops->ctx, race->clock);  /* row -1 is the clock (00512d90) */
    else if (row >= 0 && row < REPLAY_CUTS) ops->camera(ops->ctx, race->replay_cuts[row].word);  /* port guard: the cuts */
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

void race_replay_tick_flags(RaceState *race)
{
    uint8_t flags = (race->tick & 1) == 0 ? EVEN_TICK : 0;  /* a dword write: 004fcdb1..3 are read nowhere */
    if ((race->tick & 3) == 0) flags |= FOURTH_TICK;
    if ((race->tick & 7) == 0) flags |= EIGHTH_TICK;
    race->tick_flags = flags;
}

enum { CLOCK_WRAP = 0x10 * MINUTE_TICKS, CATCHING_UP = 3, NOTICE_TICK_START = -1000 };

void race_replay_jump(RaceState *race, RaceReplayDirector *w, const RaceReplayJumpOps *ops, int32_t clock, int32_t until)
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
    race_replay_tick_flags(race);
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

enum {
    CUT_DIRECT_TICKS = 4, CUT_FRAMES = 0, CUT_PANEL = 6, CLOSE_TICKS = 3, CLOSE_FRAMES = 2, CLOSE_PANEL = 8,
    CLOSE_WORD = 0xe4, CLOSE_FIRST = 0xdb, CLOSE_LAST = 0xf7, CLOSE_TENTHS = 10, CLOSE_ABOVE = 4, NEAR_NODES = 3,
    DIRECTOR_DONE = 0x20
};

/* Another human's car within NEAR_NODES nodes of the player's */
static int human_near(const RaceState *race)
{
    const Car *player = race->player >= 0 && race->player < RACE_CARS ? race->cars[race->player] : NULL;
    for (int32_t i = 0; i < race->humans && i < RACE_CARS; i++) {  /* port guard: the slots */
        if (i == race->player || player == NULL || race->cars[i] == NULL) continue;
        int32_t d = (int32_t)((uint32_t)player->node - (uint32_t)race->cars[i]->node);
        if (d <= 0) d = (int32_t)(0u - (uint32_t)d);
        if (d < NEAR_NODES) return 1;
    }
    return 0;
}

void race_replay_next_cut(RaceState *race, RaceReplayDirector *w, const RaceReplayCutOps *ops)
{
    w->unknown_4c5388 = 0;
    race->camera_roll_add = 0;
    race->unknown_4c5398 = CUT_DIRECT_TICKS;
    w->unknown_4c53a0 = CUT_FRAMES;
    w->unknown_513730 = CUT_PANEL;
    w->unknown_513888 = CUT_PANEL;
    w->unknown_513894 = 0;
    w->unknown_513898 = 1;
    const int32_t row = ++race->unknown_512f78;
    if (row >= race->replay_cut_count) {
        if (w->unknown_5137e4 != 0) {
            race->end_state = DIRECTOR_DONE;
            return;
        }
        race->unknown_512f78 = 0;
        ops->jump(ops->ctx, race->replay_cuts[0].clock, race->replay_cuts[0].until);
        ops->step(ops->ctx);
        w->close_up = 0;
        return;
    }
    if (row < 0 || row >= REPLAY_CUTS) return;  /* port guard: the original reads past the cuts */
    const int32_t draw = race_rand_below(race, CLOSE_TENTHS);
    const ReplayCut *cut = &race->replay_cuts[row];
    const int near = cut->word == CLOSE_WORD ? human_near(race) : 1;
    if (cut->word >= CLOSE_FIRST && cut->word <= CLOSE_LAST && draw > CLOSE_ABOVE && w->close_up == 0 && near) {
        race->unknown_4c5398 = CLOSE_TICKS;
        w->unknown_4c53a0 = CLOSE_FRAMES;
        w->unknown_513730 = CLOSE_PANEL;
        w->unknown_513888 = CUT_PANEL;
        w->unknown_513894 = 1;
        w->unknown_513898 = 1;
        w->close_up = 1;
    } else {
        w->close_up = 0;
    }
    ops->jump(ops->ctx, cut->clock, cut->until);
    ops->step(ops->ctx);
}

enum { STILL_PANEL = 5 };

void race_replay_frame(RaceState *race, RaceReplayDirector *w, const RaceReplayFrameOps *ops)
{
    if (race->unknown_4c5398 == 0 && race->unknown_4c539c <= 0) {
        w->unknown_513730 = STILL_PANEL;
        race_replay_tick_flags(race);
        return;
    }
    if (race->tick < w->until && (race->unknown_4c538c == 0 || race->unknown_512f78 < 0)) {
        w->unknown_4c5388 = 0;
        ops->step(ops->ctx);
        return;
    }
    if (w->until == w->end_clock && race->unknown_512f78 < 0) {
        race->phase = w->phase_before;
        ops->hud_redraw(ops->ctx);
        ops->reselect(ops->ctx);
        if (race->phase == RACING) {
            w->timer = w->saved_timer;
            race->input_clock = w->saved_input_clock;
            w->unknown_4c53ac = RACING;
            w->due = w->saved_due;
        } else {
            w->unknown_4c5388 = 0;
            race->tick = race->replay_mark_ticks[0];
            race_replay_tick_flags(race);
            race->clock = race->tick % CLOCK_WRAP;
            ops->jump(ops->ctx, race->tick, w->end_clock);
            ops->step(ops->ctx);
        }
        restart_sounds(race);
        return;
    }
    if (race->unknown_512f78 >= 0) ops->next_cut(ops->ctx);
}

enum { STOPPED = 4, PAUSE_TICKS = 2, TICK_GROUP = 4 };

typedef struct {
    int32_t direct_ticks, frames, panel, panel2, speed;
    int unmute;
} ReplaySpeed;

/* buttons 6..9: 004c5398, 004c53a0, 00513730, 00513888, 00513894; 00513898 is 1 for each */
static const ReplaySpeed REPLAY_SPEEDS[] = {
    { 4, 0, 6, 6, 0, 1 }, { 3, 2, 8, 6, 1, 1 }, { 2, 4, 9, 6, 2, 1 }, { 1, 8, 10, 6, 3, 0 }
};

void race_replay_button(RaceState *race, RaceReplayDirector *w, const RaceReplayButtonOps *ops, int32_t button,
                        int32_t held)
{
    if (race->phase == RACING) return;
    if (button >= REPLAY_PLAY && button <= REPLAY_SLOWEST) {
        const ReplaySpeed *s = &REPLAY_SPEEDS[button - REPLAY_PLAY];
        race->unknown_4c5398 = s->direct_ticks;
        w->unknown_4c53a0 = s->frames;
        w->unknown_513730 = s->panel;
        w->unknown_513888 = s->panel2;
        w->unknown_513894 = s->speed;
        w->unknown_513898 = 1;
        if (s->unmute) ops->unmute(ops->ctx);
    } else if (button == REPLAY_PAUSE && held == 0) {
        if (race->phase != CATCHING_UP && race->unknown_4c539c == 0) {
            ops->mute(ops->ctx);
            race->unknown_4c5398 = 0;
            race->unknown_4c539c = PAUSE_TICKS;
            w->unknown_513730 = STILL_PANEL;
        }
    } else {
        return;
    }
    if (race->phase == STOPPED) {
        w->phase_before = STOPPED;
        race->unknown_512f7c = 1;
        ops->jump(ops->ctx, race->tick - race->tick % TICK_GROUP, w->end_clock);
        race->unknown_51371c = 0;
    }
}

enum { END_BACK = 8, NO_CUT = -2, END_KIND = 4, LEADER_TILT_LOW = 0x1e0000, LEADER_TILT_HIGH = 0xdc0000 };

void race_replay_to_end(RaceState *race, RaceReplayDirector *w, const RaceReplayToEndOps *ops)
{
    int32_t from = (int32_t)((uint32_t)w->end_clock - END_BACK);
    if (from < 0) from = 0;
    ops->jump(ops->ctx, from - from % TICK_GROUP, w->end_clock);
    race->unknown_512f78 = NO_CUT;
    race->unknown_4c5398 = CUT_DIRECT_TICKS;
    w->unknown_513730 = CUT_PANEL;
    w->phase_before = RACING;
    race->camera_roll_add = 0;
    w->unknown_4c53a0 = 0;
    w->unknown_513888 = 0;
    w->unknown_513894 = 0;
    w->unknown_513898 = 1;
    race->view_taken = 1;
    race->chase.next_kind = w->saved_kind;
    if (w->saved_kind == 0 && race->leader != NULL && race->leader->roll > LEADER_TILT_LOW &&
        race->leader->roll < LEADER_TILT_HIGH)  /* port guard: no leader */
        race->chase.kind = END_KIND;
    w->unknown_4c5388 = 0;
    ops->unmute(ops->ctx);
    restart_sounds(race);
    ops->road_yaw(ops->ctx);
}

enum { SCRUB_TAIL = 300, SCRUB_SPAN = 0x6cfc, SCRUB_STEP_HELD = 16, SCRUB_BACK_PANEL = 4, SCRUB_ON_PANEL = 3 };

static int32_t minus(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }

void race_replay_scrub(RaceState *race, RaceReplayDirector *w, const RaceReplayScrubOps *ops, int32_t button,
                       int32_t held)
{
    const int32_t step = held == 0 ? TICK_GROUP : (int32_t)((uint32_t)(held / SCRUB_STEP_HELD) << 2);
    const int32_t last = minus(w->end_clock, SCRUB_TAIL);
    const int32_t first = minus(w->end_clock, SCRUB_SPAN);
    int32_t t = race->tick > last ? last : race->tick;
    int32_t panel = -1;
    if (button == REPLAY_TO_START) {
        t = first < 0 ? 0 : first;
        panel = SCRUB_BACK_PANEL;
    } else if (button == REPLAY_TO_END) {
        t = last < first ? first : last;
        if (t < 0) t = 0;
        panel = SCRUB_ON_PANEL;
    } else if (button == REPLAY_BACK) {
        if (t > step) t = minus(t, step);
        if (t < first) t = first;
        panel = SCRUB_BACK_PANEL;
    } else if (button == REPLAY_FORWARD) {
        if (t < minus(last, step)) t = (int32_t)((uint32_t)t + (uint32_t)step);
        if (t < 0) t = 0;
        panel = SCRUB_ON_PANEL;
    }
    if (panel >= 0) {
        w->unknown_513730 = panel;
        race->unknown_51371c = 0;
    }
    race->tick = t - t % TICK_GROUP;
    if (race->phase == RACING) ops->keep_scenery(ops->ctx);
    race->phase = STOPPED;
    w->unknown_4b92cc = 0;
    race->unknown_512f78 = NO_CUT;
    race->camera_roll_add = 0;
    race->unknown_4c5398 = CUT_DIRECT_TICKS;
    w->unknown_4c53a0 = 0;
    ops->mute(ops->ctx);
}

enum { BEGIN_LONG_BACK = 27000, BEGIN_BACK = 600, CUT_PLAYING = -1 };

void race_replay_begin(RaceState *race, RaceReplayDirector *w, const RaceReplayJumpTo *ops, int32_t how)
{
    if (how == REPLAY_DIRECTED) {
        if (race->phase == RACING) w->saved_kind = race->chase.next_kind;
        race_replay_cuts_build(race, w->end_clock);
        if (race->replay_cut_count > 0) {
            race->unknown_512f78 = 0;
            ops->jump(ops->ctx, race->replay_cuts[0].clock, race->replay_cuts[0].until);
            w->unknown_4c53a0 = 0;
            w->unknown_513730 = CUT_PANEL;
            w->unknown_513888 = CUT_PANEL;
            w->unknown_513894 = 0;
            race->unknown_4c5398 = CUT_DIRECT_TICKS;
            w->unknown_513898 = 1;
        }
        return;
    }
    race->unknown_512f78 = NO_CUT;
    race->camera_roll_add = 0;
    race->unknown_4c5398 = CUT_DIRECT_TICKS;
    w->unknown_4c53a0 = 0;
    w->unknown_513730 = CUT_PANEL;
    w->unknown_513888 = CUT_PANEL;
    w->unknown_513894 = 0;
    race->tick = minus(w->end_clock, how == REPLAY_LONG ? BEGIN_LONG_BACK : BEGIN_BACK);
    w->unknown_513898 = 1;
    if (race->tick < 0) race->tick = 0;
    race->unknown_512f7c = 1;
    w->phase_before = STOPPED;
    ops->jump(ops->ctx, race->tick - race->tick % TICK_GROUP, w->end_clock);
    if (how == REPLAY_LONG) race->unknown_51371c = 1;
    race->unknown_512f78 = CUT_PLAYING;
}

enum {
    REQUEST_SAVE_LONG = 4, REQUEST_SAVE = 5, REQUEST_SAVE_STOP = 8, REQUEST_STOP = 6, REQUEST_RACE_ON = 7,
    END_STOP = 0x20, END_STOP_LAST = 0x40, BUTTON_VIEW = 10, BUTTON_PLAY_FIRST = 5, BUTTON_PLAY_LAST = 9
};

static void keys_request(RaceState *race, RaceReplayDirector *w, const RaceReplayKeysOps *ops)
{
    if (w->request == REQUEST_SAVE_LONG) {
        ops->load(ops->ctx);
        w->request = REPLAY_LONG;
    }
    if (w->request == REQUEST_SAVE || w->request == REQUEST_SAVE_STOP) {
        if (w->request == REQUEST_SAVE_STOP) w->unknown_5137e4 = 1;
        ops->load(ops->ctx);
        w->request = REPLAY_DIRECTED;
    }
    if (w->request != 0 && w->request <= REPLAY_LONG) {
        ops->begin(ops->ctx, w->request);
        w->request = 0;
    } else if (w->request == REQUEST_RACE_ON) {
        if (w->unknown_513740 == 0) ops->to_end(ops->ctx);
        else race->end_state = w->unknown_5137e4 != 0 ? END_STOP_LAST : END_STOP;
        race->unknown_51371c = 0;
        w->request = 0;
    } else if (w->request == REQUEST_STOP) {
        race->end_state = END_STOP;
    }
}

void race_replay_keys(RaceState *race, RaceReplayDirector *w, const RaceReplayKeysOps *ops)
{
    if (w->request != 0) keys_request(race, w, ops);
    if (race->phase == RACING) {
        if (race->replay != 0 || race->viewed != race->player || race->unknown_513e6c != 0) ops->panel_draw(ops->ctx);
        return;
    }
    w->button = 0;
    ops->panel_input(ops->ctx);
    if (w->button == 0) {
        w->held = 0;
    } else {
        if (w->button == w->last_button) {
            w->held++;
        } else {
            w->held = 0;
            w->unknown_513888 = 0;
            w->unknown_513898 = 1;
        }
        if (w->button >= REPLAY_TO_START && w->button <= REPLAY_FORWARD) {
            ops->scrub(ops->ctx, w->button, w->held);
            w->unknown_4c5388 = 1;
            race->unknown_51371c = 0;
        } else if (w->held == 0 && w->button == BUTTON_VIEW && race->phase != STOPPED) {
            ops->cycle_view(ops->ctx);
            race->camera_roll_add = 0;
            if (race->phase == SEEKING && race->unknown_4c5398 == 0 && race->unknown_4c539c <= 0)
                race->unknown_4c539c = SEEKING;
            w->unknown_4c5388 = 0;
            race->unknown_51371c = 0;
        } else if (w->button >= BUTTON_PLAY_FIRST && w->button <= BUTTON_PLAY_LAST) {
            ops->button(ops->ctx, w->button, w->held);
            w->unknown_4c5388 = 0;
        }
    }
    w->last_button = w->button;
}

enum { SEEKING_ON = 3, CLOCK_SPAN = 0x7080 };

void race_replay_update(RaceState *race, RaceReplayDirector *w, const RaceReplayUpdateOps *ops, uint32_t camera_keys)
{
    if (camera_keys == REPLAY_END_KEYS && race->end_request != 0 && race->phase != SEEKING_ON && w->frames_played > 0) {
        race->end_state = race->end_request;
        race->camera_roll_add = 0;
        w->unknown_4c5388 = 0;
        return;
    }
    ops->keys(ops->ctx);
    if ((race->tick_flags & FOURTH_TICK) == 0) return;
    race->clock = race->tick % CLOCK_SPAN;
    if (race->phase == RACING) {
        ops->record(ops->ctx);
        w->end_clock = race->tick;
    } else if (race->phase == SEEKING) {
        ops->frame(ops->ctx);
        w->frames_played++;
    } else if (race->phase == SEEKING_ON) {
        if (race->tick >= w->target) {
            race->phase = SEEKING;
            restart_sounds(race);
            w->frames_played++;
            race->unknown_513728 = 1;
        }
        if (race->tick < w->until) ops->step(ops->ctx);
    } else if (race->phase == STOPPED) {
        race_replay_tick_flags(race);
    }
}
