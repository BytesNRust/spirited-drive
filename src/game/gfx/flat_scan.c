/* Scan conversion of one polygon edge into the per-row left / right tables.
 * Ported from NFS_WIN.EXE 00484596 and 00484654 (the first chain of the
 * outline), 00484786 (the second chain, in the convex and the per-row
 * variants) and their shared tail at 00484bbd.
 *
 * Both chains are walked downwards from the topmost point. The first writes
 * the x of every row it touches into both tables; the second widens them, so
 * that afterwards left holds the smallest and right the largest x per row.
 * A shallow edge crosses several columns per row: the first chain keeps the
 * outermost column on each side. Each octant of the edge has its own loop,
 * kept in the original's order so the table contents match even where the
 * outline is not convex. */
#include "game/gfx/flat_internal.h"

#include "game/gfx/x86_int.h"

#define ROWS FLAT_MAX_ROWS

static const uint32_t HALF = 0x80000000u;

static int32_t left_at(const RowTables *t, int64_t p) { return table_get(t, p); }
static int32_t right_at(const RowTables *t, int64_t p) { return table_get(t, p + ROWS); }
static void set_left(RowTables *t, int64_t p, int32_t v) { table_set(t, p, v); }
static void set_right(RowTables *t, int64_t p, int32_t v) { table_set(t, p + ROWS, v); }

/* rep stosd: count words from start, the part outside the tables dropped. */
static void fill_words(RowTables *t, int64_t start, int32_t count, int32_t value)
{
    int64_t from = start < 0 ? 0 : start;
    int64_t to = start + count > TABLE_WORDS ? TABLE_WORDS : start + count;
    for (int64_t i = from; i < to; i++) t->words[i] = value;
}

/* The row an edge starts on: y rounded to nearest. */
static int32_t first_row(const Edge *e) { return x_add(e->y, e->y_frac >= HALF ? 1 : 0); }

/* x as 32.32 rounded to the nearest column, the form the sloped loops step. */
typedef struct {
    uint32_t lo;
    int32_t hi;
} Fix;

static Fix fix_start(const Edge *e)
{
    return (Fix){ e->x_frac + HALF, x_add(e->x, e->x_frac >= HALF ? 1 : 0) };
}

static void fix_sub(Fix *f, uint32_t step)
{
    const uint32_t old = f->lo;
    f->lo -= step;
    if (old < step) f->hi = x_sub(f->hi, 1);
}

static int fix_add(Fix *f, uint32_t step)  /* returns the carry out of the fraction */
{
    f->lo += step;
    const int carry = f->lo < step;
    if (carry) f->hi = x_add(f->hi, 1);
    return carry;
}

/* 00484596: rows the outline spends outside the clip rectangle get an empty
 * span (left == clip_left, right == clip_left - 1, or the right side's pair). */
static void fill_outside_both(RowTables *t, const Clip *c, const Edge *e)
{
    const int32_t start = first_row(e);
    if (e->left_before > 0) {
        fill_words(t, x_sub(start, e->left_before), e->left_before, c->left);
        fill_words(t, (int64_t)x_sub(start, e->left_before) + ROWS, e->left_before, x_sub(c->left, 1));
    }
    if (e->right_before > 0) {
        fill_words(t, x_sub(start, e->right_before), e->right_before, c->right);
        fill_words(t, (int64_t)x_sub(start, e->right_before) + ROWS, e->right_before,
                   x_sub(c->right, 1));
    }
    const int64_t after = x_add(e->y_end, 1);
    if (e->left_after > 0) {
        fill_words(t, after, e->left_after, c->left);
        fill_words(t, after + ROWS, e->left_after, x_sub(c->left, 1));
    }
    if (e->right_after > 0) {
        fill_words(t, after, e->right_after, c->right);
        fill_words(t, after + ROWS, e->right_after, x_sub(c->right, 1));
    }
}

/* 00484bc4: the same rows after the second chain, touching one table each. */
static void fill_outside_one(RowTables *t, const Clip *c, const Edge *e)
{
    const int32_t start = first_row(e);
    if (e->left_before > 0) fill_words(t, x_sub(start, e->left_before), e->left_before, c->left);
    if (e->right_before > 0)
        fill_words(t, (int64_t)x_sub(start, e->right_before) + ROWS, e->right_before,
                   x_sub(c->right, 1));
    const int64_t after = x_add(e->y_end, 1);
    if (e->left_after > 0) fill_words(t, after, e->left_after, c->left);
    if (e->right_after > 0) fill_words(t, after + ROWS, e->right_after, x_sub(c->right, 1));
}

void flat_scan_first(RowTables *t, const Clip *c, int clipping, const Edge *e)
{
    if (clipping) fill_outside_both(t, c, e);
    int32_t n = e->count;
    if (n <= 0) return;
    int64_t p = e->y;
    int32_t x = e->x;
    Fix f;
    uint32_t lo;
    switch (e->kind) {
    case EDGE_VERTICAL:
    case EDGE_DIAGONAL_LEFT:
    case EDGE_DIAGONAL_RIGHT: {
        const int32_t step = e->kind == EDGE_VERTICAL ? 0 : e->kind == EDGE_DIAGONAL_LEFT ? -1 : 1;
        for (; n > 0; n--, p++, x = x_add(x, step)) {
            set_right(t, p, x);
            set_left(t, p, x);
        }
        return;
    }
    case EDGE_STEEP_LEFT:
    case EDGE_STEEP_RIGHT:
        f = fix_start(e);
        for (; n > 0; n--, p++) {
            set_right(t, p, f.hi);
            set_left(t, p, f.hi);
            if (e->kind == EDGE_STEEP_LEFT) fix_sub(&f, e->slope);
            else fix_add(&f, e->slope);
        }
        return;
    case EDGE_SHALLOW_LEFT:
        lo = e->y_frac + HALF;
        if (e->y_frac >= HALF) p++;
        for (;;) {
            set_right(t, p, x);
            for (;;) {
                lo += e->slope;
                if (lo < e->slope) {  /* the row ends here */
                    set_left(t, p, x);
                    p++;
                    x = x_sub(x, 1);
                    if (--n == 0) return;
                    break;
                }
                x = x_sub(x, 1);
                if (--n == 0) {
                    set_left(t, p, x_add(x, 1));
                    return;
                }
            }
        }
    case EDGE_SHALLOW_RIGHT:
        lo = e->y_frac + HALF;
        if (e->y_frac >= HALF) p++;
        for (;;) {
            set_left(t, p, x);
            for (;;) {
                lo += e->slope;
                if (lo < e->slope) {
                    set_right(t, p, x);
                    p++;
                    x = x_add(x, 1);
                    if (--n == 0) return;
                    break;
                }
                x = x_add(x, 1);
                if (--n == 0) {
                    set_right(t, p, x_sub(x, 1));
                    return;
                }
            }
        }
    default: return;
    }
}

/* Second chain, convex shortcut (flag byte 0): the first row where the edge
 * lies outside the first chain's span decides, and every remaining row of the
 * edge is then written to that side without looking again. */
static void second_convex(RowTables *t, const Edge *e)
{
    int32_t n = e->count;
    int64_t p = e->y, q;
    int32_t x = e->x;
    uint32_t lo;
    int carry;
    Fix f;
    switch (e->kind) {
    case EDGE_VERTICAL:
    case EDGE_DIAGONAL_LEFT:
    case EDGE_DIAGONAL_RIGHT: {
        const int32_t step = e->kind == EDGE_VERTICAL ? 0 : e->kind == EDGE_DIAGONAL_LEFT ? -1 : 1;
        for (; n > 0; n--, p++, x = x_add(x, step)) {
            if (left_at(t, p) > x) {
                for (; n > 0; n--, p++, x = x_add(x, step)) set_left(t, p, x);
                return;
            }
            if (right_at(t, p) < x) {
                for (; n > 0; n--, p++, x = x_add(x, step)) set_right(t, p, x);
                return;
            }
        }
        return;
    }
    case EDGE_STEEP_LEFT:
    case EDGE_STEEP_RIGHT:
        f = fix_start(e);
        for (; n > 0; n--, p++) {
            const int left_side = left_at(t, p) > f.hi;
            const int right_side = !left_side && right_at(t, p) < f.hi;
            if (left_side || right_side) {
                for (; n > 0; n--, p++) {
                    if (left_side) set_left(t, p, f.hi);
                    else set_right(t, p, f.hi);
                    if (e->kind == EDGE_STEEP_LEFT) fix_sub(&f, e->slope);
                    else fix_add(&f, e->slope);
                }
                return;
            }
            if (e->kind == EDGE_STEEP_LEFT) fix_sub(&f, e->slope);
            else fix_add(&f, e->slope);
        }
        return;
    case EDGE_SHALLOW_LEFT:
        lo = e->y_frac + HALF;
        if (e->y_frac >= HALF) p++;
    next_column_left:
        if (right_at(t, p) < x) {  /* right side: the first column of each remaining row */
            q = p;
            do {
                set_right(t, q++, x);
                do {
                    x = x_sub(x, 1);
                    lo += e->slope;
                    carry = lo < e->slope;
                    if (--n == 0) return;
                } while (!carry);
            } while (1);
        }
        lo += e->slope;
        if (lo < e->slope) {  /* the row ends in this column */
            if (left_at(t, p) > x) {  /* left side: the last column of each remaining row */
                set_left(t, p++, x);
                x = x_sub(x, 1);
                if (--n == 0) return;
                for (;;) {
                    lo += e->slope;
                    if (lo < e->slope) {
                        set_left(t, p++, x);
                        x = x_sub(x, 1);
                        if (--n == 0) return;
                        continue;
                    }
                    x = x_sub(x, 1);
                    if (--n == 0) {
                        set_left(t, p, x_add(x, 1));
                        return;
                    }
                }
            }
            p++;
        }
        x = x_sub(x, 1);
        if (--n != 0) goto next_column_left;
        x = x_add(x, 1);
        if (left_at(t, p) > x) set_left(t, p, x);
        return;
    case EDGE_SHALLOW_RIGHT:
        lo = e->y_frac + HALF;
        if (e->y_frac >= HALF) p++;
    next_column_right:
        if (left_at(t, p) > x) {  /* left side: the first column of each remaining row */
            for (;;) {
                set_left(t, p++, x);
                for (;;) {
                    x = x_add(x, 1);
                    if (--n == 0) return;
                    lo += e->slope;
                    if (lo < e->slope) break;
                }
            }
        }
        lo += e->slope;
        if (lo < e->slope) {
            if (right_at(t, p) < x) {  /* right side: the last column of each remaining row */
                q = p;
                set_right(t, q, x);
                for (;;) {
                    x = x_add(x, 1);
                    lo += e->slope;
                    if (lo < e->slope) {
                        q++;
                        if (--n == 0) {
                            set_right(t, q, x_sub(x, 1));
                            return;
                        }
                        set_right(t, q, x);
                    } else if (--n == 0) {
                        set_right(t, q + 1, x_sub(x, 1));
                        return;
                    }
                }
            }
            p++;
        }
        x = x_add(x, 1);
        if (--n != 0) goto next_column_right;
        return;
    default: return;
    }
}

static void widen(RowTables *t, int64_t p, int32_t x)
{
    if (right_at(t, p) < x) set_right(t, p, x);
    else if (left_at(t, p) > x) set_left(t, p, x);
}

/* Second chain, per-row merge (flag byte 1): every row on its own widens the
 * span to take in the edge's x, so concave outlines draw their extent. */
static void second_merge(RowTables *t, const Edge *e)
{
    int32_t n = e->count;
    int64_t p = e->y;
    int32_t x = e->x;
    uint32_t lo;
    Fix f;
    switch (e->kind) {
    case EDGE_VERTICAL:
    case EDGE_DIAGONAL_LEFT:
    case EDGE_DIAGONAL_RIGHT: {
        const int32_t step = e->kind == EDGE_VERTICAL ? 0 : e->kind == EDGE_DIAGONAL_LEFT ? -1 : 1;
        for (; n > 0; n--, p++, x = x_add(x, step)) widen(t, p, x);
        return;
    }
    case EDGE_STEEP_LEFT:
    case EDGE_STEEP_RIGHT:
        f = fix_start(e);
        for (; n > 0; n--, p++) {
            widen(t, p, f.hi);
            if (e->kind == EDGE_STEEP_LEFT) fix_sub(&f, e->slope);
            else fix_add(&f, e->slope);
        }
        return;
    case EDGE_SHALLOW_LEFT:
        lo = e->y_frac + HALF;
        if (e->y_frac >= HALF) p++;
        for (;;) {
            if (right_at(t, p) < x) set_right(t, p, x);
            for (;;) {
                lo += e->slope;
                if (lo < e->slope) {  /* the row ends here */
                    if (left_at(t, p) > x) set_left(t, p, x);
                    p++;
                    x = x_sub(x, 1);
                    if (--n == 0) return;
                    break;
                }
                x = x_sub(x, 1);
                if (--n == 0) {
                    x = x_add(x, 1);
                    if (left_at(t, p) > x) set_left(t, p, x);
                    return;
                }
            }
        }
    case EDGE_SHALLOW_RIGHT:
        lo = e->y_frac + HALF;
        if (e->y_frac >= HALF) p++;
        for (;;) {
            if (left_at(t, p) > x) set_left(t, p, x);
            for (;;) {
                lo += e->slope;
                if (lo < e->slope) {
                    if (right_at(t, p) < x) set_right(t, p, x);
                    p++;
                    x = x_add(x, 1);
                    if (--n == 0) return;
                    break;
                }
                x = x_add(x, 1);
                if (--n == 0) {
                    x = x_sub(x, 1);
                    if (right_at(t, p) < x) set_right(t, p, x);
                    return;
                }
            }
        }
    default: return;
    }
}

void flat_scan_second(RowTables *t, const Clip *c, int clipping, int merge, const Edge *e)
{
    if (e->count > 0) {
        if (e->kind == EDGE_HORIZONTAL_LEFT || e->kind == EDGE_HORIZONTAL || e->kind == EDGE_POINT ||
            e->kind > EDGE_POINT)
            return;  /* the jump table sends these straight to a ret, without the tail */
        if (merge) second_merge(t, e);
        else second_convex(t, e);
    }
    if (clipping) fill_outside_one(t, c, e);
}
