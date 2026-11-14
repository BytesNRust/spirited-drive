/* port (dep55): see best_times_pages.h. */
#include "game/frontend/best_times_pages.h"

#include "game/frontend/records_route.h"
#include "game/race/weather.h"
#include "game/sys/free_laps.h"
#include "game/sys/port_settings.h"
#include "game/sys/race_records.h"
#include "game/sys/text.h"

#include <stdio.h>
#include <string.h>

enum {
    TEXT = 0x50, ROWS = RACE_RECORDS_ROWS, ROW = RACE_RECORDS_ROW, FIRST_CIRCUIT = RACE_RECORDS_FIRST_CIRCUIT,
    /* the exe's columns and rows (best_times.c), and the pages' own: the cheats' letters after a shortened name, the conditions in small
     * words in the gap between the time and the car */
    NAME_X = 8, NAME_WIDTH = 0xd4, MARK_X = 0xe0, TIME_X = 0x118, COND_END = 0x1da, CAR_X = 0x1e0, SHIFT_X = 0x24e,
    ROW_Y = 0x73, ROW_STEP = 0x16, TOP_Y = 0x17c, SMALL_DROP = 5,
    /* the strip, at three quarters the words' size (the longest table runs some 500 px): the page's line centred just above the
     * footer's Road & Track item (between its arrows, so their hit rects stay apart), the table's line right of the "Best times" header */
    STRIP_NUM = 3, STRIP_DEN = 4, STRIP_SMALL_DEN = 5, STRIP_RIGHT = 0x27c, PAGE_CENTRE_X = 311, PAGE_Y = 430, TABLE_X = 0x8c, TABLE_Y = 0x4c, HIT_ABOVE = 4, HIT_BELOW = 16, HIT_PAD = 5,
    NO_RECORDS_Y = ROW_Y + 4 * ROW_STEP, CENTRE_X = 320
};

static const char *const PAGE_NAME[2][2] = { { "TOP 10", "BEST LAP" }, { "TOP 10", "SEGMENTS" } };   /* a circuit's, a stage's */
static const char *const CAR[12] = { "RX-7", "NSX", "SUPRA", "911", "ZR-1", "VIPER", "512TR", "DIABLO", "PACK", "CLOCK", "CLASS", "WARRIOR" };

static int is_stage(int track) { return track < FIRST_CIRCUIT; }
static int32_t d32(const uint8_t *p) { return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24); }
static const char *car_name(int32_t car) { return car >= 0 && car < 12 ? CAR[car] : ""; }

int best_times_pages_count(int track) { (void)track; return 2; }

void best_times_pages_open(BestTimesPlus *plus, int track, int layout, int cheats, int laps, int laps_choice)
{
    plus->layout = layout & (RACE_RECORDS_LAYOUTS - 1);
    plus->cheats = cheats != 0;
    plus->page = 0;
    plus->focus = BEST_FOCUS_FOOTER;
    best_times_pages_track(plus, track, laps, laps_choice);
}

void best_times_pages_track(BestTimesPlus *plus, int track, int free_laps, int laps_choice)
{
    plus->laps = is_stage(track) ? 0 : free_laps > 0 ? free_laps : free_laps_exe_laps(track, laps_choice);
    if (plus->page >= best_times_pages_count(track)) plus->page = 0;
    if (is_stage(track) && plus->focus == BEST_FOCUS_LAPS) plus->focus = BEST_FOCUS_PAGE;
}

/* the lap counts the laps field steps through, ascending: the track's own three, those with a table, and the one shown */
static int lap_counts(const BestTimesPlus *plus, int track, int out[RACE_RECORDS_LAPS_MAX])
{
    int have[RACE_RECORDS_LAPS_MAX + 1] = { 0 }, tables[RACE_RECORDS_LAPS_MAX], n = 0;
    for (int choice = 0; choice < 3; choice++) have[free_laps_exe_laps(track, choice)] = 1;
    const int k = race_records_laps(track, plus->layout, plus->cheats, tables, RACE_RECORDS_LAPS_MAX);
    for (int i = 0; i < k; i++) have[tables[i]] = 1;
    if (plus->laps >= 1 && plus->laps <= RACE_RECORDS_LAPS_MAX) have[plus->laps] = 1;
    for (int laps = 1; laps <= RACE_RECORDS_LAPS_MAX; laps++)
        if (have[laps]) out[n++] = laps;
    return n;
}

static int round_step(int value, int dir, int count) { return ((value + dir) % count + count) % count; }

void best_times_pages_act(BestTimesPlus *plus, int track, int item)
{
    switch (item) {
    case BEST_PAGES_LAYOUT_DOWN: plus->layout = round_step(plus->layout, -1, RACE_RECORDS_LAYOUTS); break;
    case BEST_PAGES_LAYOUT_UP: plus->layout = round_step(plus->layout, 1, RACE_RECORDS_LAYOUTS); break;
    case BEST_PAGES_CHEATS:
    case BEST_PAGES_CHEATS_DOWN:
    case BEST_PAGES_CHEATS_UP: plus->cheats ^= 1; break;
    case BEST_PAGES_PAGE_DOWN: plus->page = round_step(plus->page, -1, best_times_pages_count(track)); break;
    case BEST_PAGES_PAGE_UP: plus->page = round_step(plus->page, 1, best_times_pages_count(track)); break;
    case BEST_PAGES_LAPS_DOWN:
    case BEST_PAGES_LAPS_UP: {
        if (is_stage(track)) break;
        int counts[RACE_RECORDS_LAPS_MAX], at = 0;
        const int n = lap_counts(plus, track, counts);
        for (int i = 0; i < n; i++)
            if (counts[i] == plus->laps) at = i;
        plus->laps = counts[round_step(at, item == BEST_PAGES_LAPS_UP ? 1 : -1, n)];
        break;
    }
    default: break;
    }
}

void best_times_pages_step(BestTimesPlus *plus, int track, int dir)
{
    static const int DOWN[] = { BEST_PAGES_LAYOUT_DOWN, BEST_PAGES_CHEATS_DOWN, BEST_PAGES_LAPS_DOWN, BEST_PAGES_PAGE_DOWN };
    static const int UP[] = { BEST_PAGES_LAYOUT_UP, BEST_PAGES_CHEATS_UP, BEST_PAGES_LAPS_UP, BEST_PAGES_PAGE_UP };
    if (plus->focus < BEST_FOCUS_LAYOUT || plus->focus > BEST_FOCUS_PAGE) return;
    best_times_pages_act(plus, track, (dir < 0 ? DOWN : UP)[plus->focus]);
}

void best_times_pages_focus(BestTimesPlus *plus, int track, int dir)
{
    plus->focus = round_step(plus->focus, dir, BEST_FOCUS_FOOTER + 1);
    if (is_stage(track) && plus->focus == BEST_FOCUS_LAPS) plus->focus = round_step(plus->focus, dir, BEST_FOCUS_FOOTER + 1);
}

void best_times_conditions_text(RecordMarks m, char *out, int size)
{
    static const char *const WEATHER[4] = { "CLR", "FOG", "RAIN", "STRM" };   /* four letters at most: the gap before the car is some 105 px */
    static const char *const TIME[4] = { "DAY", "DAWN", "DUSK", "NGT" };
    static const char *const LEVEL[3] = { "O", "E", "H" };   /* Original, Easy, Hard */
    if (!(m.weather & RECORD_MARK_KNOWN)) {
        snprintf(out, (size_t)size, "-");
        return;
    }
    const int kind = m.weather & RECORD_WEATHER_KIND;
    const char *weather = m.weather & RECORD_WEATHER_SNOW ? "SNOW" : m.weather & RECORD_WEATHER_WET ? "WET" : kind < 4 ? WEATHER[kind] : "?";
    const int t = m.time & 0x0f;
    const char *time = t < 4 ? TIME[t] : "?";
    const int rivals = m.difficulty & 3, police = m.difficulty >> 2 & 3, traffic = m.difficulty >> 4 & 3;
    const char *level = rivals == police && police == traffic && rivals < 3 ? LEVEL[rivals] : "M";
    snprintf(out, (size_t)size, "%s %s %s", weather, time, level);
}

/* a word of the strip at x, lit or not, its hit rect kept; the x after it */
static int strip_den = STRIP_DEN;   /* the size the strip's line now drawn takes (STRIP_NUM / strip_den) */

static int word(const BestTimesOps *ops, const char *text, int x, int y, int lit, BestPagesHits *hits, int item)
{
    const int w = ops->plus_scaled_width(ops->ctx, text, STRIP_NUM, strip_den);
    ops->plus_scaled(ops->ctx, lit ? TEXT_YELLOW : TEXT_CREAM, text, x, y, STRIP_NUM, strip_den);
    if (item >= 0) {
        hits->x0[item] = (int16_t)(x - HIT_PAD), hits->x1[item] = (int16_t)(x + w + HIT_PAD);
        hits->y0[item] = (int16_t)(y - HIT_ABOVE), hits->y1[item] = (int16_t)(y + HIT_BELOW);
    }
    return x + w;
}

static void strip(const BestTimesState *st, int track, int lit, BestPagesHits *hits, const BestTimesOps *ops)
{
    const BestTimesPlus *const p = st->plus;
    char text[TEXT], layout[TEXT], whole[2 * TEXT];
    /* the table's line at three quarters, or at three fifths when that runs past the right edge (all three layouts) */
    records_layout_text(p->layout, layout, sizeof layout);
    snprintf(whole, sizeof whole, "< %s > - < %s > - < 99 LAPS >", layout, p->cheats ? "CHEATS" : "NO CHEATS");
    strip_den = TABLE_X + ops->plus_scaled_width(ops->ctx, whole, STRIP_NUM, STRIP_DEN) > STRIP_RIGHT ? STRIP_SMALL_DEN : STRIP_DEN;
    int gap = ops->plus_scaled_width(ops->ctx, " ", STRIP_NUM, strip_den);
    int x = TABLE_X;
    const int on_layout = p->focus == BEST_FOCUS_LAYOUT, on_cheats = p->focus == BEST_FOCUS_CHEATS, on_laps = p->focus == BEST_FOCUS_LAPS;
    x = word(ops, "<", x, TABLE_Y, on_layout || lit == BEST_PAGES_LAYOUT_DOWN, hits, BEST_PAGES_LAYOUT_DOWN) + gap;
    x = word(ops, layout, x, TABLE_Y, on_layout, hits, -1) + gap;
    x = word(ops, ">", x, TABLE_Y, on_layout || lit == BEST_PAGES_LAYOUT_UP, hits, BEST_PAGES_LAYOUT_UP) + gap;
    x = word(ops, "-", x, TABLE_Y, 0, hits, -1) + gap;
    x = word(ops, "<", x, TABLE_Y, on_cheats || lit == BEST_PAGES_CHEATS_DOWN, hits, BEST_PAGES_CHEATS_DOWN) + gap;
    x = word(ops, p->cheats ? "CHEATS" : "NO CHEATS", x, TABLE_Y, on_cheats || lit == BEST_PAGES_CHEATS, hits, BEST_PAGES_CHEATS) + gap;
    x = word(ops, ">", x, TABLE_Y, on_cheats || lit == BEST_PAGES_CHEATS_UP, hits, BEST_PAGES_CHEATS_UP) + gap;
    x = word(ops, "-", x, TABLE_Y, 0, hits, -1) + gap;
    if (is_stage(track)) {
        (void)word(ops, "OPEN ROAD", x, TABLE_Y, 0, hits, -1);
        hits->x0[BEST_PAGES_LAPS_DOWN] = hits->x1[BEST_PAGES_LAPS_DOWN] = hits->x0[BEST_PAGES_LAPS_UP] = hits->x1[BEST_PAGES_LAPS_UP] = -1;
    } else {
        x = word(ops, "<", x, TABLE_Y, on_laps || lit == BEST_PAGES_LAPS_DOWN, hits, BEST_PAGES_LAPS_DOWN) + gap;
        snprintf(text, sizeof text, "%d LAP%s", p->laps, p->laps == 1 ? "" : "S");
        x = word(ops, text, x, TABLE_Y, on_laps, hits, -1) + gap;
        (void)word(ops, ">", x, TABLE_Y, on_laps || lit == BEST_PAGES_LAPS_UP, hits, BEST_PAGES_LAPS_UP);
    }
    /* the page's line, centred just above the footer's Road & Track item (its arrows' pointer spots 231 and 390) */
    strip_den = STRIP_DEN;
    gap = ops->plus_scaled_width(ops->ctx, " ", STRIP_NUM, strip_den);
    const int on_page = p->focus == BEST_FOCUS_PAGE;
    const char *const name = PAGE_NAME[is_stage(track)][p->page];
    const int arrow = ops->plus_scaled_width(ops->ctx, ">", STRIP_NUM, STRIP_DEN);
    x = PAGE_CENTRE_X - (2 * arrow + 2 * gap + ops->plus_scaled_width(ops->ctx, name, STRIP_NUM, STRIP_DEN)) / 2;
    x = word(ops, "<", x, PAGE_Y, on_page || lit == BEST_PAGES_PAGE_DOWN, hits, BEST_PAGES_PAGE_DOWN) + gap;
    x = word(ops, name, x, PAGE_Y, on_page, hits, -1) + gap;
    (void)word(ops, ">", x, PAGE_Y, on_page || lit == BEST_PAGES_PAGE_UP, hits, BEST_PAGES_PAGE_UP);
}

/* the gearbox's mark: 0 automatic, 1 manual */
static void shift(const BestTimesInputs *in, int32_t gearbox, int32_t y, const BestTimesOps *ops)
{
    if (gearbox == 0) ops->text_shadowed(ops->ctx, in->german ? "  z" : "t.t.", SHIFT_X, y);
    else if (gearbox == 1) ops->text_shadowed(ops->ctx, in->german ? "  d" : "h.h.", SHIFT_X, y);
}

/* a record's row at y: the name, its cheats, the time (or the speed), its conditions, the car, the gearbox */
static void record_row(const BestTimesInputs *in, const RecordRow *r, int32_t y, int speed, const BestTimesOps *ops)
{
    char text[TEXT];
    ops->text_select(ops->ctx, 1);
    ops->text_fit(ops->ctx, (const char *)r->row, NAME_X, y, NAME_WIDTH);
    records_mark_text(r->marks.cheats, text);
    if (text[0] != '\0') ops->text_shadowed(ops->ctx, text, MARK_X, y);
    if (speed) ops->speed_text(ops->ctx, text, d32(r->row + RECORD_ROW_SPEED));
    else ops->time_text(ops->ctx, text, d32(r->row + RECORD_ROW_TIME));
    ops->text_shadowed(ops->ctx, text, TIME_X, y);
    best_times_conditions_text(r->marks, text, sizeof text);
    /* right against the car's column, half size (two fifths after a speed, which runs wider than a time) */
    const int num = speed ? 2 : 1, den = speed ? 5 : 2;
    ops->plus_scaled(ops->ctx, TEXT_CREAM, text, COND_END - ops->plus_scaled_width(ops->ctx, text, num, den), y + SMALL_DROP, num, den);
    ops->text_shadowed(ops->ctx, car_name(d32(r->row + RECORD_ROW_CAR)), CAR_X, y);
    shift(in, d32(r->row + RECORD_ROW_GEARBOX), y, ops);
}

static void centred(const BestTimesOps *ops, int style, const char *text, int32_t y)
{
    ops->plus_text(ops->ctx, style, text, CENTRE_X - ops->plus_text_width(ops->ctx, text) / 2, y);
}

static void label(const BestTimesOps *ops, const char *text, int32_t y) { ops->plus_text(ops->ctx, TEXT_YELLOW, text, NAME_X, y); }

static void page_totals(const BestTimesInputs *in, const RecordTable *t, const BestTimesOps *ops)
{
    int shown = 0;
    for (int i = 0; t != NULL && i < ROWS; i++)
        if (race_records_row_set(&t->totals[i])) record_row(in, &t->totals[i], i * ROW_STEP + ROW_Y, 0, ops), shown++;
    if (shown == 0) centred(ops, TEXT_CREAM, "NO RECORDS", NO_RECORDS_Y);
}

static void top_speed(const BestTimesInputs *in, const RecordTable *t, const BestTimesOps *ops)
{
    if (t != NULL && race_records_row_set(&t->top)) record_row(in, &t->top, TOP_Y, 1, ops);
    else centred(ops, TEXT_CREAM, "NO RECORDS", TOP_Y);
}

static void page_lap(const BestTimesInputs *in, const RecordRow *lap, const BestTimesOps *ops)
{
    label(ops, "BEST LAP", ROW_Y);
    if (lap != NULL && race_records_row_set(lap)) record_row(in, lap, ROW_Y + ROW_STEP, 0, ops);
    else centred(ops, TEXT_CREAM, "NO RECORDS", ROW_Y + ROW_STEP);
}

static void page_segments(const BestTimesInputs *in, const RecordTable *t, const BestTimesOps *ops)
{
    char text[TEXT];
    for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) {
        const int32_t y = (3 * k) * ROW_STEP + ROW_Y;
        snprintf(text, sizeof text, "SEGMENT %d", k + 1);
        label(ops, text, y);
        if (t != NULL && race_records_row_set(&t->segment_time[k])) record_row(in, &t->segment_time[k], y + ROW_STEP, 0, ops);
        else centred(ops, TEXT_CREAM, "NO RECORDS", y + ROW_STEP);
        if (t != NULL && race_records_row_set(&t->segment_speed[k])) record_row(in, &t->segment_speed[k], y + 2 * ROW_STEP, 1, ops);
    }
}

void best_times_pages_draw(const BestTimesInputs *in, const BestTimesState *st, int lit, BestPagesHits *hits, const BestTimesOps *ops)
{
    const int track = (int16_t)(st->config[TRACK_SELECT_TRACK] | st->config[TRACK_SELECT_TRACK + 1] << 8);
    const BestTimesPlus *const p = st->plus;
    const RecordTable *const t = race_records_find(track, p->layout, p->cheats, p->laps);
    strip(st, track, lit, hits, ops);
    if (p->page == 0) page_totals(in, t, ops);
    else if (is_stage(track)) page_segments(in, t, ops);
    else page_lap(in, race_records_lap_find(track, p->layout, p->cheats), ops);
    top_speed(in, t, ops);   /* the art's "Top speed" under every page */
}
