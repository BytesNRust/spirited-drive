/* See car_records_view.h. */
#include "game/frontend/car_records_view.h"

#include "formats/stock_content.h"
#include "game/frontend/record_conditions.h"
#include "game/frontend/stat_text.h"
#include "game/frontend/text_wrap.h"
#include "game/sys/text.h"
#include "game/sys/track_registry.h"

#include <stdio.h>
#include <string.h>

enum {
    HEAD_NUM = 2, HEAD_DEN = 3,          /* the column heads (Best Times' red ones are the art's own: these are as small) */
    SEED_NUM = 3, SEED_DEN = 4,          /* a speed */
    SEG_NUM = 3, SEG_DEN = 4,            /* a stage's three segment times, side by side */
    NAME_X = 8, NAME_W = 205, TOTAL_X = 222, LAP_X = 304, SEG_STEP = 62, SPEED_X = 484, COND_RIGHT = 636,   /* Best Times' columns, moved for ours */
    PAGER_NUM = 3, PAGER_DEN = 4,        /* the selector's size: Best Times' "< TOP 10 >" (best_times_pages.c) */
    LUNAR = 10
};

void car_records_view_make(CarRecordsView *v, const char *car, const TrackList *tracks)
{
    memset(v, 0, sizeof *v);
    snprintf(v->car, sizeof v->car, "%s", car != NULL ? car : "");
    for (int i = 0; tracks != NULL && i < tracks->count && v->count < CAR_RECORDS_MAX; i++) {
        const int track = tracks->track[i];
        CarRecordsRow *const r = &v->row[v->count];
        if (track >= TRACK_ADDED_FIRST) {
            const TrackPiece *const piece = track_registry_piece(track);
            if (piece == NULL) continue;
            text_words(piece->name, r->name, sizeof r->name);
            car_records_find(v->car, -1, piece->id, piece->spec.kind == TRACK_KIND_STAGE, &r->record);
        } else if (track == LUNAR) {
            snprintf(r->name, sizeof r->name, "%s", "LUNAR SPRINGS");
            car_records_find(v->car, -1, "lunar", 0, &r->record);
        } else {
            const char *const name = stock_track_name(track);
            if (name == NULL) continue;
            text_words(name, r->name, sizeof r->name);
            car_records_find(v->car, track, NULL, track < RACE_RECORDS_FIRST_CIRCUIT, &r->record);
        }
        v->count++;
    }
}

int car_records_view_pages(const CarRecordsView *v) { return v->count > 0 ? (v->count + CAR_RECORDS_ROWS - 1) / CAR_RECORDS_ROWS : 1; }

void car_records_view_step(CarRecordsView *v, int dir)
{
    const int pages = car_records_view_pages(v);
    v->page = ((v->page + dir) % pages + pages) % pages;
}

/* the conditions' pen: the main face scaled, as Best Times' */
static void pen_text(void *ctx, const char *text, int32_t x, int32_t y, int32_t num, int32_t den) { (void)ctx; text_draw_smooth(text, x, y, num, den); }
static int32_t pen_width(void *ctx, const char *text, int32_t num, int32_t den) { (void)ctx; return text_width_scaled(text, num, den); }
static void pen_ink(void *ctx, const char *text, int32_t num, int32_t den, int32_t *top, int32_t *bottom)
{
    int t, b;
    (void)ctx;
    text_ink_scaled(text, num, den, &t, &b);
    *top = t, *bottom = b;
}

static void put(const char *text, int x, int y, int num, int den) { text_draw_smooth(text, x, y, num, den); }

/* `text` cut (with a "." at its end) to fit `width` at the main face's size */
static void fit(const char *text, int width, char *out, int size)
{
    snprintf(out, (size_t)size, "%s", text);
    while (out[0] != 0 && text_width(out) > width) {
        const size_t n = strlen(out);
        out[n - 1] = 0;
        if (n > 1) out[n - 2] = '.';
    }
}

static void row_draw(const CarRecordsRow *r, int y, int kmh)
{
    char name[CAR_RECORDS_NAME], text[STAT_TEXT];
    fit(r->name, NAME_W, name, sizeof name);
    text_select(TEXT_YELLOW);
    text_draw_shadowed(name, NAME_X, y);   /* Best Times' rows: yellow, the main face, a shadow */
    const CarRecord *const c = &r->record;
    if (!c->found) {
        text_select(TEXT_DIM);
        put("--", TOTAL_X, y, 1, 1);   /* one "--" for each column, no shadow: nothing is there */
        put("--", LAP_X, y, 1, 1);
        put("--", SPEED_X, y, 1, 1);
        return;
    }
    text_select(TEXT_YELLOW);
    stat_time_text(text, c->total);
    text_draw_shadowed(text, TOTAL_X, y);
    if (c->stage) {
        for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) {
            if (c->segment[k] > 0) stat_time_text(text, c->segment[k]);
            else snprintf(text, sizeof text, "--");
            put(text, LAP_X + k * SEG_STEP, y + 2, SEG_NUM, SEG_DEN);
        }
    } else {
        if (c->lap > 0) stat_time_text(text, c->lap);
        else snprintf(text, sizeof text, "--");
        text_draw_shadowed(text, LAP_X, y);
    }
    if (c->speed > 0) stat_speed_text(text, kmh, c->speed);
    else snprintf(text, sizeof text, "--");
    put(text, SPEED_X, y + 2, SEED_NUM, SEED_DEN);
    const ConditionsPen pen = { NULL, pen_text, pen_width, pen_ink };
    text_select(TEXT_CREAM);
    record_conditions_draw(c->marks, COND_RIGHT, y, 1, 2, &pen);   /* "CLR DAY" over "ORIGINAL", as Best Times' rows */
}

void car_records_view_draw(const CarRecordsView *v, int kmh)
{
    text_select(TEXT_CREAM);
    text_draw_shadowed("RECORDS", NAME_X, CAR_RECORDS_LABEL_Y);   /* where the art's "Best times" is */
    text_select(TEXT_RED);
    put("TRACK", NAME_X, CAR_RECORDS_HEAD_Y, HEAD_NUM, HEAD_DEN);
    put("TOTAL", TOTAL_X, CAR_RECORDS_HEAD_Y, HEAD_NUM, HEAD_DEN);
    put("LAP / SEGMENTS", LAP_X, CAR_RECORDS_HEAD_Y, HEAD_NUM, HEAD_DEN);
    put("TOP SPEED", SPEED_X, CAR_RECORDS_HEAD_Y, HEAD_NUM, HEAD_DEN);
    const int first = v->page * CAR_RECORDS_ROWS;
    for (int i = 0; i < CAR_RECORDS_ROWS && first + i < v->count; i++) row_draw(&v->row[first + i], CAR_RECORDS_ROW0 + i * CAR_RECORDS_STEP, kmh);
    char pager[24];
    snprintf(pager, sizeof pager, "< %d / %d >", v->page + 1, car_records_view_pages(v));
    text_select(TEXT_CREAM);
    put(pager, CAR_RECORDS_PAGER_X - text_width_scaled(pager, PAGER_NUM, PAGER_DEN) / 2, CAR_RECORDS_PAGER_Y, PAGER_NUM, PAGER_DEN);
}

int car_records_view_hit(const CarRecordsView *v, int x, int y)
{
    (void)v;
    if (y < CAR_RECORDS_PAGER_Y - 8 || y > CAR_RECORDS_PAGER_Y + 22 || x < CAR_RECORDS_PAGER_X - 70 || x > CAR_RECORDS_PAGER_X + 70) return 0;
    return x < CAR_RECORDS_PAGER_X ? -1 : 1;
}
