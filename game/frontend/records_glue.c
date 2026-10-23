/* port: see records_glue.h. */
#include "game/frontend/records_glue.h"

#include "game/platform/platform.h"

#include <stdio.h>
#include <string.h>

enum { TRACKS = 9, SCANCODE_TAB = 0x0f, FIRST_RECORD = 0xea, SCREEN_RECORDS = 0x19f, TRACK_WORD = 0x95, CAR_WORD = 0x9b,
       BLOCK_AT = 0xb5 };

static RaceKind current;
static int in_tournament, loaded;

static void load_once(void)
{
    if (!loaded) port_records_load(), loaded = 1;
}

void records_glue_begin(int race_type, const uint8_t *races, int variant_m, int variant_r, int explode, int nitro,
                        int turbo)
{
    load_once();
    current = records_race_kind(variant_m, variant_r, explode, nitro, turbo);
    in_tournament = race_type == RECORDS_TOURNAMENT;
    if (!in_tournament) return;
    int first = 1;
    for (int i = 0; i < 2 * TRACKS; i++) first &= races[i] == 0;
    if (first) {
        port_records_set_tournament(current.marks);
        if (!port_records_save()) fprintf(stderr, "records: records.dat could not be saved\n");
    }
}

RaceKind records_glue_kind(RouteOp op)
{
    load_once();
    RaceKind kind = current;
    if (op == ROUTE_TOTALS && in_tournament) kind.marks = port_records_tournament();
    return kind;
}

int32_t records_glue_route(RouteOp op, uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME],
                           int32_t player, const int32_t times[RECORD_CHECK_TIMES])
{
    return records_route(records_glue_kind(op), op, image, game, player, times);
}

void records_glue_shown(uint8_t out[RECORD_CHECK_IMAGE], const uint8_t *image)
{
    const int track = (int16_t)(image[TRACK_WORD] | image[TRACK_WORD + 1] << 8);
    const RaceKind kind = records_kind_with_car(records_glue_kind(ROUTE_SEGMENT), (int16_t)(image[CAR_WORD] | image[CAR_WORD + 1] << 8));
    memcpy(out, image, RECORD_CHECK_IMAGE);
    if (kind.variant == RECORDS_NORMAL && kind.marks == 0) return;
    const uint8_t *block = port_records_block(track, kind.variant, kind.marks != 0);
    if (block != NULL) memcpy(out + BLOCK_AT + RECORDS_BLOCK * track, block, RECORDS_BLOCK);
}

/* a table's records as the screen reads them: the tracks' blocks from their first record, 0xa6b apart */
static void view_of(uint8_t view[BEST_TIMES_RECORDS], RecordVariant v, int with_cheats)
{
    memset(view, 0, BEST_TIMES_RECORDS);
    for (int t = 0; t < TRACKS; t++) {
        const uint8_t *block = port_records_block(t, v, with_cheats);
        size_t n = RECORDS_BLOCK - FIRST_RECORD;
        if (block == NULL) continue;
        if (t * (size_t)RECORDS_BLOCK + n > BEST_TIMES_RECORDS) n = BEST_TIMES_RECORDS - t * (size_t)RECORDS_BLOCK;
        memcpy(view + t * RECORDS_BLOCK, block + FIRST_RECORD, n);
    }
}

BestTimesPlus *records_glue_best_plus(const uint8_t *image, int variant_m, int variant_r)
{
    static BestTimesPlus plus;
    static uint8_t views[2][BEST_TIMES_RECORDS];
    load_once();
    const RaceKind kind = records_race_kind(variant_m, variant_r, 0, 0, 0);
    plus.cheats = 0;
    if (kind.variant == RECORDS_NORMAL) {
        plus.records[0] = image + SCREEN_RECORDS;
    } else {
        view_of(views[0], kind.variant, 0);
        plus.records[0] = views[0];
    }
    view_of(views[1], kind.variant, 1);
    plus.records[1] = views[1];
    for (int t = 0; t < TRACKS; t++) plus.marks[t] = port_records_marks(t, kind.variant);
    return &plus;
}

int32_t records_glue_toggle(void *ctx)
{
    static int was;
    (void)ctx;
    const int down = platform_key_down(SCANCODE_TAB);
    const int pressed = down && !was;
    was = down;
    return pressed;
}
