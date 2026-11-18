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
    FONT_NUM = 7, FONT_DEN = 8,          /* the rows' words: the main face at 7/8, as the SETUP panel's */
    HEAD_NUM = 2, HEAD_DEN = 3,          /* the column heads */
    SEG_NUM = 1, SEG_DEN = 2,            /* a stage's three segment times, side by side */
    TRACK_X = 14, TRACK_W = 150,
    TOTAL_RIGHT = 262, LAP_X = 276, SEG_STEP = 62, SPEED_RIGHT = 516, COND_RIGHT = 628,
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
static void put_right(const char *text, int right, int y, int num, int den) { put(text, right - text_width_scaled(text, num, den), y, num, den); }

/* `text` cut (with a "." at its end) to fit `width` at the rows' size */
static void fit(const char *text, int width, char *out, int size)
{
    snprintf(out, (size_t)size, "%s", text);
    while (out[0] != 0 && text_width_scaled(out, FONT_NUM, FONT_DEN) > width) {
        const size_t n = strlen(out);
        out[n - 1] = 0;
        if (n > 1) out[n - 2] = '.';
    }
}

static void row_draw(const CarRecordsRow *r, int y, int kmh)
{
    char name[CAR_RECORDS_NAME], text[STAT_TEXT];
    fit(r->name, TRACK_W, name, sizeof name);
    text_select(TEXT_CREAM);
    put(name, TRACK_X, y, FONT_NUM, FONT_DEN);
    const CarRecord *const c = &r->record;
    if (!c->found) {
        text_select(TEXT_DIM);
        put_right("-", TOTAL_RIGHT, y, FONT_NUM, FONT_DEN);
        put("-", LAP_X, y, FONT_NUM, FONT_DEN);
        put_right("-", SPEED_RIGHT, y, FONT_NUM, FONT_DEN);
        return;
    }
    text_select(TEXT_WHITE);
    stat_time_text(text, c->total);
    put_right(text, TOTAL_RIGHT, y, FONT_NUM, FONT_DEN);
    if (c->stage) {
        for (int k = 0; k < RACE_RECORDS_SEGMENTS; k++) {
            if (c->segment[k] > 0) stat_time_text(text, c->segment[k]);
            else snprintf(text, sizeof text, "-");
            put(text, LAP_X + k * SEG_STEP, y, SEG_NUM, SEG_DEN);
        }
    } else {
        if (c->lap > 0) stat_time_text(text, c->lap);
        else snprintf(text, sizeof text, "-");
        put(text, LAP_X, y, FONT_NUM, FONT_DEN);
    }
    if (c->speed > 0) stat_speed_text(text, kmh, c->speed);
    else snprintf(text, sizeof text, "-");
    put_right(text, SPEED_RIGHT, y, FONT_NUM, FONT_DEN);
    const ConditionsPen pen = { NULL, pen_text, pen_width, pen_ink };
    text_select(TEXT_CREAM);
    record_conditions_draw(c->marks, COND_RIGHT, y, 1, 2, &pen);   /* "CLR DAY" over "ORIGINAL", as Best Times' rows */
}

void car_records_view_draw(const CarRecordsView *v, int kmh)
{
    text_select(TEXT_RED);
    text_draw_smooth("RECORDS", 8, CAR_RECORDS_TOP + 4, 5, 4);
    put("TRACK", TRACK_X, CAR_RECORDS_HEAD_Y, HEAD_NUM, HEAD_DEN);
    put_right("TOTAL", TOTAL_RIGHT, CAR_RECORDS_HEAD_Y, HEAD_NUM, HEAD_DEN);
    put("LAP / SEGMENTS", LAP_X, CAR_RECORDS_HEAD_Y, HEAD_NUM, HEAD_DEN);
    put_right("TOP SPEED", SPEED_RIGHT, CAR_RECORDS_HEAD_Y, HEAD_NUM, HEAD_DEN);
    put_right("CONDITIONS", COND_RIGHT, CAR_RECORDS_HEAD_Y, HEAD_NUM, HEAD_DEN);
    const int first = v->page * CAR_RECORDS_ROWS;
    for (int i = 0; i < CAR_RECORDS_ROWS && first + i < v->count; i++) row_draw(&v->row[first + i], CAR_RECORDS_ROW0 + i * CAR_RECORDS_STEP, kmh);
    char pager[24];
    snprintf(pager, sizeof pager, "< %d / %d >", v->page + 1, car_records_view_pages(v));
    text_select(TEXT_YELLOW);
    put(pager, (CAR_RECORDS_LEFT + CAR_RECORDS_RIGHT - text_width_scaled(pager, FONT_NUM, FONT_DEN)) / 2, CAR_RECORDS_PAGER_Y, FONT_NUM, FONT_DEN);
}

int car_records_view_hit(const CarRecordsView *v, int x, int y)
{
    (void)v;
    const int mid = (CAR_RECORDS_LEFT + CAR_RECORDS_RIGHT) / 2;
    if (y < CAR_RECORDS_PAGER_Y - 6 || y > CAR_RECORDS_PAGER_Y + 30 || x < mid - 90 || x > mid + 90) return 0;
    return x < mid ? -1 : 1;
}
