/* port: see records_route.h. */
#include "game/frontend/records_route.h"

#include "formats/stock_content.h"
#include "game/race/car_registry.h"
#include "game/race/difficulty.h"
#include "game/race/weather_snow.h"
#include "game/sys/extras_marks.h"
#include "game/sys/free_laps.h"
#include "game/sys/mod_set.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"
#include "game/sys/race_records_added.h"
#include "game/sys/race_records_merge.h"
#include "game/sys/race_records_tracks.h"
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

/* port (dep59): the added car the player drives in the race running, or NULL (its records are in race_records_added.h's store) */
static const CarPiece *added_player(void) { return car_registry_piece(car_registry_race_player()); }

/* port (dep59): an added car's rows name it by their table's id: their car words 0 on disk, `car` (its number now) on the screens */
static void car_words(RecordRow *r, int32_t car)
{
    if (!race_records_row_set(r)) return;
    memcpy(r->row + RECORD_ROW_CAR, &car, 4);
    memcpy(r->row + RECORD_ROW_CAR_B, &car, 4);
}

static void table_car_words(RecordTable *t, RecordRow *lap, int32_t car)
{
    for (int i = 0; i < ROWS; i++) car_words(&t->totals[i], car);
    car_words(&t->top, car);
    for (int k = 0; k < SEGMENTS; k++) car_words(&t->segment_time[k], car), car_words(&t->segment_speed[k], car);
    if (lap != NULL) car_words(lap, car);
}

/* the original's check on a copy of the image whose track block holds `t` and `lap`, what it took back into them with `m`. The car word is
 * the original's no-records car's for the Warrior (record_check's early return: its times are kept here, with its mark) and an added car
 * (`car_zero`): the check runs as a car the original records (the car word is read by nothing else in the check) */
static int32_t check_table(RouteOp op, const uint8_t *image, const uint8_t *game, int32_t player, const int32_t *times, int car_zero,
                           RecordMarks m, RecordTable *t, RecordRow *lap)
{
    static uint8_t scratch[RECORD_CHECK_IMAGE];
    const int track = g16(image + TRACK);
    memcpy(scratch, image, sizeof scratch);
    uint8_t *const block = scratch + FIRST_BLOCK + BLOCK * track;
    block_from(block, track, t, lap);
    scratch[DIFFICULTY] = 0, scratch[DIFFICULTY + 1] = 0;
    if (car_zero) scratch[CAR] = 0, scratch[CAR + 1] = 0;
    const int32_t answer = original(op, scratch, game, player, times);
    if (answer == nothing(op)) return answer;
    const int segment_word = g16(image + SEGMENT);
    block_to(block, track, op, answer, segment_word ? segment_word - 1 : 2, m, t, lap);
    return answer;
}

/* port (dep59 step 4): a stock car's race into its car's own table too (race_records_added.h), ranked on its own; a car with no id (a
 * records column that is no car) keeps nothing there */
static int32_t route_stock_car(RaceKind kind, RouteOp op, const uint8_t *image, const uint8_t *game, int32_t player, const int32_t *times,
                               RecordTable **table, RecordRow **best_lap)
{
    const int track = g16(image + TRACK), car = g16(image + CAR);
    const char *const id = stock_car_id(car);
    *table = NULL, *best_lap = NULL;
    if (id == NULL) return nothing(op);
    const int cheats = records_has_cheats(kind), laps = records_race_laps(image);
    RecordTable *const t = race_records_added_make(track, kind.layout, cheats, laps, id, NULL);
    RecordRow *const lap = is_stage(track) ? NULL : race_records_added_lap_make(track, kind.layout, cheats, id, NULL);
    if (t == NULL || (!is_stage(track) && lap == NULL)) {
        port_log("records: no table of %s for track %d layout %d cheats %d laps %d\n", id, track, kind.layout, cheats, laps);
        return nothing(op);
    }
    *table = t, *best_lap = lap;
    const int32_t answer = check_table(op, image, game, player, times, car == WARRIOR, records_row_marks(kind), t, lap);
    if (answer != nothing(op) && !race_records_added_save()) port_log("records: race_records_cars.dat could not be saved\n");
    return answer;
}

/* port (dep59 step 4): in an edition of its own, what the check on the car's own table (`t`, `lap`) took, read against the edition's
 * table (race_records_merge.h), which the results screens show: a total's place among its cars' rows (none when it is not in their ten),
 * a lap, top speed or segment kept only while it is still their best. The standard edition: `answer` as it is */
static int32_t edition_answer(RouteOp op, const uint8_t *image, RaceKind kind, int32_t answer, const RecordTable *t, const RecordRow *lap,
                              int32_t standard)
{
    static Edition e;
    const char *ids[EDITION_LIST_MAX];
    const int n = race_records_edition_cars(&e, ids, EDITION_LIST_MAX);
    if (n == 0) return standard;
    if (answer == nothing(op) || t == NULL) return nothing(op);
    const int track = g16(image + TRACK), cheats = records_has_cheats(kind), laps = records_race_laps(image);
    if (op == ROUTE_LAP) {
        RecordRow best;
        return lap != NULL && race_records_cars_lap_merge(track, kind.layout, cheats, ids, n, &best) && race_records_row_index(&best, 1, lap) == 0
                   ? answer : nothing(op);
    }
    static RecordTable joined;
    if (!race_records_cars_merge(track, kind.layout, cheats, laps, ids, n, &joined)) return nothing(op);
    if (op == ROUTE_TOTALS) {
        const int at = answer >= 0 && answer < ROWS ? race_records_row_index(joined.totals, ROWS, &t->totals[answer]) : -1;
        return at >= 0 ? at : nothing(op);
    }
    if (op == ROUTE_TOP) return race_records_row_index(&joined.top, 1, &t->top) == 0 ? answer : nothing(op);
    const int word = g16(image + SEGMENT), k = word ? word - 1 : 2;   /* check_table's segment */
    if (k < 0 || k >= SEGMENTS) return nothing(op);
    int32_t kept = 0;
    if ((answer & 1) && race_records_row_index(&joined.segment_time[k], 1, &t->segment_time[k]) == 0) kept |= 1;
    if ((answer & 2) && race_records_row_index(&joined.segment_speed[k], 1, &t->segment_speed[k]) == 0) kept |= 2;
    return kept;
}

/* port (dep59 step 5, T5): the store and table of a race on a track race_records.dat has no table for (race_records_tracks.h): its id,
 * the player's car's id (an added car's, or a stock car's), the layout without Lunar's rally (Lunar is the rally, its tables are its
 * own: normal, mirror, reverse). 0 when the car has no id or the image's track is none of the nine blocks the check runs on */
typedef struct {
    const char *track, *car, *short_name;
    int layout, cheats, laps, car_zero;
} TrackRace;

static int track_race_of(const char *track, RaceKind kind, const uint8_t *image, TrackRace *out)
{
    const int index = g16(image + TRACK), car = g16(image + CAR);
    const CarPiece *const added = added_player();
    if (index < 0 || index >= RACE_RECORDS_TRACKS) return 0;
    if (added == NULL) kind = records_kind_with_car(kind, car);
    out->track = track;
    out->car = added != NULL ? added->id : stock_car_id(car);
    out->short_name = added != NULL ? added->short_name : NULL;
    out->layout = strcmp(track, stock_track_id(STOCK_TRACK_LUNAR)) == 0 ? kind.layout & ~LAYOUT_RALLY : kind.layout;
    out->cheats = records_has_cheats(kind), out->laps = records_race_laps(image);
    out->car_zero = car == WARRIOR || added != NULL;
    return out->car != NULL;
}

/* port (dep59 step 5, T5): Lunar's race and an added track's keep their records only in race_records_tracks.dat, the player's car's
 * table, ranked there alone (never race_records.dat nor race_records_cars.dat, whose tracks an older version knows) */
static int32_t route_track(const char *track, RaceKind kind, RouteOp op, const uint8_t *image, const uint8_t *game, int32_t player,
                           const int32_t *times)
{
    TrackRace r;
    const int index = g16(image + TRACK);
    if (!track_race_of(track, kind, image, &r)) return nothing(op);
    if (added_player() == NULL) kind = records_kind_with_car(kind, g16(image + CAR));
    RecordTable *const t = race_records_tracks_make(track, r.layout, r.cheats, r.laps, r.car, r.short_name);
    RecordRow *const lap = is_stage(index) ? NULL : race_records_tracks_lap_make(track, r.layout, r.cheats, r.car, r.short_name);
    if (t == NULL || (!is_stage(index) && lap == NULL)) {
        port_log("records: no table of %s on %s layout %d cheats %d laps %d\n", r.car, track, r.layout, r.cheats, r.laps);
        return nothing(op);
    }
    const int32_t answer = check_table(op, image, game, player, times, r.car_zero, records_row_marks(kind), t, lap);
    if (answer == nothing(op)) return answer;
    if (added_player() != NULL) table_car_words(t, lap, 0);
    if (!race_records_tracks_save()) port_log("records: race_records_tracks.dat could not be saved\n");
    return answer;
}

/* Restomod's: the race's table into a copy of the image's track block, the original's check on it, what it took back with the marks.
 * port (dep59): the table is the added car's own when the player drives one (race_records_added.h); a stock car's race goes to its own
 * table too (dep59 step 4), and the answer is race_records.dat's */
static int32_t route_unified(RaceKind kind, RouteOp op, uint8_t *image, const uint8_t *game, int32_t player, const int32_t *times)
{
    const int track = g16(image + TRACK);
    if (op == ROUTE_SEGMENT && !is_stage(track)) return nothing(op);   /* (the exe checks segments on a stage only) */
    if (op == ROUTE_LAP && is_stage(track)) return nothing(op);
    const char *const own_track = race_records_tracks_race();
    if (own_track != NULL) return route_track(own_track, kind, op, image, game, player, times);   /* port (dep59 step 5, T5) */
    const CarPiece *const added = added_player();
    if (added == NULL) kind = records_kind_with_car(kind, g16(image + CAR));   /* (the menus' car word: not the added car's) */
    const int cheats = records_has_cheats(kind), laps = records_race_laps(image);
    RecordTable *const t = added != NULL ? race_records_added_make(track, kind.layout, cheats, laps, added->id, added->short_name)
                                         : race_records_make(track, kind.layout, cheats, laps);
    RecordRow *const lap = is_stage(track) ? NULL
                         : added != NULL ? race_records_added_lap_make(track, kind.layout, cheats, added->id, added->short_name)
                                         : race_records_lap_make(track, kind.layout, cheats);
    if (t == NULL || (!is_stage(track) && lap == NULL)) {
        port_log("records: no table for track %d layout %d cheats %d laps %d\n", track, kind.layout, cheats, records_race_laps(image));
        return nothing(op);
    }
    const int32_t answer = check_table(op, image, game, player, times, g16(image + CAR) == WARRIOR || added != NULL, records_row_marks(kind), t, lap);
    if (added == NULL) {   /* race_records.dat as before, then the car's own table; in an edition, read against the edition's */
        if (answer != nothing(op) && !race_records_save()) port_log("records: race_records.dat could not be saved\n");
        RecordTable *own;
        RecordRow *own_lap;
        const int32_t car_answer = route_stock_car(kind, op, image, game, player, times, &own, &own_lap);
        return edition_answer(op, image, kind, car_answer, own, own_lap, answer);
    }
    if (answer == nothing(op)) return answer;
    table_car_words(t, lap, 0);
    if (!race_records_added_save()) port_log("records: race_records_cars.dat could not be saved\n");
    return edition_answer(op, image, kind, answer, t, lap, answer);
}

int32_t records_route(RaceKind kind, RouteOp op, uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME], int32_t player,
                      const int32_t times[RECORD_CHECK_TIMES])
{
    if (mod_set_session_modified()) {   /* port (dep59, temporary until step 5): --mods served a file that changes the race */
        static bool said;
        if (!said) port_log("records: not kept: --mods served a file that changes the race\n"), said = true;
        return nothing(op);
    }
    /* port (dep59): a race with an added car keeps the player's in that car's own table, in Restomod (route_unified); never in the
     * original's tables, which name a car by its number */
    if (car_registry_race_has_added() && (!port_records_unified() || added_player() == NULL)) return nothing(op);
    const char *const own_track = race_records_tracks_race();
    if (own_track != NULL && strcmp(own_track, stock_track_id(STOCK_TRACK_LUNAR)) != 0 && !port_records_unified())
        return nothing(op);   /* port (dep59 step 5, T5): an added track has no table in config.dat's image (Lunar's is Rusty's there, as ever) */
    if (!port_records_unified()) {
        /* the original's tables mix every race, so a race the original could not run keeps no record there: a reversed track, Easy or
         * Hard, a wet race or a rally on snow (Restomod's own tables keep them, as their layout and marks) */
        if (reverse_track_running() || !difficulty_running_original() || race_grip_nonstandard(port_rain_grip_active(), snow_grip_get()))
            return nothing(op);
        return route_original(op, image, game, player, times);
    }
    return route_unified(kind, op, image, game, player, times);
}

void records_route_shown(RaceKind kind, const uint8_t image[RECORD_CHECK_IMAGE], uint8_t out[RECORD_CHECK_IMAGE], uint8_t slots[RECORDS_MARK_SLOTS],
                         RecordMarks conds[RECORDS_MARK_SLOTS])
{
    memcpy(out, image, RECORD_CHECK_IMAGE);
    memset(slots, 0, RECORDS_MARK_SLOTS);
    if (conds != NULL) memset(conds, 0, sizeof(RecordMarks) * RECORDS_MARK_SLOTS);
    if (!port_records_unified()) return;
    const int track = g16(image + TRACK);
    const CarPiece *const added = car_registry_race_has_added() ? added_player() : NULL;
    if (added == NULL) kind = records_kind_with_car(kind, g16(image + CAR));
    const int cheats = records_has_cheats(kind);
    static RecordTable empty, shown;
    static RecordRow no_lap, shown_lap;
    static Edition e;
    const char *ids[EDITION_LIST_MAX];
    const int cars = race_records_edition_cars(&e, ids, EDITION_LIST_MAX);
    RecordTable *t = added != NULL ? race_records_added_find(track, kind.layout, cheats, records_race_laps(image), added->id)
                                   : race_records_find(track, kind.layout, cheats, records_race_laps(image));
    RecordRow *lap = is_stage(track) ? NULL : added != NULL ? race_records_added_lap_find(track, kind.layout, cheats, added->id)
                                                            : race_records_lap_find(track, kind.layout, cheats);
    const char *const own_track = race_records_tracks_race();
    TrackRace r;
    if (own_track != NULL) {   /* port (dep59 step 5, T5): the player's car's table of race_records_tracks.dat */
        const int known = track_race_of(own_track, kind, image, &r);
        t = known ? race_records_tracks_find(own_track, r.layout, r.cheats, r.laps, r.car) : NULL;
        lap = known && !is_stage(track) ? race_records_tracks_lap_find(own_track, r.layout, r.cheats, r.car) : NULL;
        if (t != NULL) shown = *t, t = &shown;
        if (lap != NULL) shown_lap = *lap, lap = &shown_lap;
        if (added != NULL && t != NULL) table_car_words(t, lap, car_registry_race_player());
    } else if (cars > 0) {   /* port (dep59 step 4): an edition of its own shows its cars' records joined (race_records_merge.h) */
        t = race_records_cars_merge(track, kind.layout, cheats, records_race_laps(image), ids, cars, &shown) ? &shown : NULL;
        lap = !is_stage(track) && race_records_cars_lap_merge(track, kind.layout, cheats, ids, cars, &shown_lap) ? &shown_lap : NULL;
    } else if (added != NULL && t != NULL) {   /* port (dep59): a copy, its rows naming the car by its number now */
        shown = *t, t = &shown;
        if (lap != NULL) shown_lap = *lap, lap = &shown_lap;
        table_car_words(t, lap, car_registry_race_player());
    }
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
    RecordMarks all[RECORDS_MARK_SLOTS];
    memset(all, 0, sizeof all);
    if (lap != NULL) all[SLOT_SEGMENT] = lap->marks;
    if (is_stage(track))
        for (int k = 0; k < SEGMENTS; k++) all[SLOT_SEGMENT + k] = t->segment_time[k].marks, all[SLOT_SPEED_SEGMENT + k] = t->segment_speed[k].marks;
    all[SLOT_TOP] = t->top.marks;
    for (int i = 0; i < ROWS; i++) all[records_slot_totals(track, 0, i)] = t->totals[i].marks;
    for (int s = 0; s < RECORDS_MARK_SLOTS; s++) slots[s] = all[s].cheats;
    if (conds != NULL) memcpy(conds, all, sizeof all);
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
