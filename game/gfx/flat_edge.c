/* Edge setup for the flat-colour polygon and line code. Ported from
 * NFS_WIN.EXE 0049f73f (clips the edge to the clip rectangle) and 0049f730
 * (the same without the clipping, for edges known to lie inside it).
 *
 * An edge is turned into a start point, a step count and, for sloped edges, a
 * 0.32 slope with the octant it belongs to. Clipping moves the start and end
 * points onto the clip rectangle and counts the rows the outline spends left
 * or right of it, which the scan converters fill with empty spans. */
#include "game/gfx/flat_internal.h"

#include "game/gfx/x86_int.h"

enum {
    OUT_RIGHT = 1,
    OUT_LEFT = 2,
    OUT_ABOVE = 4,
    OUT_BELOW = 8,
    RECIPROCAL_LIMIT = 50,       /* 004f61f8: below it the slope comes from a table */
    COORD_LIMIT = 16000,         /* coordinates past it are scaled down first */
    SCALE_STEPS = 1 << 16        /* the original has no bound */
};

static const uint32_t HALF = 0x80000000u;

static uint64_t pair(int32_t hi, uint32_t lo) { return ((uint64_t)(uint32_t)hi << 32) | lo; }

static void put_x(Edge *e, uint64_t v)
{
    e->x_frac = (uint32_t)v;
    e->x = (int32_t)(uint32_t)(v >> 32);
}

static void put_y(Edge *e, uint64_t v)
{
    e->y_frac = (uint32_t)v;
    e->y = (int32_t)(uint32_t)(v >> 32);
}

/* edx:eax / d with the quotient rounded to nearest, as the sequence
 * div / shr / sub / adc computes it. The original faults when the quotient
 * does not fit 32 bits or d is 0; those inputs are not reproduced. */
static uint32_t div_round(uint32_t hi, uint32_t lo, uint32_t d)
{
    if (d == 0) return 0;
    const uint64_t n = ((uint64_t)hi << 32) | lo;
    const uint32_t q = (uint32_t)(n / d);
    const uint32_t r = (uint32_t)(n % d);
    return q + ((d >> 1) < r ? 1u : 0u);
}

/* The slope of an edge: minor / major as 0.32. Majors under 50 come from the
 * game's table of 16-bit reciprocals (floor(minor * 65536 / major), measured
 * to match the exe's table for every entry), larger ones from a division. */
static uint32_t edge_slope(uint32_t minor, uint32_t major)
{
    if ((int32_t)major < RECIPROCAL_LIMIT) return ((minor << 16) / major) << 16;
    return div_round(minor, 0, major);
}

/* 0049fc31: an edge wholly outside the clip rectangle. Leaves it with no
 * steps and, for the sides that matter, the rows it spans recorded. */
static int reject(const Clip *c, Edge *e, uint8_t code)
{
    e->outcode = code;
    e->count = 0;
    if (code & OUT_ABOVE) {
        e->y = c->top;
        e->y_frac = 0;
        e->y_end = x_sub(c->top, 1);
    } else if (code & OUT_BELOW) {
        e->y = c->bottom;
        e->y_frac = 0;
    } else {
        int32_t last = e->y_end >= c->bottom ? x_sub(c->bottom, 1) : e->y_end;
        int32_t first = x_add(e->y, e->y_frac >= HALF ? 1 : 0);
        if (first < c->top) first = c->top;
        e->y = first;
        e->y_frac = 0;
        const int32_t rows = x_add(x_sub(last, first), 1);
        e->y_end = x_sub(first, 1);
        if (code & OUT_LEFT) e->left_after = x_add(e->left_after, rows);
        else e->right_after = x_add(e->right_after, rows);
    }
    return code;
}

/* 0049fe8d: both ends on one row. */
static int setup_horizontal(const Clip *c, int clipping, int32_t x0, int32_t x1, int32_t y,
                            Edge *e)
{
    int32_t lo = x0, hi = x1;
    e->kind = x0 == x1 ? EDGE_POINT : EDGE_HORIZONTAL;
    if (x0 > x1) {
        e->kind = EDGE_HORIZONTAL_LEFT;
        e->x = x1;
        e->x_end = x0;
        lo = x1;
        hi = x0;
    }
    if (!clipping) {
        e->count = x_add(x_sub(hi, lo), 1);
        return 0;
    }
    if (y < c->top || y >= c->bottom) {
        const int32_t row = y < c->top ? c->top : c->bottom;
        e->y = row;
        e->y_end = row;
        e->outcode = y < c->top ? OUT_ABOVE : OUT_BELOW;
        e->count = 0;
        return e->outcode;
    }
    e->count = x_add(x_sub(hi, lo), 1);
    if (hi < c->left || lo >= c->right) {
        const int left = hi < c->left;
        e->y_end = x_sub(e->y_end, 1);
        if (left) e->left_after = 1;
        else e->right_after = 1;
        e->outcode = left ? OUT_LEFT : OUT_RIGHT;
        e->count = 0;
        return e->outcode;
    }
    if (c->left > lo) {
        e->x = c->left;
        e->count = x_sub(e->count, x_sub(c->left, lo));
    }
    if (hi > c->right - 1) {
        e->count = x_sub(e->count, x_sub(hi, c->right - 1));
        e->x_end = c->right - 1;
    }
    return 0;
}

/* 0049f79a: classifies both ends against the clip rectangle. Returns the
 * outcode (OUT_* bits) to clip by, or -1 when the edge is trivially outside
 * (with *reject_code set). */
static int classify(const Clip *c, const Edge *e, uint8_t *reject_code)
{
    uint8_t start = 0, end = 0;
    if (e->y >= c->bottom) {
        *reject_code = OUT_BELOW;
        return -1;
    }
    if (e->y < c->top) start |= OUT_ABOVE;
    if (e->y_end < c->top) {
        *reject_code = OUT_ABOVE;
        return -1;
    }
    if (e->y_end >= c->bottom) end |= OUT_BELOW;
    if (e->x < c->left) start |= OUT_LEFT;
    if (e->x >= c->right) start |= OUT_RIGHT;
    if (e->x_end < c->left) end |= OUT_LEFT;
    if (e->x_end >= c->right) end |= OUT_RIGHT;
    if (start & end) {
        *reject_code = start & end;
        return -1;
    }
    return start | end;
}

/* 0049ff4d: an edge too long for 32-bit deltas, or with a coordinate past
 * +-16000, is shortened from both ends a quarter of its length at a time until
 * both ends are in range, counting the rows cut off outside the clip. Returns
 * 1 when the edge is in range, 0 if it never gets there (the original loops
 * forever, e.g. for two points on one row far outside the range). */
static int scale_down(const Clip *c, Edge *e)
{
    const int32_t dy = x_sar(x_sub(x_sar(e->y_end, 1), x_sar(e->y, 1)), 1);
    const int32_t dx = x_sar(x_sub(x_sar(e->x_end, 1), x_sar(e->x, 1)), 1);
    for (int steps = 0; steps < SCALE_STEPS; steps++) {
        if (e->y <= -COORD_LIMIT) goto move_start;
        if (e->y_end >= COORD_LIMIT) goto move_end;
        if (e->x <= -COORD_LIMIT || e->x >= COORD_LIMIT) goto move_start;
        if (e->x_end <= -COORD_LIMIT || e->x_end >= COORD_LIMIT) goto move_end;
        return 1;
    move_start: {  /* 0049ff72 */
        e->y = x_add(e->y, dy);
        if (e->y > c->top) {
            int32_t rows = x_sub(e->y, c->top);
            if (rows > dy) rows = dy;
            int add = 1;
            if (e->y > c->bottom) {
                const int32_t past = x_sub(e->y, c->bottom);
                add = rows > past;
                rows = x_sub(rows, past);
            }
            if (add) {
                if (e->x < c->left) e->left_before = x_add(e->left_before, rows);
                else e->right_before = x_add(e->right_before, rows);
            }
        }
        e->x = x_add(e->x, dx);
        continue;
    }
    move_end: {  /* 0049ffe0 */
        e->y_end = x_sub(e->y_end, dy);
        const int32_t below = x_sub(e->y_end, c->bottom);
        if ((int64_t)below + 1 < 0) {
            int32_t rows = x_sub(0, x_add(below, 1));
            if (rows > dy) rows = dy;
            const int32_t above = x_add(x_sub(e->y_end, c->top), 1);
            int add = 1;
            if ((int64_t)x_sub(e->y_end, c->top) + 1 < 0) {
                const int64_t sum = (int64_t)rows + above;
                add = sum > 0;
                rows = (int32_t)sum;
            }
            if (add) {
                if (e->x_end < c->left) e->left_after = x_add(e->left_after, rows);
                else e->right_after = x_add(e->right_after, rows);
            }
        }
        e->x_end = x_sub(e->x_end, dx);
    }
    }
    return 0;
}

/* 0049f8f8: cuts the part of the edge above the clip rectangle. */
static void clip_above(const Clip *c, Edge *e)
{
    const int32_t old_y = e->y;
    const int32_t rows = x_sub(c->top, old_y);
    e->y = c->top;
    const uint64_t moved = (uint64_t)e->slope * (uint32_t)rows;
    switch (e->kind) {
    case EDGE_VERTICAL: break;
    case EDGE_DIAGONAL_LEFT: e->x = x_sub(e->x, rows); break;
    case EDGE_DIAGONAL_RIGHT: e->x = x_add(e->x, rows); break;
    case EDGE_STEEP_LEFT: put_x(e, pair(e->x, e->x_frac) - moved); break;
    case EDGE_STEEP_RIGHT: put_x(e, pair(e->x, e->x_frac) + moved); break;
    case EDGE_SHALLOW_LEFT:
    case EDGE_SHALLOW_RIGHT: {
        e->y = old_y;  /* x-major: the start moves along x, y follows from the slope */
        const uint32_t steps = div_round((uint32_t)rows, 0, e->slope);
        e->x = e->kind == EDGE_SHALLOW_LEFT ? x_sub(e->x, (int32_t)steps)
                                            : x_add(e->x, (int32_t)steps);
        e->count = x_sub(e->count, (int32_t)steps);
        if (e->count <= 0) {
            e->count = 1;
            e->y = c->top;
            e->x = e->x_end;
            return;
        }
        put_y(e, pair(e->y, e->y_frac) + (uint64_t)steps * e->slope);
        return;
    }
    default: break;
    }
    e->count = x_sub(e->count, rows);
}

/* 0049f9e3: cuts the part below the clip rectangle. */
static void clip_below(const Clip *c, Edge *e)
{
    const int32_t last = x_sub(c->bottom, 1);
    const int32_t rows = x_sub(e->y_end, last);
    e->y_end = last;
    switch (e->kind) {
    case EDGE_VERTICAL: e->count = x_sub(e->count, rows); break;
    case EDGE_DIAGONAL_LEFT:
        e->x_end = x_add(e->x_end, rows);
        e->count = x_sub(e->count, rows);
        break;
    case EDGE_DIAGONAL_RIGHT:
        e->x_end = x_sub(e->x_end, rows);
        e->count = x_sub(e->count, rows);
        break;
    case EDGE_STEEP_LEFT:
    case EDGE_STEEP_RIGHT: {
        e->count = x_sub(e->count, rows);
        const uint64_t run = (uint64_t)e->slope * (uint32_t)x_sub(e->count, 1);
        const uint64_t start = pair(e->x, e->x_frac);
        const uint64_t end = e->kind == EDGE_STEEP_LEFT ? start - run : start + run;
        e->x_end = (int32_t)(uint32_t)((end + HALF) >> 32);
        break;
    }
    case EDGE_SHALLOW_LEFT:
    case EDGE_SHALLOW_RIGHT: {
        /* columns until y reaches the last row */
        const int64_t gap = (int64_t)last - e->y - (e->y_frac != 0 ? 1 : 0);
        uint32_t steps = 0;
        if (gap >= 0)
            steps = div_round((uint32_t)gap, e->y_frac != 0 ? (uint32_t)(0u - e->y_frac) : 0,
                              e->slope);
        e->x_end = e->kind == EDGE_SHALLOW_LEFT ? x_sub(e->x, (int32_t)steps)
                                                : x_add(e->x, (int32_t)steps);
        e->count = x_add((int32_t)steps, 1);
        break;
    }
    default: break;
    }
}

/* 0049fadc: cuts the part left of the clip rectangle. Returns 0 when nothing
 * of the edge is left (the caller rejects it as OUT_LEFT). */
static int clip_left(const Clip *c, Edge *e)
{
    const int32_t left = c->left;
    switch (e->kind) {
    case EDGE_DIAGONAL_LEFT: {
        const int32_t cut = x_sub(left, e->x_end);
        e->x_end = left;
        e->left_after = x_add(e->left_after, cut);
        e->count = x_sub(e->count, cut);
        e->y_end = x_sub(e->y_end, cut);
        return 1;
    }
    case EDGE_DIAGONAL_RIGHT: {
        const int32_t cut = x_sub(left, e->x);
        e->x = left;
        e->left_before = x_add(e->left_before, cut);
        e->y = x_add(e->y, cut);
        e->count = x_sub(e->count, cut);
        return 1;
    }
    case EDGE_STEEP_LEFT: {  /* the end is cut where x crosses the clip edge */
        e->x_end = left;
        uint32_t rows = 1;
        if (e->x >= left) rows = div_round((uint32_t)x_sub(e->x, left), e->x_frac, e->slope) + 1;
        const int32_t old = e->count;
        e->count = (int32_t)rows;
        e->left_after = x_add(e->left_after, x_sub(old, (int32_t)rows));
        e->y_end = x_add(x_sub((int32_t)rows, 1), e->y);
        return 1;
    }
    case EDGE_STEEP_RIGHT: {  /* the start is cut */
        const int64_t gap = (int64_t)left - e->x - (e->x_frac != 0 ? 1 : 0);
        if (gap < 0) return 0;
        const uint32_t rows = div_round((uint32_t)gap, e->x_frac != 0 ? 0u - e->x_frac : 0, e->slope);
        e->y = x_add(e->y, (int32_t)rows);
        e->left_before = x_add(e->left_before, (int32_t)rows);
        e->count = x_sub(e->count, (int32_t)rows);
        if (e->count <= 0) return 0;
        put_x(e, pair(e->x, e->x_frac) + (uint64_t)rows * e->slope);
        return 1;
    }
    case EDGE_SHALLOW_LEFT: {
        const uint32_t cols = (uint32_t)x_sub(e->x, left);
        e->x_end = left;
        e->count = x_add((int32_t)cols, 1);
        const uint64_t y = pair(e->y, e->y_frac) + (uint64_t)cols * e->slope + HALF;
        const int32_t new_end = (int32_t)(uint32_t)(y >> 32);
        e->left_after = x_add(e->left_after, x_sub(e->y_end, new_end));
        e->y_end = new_end;
        return 1;
    }
    case EDGE_SHALLOW_RIGHT: {
        const int32_t cols = x_sub(left, e->x);
        e->x = left;
        e->count = x_sub(e->count, cols);
        const uint64_t before = pair(e->y, e->y_frac);
        const uint64_t after = before + (uint64_t)(uint32_t)cols * e->slope;
        put_y(e, after);
        e->left_before = x_add(e->left_before, x_sub((int32_t)(uint32_t)((after + HALF) >> 32),
                                                     (int32_t)(uint32_t)((before + HALF) >> 32)));
        return 1;
    }
    default: return 0;
    }
}

/* 0049fcc2: cuts the part right of the clip rectangle. Returns 0 when nothing
 * of the edge is left (the caller rejects it as OUT_RIGHT). */
static int clip_right(const Clip *c, Edge *e)
{
    const int32_t last = x_sub(c->right, 1);
    switch (e->kind) {
    case EDGE_DIAGONAL_LEFT: {  /* the start is cut */
        const int32_t cut = x_sub(e->x, last);
        e->x = last;
        e->y = x_add(e->y, cut);
        e->count = x_sub(e->count, cut);
        e->right_before = x_add(e->right_before, cut);
        return 1;
    }
    case EDGE_DIAGONAL_RIGHT: {  /* the end is cut */
        const int32_t cut = x_sub(e->x_end, last);
        e->x_end = last;
        e->right_after = x_add(e->right_after, cut);
        e->count = x_sub(e->count, cut);
        e->y_end = x_sub(e->y_end, cut);
        return 1;
    }
    case EDGE_STEEP_LEFT: {
        const uint32_t rows = div_round((uint32_t)x_add(x_sub(e->x, c->right), 1), e->x_frac, e->slope);
        e->count = x_sub(e->count, (int32_t)rows);
        if (e->count <= 0) return 0;
        e->y = x_add(e->y, (int32_t)rows);
        e->right_before = x_add(e->right_before, (int32_t)rows);
        put_x(e, pair(e->x, e->x_frac) - (uint64_t)rows * e->slope);
        return 1;
    }
    case EDGE_STEEP_RIGHT: {
        e->x_end = last;
        const int64_t gap = (int64_t)last - e->x - (e->x_frac != 0 ? 1 : 0);
        if (gap < 0) return 0;
        const uint32_t rows = div_round((uint32_t)gap, e->x_frac != 0 ? 0u - e->x_frac : 0, e->slope) + 1;
        const int32_t old = e->count;
        e->count = (int32_t)rows;
        e->right_after = x_add(e->right_after, x_sub(old, (int32_t)rows));
        e->y_end = x_add(x_sub((int32_t)rows, 1), e->y);
        return 1;
    }
    case EDGE_SHALLOW_LEFT: {
        const int32_t cols = x_sub(e->x, last);
        e->x = last;
        e->count = x_sub(e->count, cols);
        const uint64_t before = pair(e->y, e->y_frac);
        const uint64_t after = before + (uint64_t)(uint32_t)cols * e->slope;
        put_y(e, after);
        e->right_before = x_add(e->right_before, x_sub((int32_t)(uint32_t)((after + HALF) >> 32),
                                                       (int32_t)(uint32_t)((before + HALF) >> 32)));
        return 1;
    }
    case EDGE_SHALLOW_RIGHT: {
        const int32_t cols = x_sub(c->right, e->x);
        e->x_end = last;
        e->count = cols;
        const uint64_t y = pair(e->y, e->y_frac) + (uint64_t)(uint32_t)x_sub(cols, 1) * e->slope + HALF;
        const int32_t new_end = (int32_t)(uint32_t)(y >> 32);
        e->right_after = x_add(e->right_after, x_sub(e->y_end, new_end));
        e->y_end = new_end;
        return 1;
    }
    default: return 0;
    }
}

/* 0049f79a to 0049f8a4: dx, dy, octant, slope and step count of an edge. Returns
 * 0 on success, -1 if a delta overflows 32 bits (the edge is then scaled down). */
static int measure(Edge *e)
{
    int64_t dy64 = (int64_t)e->y_end - e->y;
    int64_t dx64 = (int64_t)e->x_end - e->x;
    if (dy64 != (int32_t)dy64 || dx64 != (int32_t)dx64) return -1;
    uint32_t major = (uint32_t)dy64, minor = (uint32_t)dx64;
    if (dx64 == 0) {
        e->kind = EDGE_VERTICAL;
        e->count = x_add((int32_t)major, 1);
        return 0;
    }
    int x_major = 0;
    if (dx64 < 0) {
        if (dx64 == INT32_MIN) return -1;  /* neg overflows */
        minor = (uint32_t)-dx64;
        if (minor == major) {
            e->kind = EDGE_DIAGONAL_LEFT;
            e->count = x_add((int32_t)major, 1);
            return 0;
        }
        e->kind = minor < major ? EDGE_STEEP_LEFT : EDGE_SHALLOW_LEFT;
        x_major = minor > major;
    } else {
        if (minor == major) {
            e->kind = EDGE_DIAGONAL_RIGHT;
            e->count = x_add((int32_t)major, 1);
            return 0;
        }
        e->kind = minor < major ? EDGE_STEEP_RIGHT : EDGE_SHALLOW_RIGHT;
        x_major = minor > major;
    }
    if (x_major) {
        const uint32_t t = minor;
        minor = major;
        major = t;
    }
    e->slope = edge_slope(minor, major);
    if (major == (uint32_t)INT32_MAX) return -1;  /* inc overflows */
    e->count = (int32_t)major + 1;
    return 0;
}

/* 0049fe21: after the top and bottom are cut, classifies the x extent again.
 * Returns the remaining outcode (0 when the edge is inside) or -1 and the
 * reject code. */
static int reclassify(const Clip *c, const Edge *e, uint8_t *reject_code)
{
    uint8_t start = 0, end = 0;
    const int32_t rounded = x_add(e->x, e->x_frac >= HALF ? 1 : 0);
    if (e->x < c->left) start |= OUT_LEFT;
    if (rounded >= c->right) start |= OUT_RIGHT;
    if (e->x_end < c->left) end |= OUT_LEFT;
    if (e->x_end >= c->right) end |= OUT_RIGHT;
    if (start & end) {
        *reject_code = start & end;
        return -1;
    }
    return start | end;
}

int flat_edge_setup(const Clip *c, int clipping, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                    Edge *e)
{
    e->kind = EDGE_UNSET;
    e->x_frac = 0;
    e->y_frac = 0;
    e->left_before = e->left_after = e->right_before = e->right_after = 0;
    if (y0 <= y1) {
        e->x = x0, e->y = y0, e->x_end = x1, e->y_end = y1;
    } else {
        e->x = x1, e->y = y1, e->x_end = x0, e->y_end = y0;
    }
    if (y0 == y1) return setup_horizontal(c, clipping, x0, x1, y0, e);

    for (;;) {
        uint8_t code = 0;
        int out = 0;
        if (clipping) {
            out = classify(c, e, &code);
            if (out < 0) return reject(c, e, code);
        }
        if (measure(e) < 0) {
            if (!scale_down(c, e)) {
                e->count = 0;
                return 0;
            }
            continue;  /* 0049f79a again with the shortened edge */
        }
        while (out != 0) {
            if (out & OUT_ABOVE) {
                clip_above(c, e);
                if (out & OUT_BELOW) clip_below(c, e);
            } else if (out & OUT_BELOW) {
                clip_below(c, e);
            } else if (out & OUT_LEFT) {  /* 0049fadc, then 0049fe7a */
                if (!clip_left(c, e)) return reject(c, e, OUT_LEFT);
                if (out & OUT_RIGHT) {
                    if (!clip_right(c, e)) return reject(c, e, OUT_RIGHT);
                }
                return 0;
            } else {
                if (!clip_right(c, e)) return reject(c, e, OUT_RIGHT);
                return 0;
            }
            out = reclassify(c, e, &code);
            if (out < 0) return reject(c, e, code);
        }
        return 0;
    }
}
