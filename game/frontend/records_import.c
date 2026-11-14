/* port (dep55): see records_import.h. */
#include "game/frontend/records_import.h"

#include "game/frontend/config_dat.h"
#include "game/frontend/records_route.h"
#include "game/platform/platform.h"
#include "game/sys/free_laps.h"
#include "game/sys/port_log.h"
#include "game/sys/race_records.h"
#include "game/sys/resource.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    FIRST_BLOCK = 0xb5, BLOCK = 0xa6b, ROW = RACE_RECORDS_ROW, ROWS = RACE_RECORDS_ROWS, SEGMENTS = RACE_RECORDS_SEGMENTS, CHOICES = 3,
    CHOICE = 0x186, TOTALS_AT = 0xea, TOP_AT = 0x57c, SEGMENT_SPEED_AT = 0x75, STAGE_TOTALS_AT = 0x5a3, STAGE_SPEEDS_AT = 0x5af,
    /* records.dat (TNFSREC1): the magic, the tournament's marks, then per track, per variant (normal, mirror, rally) its blocks (the
     * normal's with cheats only, the others' without and with) and its 64 marks */
    OLD_MAGIC = 8, OLD_VARIANTS = 3, OLD_MARKS = 64,
    OLD_SIZE = OLD_MAGIC + 1 + RACE_RECORDS_TRACKS * ((2 * OLD_VARIANTS - 1) * BLOCK + OLD_VARIANTS * OLD_MARKS),
    /* free_records.dat (TNFSFRE1): the magic, a u32 count, then each (track, variant, cheats, laps, ten rows, ten marks) */
    FREE_HEADER = OLD_MAGIC + 4, FREE_ENTRY = 4 + CHOICE + ROWS
};

static const uint8_t *block_of(const uint8_t *image, int track) { return image + FIRST_BLOCK + BLOCK * track; }
static int is_stage(int track) { return track < RACE_RECORDS_FIRST_CIRCUIT; }
static int layout_of(int variant) { return variant == 1 ? LAYOUT_MIRROR : variant == 2 ? LAYOUT_RALLY : LAYOUT_NORMAL; }

/* the CD's config.dat (the built-in defaults when there is none): the records the old files' tables started as */
static const uint8_t *cd_image(void)
{
    static ConfigDat c;
    static int done;
    if (done) return c.image;
    done = 1;
    char path[512];
    size_t size = 0;
    snprintf(path, sizeof path, "%s%s", resource_dir(PATH_CONFIG), "config.dat");
    uint8_t *data = platform_read_file(path, &size);
    if (data == NULL) {
        for (char *p = path; *p; p++) *p = (char)toupper((unsigned char)*p);
        data = platform_read_file(path, &size);
    }
    const int ok = data != NULL && config_dat_parse(&c, data, size) == CONFIG_OK;
    if (data != NULL) platform_free_file(data);
    if (!ok) config_dat_defaults(&c, 0);
    return c.image;
}

static RecordMarks cheats_mark(int cheats)
{
    RecordMarks m = { (uint8_t)cheats, 0, 0, 0 };
    return m;
}

/* one row into `to`, with its cheats */
static void take(RecordRow *to, const uint8_t *row, int cheats)
{
    memcpy(to->row, row, ROW);
    to->marks = cheats_mark(cheats);
}

/* config.dat's records of a track as its normal tables without cheats, every row as it is (the CD's too: they are what the player has) */
static int seed_track(const uint8_t *image, int track)
{
    const uint8_t *const block = block_of(image, track);
    int made = 0;
    for (int choice = 0; choice < (is_stage(track) ? 1 : CHOICES); choice++) {
        RecordTable *const t = race_records_make(track, LAYOUT_NORMAL, 0, is_stage(track) ? 0 : free_laps_exe_laps(track, choice));
        if (t == NULL) continue;
        made++;
        for (int i = 0; i < ROWS; i++) take(&t->totals[i], block + TOTALS_AT + CHOICE * choice + ROW * i, 0);
        take(&t->top, block + TOP_AT, 0);
        if (!is_stage(track)) continue;
        for (int k = 0; k < SEGMENTS; k++) {
            take(&t->segment_time[k], block + ROW * k, 0);
            take(&t->segment_speed[k], block + SEGMENT_SPEED_AT + ROW * k, 0);
            memcpy(&t->stage_speeds[k], block + STAGE_SPEEDS_AT + 4 * k, 4);
            memcpy(&t->stage_totals[k], block + STAGE_TOTALS_AT + 4 * k, 4);
        }
    }
    RecordRow *const lap = is_stage(track) ? NULL : race_records_lap_make(track, LAYOUT_NORMAL, 0);
    if (lap != NULL) take(lap, block, 0);
    return made;
}

/* 1 when `row` is one of `n` default rows */
static int is_default(const uint8_t *row, const uint8_t *defaults, int n)
{
    for (int i = 0; i < n; i++)
        if (memcmp(row, defaults + ROW * i, ROW) == 0) return 1;
    return 0;
}

/* the player's rows of a group (the ten totals, or one record) in order, the defaults left out, into `to` (emptied first); how many */
static int take_rows(RecordRow *to, int n, const uint8_t *rows, const uint8_t *defaults, const uint8_t *marks, int cheats)
{
    int kept = 0;
    for (int i = 0; i < n; i++) race_records_row_empty(&to[i]);
    for (int i = 0; i < n; i++) {
        const uint8_t *const row = rows + ROW * i;
        int32_t set;
        memcpy(&set, row + RECORD_ROW_SET, 4);
        if (set == 0 || is_default(row, defaults, n)) continue;
        take(&to[kept++], row, cheats ? (marks != NULL ? marks[i] : 0) : 0);
    }
    return kept;
}

/* a block of records.dat into the tables of (track, layout, cheats); a table is made only for a row of the player's */
static void import_block(int track, int layout, int cheats, const uint8_t *block, const uint8_t *marks)
{
    const uint8_t *const cd = block_of(cd_image(), track);
    for (int choice = 0; choice < (is_stage(track) ? 1 : CHOICES); choice++) {
        RecordTable t;
        uint8_t row_marks[ROWS];
        for (int i = 0; i < ROWS; i++) row_marks[i] = marks[records_slot_totals(track, choice, i)];
        int kept = take_rows(t.totals, ROWS, block + TOTALS_AT + CHOICE * choice, cd + TOTALS_AT + CHOICE * choice, row_marks, cheats);
        kept += take_rows(&t.top, 1, block + TOP_AT, cd + TOP_AT, &marks[SLOT_TOP], cheats);
        memset(t.stage_speeds, 0, sizeof t.stage_speeds), memset(t.stage_totals, 0, sizeof t.stage_totals);
        for (int k = 0; k < SEGMENTS; k++) race_records_row_empty(&t.segment_time[k]), race_records_row_empty(&t.segment_speed[k]);
        if (is_stage(track)) {
            for (int k = 0; k < SEGMENTS; k++) {
                kept += take_rows(&t.segment_time[k], 1, block + ROW * k, cd + ROW * k, &marks[SLOT_SEGMENT + k], cheats);
                kept += take_rows(&t.segment_speed[k], 1, block + SEGMENT_SPEED_AT + ROW * k, cd + SEGMENT_SPEED_AT + ROW * k,
                                  &marks[SLOT_SPEED_SEGMENT + k], cheats);
            }
            if (race_records_row_set(&t.totals[0])) {
                memcpy(t.stage_speeds, block + STAGE_SPEEDS_AT, sizeof t.stage_speeds);
                memcpy(t.stage_totals, block + STAGE_TOTALS_AT, sizeof t.stage_totals);
            }
        }
        if (kept == 0) continue;
        RecordTable *const to = race_records_make(track, layout, cheats, is_stage(track) ? 0 : free_laps_exe_laps(track, choice));
        if (to != NULL) *to = t;
    }
    if (is_stage(track)) return;
    RecordRow lap;
    if (take_rows(&lap, 1, block, cd, &marks[SLOT_SEGMENT], cheats) == 0) return;
    RecordRow *const to = race_records_lap_make(track, layout, cheats);
    if (to != NULL) *to = lap;
}

static int import_records_dat(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file("records.dat", &size);
    if (data == NULL) return 0;
    const int ok = size == OLD_SIZE && memcmp(data, "TNFSREC1", OLD_MAGIC) == 0;
    if (!ok) port_log("records: records.dat is not one of ours, not imported\n");
    const uint8_t *at = data + OLD_MAGIC;
    if (ok) race_records_set_tournament(*at++);
    for (int t = 0; ok && t < RACE_RECORDS_TRACKS; t++)
        for (int v = 0; v < OLD_VARIANTS; v++) {
            const uint8_t *blocks[2] = { NULL, NULL };
            for (int w = v == 0; w < 2; w++) blocks[w] = at, at += BLOCK;
            const uint8_t *const marks = at;
            at += OLD_MARKS;
            for (int w = v == 0; w < 2; w++) import_block(t, layout_of(v), w, blocks[w], marks);
        }
    platform_free_file(data);
    return ok;
}

/* the rows a free table started as: the nearest laps choice's CD rows, each time scaled by laps over its count */
static void free_seed(int track, int laps, uint8_t out[CHOICE])
{
    int best = 0, distance = 1 << 30;
    for (int choice = 0; choice < CHOICES; choice++) {
        const int exe = free_laps_exe_laps(track, choice), d = abs(exe - laps);
        if (exe != 0 && d < distance) best = choice, distance = d;
    }
    const int exe_laps = free_laps_exe_laps(track, best);
    memcpy(out, block_of(cd_image(), track) + TOTALS_AT + CHOICE * best, CHOICE);
    for (int row = 0; row < ROWS && exe_laps > 0; row++) {
        int32_t time;
        memcpy(&time, out + ROW * row + RECORD_ROW_TIME, 4);
        const int64_t v = (int64_t)time * laps / exe_laps;
        time = v > INT32_MAX ? INT32_MAX : (int32_t)v;
        memcpy(out + ROW * row + RECORD_ROW_TIME, &time, 4);
    }
}

static int import_free_records(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file("free_records.dat", &size);
    if (data == NULL) return 0;
    uint32_t n = 0;
    int ok = size >= FREE_HEADER && memcmp(data, "TNFSFRE1", OLD_MAGIC) == 0;
    if (ok) {
        for (int k = 0; k < 4; k++) n |= (uint32_t)data[OLD_MAGIC + k] << (8 * k);
        ok = n <= (size - FREE_HEADER) / FREE_ENTRY && size == FREE_HEADER + (size_t)n * FREE_ENTRY;
    }
    if (!ok) port_log("records: free_records.dat is not one of ours, not imported\n");
    for (uint32_t i = 0; ok && i < n; i++) {
        const uint8_t *const e = data + FREE_HEADER + (size_t)i * FREE_ENTRY;
        const int track = e[0], variant = e[1], cheats = e[2], laps = e[3];
        if (track < RACE_RECORDS_FIRST_CIRCUIT || track >= RACE_RECORDS_TRACKS || variant >= OLD_VARIANTS || cheats > 1 || laps < 1 ||
            laps > RACE_RECORDS_LAPS_MAX)
            continue;
        uint8_t seed[CHOICE];
        free_seed(track, laps, seed);
        RecordRow rows[ROWS];
        if (take_rows(rows, ROWS, e + 4, seed, e + 4 + CHOICE, cheats) == 0) continue;
        RecordTable *const t = race_records_make(track, layout_of(variant), cheats, laps);
        if (t != NULL) memcpy(t->totals, rows, sizeof rows);
    }
    platform_free_file(data);
    return ok;
}

int records_import(const uint8_t config_image[RECORD_CHECK_IMAGE])
{
    int made = 0;
    for (int t = 0; t < RACE_RECORDS_TRACKS; t++) made += seed_track(config_image, t);
    const int old = import_records_dat(), free_ones = import_free_records();
    port_log("records: race_records.dat made from config.dat%s%s\n", old ? ", records.dat" : "", free_ones ? ", free_records.dat" : "");
    if (!race_records_save()) port_log("records: race_records.dat could not be saved\n");
    return made;
}
