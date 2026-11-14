/* port: see records_glue.h. */
#include "game/frontend/records_glue.h"

#include "game/frontend/best_times_pages.h"
#include "game/frontend/records_import.h"
#include "game/platform/platform.h"
#include "game/sys/extras_marks.h"
#include "game/sys/free_laps.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"
#include "game/sys/race_records.h"
#include "game/sys/reverse_track.h"

#include <string.h>

enum { TRACKS = 9, TRACK_WORD = 0x95, CAR_WORD = 0x9b, DIFFICULTY_WORD = 0xb3 };

static RaceKind current;
static int in_tournament, tournament_first, loaded;

void records_glue_load(const uint8_t config_image[RECORD_CHECK_IMAGE])
{
    if (loaded || !port_records_unified() || config_image == NULL) return;
    loaded = 1;
    switch (race_records_load()) {
    case RACE_RECORDS_LOADED: break;
    case RACE_RECORDS_NO_FILE:
        if (platform_user_file_exists("race_records.dat")) {   /* there, but not read (locked, an I/O error): not made again over it */
            race_records_hold(1);
            port_log("records: race_records.dat could not be read; the records are not saved this session\n");
            break;
        }
        records_import(config_image);
        break;
    case RACE_RECORDS_DAMAGED: records_import(config_image); break;   /* started again as on the first run (the damaged file's copy kept) */
    }
}

void records_glue_begin(const uint8_t config_image[RECORD_CHECK_IMAGE], int race_type, const uint8_t *races, int variant_m,
                        int variant_r, int explode, int nitro, int turbo)
{
    records_glue_load(config_image);
    current = records_race_kind(variant_m, variant_r, 0, explode, nitro, turbo);
    in_tournament = race_type == RECORDS_TOURNAMENT;
    tournament_first = 0;
    if (!in_tournament || !port_records_unified()) return;
    int first = 1;
    for (int i = 0; i < 2 * TRACKS; i++) first &= races[i] == 0;
    tournament_first = first;
    if (first) {
        race_records_set_tournament(current.marks);
        if (!race_records_save()) port_log("records: race_records.dat could not be saved\n");
    }
}

RaceKind records_glue_kind(RouteOp op)
{
    RaceKind kind = current;
    if (reverse_track_running()) kind.layout |= LAYOUT_REVERSE;
    if (race_records_lunar()) kind.marks |= EXTRAS_MARK_LUNAR;   /* known once the race started: the tournament's first race keeps it too */
    if (tournament_first && race_records_lunar() && port_records_unified() && !(race_records_tournament() & EXTRAS_MARK_LUNAR)) {
        race_records_set_tournament(race_records_tournament() | EXTRAS_MARK_LUNAR);
        if (!race_records_save()) port_log("records: race_records.dat could not be saved\n");
    }
    if (op == ROUTE_TOTALS && in_tournament && port_records_unified()) kind.marks = race_records_tournament();
    return kind;
}

int32_t records_glue_route(RouteOp op, uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME], int32_t player,
                           const int32_t times[RECORD_CHECK_TIMES])
{
    records_glue_load(image);
    return records_route(records_glue_kind(op), op, image, game, player, times);
}

/* the kind a table shown after `op` is read from: a totals check takes its tournament's cheats (records_glue_kind) */
static RaceKind shown_kind(RouteOp op, const uint8_t *image)
{
    const int car = (int16_t)(image[CAR_WORD] | image[CAR_WORD + 1] << 8);
    return records_kind_with_car(records_glue_kind(op), car);
}

int records_glue_cheats(RouteOp op, const uint8_t *image) { return port_records_unified() && records_has_cheats(shown_kind(op, image)); }

const char *records_glue_line(RouteOp op, const uint8_t *image)
{
    static char line[64];
    if (!port_records_unified()) return NULL;
    const RaceKind kind = shown_kind(op, image);
    records_table_line(kind.layout, records_has_cheats(kind), records_race_laps(image), line, sizeof line);
    return line;
}

BestTimesPlus *records_glue_best_plus(const uint8_t *image, int variant_m, int variant_r, int explode, int nitro, int turbo, int car)
{
    static BestTimesPlus plus;
    if (!port_records_unified()) return NULL;
    records_glue_load(image);
    const RaceKind kind = records_kind_with_car(records_race_kind(variant_m, variant_r, reverse_track_wanted(), explode, nitro, turbo), car);
    const int track = (int16_t)(image[TRACK_WORD] | image[TRACK_WORD + 1] << 8);
    best_times_pages_open(&plus, track, kind.layout, records_has_cheats(kind), port_laps(), (int16_t)(image[DIFFICULTY_WORD] | image[DIFFICULTY_WORD + 1] << 8));
    return &plus;
}

void records_glue_shown_as(RouteOp op, uint8_t out[RECORD_CHECK_IMAGE], const uint8_t *image, ResultsMarks *marks)
{
    static uint8_t slots[RECORDS_MARK_SLOTS];
    records_glue_load(image);
    records_route_shown(shown_kind(op, image), image, out, slots);
    marks->slots = records_glue_cheats(op, image) ? slots : NULL;
    marks->choice = records_choice(image);
    marks->line = records_glue_line(op, image);
}
