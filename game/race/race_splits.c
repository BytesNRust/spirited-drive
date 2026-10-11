/* The split times (race_splits.h). */
#include "game/race/race_split_times.h"

#include "game/sys/fixed_math.h"

#include <stddef.h>
#include <string.h>

enum { GAP_SHOWN = 0x5a };  /* ticks a new gap stays up (0045b1d0) */

/* The block as the original's bytes: the counters index it unchecked, so a
 * word is read or written at its byte offset, kept inside the block (port
 * guard: the original reaches the globals around it). */
static int32_t word_at(const RaceSplits *s, int64_t at)
{
    int32_t v = 0;
    if (at >= 0 && at + 4 <= (int64_t)sizeof *s) memcpy(&v, (const uint8_t *)s + at, sizeof v);
    return v;
}

static void set_word(RaceSplits *s, int64_t at, int32_t v)
{
    if (at >= 0 && at + 4 <= (int64_t)sizeof *s) memcpy((uint8_t *)s + at, &v, sizeof v);
}

/* The tick into `ticks`[*counter] once `progress` is past checkpoint
 * [*counter], then the counter on (read again: the tick may have landed on
 * it). */
static void pass(RaceSplits *s, int32_t progress, size_t ticks, size_t counter, int32_t tick)
{
    const int64_t at = (int32_t)((uint32_t)word_at(s, (int64_t)counter) << 2);  /* shl 2, as the original */
    if (progress <= word_at(s, offsetof(RaceSplits, checkpoints) + at)) return;
    set_word(s, (int64_t)ticks + at, tick);
    set_word(s, (int64_t)counter, word_at(s, (int64_t)counter) + 1);
}

void race_splits_pass(RaceState *race, int32_t progress, int32_t rival_progress)
{
    RaceSplits *s = &race->splits;
    pass(s, progress, offsetof(RaceSplits, passed_ticks), offsetof(RaceSplits, passed), race->tick);
    if (race->race_type != 0)
        pass(s, rival_progress, offsetof(RaceSplits, rival_ticks), offsetof(RaceSplits, rival_passed), race->tick);
}

void race_splits_gap(RaceState *race)
{
    RaceSplits *s = &race->splits;
    const int32_t was = s->gap;
    if (s->passed != 0 && s->rival_passed != 0) {
        const int32_t n = s->passed > s->rival_passed ? s->rival_passed : s->passed;
        const int64_t at = (int32_t)((uint32_t)n << 2);
        s->gap = word_at(s, offsetof(RaceSplits, rival_ticks) - 4 + at) -
                 word_at(s, offsetof(RaceSplits, passed_ticks) - 4 + at);
    }
    if (was != s->gap) s->gap_shown = GAP_SHOWN;
    if (s->gap_shown > 0) s->gap_shown--;
}

enum {
    BEST_LAP_NONE = 99999,        /* record +0x1a0, +0x1a4, +0x1b4 */
    BEST_PLACE_NONE = 999,        /* record +0x1a8, +0x1ac */
    LAP_WORDS_FROM = 8,           /* record +8 .. +0x48: laps 1..17 */
    LAP_WORDS_TO = 0x4c,
    CHECKPOINT_ROW = 0x14,        /* 5 words */
    SPLITS_A_LAP = 5,
    RIVAL_TRACKED = 0x55,         /* a time trial's rival counter starts full */
    RIVAL_RECORD_LAPS = 0x4c,     /* the other player's record: its lap words from +0x4c */
    TICK_OFFSET = 300,
    TRACK_RECORDS = 0xa6b,        /* bytes of saved records a track */
    VARIANT_RECORD = 0x27,        /* a stage's: its best time at +0x27 x variant */
    CIRCUIT_RECORDS = 0xea,       /* a circuit's records from +0xea, 0x186 bytes each */
    CIRCUIT_RECORD = 0x186,
    LAST_STAGE = 2,               /* tracks 0..2 are the stages */
    CHECKPOINT_SNAP = 0x27        /* a stage's last checkpoint this close to the loop's end is moved onto it */
};

static int32_t record_word(const RaceState *race, int64_t at)
{
    int32_t v = 0;
    if (at >= 0 && at + 4 <= (int64_t)sizeof race->players) memcpy(&v, (const uint8_t *)race->players + at, sizeof v);
    return v;
}

static void set_record_word(RaceState *race, int64_t at, int32_t v)
{
    if (at >= 0 && at + 4 <= (int64_t)sizeof race->players) memcpy((uint8_t *)race->players + at, &v, sizeof v);
}

static int32_t table_word(int64_t at)
{
    int32_t v = 0;
    if (at >= 0 && at + 4 <= RACE_SPLIT_TABLES_SIZE) memcpy(&v, RACE_SPLIT_TABLES + at, sizeof v);  /* port guard */
    return v;
}

static int32_t saved_word(const RaceState *race, int64_t at)
{
    int32_t v = 0;
    if (race->records != NULL && at >= 0 && at + 4 <= (int64_t)race->records_size)
        memcpy(&v, race->records + at, sizeof v);
    return v;
}

static void clear_record(RaceState *race, int32_t i)
{
    const int64_t r = (int64_t)i * (int64_t)sizeof(PlayerRecord);
    set_record_word(race, r + 0x1a0, BEST_LAP_NONE);
    set_record_word(race, r + 0x1a4, BEST_LAP_NONE);
    set_record_word(race, r + 0x1a8, BEST_PLACE_NONE);
    set_record_word(race, r + 0x1ac, BEST_PLACE_NONE);
    set_record_word(race, r + 0x1b4, BEST_LAP_NONE);
    for (int64_t at = 0x1b0; at <= 0x1cc; at += 4)
        if (at != 0x1b4) set_record_word(race, r + at, 0);
    set_record_word(race, r + (int64_t)sizeof(PlayerRecord), 0);  /* the place kept one record on (00440c30) */
    for (int64_t at = LAP_WORDS_FROM; at < LAP_WORDS_TO; at += 4) set_record_word(race, r + at, 0);
}

/* A circuit's rival: the record's laps spread by the track's factors. */
static void rival_from_circuit_record(RaceState *race, RaceSplits *s)
{
    const int64_t row = (int64_t)race->track * 3 * CHECKPOINT_ROW;
    const int32_t first_lap = table_word(RACE_SPLIT_FACTORS + row + 2 * CHECKPOINT_ROW);
    const int64_t record = (int64_t)race->record_slot * CIRCUIT_RECORD + (int64_t)race->track * TRACK_RECORDS;
    for (int32_t n = 0; n < race->laps * SPLITS_A_LAP; n++) {
        int32_t t;
        if (n < SPLITS_A_LAP) {
            t = fx_mul_round(table_word(RACE_SPLIT_FACTORS + row + 4 * n), first_lap);
        } else {
            const int32_t span = saved_word(race, CIRCUIT_RECORDS + record) - first_lap, laps = race->laps_setting - 1;
            /* port guard: one lap set divides by zero (the original faults), INT_MIN / -1 overflows */
            const int32_t lap = laps == 0 || (laps == -1 && span == INT32_MIN) ? 0 : span / laps;
            t = fx_mul_round(table_word(RACE_SPLIT_FACTORS + row + CHECKPOINT_ROW + 4 * (n % SPLITS_A_LAP)), lap);
            t += (n - SPLITS_A_LAP) / SPLITS_A_LAP * lap + first_lap;
        }
        set_word(s, offsetof(RaceSplits, rival_ticks) + 4 * (int64_t)n, t + TICK_OFFSET);
    }
    set_word(s, offsetof(RaceSplits, rival_ticks) + 4 * (int64_t)race->laps * SPLITS_A_LAP,
             saved_word(race, CIRCUIT_RECORDS + record) + TICK_OFFSET);
}

/* A stage's rival: its best time spread by the track's factors. */
static void rival_from_stage_record(RaceState *race, RaceSplits *s)
{
    const int32_t best = saved_word(race, (int64_t)race->track * TRACK_RECORDS + (int64_t)race->track_variant * VARIANT_RECORD);
    const int64_t row = ((int64_t)race->track * 3 + race->track_variant) * CHECKPOINT_ROW;
    for (int n = 0; n < SPLITS_A_LAP; n++)
        s->rival_ticks[n] = fx_mul_round(table_word(RACE_SPLIT_FACTORS + row + 4 * n), best) + TICK_OFFSET;
    s->rival_ticks[SPLITS_A_LAP] = best + TICK_OFFSET;
    int32_t off = s->checkpoints[4] - race->loop_nodes;
    if (off <= 0) off = -off;
    if (off < CHECKPOINT_SNAP) s->checkpoints[4] = race->loop_nodes;
}

void race_splits_start(RaceState *race)
{
    RaceSplits *s = &race->splits;
    for (int32_t i = 0; i < race->finish_cars; i++) clear_record(race, i);
    const int64_t template_row = ((int64_t)race->track * 3 + race->track_variant) * CHECKPOINT_ROW;
    for (int lap = 0; lap < RACE_SPLITS / SPLITS_A_LAP; lap++)
        for (int k = 0; k < SPLITS_A_LAP; k++)
            s->checkpoints[lap * SPLITS_A_LAP + k] = table_word(template_row + 4 * k) + lap * race->loop_nodes;
    s->passed = 0;
    if (race->race_type != 0) {
        s->rival_passed = 0;
        memset(s->rival_ticks, 0, sizeof s->rival_ticks);
    } else {
        s->rival_passed = RIVAL_TRACKED;
        const int64_t other = race->player == 0 ? (int64_t)sizeof(PlayerRecord) : 0;
        for (int n = 0; n < RACE_SPLITS; n++) {
            int32_t k = n + race->track_variant * SPLITS_A_LAP;
            if (k > RACE_SPLITS - 1) k = RACE_SPLITS - 1;
            s->rival_ticks[n] = record_word(race, other + RIVAL_RECORD_LAPS + 4 * (int64_t)k);
        }
        if (s->rival_ticks[2] == 0) {
            if (race->track > LAST_STAGE) rival_from_circuit_record(race, s);
            else rival_from_stage_record(race, s);
        }
    }
    memset(s->passed_ticks, 0, sizeof s->passed_ticks);
    if (race->track_variant == 0) {
        s->unknown_cc50 = 0;
        set_record_word(race, 0, 0);
        set_record_word(race, 4, 0);
    }
    s->gap = 0;
    s->gap_shown = 0;
}

/* 004c5ce0 + 004fb7f0 x 0x20: the word 0043fbac divides by. The rows are
 * not one table: the first words of the model pace factors (004c5ce0, 60293)
 * and model coast ticks (004c5d00, 3), then default_profile_odds (004c5d20)
 * and police_node (004c5d40). */
static int32_t rescale_by(const RaceState *race, int32_t row)
{
    switch (row) {
    case 0: return 60293;
    case 1: return 3;
    case 2: return race->default_profile_odds;
    case 3: return race->police_node;
    default: return 0;  /* port guard: further rows are other globals */
    }
}

void race_laps_start(RaceState *race, int32_t loop_nodes, int32_t laps)
{
    race->loop_nodes = loop_nodes;
    race->laps = laps;
    race->unknown_509d60 = 0;
    race->first_move_tick = 0;
    race->unknown_509d68 = 0;
    race->end_state = 0;
    race->end_request = 0;
    race->unknown_509d5c = 1;
    const int32_t by = rescale_by(race, race->unknown_4fb7f0), x = race->unknown_509d78;
    if (by != 0 && !(by == -1 && x == INT32_MIN)) {  /* port guard: the original's idiv faults there */
        const int32_t q = x / by, rem = x % by;
        const int64_t frac = ((int64_t)rem * 65536) / by;
        race->unknown_509d78 = (int32_t)((uint32_t)q << 16) + (int32_t)frac;
    }
    race->unknown_509d7c = 0;
    if (race->race_type == 0) {
        race->finish_cars = 1;
        race->finish_humans = 1;
    } else if (race->race_type == 1) {
        race->finish_cars = 2;
        race->finish_humans = race->humans;
    } else if (race->race_type == 2 || race->race_type == 3) {
        race->finish_cars = race->racers;
        race->finish_humans = race->humans;
    }
    for (int32_t i = 0; i < 9; i++) race->results_order[i] = i;
    race_splits_start(race);
    race->unknown_4bc660 = 0;
}
