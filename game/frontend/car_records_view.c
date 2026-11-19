/* See car_records_view.h. */
#include "game/frontend/car_records_view.h"

#include "formats/stock_content.h"
#include "game/frontend/stat_text.h"
#include "game/frontend/text_wrap.h"
#include "game/sys/text.h"
#include "game/sys/text_label.h"
#include "game/sys/track_registry.h"
#include "game/sys/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    HEAD_NUM = 2, HEAD_DEN = 3,          /* the column heads, as small as the art's own on Best Times */
    NOTE_NUM = 1, NOTE_DEN = 2,          /* the note under the table */
    /* three columns, the two figures well to the right (the name's room is what is left) */
    NAME_X = 8, NAME_W = 340, TIME_X = 372, SPEED_X = 528,
    PAGER_NUM = 11, PAGER_DEN = 12,      /* DONE's letters: the CD's 11 cap rows (the main face's capitals are 12 at 1/1, measured) */
    LUNAR = 10
};

static const char NOTE[] = "circuits: best lap (1 LAP) or best race - stages: full stage - normal layout, no cheats";

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

int car_records_reddish(int r, int g, int b)
{
    const int max = r > g ? (r > b ? r : b) : (g > b ? g : b), min = r < g ? (r < b ? r : b) : (g < b ? g : b), span = max - min;
    if (max <= 0 || r != max || span * 100 < 50 * max) return 0;
    return 4 * abs(g - b) <= span;   /* the hue within 15 degrees of red: 60 |g - b| / span <= 15 */
}

void car_records_limit_tint(uint8_t *rgb, size_t pixels)
{
    long sum = 0;
    for (size_t i = 0; i < pixels; i++) {
        const uint8_t *p = rgb + 3 * i;
        sum += p[0] > p[1] ? (p[0] > p[2] ? p[0] : p[2]) : (p[1] > p[2] ? p[1] : p[2]);
    }
    const long mean = pixels > 0 ? sum / (long)pixels : 0, cap = 255L * CAR_RECORDS_V_MAX / 100;
    for (size_t i = 0; i < pixels; i++) {
        uint8_t *p = rgb + 3 * i;
        int c[3] = { p[0], p[1], p[2] };
        if (mean > cap)
            for (int k = 0; k < 3; k++) c[k] = (int)(c[k] * cap / mean);
        const int max = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]);
        const int min = c[0] < c[1] ? (c[0] < c[2] ? c[0] : c[2]) : (c[1] < c[2] ? c[1] : c[2]);
        if (max > 0 && (max - min) * 100 > CAR_RECORDS_S_MAX * max)   /* too saturated: the distance of each channel to the greatest is shortened */
            for (int k = 0; k < 3; k++) c[k] = max - (max - c[k]) * CAR_RECORDS_S_MAX * max / (100 * (max - min));
        for (int k = 0; k < 3; k++) p[k] = (uint8_t)(c[k] < 0 ? 0 : c[k] > 255 ? 255 : c[k]);
    }
}

/* the text blended over the page (text_label.h): its edges are mixes of the letter and what is under it, as the CD's art has them */
static void ink_label(const CarRecordsInk *ink, const char *text, int x, int y, int num, int den, int colour, int shadow_px, int relief)
{
    int tw, th;
    uint8_t *const to = video_target(&tw, &th);
    if (to == NULL || ink->palette == NULL) return;
    const TextLabelLook look = { colour, relief, shadow_px > 0 ? ink->shadow : -1, shadow_px };
    text_label_draw(to, tw, th, ink->palette, text, x, y, num, den, &look);
}

void car_records_ink_text(const CarRecordsInk *ink, const char *text, int x, int y, int num, int den, int colour, int shadow_px)
{
    ink_label(ink, text, x, y, num, den, colour, shadow_px, 0);
}

static void row_draw(const CarRecordsRow *r, int y, int kmh)
{
    char name[CAR_RECORDS_NAME], text[STAT_TEXT];
    fit(r->name, NAME_W, name, sizeof name);
    text_select(TEXT_YELLOW);
    text_draw_shadowed(name, NAME_X, y);   /* Best Times' rows: yellow, the main face, a shadow */
    const CarRecord *const c = &r->record;
    if (c->best > 0) stat_time_text(text, c->best);
    else snprintf(text, sizeof text, "--");
    text_select(c->best > 0 ? TEXT_YELLOW : TEXT_DIM);
    if (c->best > 0) text_draw_shadowed(text, TIME_X, y);
    if (c->best > 0 && c->laps > 0) {   /* what the time is, beside it in small letters: "1 LAP" or the laps of the race */
        char laps[16];
        snprintf(laps, sizeof laps, "%d LAP%s", c->laps, c->laps == 1 ? "" : "S");
        put(laps, TIME_X + text_width(text) + 8, y + 7, 1, 2);
    }
    else put(text, TIME_X, y, 1, 1);   /* "--" without a shadow: nothing is there */
    if (c->speed > 0) stat_speed_text(text, kmh, c->speed);
    else snprintf(text, sizeof text, "--");
    text_select(c->speed > 0 ? TEXT_YELLOW : TEXT_DIM);
    if (c->speed > 0) text_draw_shadowed(text, SPEED_X, y);
    else put(text, SPEED_X, y, 1, 1);
}

void car_records_view_draw(const CarRecordsView *v, int kmh, const CarRecordsInk *ink)
{
    car_records_ink_text(ink, "Best Times", NAME_X, CAR_RECORDS_LABEL_Y, 1, 1, ink->cream, 2);   /* where the art's "Best times" is */
    car_records_ink_text(ink, "TRACK", NAME_X, CAR_RECORDS_HEAD_Y, HEAD_NUM, HEAD_DEN, ink->red, ink->head_shadow);   /* flat red, no white edge; a dark shadow only on a reddish page */
    car_records_ink_text(ink, "BEST TIME", TIME_X, CAR_RECORDS_HEAD_Y, HEAD_NUM, HEAD_DEN, ink->red, ink->head_shadow);
    car_records_ink_text(ink, "TOP SPEED", SPEED_X, CAR_RECORDS_HEAD_Y, HEAD_NUM, HEAD_DEN, ink->red, ink->head_shadow);
    const int first = v->page * CAR_RECORDS_ROWS;
    for (int i = 0; i < CAR_RECORDS_ROWS && first + i < v->count; i++) row_draw(&v->row[first + i], CAR_RECORDS_ROW0 + i * CAR_RECORDS_STEP, kmh);
    car_records_ink_text(ink, NOTE, NAME_X, CAR_RECORDS_NOTE_Y, NOTE_NUM, NOTE_DEN, ink->cream, 1);   /* what the figures are, small, under the table */
}

void car_records_view_footer(const CarRecordsView *v, const CarRecordsInk *ink)
{
    int top, bottom;
    (void)v;
    text_ink_scaled("DONE", PAGER_NUM, PAGER_DEN, &top, &bottom);   /* the capitals' first row on the measured one */
    ink_label(ink, "DONE", CAR_RECORDS_DONE_X, CAR_RECORDS_FOOT_CAP - top, PAGER_NUM, PAGER_DEN, ink->yellow, 2, 1);   /* the items' relief */
}
