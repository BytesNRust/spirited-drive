/* Flat-colour polygon and line fill. Ported from NFS_WIN.EXE 0048434e,
 * 00484377, 00484389 (the polygon, three entry points sharing one body;
 * 0048274c enters the same body with the span filler 0049f13c),
 * 00484254 (line), 004a0048 (line drawer), 004a0290 (span filler) and the
 * pixel and rectangle routines below them, 0049f3a0 and 004829b0 / 00496510,
 * for an 8-bit surface.
 *
 * The polygon is split into two chains of edges between its topmost and
 * bottommost points; flat_edge.c turns each into an edge record (clipping it),
 * flat_scan.c writes the per-row left and right x of the chains into tables,
 * and the span filler fills each row from left to right with a clipped
 * rectangle fill. The tables start at zero here; the original's sit on the
 * stack uninitialised, which only shows for outlines that leave a row
 * unwritten (concave or crossing input). */
#include "game/gfx/flat.h"
#include "game/gfx/raster_record.h"

#include "game/gfx/flat_internal.h"
#include "game/gfx/x86_int.h"

#include <string.h>

enum { OUTCODE_ACCEPT = 0 };

static const uint32_t HALF16 = 0x8000u;

static int target_valid(const QuadTarget *t)
{
    return t != NULL && t->pixels != NULL && t->pitch > 0 && t->clip_left >= 0 &&
           t->clip_left < t->clip_right && t->clip_top >= 0 && t->clip_top < t->clip_bottom &&
           t->clip_bottom <= FLAT_MAX_ROWS;
}

static Clip clip_of(const QuadTarget *t)
{
    return (Clip){ t->clip_left, t->clip_top, t->clip_right, t->clip_bottom };
}

/* port (render bands): the rows a draw may store to, apart from the clip that decides the geometry: a band's draw keeps
 * the whole clip box for every edge and span it works out and only drops the stores outside its rows, so what it
 * paints inside them is what the unbanded draw paints there (a narrowed clip changes the edges' arithmetic) */
typedef struct {
    int32_t top, bottom;
} Rows;
static const Rows ALL_ROWS = { INT32_MIN, INT32_MAX };

/* 0049f3a0: one pixel, dropped outside the clip rectangle. */
void flat_plot(const QuadTarget *t, int32_t x, int32_t y, uint32_t colour)
{
    raster_record_sync(t);   /* port: not recorded; what was is drawn first */
    if (x < t->clip_left || x >= t->clip_right || y < t->clip_top || y >= t->clip_bottom) return;
    t->pixels[(size_t)y * (size_t)t->pitch + (size_t)x] = (uint8_t)colour;
}

/* 0048f5f0: one pixel read, 0 outside the clip rectangle. */
uint32_t flat_peek(const QuadTarget *t, int32_t x, int32_t y)
{
    raster_record_sync(t);
    if (x < t->clip_left || x >= t->clip_right || y < t->clip_top || y >= t->clip_bottom) return 0;
    return t->pixels[(size_t)y * (size_t)t->pitch + (size_t)x];
}

/* 004829b0 -> 00496510: a w x h rectangle of one colour (the 8-bit fill at
 * 0049f290), clipped to the clip rectangle. The size is a pair of 16-bit
 * fields in the original, so w and h wrap to 16 bits signed. */
static void fill_rect_rows(const QuadTarget *t, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t colour,
                           const Rows *rows)
{
    w = (int16_t)w;
    h = (int16_t)h;
    int32_t above = x_sub(t->clip_top, y);
    if (above > 0) {
        y = x_add(y, above);
        h = x_sub(h, above);
    }
    int32_t below = x_sub(x_add(y, h), t->clip_bottom);
    if (below > 0) h = x_sub(h, below);
    int32_t before = x_sub(t->clip_left, x);
    if (before > 0) {
        x = x_add(x, before);
        w = x_sub(w, before);
    }
    int32_t past = x_sub(x_add(w, x), t->clip_right);
    if (past > 0) w = x_sub(w, past);
    if (w <= 0 || h <= 0) return;
    if (y < rows->top) h = x_sub(h, x_sub(rows->top, y)), y = rows->top;   /* port: the stores' rows only */
    if (h > 0 && y + h > rows->bottom) h = x_sub(rows->bottom, y);
    if (h <= 0) return;
    uint8_t *row = t->pixels + (size_t)y * (size_t)t->pitch + (size_t)x;
    for (int32_t i = 0; i < h; i++, row += t->pitch) memset(row, (int)(colour & 0xFF), (size_t)w);
}

/* port (render bands): a recorded draw replayed over a band's rows, the whole clip deciding the edges */
static void replay_fill_rect(const RasterCmd *c, const QuadTarget *whole, int32_t top, int32_t bottom)
{
    const Rows rows = { top, bottom };
    fill_rect_rows(whole, c->a, c->b, c->c, c->d, (uint32_t)c->colour, &rows);
}

void flat_fill_rect(const QuadTarget *t, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t colour)
{
    struct RasterRecorder *rec = raster_recorder_for(t);
    if (rec != NULL) {
        RasterCmd *c = raster_record_add(rec, t, replay_fill_rect);
        c->a = x, c->b = y, c->c = w, c->d = h, c->colour = (int32_t)colour;
        const int32_t ends[4] = { x, y, x, x_add(y, (int16_t)h) };
        raster_cmd_rows(c, ends, 2);
        return;
    }
    fill_rect_rows(t, x, y, w, h, colour, &ALL_ROWS);
}

/* 004827f0 */
void flat_frame_rect(const QuadTarget *t, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t colour)
{
    if (x1 < x0) {
        const int32_t x = x0;
        x0 = x1, x1 = x;
    }
    if (y1 < y0) {
        const int32_t y = y0;
        y0 = y1, y1 = y;
    }
    const int32_t w = x1 - x0 + 1, h = y1 - y0 - 1;
    flat_fill_rect(t, x0, y0, w, 1, colour);
    flat_fill_rect(t, x1, y0 + 1, 1, h, colour);
    flat_fill_rect(t, x0, y0 + 1, 1, h, colour);
    flat_fill_rect(t, x0, y1, w, 1, colour);
}

/* 004a0290: rows y .. y + count - 1, each filled from left[i] to right[i]
 * inclusive. */
static void fill_rows(const QuadTarget *t, const int32_t *left, const int32_t *right, int32_t y,
                      int32_t count, uint32_t colour, const Rows *rows)
{
    for (int32_t i = 0; i < count; i++)
        fill_rect_rows(t, left[i], x_add(y, i), x_add(x_sub(right[i], left[i]), 1), 1, colour, rows);
}

/* 0049f13c: rows y .. y + count - 1, each pixel from left[i] to right[i]
 * inclusive replaced by remap[pixel]. Unlike 004a0290 it does not clip x;
 * the port drops the pixels outside the surface's row. */
static void shade_rows(const QuadTarget *t, const int32_t *left, const int32_t *right, int32_t y,
                       int32_t count, const uint8_t *remap, const Rows *rows)
{
    for (int32_t i = 0; i < count; i++) {
        if (x_add(y, i) < rows->top || x_add(y, i) >= rows->bottom) continue;
        uint8_t *row = t->pixels + (size_t)x_add(y, i) * (size_t)t->pitch;
        for (int64_t x = left[i]; x <= right[i]; x++)
            if (x >= 0 && x < t->pitch) row[x] = remap[row[x]];
    }
}

/* 004a0048: the line drawer. Positions are rounded 16.16 values whose
 * integer part is kept to 16 bits, as the original does; the pixels are then
 * clipped one by one (or by the rectangle fill for horizontal and vertical
 * runs). */
static void draw_edge_rows(const QuadTarget *t, const Edge *e, const Rows *rows)
{
    const uint32_t xa = ((uint32_t)e->x << 16) + ((e->x_frac >> 16) & 0xFFFFu) + HALF16;
    const uint32_t ya = ((uint32_t)e->y << 16) + ((e->y_frac >> 16) & 0xFFFFu) + HALF16;
    int32_t px = (int32_t)((xa >> 16) & 0xFFFFu);
    int32_t y16 = (int32_t)((ya >> 16) & 0xFFFFu);
    const uint32_t colour = (uint32_t)e->colour;
    const uint32_t step = (e->slope >> 16) & 0xFFFFu;
    int32_t py = e->y;
    uint32_t acc;
    int32_t n = e->count;
    switch (e->kind) {
    case EDGE_HORIZONTAL_LEFT:
    case EDGE_HORIZONTAL:
        if (n != 0) fill_rect_rows(t, px, y16, n, 1, colour, rows);
        return;
    case EDGE_VERTICAL:
        if (n != 0) fill_rect_rows(t, px, py, 1, n, colour, rows);
        return;
    case EDGE_DIAGONAL_LEFT:
    case EDGE_DIAGONAL_RIGHT:
        for (; n > 0; n--, py = x_add(py, 1)) {
            if (py >= rows->top && py < rows->bottom) flat_plot(t, px, py, colour);
            px = x_add(px, e->kind == EDGE_DIAGONAL_LEFT ? -1 : 1);
        }
        return;
    case EDGE_STEEP_LEFT:
    case EDGE_STEEP_RIGHT:
        acc = xa & 0xFFFFu;
        for (; n > 0; n--, py = x_add(py, 1)) {
            if (py >= rows->top && py < rows->bottom) flat_plot(t, px, py, colour);
            acc += step;
            if (acc & 0xFFFF0000u) {
                px = x_add(px, e->kind == EDGE_STEEP_LEFT ? -1 : 1);
                acc &= 0xFFFFu;
            }
        }
        return;
    case EDGE_SHALLOW_LEFT:
    case EDGE_SHALLOW_RIGHT:
        acc = ya & 0xFFFFu;
        for (; n > 0; n--) {
            acc += step;
            if (y16 >= rows->top && y16 < rows->bottom) flat_plot(t, px, y16, colour);
            px = x_add(px, e->kind == EDGE_SHALLOW_LEFT ? -1 : 1);
            if (acc & 0xFFFF0000u) {
                y16 = x_add(y16, 1);
                acc &= 0xFFFFu;
            }
        }
        return;
    case EDGE_POINT: if (y16 >= rows->top && y16 < rows->bottom) flat_plot(t, px, y16, colour); return;
    default: return;
    }
}

void flat_draw_edge(const QuadTarget *t, const Edge *e)
{
    raster_record_sync(t);
    draw_edge_rows(t, e, &ALL_ROWS);
}

static void line_rows(const QuadTarget *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t colour,
                      const Rows *rows)
{
    if (!target_valid(target)) return;
    const Clip clip = clip_of(target);
    Edge e;
    memset(&e, 0, sizeof e);
    if (flat_edge_setup(&clip, 1, x0, y0, x1, y1, &e) != OUTCODE_ACCEPT || e.count <= 0) return;
    e.colour = (int32_t)colour;
    draw_edge_rows(target, &e, rows);
}

static void replay_line(const RasterCmd *c, const QuadTarget *whole, int32_t top, int32_t bottom)
{
    const Rows rows = { top, bottom };
    line_rows(whole, c->v[0], c->v[1], c->v[2], c->v[3], (uint32_t)c->colour, &rows);
}

void flat_line(const QuadTarget *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t colour)
{
    struct RasterRecorder *rec = raster_recorder_for(target);
    if (rec != NULL) {
        RasterCmd *c = raster_record_add(rec, target, replay_line);
        c->v[0] = x0, c->v[1] = y0, c->v[2] = x1, c->v[3] = y1, c->colour = (int32_t)colour;
        raster_cmd_rows(c, c->v, 2);
        return;
    }
    line_rows(target, x0, y0, x1, y1, colour, &ALL_ROWS);
}

void flat_line_rows(const QuadTarget *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t colour,
                    int32_t top, int32_t bottom)
{
    const Rows rows = { top, bottom };
    line_rows(target, x0, y0, x1, y1, colour, &rows);
}

/* The two passes of 0048434e / 00484377 / 00484389 over the points from the
 * topmost one (the last of equal topmost), forwards then backwards, until the
 * bottommost (the first of equal bottommost). */
static void draw_chains(RowTables *tables, const Clip *clip, int clipping, int merge, int count,
                        const int32_t *pts, int top, int bottom)
{
    Edge e;
    memset(&e, 0, sizeof e);
    for (int pass = 0; pass < 2; pass++) {
        int i = top;
        do {
            const int32_t x0 = pts[2 * i], y0 = pts[2 * i + 1];
            i = pass == 0 ? (i + 1 > count - 1 ? 0 : i + 1) : (i - 1 < 0 ? count - 1 : i - 1);
            const int32_t x1 = pts[2 * i], y1 = pts[2 * i + 1];
            if (y1 > y0) {
                flat_edge_setup(clip, clipping, x0, y0, x1, y1, &e);
                if (pass == 0) flat_scan_first(tables, clip, clipping, &e);
                else flat_scan_second(tables, clip, clipping, merge, &e);
            }
        } while (i != bottom);
    }
}

/* remap NULL: 004a0290 fills the rows with colour; else 0049f13c remaps them
 * (0048274c), and colour is only that of the line fallbacks. */
static void polygon(const QuadTarget *target, uint32_t colour, const uint8_t *remap, int count,
                    const int32_t *pts, int merge, const Rows *rows)
{
    if (!target_valid(target) || pts == NULL || count < 1) return;
    if (count == 1) {
        line_rows(target, pts[0], pts[1], pts[0], pts[1], colour, rows);
        return;
    }
    const Clip clip = clip_of(target);
    int32_t x_min = pts[0], x_max = pts[0], y_min = pts[1], y_max = pts[1];
    int top = 0, bottom = 0;
    for (int i = 1; i < count; i++) {
        const int32_t x = pts[2 * i], y = pts[2 * i + 1];
        if (y <= y_min) y_min = y, top = i;
        if (y > y_max) y_max = y, bottom = i;
        if (x < x_min) x_min = x;
        if (x > x_max) x_max = x;
    }
    if (x_sub(y_max, y_min) <= 0 || x_max < clip.left || x_min >= x_sub(clip.right, 1) ||
        y_max < clip.top || y_min >= clip.bottom)
        return;
    const int clipping = x_max > x_sub(clip.right, 1) || x_min < clip.left ||
                         y_max >= clip.bottom || y_min < clip.top;
    if (x_min == x_max) {  /* every point on one column */
        line_rows(target, x_min, y_min, x_max, y_max, colour, rows);
        return;
    }
    RowTables tables;
    memset(&tables, 0, sizeof tables);
    draw_chains(&tables, &clip, clipping, merge, count, pts, top, bottom);

    const int32_t last = y_max >= clip.bottom ? x_sub(clip.bottom, 1) : y_max;
    const int32_t first = y_min < clip.top ? clip.top : y_min;
    const int32_t nrows = x_sub(last, first);
    if (nrows <= 0) return;
    int32_t left[FLAT_MAX_ROWS], right[FLAT_MAX_ROWS];
    for (int32_t i = 0; i <= nrows && i < FLAT_MAX_ROWS; i++) {
        left[i] = table_get(&tables, x_add(first, i));
        right[i] = table_get(&tables, (int64_t)x_add(first, i) + FLAT_MAX_ROWS);
    }
    if (remap != NULL) shade_rows(target, left, right, first, nrows + 1, remap, rows);
    else fill_rows(target, left, right, first, nrows + 1, colour, rows);
}

/* port (render bands): a recorded polygon replayed over a band's rows; more points than a command holds are drawn at
 * once, after what has been recorded */
static void replay_polygon(const RasterCmd *c, const QuadTarget *whole, int32_t top, int32_t bottom)
{
    const Rows rows = { top, bottom };
    polygon(whole, (uint32_t)c->colour, c->table, c->count, c->v, (int)c->mode, &rows);
}

static int record_polygon(const QuadTarget *target, uint32_t colour, const uint8_t *remap, int count, const int32_t *pts,
                          int merge)
{
    struct RasterRecorder *rec = raster_recorder_for(target);
    if (rec == NULL) return 0;
    if (pts == NULL || count < 1) return 1;   /* polygon() draws nothing for these */
    if (2 * count > RASTER_CMD_INTS) {
        raster_record_sync(target);
        return 0;
    }
    RasterCmd *c = raster_record_add(rec, target, replay_polygon);
    c->colour = (int32_t)colour, c->table = remap, c->count = count, c->mode = merge;
    memcpy(c->v, pts, 2 * (size_t)count * sizeof *pts);
    raster_cmd_rows(c, c->v, count);
    return 1;
}

void flat_polygon(const QuadTarget *target, uint32_t colour, int count, const int32_t *points)
{
    if (record_polygon(target, colour, NULL, count, points, 0)) return;
    polygon(target, colour, NULL, count, points, 0, &ALL_ROWS);
}

void flat_polygon_merge(const QuadTarget *target, uint32_t colour, int count,
                        const int32_t *points)
{
    if (record_polygon(target, colour, NULL, count, points, 1)) return;
    polygon(target, colour, NULL, count, points, 1, &ALL_ROWS);
}

void flat_polygon_shade(const QuadTarget *target, const uint8_t *remap, int count, const int32_t *points,
                        int merge)
{
    if (remap == NULL) return;
    if (record_polygon(target, 0, remap, count, points, merge != 0)) return;
    polygon(target, 0, remap, count, points, merge != 0, &ALL_ROWS);
}

void flat_polygon_rows(const QuadTarget *target, uint32_t colour, int count, const int32_t *points, int32_t top,
                       int32_t bottom)
{
    const Rows rows = { top, bottom };
    polygon(target, colour, NULL, count, points, 0, &rows);
}

void flat_polygon_shade_rows(const QuadTarget *target, const uint8_t *remap, int count, const int32_t *points,
                             int merge, int32_t top, int32_t bottom)
{
    const Rows rows = { top, bottom };
    if (remap != NULL) polygon(target, 0, remap, count, points, merge != 0, &rows);
}
