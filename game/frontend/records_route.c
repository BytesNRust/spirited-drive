/* port: see records_route.h. */
#include "game/frontend/records_route.h"

#include "game/race/difficulty.h"
#include "game/race/weather_snow.h"
#include "game/sys/extras_marks.h"
#include "game/sys/free_laps.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"
#include "game/sys/reverse_track.h"

#include <stdio.h>
#include <string.h>

enum {
    TRACK = 0x95, CAR = 0x9b, SEGMENT = 0x93, DIFFICULTY = 0xb3, FIRST_BLOCK = 0xb5, BLOCK = 0xa6b, ROWS = RACE_RECORDS_ROWS,
    ROW = RACE_RECORDS_ROW, SEGMENTS = RACE_RECORDS_SEGMENTS, WARRIOR = 11,
    /* a track's block, from its start (record_check.c's offsets less FIRST_BLOCK) */
    SEGMENT_TIME_AT = 0, SEGMENT_SPEED_AT = 0x75, TOTALS_AT = 0xea, TOP_AT = 0x57c, STAGE_TOTALS_AT = 0x5a3, STAGE_SPEEDS_AT = 0x5af
};

static int16_t g16(const uint8_t *at) { return (int16_t)(uint16_t)(at[0] | at[1] << 8); }
static int is_stage(int track) { return track < RACE_RECORDS_FIRST_CIRCUIT; }

int records_slot_totals(int track, int choice, int row)
{
    if (row < 0 || row >= ROWS || choice < 0 || choice > 2) return -1;
    return SLOT_TOTALS + (!is_stage(track) ? SLOT_CHOICE * choice : 0) + row;
}

int records_choice(const uint8_t image[RECORD_CHECK_IMAGE])
{
    if (port_records_unified()) return 0;
    const int menu = g16(image + DIFFICULTY);
    const int free = free_laps_count() != 0 ? free_laps_choice(g16(image + TRACK), free_laps_count()) : -1;
    return free >= 0 ? free : menu;
}

int records_has_cheats(RaceKind kind) { return kind.marks != 0; }

RaceKind records_kind_with_car(RaceKind kind, int car)
{
    if (car == WARRIOR) kind.marks |= EXTRAS_MARK_WARRIOR;
    return kind;
}

RaceKind records_race_kind(int variant_m, int variant_r, int reversed, int explode, int nitro, int turbo)
{
    RaceKind k = { (variant_m ? LAYOUT_MIRROR : 0) | (reversed ? LAYOUT_REVERSE : 0) | (variant_r ? LAYOUT_RALLY : 0), 0 };
    if (nitro) k.marks |= EXTRAS_MARK_NITRO;
    if (explode) k.marks |= EXTRAS_MARK_EXPLODE;
    if (turbo) k.marks |= EXTRAS_MARK_TURBO;
    return k;
}

int records_race_laps(const uint8_t image[RECORD_CHECK_IMAGE])
{
    const int track = g16(image + TRACK);
    if (is_stage(track)) return 0;
    return free_laps_count() != 0 ? free_laps_count() : free_laps_exe_laps(track, g16(image + DIFFICULTY));
}

RecordMarks records_row_marks(RaceKind kind)
{
    RecordMarks m = race_records_conditions();
    m.cheats = (uint8_t)kind.marks;
    return m;
}

static int32_t original(RouteOp op, uint8_t *image, const uint8_t *game, int32_t player, const int32_t *times)
{
    switch (op) {
    case ROUTE_SEGMENT: return record_check(image, game, player);
    case ROUTE_LAP: return record_check_lap(image, game, player);
    case ROUTE_TOP: return record_check_top(image, game, player);
    default: return record_check_totals(image, game, player, times);
    }
}

/* the answer when there was nothing to take */
static int32_t nothing(RouteOp op) { return op == ROUTE_TOTALS ? RECORD_CHECK_NONE : 0; }

/* The original's records (Stock, Restored): config.dat's image, every race mixed. A free race has no segment record and a total only for
 * a lap count the track has (its choice's table, through a copy whose choice word is that choice). */
static int32_t route_original(RouteOp op, uint8_t *image, const uint8_t *game, int32_t player, const int32_t *times)
{
    static uint8_t scratch[RECORD_CHECK_IMAGE];
    const int track = g16(image + TRACK);
    if (free_laps_count() == 0) return original(op, image, game, player, times);
    if (op == ROUTE_SEGMENT || (op == ROUTE_TOTALS && free_laps_choice(track, free_laps_count()) < 0)) return nothing(op);
    memcpy(scratch, image, sizeof scratch);
    scratch[DIFFICULTY] = (uint8_t)records_choice(image), scratch[DIFFICULTY + 1] = 0;
    const int32_t answer = original(op, scratch, game, player, times);
    if (answer != nothing(op)) memcpy(image + FIRST_BLOCK + BLOCK * track, scratch + FIRST_BLOCK + BLOCK * track, BLOCK);
    return answer;
}

/* the rows of a table (and its best lap) at their places in a track's block, the choice word 0, as the screens and the checks read it */
static void block_from(uint8_t *block, int track, const RecordTable *t, const RecordRow *lap)
{
    for (int i = 0; i < ROWS; i++) memcpy(block + TOTALS_AT + ROW * i, t->totals[i].row, ROW);
    memcpy(block + TOP_AT, t->top.row, ROW);
    if (is_stage(track)) {
        for (int k = 0; k < SEGMENTS; k++) {
            memcpy(block + SEGMENT_TIME_AT + ROW * k, t->segment_time[k].row, ROW);
            memcpy(block + SEGMENT_SPEED_AT + ROW * k, t->segment_speed[k].row, ROW);
            memcpy(block + STAGE_SPEEDS_AT + 4 * k, &t->stage_speeds[k], 4);
            memcpy(block + STAGE_TOTALS_AT + 4 * k, &t->stage_totals[k], 4);
        }
    } else if (lap != NULL) {
        memcpy(block + SEGMENT_TIME_AT, lap->row, ROW);
    }
}

/* what a check took, back from the block into the table, with the race's marks; a totals row taken at `row` pushes the ones below down */
static void block_to(const uint8_t *block, int track, RouteOp op, int32_t answer, int segment, RecordMarks m, RecordTable *t, RecordRow *lap)
{
    if (op == ROUTE_SEGMENT) {
        if (answer & 1) memcpy(t->segment_time[segment].row, block + SEGMENT_TIME_AT + ROW * segment, ROW), t->segment_time[segment].marks = m;
        if (answer & 2) memcpy(t->segment_speed[segment].row, block + SEGMENT_SPEED_AT + ROW * segment, ROW), t->segment_speed[segment].marks = m;
    } else if (op == ROUTE_LAP) {
        memcpy(lap->row, block + SEGMENT_TIME_AT, ROW), lap->marks = m;
    } else if (op == ROUTE_TOP) {
        memcpy(t->top.row, block + TOP_AT, ROW), t->top.marks = m;
    } else {
        for (int j = ROWS - 1; j > answer; j--) t->totals[j].marks = t->totals[j - 1].marks;
        t->totals[answer].marks = m;
        for (int i = 0; i < ROWS; i++) memcpy(t->totals[i].row, block + TOTALS_AT + ROW * i, ROW);
        if (is_stage(track))
            for (int k = 0; k < SEGMENTS; k++) {
                memcpy(&t->stage_speeds[k], block + STAGE_SPEEDS_AT + 4 * k, 4);
                memcpy(&t->stage_totals[k], block + STAGE_TOTALS_AT + 4 * k, 4);
            }
    }
}

/* Restomod's: the race's table into a copy of the image's track block, the original's check on it, what it took back with the marks */
static int32_t route_unified(RaceKind kind, RouteOp op, uint8_t *image, const uint8_t *game, int32_t player, const int32_t *times)
{
    static uint8_t scratch[RECORD_CHECK_IMAGE];
    const int track = g16(image + TRACK);
    if (op == ROUTE_SEGMENT && !is_stage(track)) return nothing(op);   /* (the exe checks segments on a stage only) */
    if (op == ROUTE_LAP && is_stage(track)) return nothing(op);
    kind = records_kind_with_car(kind, g16(image + CAR));
    const int cheats = records_has_cheats(kind);
    RecordTable *const t = race_records_make(track, kind.layout, cheats, records_race_laps(image));
    RecordRow *const lap = is_stage(track) ? NULL : race_records_lap_make(track, kind.layout, cheats);
    if (t == NULL || (!is_stage(track) && lap == NULL)) {
        port_log("records: no table for track %d layout %d cheats %d laps %d\n", track, kind.layout, cheats, records_race_laps(image));
        return nothing(op);
    }
    memcpy(scratch, image, sizeof scratch);
    uint8_t *const block = scratch + FIRST_BLOCK + BLOCK * track;
    block_from(block, track, t, lap);
    scratch[DIFFICULTY] = 0, scratch[DIFFICULTY + 1] = 0;
    /* record_check's early return is the original's no-records car: the Warrior's times are kept here, with its mark, so the check runs
     * as a car the original records (the car word is read by nothing else in the check) */
    if (g16(scratch + CAR) == WARRIOR) scratch[CAR] = 0, scratch[CAR + 1] = 0;
    const int32_t answer = original(op, scratch, game, player, times);
    if (answer == nothing(op)) return answer;
    const int segment_word = g16(image + SEGMENT);
    block_to(block, track, op, answer, segment_word ? segment_word - 1 : 2, records_row_marks(kind), t, lap);
    if (!race_records_save()) port_log("records: race_records.dat could not be saved\n");
    return answer;
}

int32_t records_route(RaceKind kind, RouteOp op, uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME], int32_t player,
                      const int32_t times[RECORD_CHECK_TIMES])
{
    if (!port_records_unified()) {
        /* the original's tables mix every race, so a race the original could not run keeps no record there: a reversed track, Easy or
         * Hard, a wet race or a rally on snow (Restomod's own tables keep them, as their layout and marks) */
        if (reverse_track_running() || !difficulty_running_original() || race_grip_nonstandard(port_rain_grip_active(), snow_grip_get()))
            return nothing(op);
        return route_original(op, image, game, player, times);
    }
    return route_unified(kind, op, image, game, player, times);
}

void records_route_shown(RaceKind kind, const uint8_t image[RECORD_CHECK_IMAGE], uint8_t out[RECORD_CHECK_IMAGE], uint8_t slots[RECORDS_MARK_SLOTS])
{
    memcpy(out, image, RECORD_CHECK_IMAGE);
    memset(slots, 0, RECORDS_MARK_SLOTS);
    if (!port_records_unified()) return;
    const int track = g16(image + TRACK);
    kind = records_kind_with_car(kind, g16(image + CAR));
    const int cheats = records_has_cheats(kind);
    static RecordTable empty;
    static RecordRow no_lap;
    RecordTable *t = race_records_find(track, kind.layout, cheats, records_race_laps(image));
    RecordRow *lap = is_stage(track) ? NULL : race_records_lap_find(track, kind.layout, cheats);
    if (t == NULL) {   /* no table yet: the screens show it empty (nothing is made) */
        for (int i = 0; i < ROWS; i++) race_records_row_empty(&empty.totals[i]);
        race_records_row_empty(&empty.top);
        for (int k = 0; k < SEGMENTS; k++) race_records_row_empty(&empty.segment_time[k]), race_records_row_empty(&empty.segment_speed[k]);
        t = &empty;
    }
    if (lap == NULL && !is_stage(track)) race_records_row_empty(&no_lap), lap = &no_lap;
    uint8_t *const block = out + FIRST_BLOCK + BLOCK * track;
    block_from(block, track, t, lap);
    out[DIFFICULTY] = 0, out[DIFFICULTY + 1] = 0;
    if (lap != NULL) slots[SLOT_SEGMENT] = lap->marks.cheats;
    if (is_stage(track))
        for (int k = 0; k < SEGMENTS; k++) slots[SLOT_SEGMENT + k] = t->segment_time[k].marks.cheats, slots[SLOT_SPEED_SEGMENT + k] = t->segment_speed[k].marks.cheats;
    slots[SLOT_TOP] = t->top.marks.cheats;
    for (int i = 0; i < ROWS; i++) slots[records_slot_totals(track, 0, i)] = t->totals[i].marks.cheats;
}

void records_layout_text(int layout, char *out, int size)
{
    static const char *const WORDS[] = { "MIRROR", "REVERSE", "RALLY" };
    int n = 0;
    out[0] = '\0';
    for (int b = 0; b < 3; b++)
        if (layout & (1 << b)) n += snprintf(out + n, (size_t)(size > n ? size - n : 0), "%s%s", n ? " + " : "", WORDS[b]);
    if (n == 0) snprintf(out, (size_t)size, "NORMAL");
}

void records_table_line(int layout, int cheats, int laps, char *out, int size)
{
    char words[32];
    records_layout_text(layout, words, sizeof words);
    if (laps > 0) snprintf(out, (size_t)size, "%s - %s - %d LAP%s", words, cheats ? "CHEATS" : "NO CHEATS", laps, laps == 1 ? "" : "S");
    else snprintf(out, (size_t)size, "%s - %s", words, cheats ? "CHEATS" : "NO CHEATS");
}

void records_mark_text(int marks, char out[6])
{
    int n = 0;
    if (marks & EXTRAS_MARK_NITRO) out[n++] = 'N';
    if (marks & EXTRAS_MARK_EXPLODE) out[n++] = 'E';
    if (marks & EXTRAS_MARK_TURBO) out[n++] = 'T';
    if (marks & EXTRAS_MARK_WARRIOR) out[n++] = 'W';
    if (marks & EXTRAS_MARK_LUNAR) out[n++] = 'L';
    out[n] = '\0';
}
