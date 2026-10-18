/* The followed car's skid marks (004596a8). */
#include "game/race/skid_marks.h"

#include "game/gfx/thick_line.h"
#include "game/gfx/x86_int.h"
#include "game/race/view.h"
#include "game/sys/fixed_math.h"

#include <string.h>

enum { SKID_FROM = 0x3c0000, SKID_TYRES = 3, MARK_STEP = 0xa666, MARK_SMOOTH = 0x30000, BEFORE_FIRST = 0x7d,
       MARK_SMOOTHED = 12 };

/* (t - s) x (p - s), 32-bit. */
static int32_t side_of(const int32_t s[2], const int32_t t[2], int32_t x, int32_t y)
{
    return x_sub(x_mul(x_sub(t[1], s[1]), x_sub(x, s[0])), x_mul(x_sub(t[0], s[0]), x_sub(y, s[1])));
}

void skid_stroke(const QuadTarget *target, const uint8_t *remap, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                 const int32_t s[2], const int32_t t[2], int32_t width)
{
    if (side_of(s, t, x0, y0) < 0 && side_of(s, t, x1, y1) < 0) return;
    thick_line_shade(target, remap, x0, y0, x1, y1, width);
}

void skid_rows(SkidMarks *m, const SkidRing *ring, int32_t node)
{
    if (ring->alternate != 0) return;
    memset(m->rows, 0, SKID_ROWS);
    const int32_t base = x_sub(node, 2) & ring->node_mask;
    for (int32_t i = 0; i < SKID_RECORDS; i++) {
        if (m->marks[i][SKID_FRONT_ON] == 0 && m->marks[i][SKID_REAR_ON] == 0) continue;
        int32_t d = m->marks[i][SKID_NODE];
        if (d < base) d = x_add(d, x_shl(ring->segments, 2));
        d = x_sub(d, base);
        if (d < SKID_ROWS) m->rows[d] = 1;
    }
}

enum { SKID_EVERY = -999, SKID_LOOKED = 0x7b, SKID_AROUND = 2, SKID_OWN = 3, SKID_NEAR = 0x40, SKID_WIDTH = 0x2580,
       SKID_WIDEST = 15, SKID_CAMERA = 15, SKID_SCREEN = 27, SKID_CORNERS = 4, CAMERA_SHIFT = 9 };

static int32_t previous_of(int32_t i)
{
    return i - 1 >= 0 ? i - 1 : SKID_RECORDS - 1;
}

static int row_marked(const SkidRing *ring, const SkidMarks *m, int32_t node, int32_t row)
{
    const int32_t base = x_sub(node, 2) & ring->node_mask;
    const int32_t d = x_sub(base > row ? x_add(row, x_shl(ring->segments, 2)) : row, base);
    return d <= SKID_ROWS && m->rows[d] != 0;
}

/* The corner behind moved along the edge to z 0x40. */
static void clip_near(int32_t behind[3], const int32_t front[3])
{
    const int32_t span = x_add(x_sub(front[2], behind[2]), 1), k = x_sub(front[2], SKID_NEAR);
    behind[2] = SKID_NEAR;
    for (int i = 0; i < 2; i++) behind[i] = x_add(x_div(x_mul(x_sub(behind[i], front[i]), k), span), front[i]);
}

/* z is 0x40 or more here: the original's 100 for z 0 never shows. */
static int32_t stroke_width(const SkidView *v, int32_t z, int32_t halve)
{
    int32_t w = x_div(SKID_WIDTH, z);
    if (w > SKID_WIDEST) w = SKID_WIDEST;
    if (v->high_res == 0) w /= 2;
    if (v->screen_kind != 0) w /= 2;
    if (halve != 0) w /= 2;
    return w < 1 ? 1 : w;
}

typedef struct {
    const SkidView *v;
    SkidMarks *m;
    int every;
    int32_t halve;
    const int32_t *s, *t;
} DrawPass;

/* Corner k of mark cur, placed; joined to the newer mark prev's. */
static void draw_corner(const DrawPass *p, int32_t cur, int32_t prev, int k, int joined)
{
    int32_t *c = p->m->marks[cur], *b = p->m->marks[prev];
    int32_t *cc = c + SKID_CAMERA + 3 * k, *bc = b + SKID_CAMERA + 3 * k;
    if (joined && (cc[2] < SKID_NEAR || bc[2] < SKID_NEAR)) {
        if (cc[2] <= SKID_NEAR) {
            if (bc[2] < SKID_NEAR) return;
            clip_near(cc, bc);
        } else {
            clip_near(bc, cc);
            view_project(p->v->mv->view, bc, b + SKID_SCREEN + 2 * k);
        }
    }
    view_project(p->v->mv->view, cc, c + SKID_SCREEN + 2 * k);
    if (!joined) return;
    const int pair = k < 2 ? SKID_FRONT_ON : SKID_REAR_ON;
    if (b[pair] == 0 || c[pair] == 0) return;
    const int32_t w = stroke_width(p->v, bc[2] >= cc[2] ? bc[2] : cc[2], p->halve);
    const int32_t *a = c + SKID_SCREEN + 2 * k, *e = b + SKID_SCREEN + 2 * k;
    if (p->every) skid_stroke(p->v->target, p->v->remap, a[0], a[1], e[0], e[1], p->s, p->t, w);
    else thick_line_shade(p->v->target, p->v->remap, a[0], a[1], e[0], e[1], w);
}

static void draw_mark(const DrawPass *p, int32_t cur, int32_t prev, int joined)
{
    int32_t *c = p->m->marks[cur];
    const ModelView *mv = p->v->mv;
    int32_t rel[3], at[3];
    for (int i = 0; i < 3; i++) rel[i] = x_sub(c[SKID_POINT + i], mv->camera_position[i]) >> CAMERA_SHIFT;
    mat3_apply(rel, mv->camera_matrix, at);
    for (int k = 0; k < SKID_CORNERS; k++) {
        int32_t *cc = c + SKID_CAMERA + 3 * k;
        mat3_apply(c + 3 * k, mv->camera_matrix, cc);
        for (int i = 0; i < 3; i++) cc[i] = x_add(cc[i], at[i]);
        draw_corner(p, cur, prev, k, joined);
    }
}

void skid_draw(const SkidView *v, const SkidRing *ring, SkidMarks *m, int32_t index, int32_t node, int32_t row,
               int32_t halve, const int32_t s[2], const int32_t t[2])
{
    if (ring->alternate != 0) return;
    const int every = index == SKID_EVERY;
    /* port: 004596a8 passes mark -1 when its index has just wrapped to 0, and
     * the original reads the 0xa0 bytes before the table (00517580's PDNs and
     * after); read here as the spare row past it, never a mark's tyres */
    if (index == -1) index = BEFORE_FIRST;
    int32_t cur = index, near = row, left = SKID_OWN;
    if (every) {
        if (!row_marked(ring, m, node, row)) return;
        cur = previous_of(m->index);
    } else near = m->marks[index][SKID_NODE];
    const DrawPass p = { v, m, every, halve, s, t };
    int joined = 0;
    for (int32_t looked = 0, prev = cur; looked < SKID_LOOKED; looked++) {
        const int32_t d = x_sub(near, m->marks[cur][SKID_NODE]);
        const int within = d >= -SKID_AROUND && d <= SKID_AROUND;
        if (within) draw_mark(&p, cur, prev, joined);
        joined = within;
        prev = cur, cur = previous_of(cur);
        if (!every && --left == 0) return;
    }
}

enum { PUFF_MARKS = 4, PUFF_AGE = 0x30 };

void skid_puffs(const SkidPuffOps *ops, const SkidMarks *m, int32_t tick, int32_t corner, int32_t size)
{
    int32_t i = previous_of(m->index), grown = size;
    for (int n = 0; n < PUFF_MARKS; n++) {
        const int32_t *mark = m->marks[i];
        const int32_t *at = mark + SKID_SCREEN + 2 * corner;
        const int32_t age = x_sub(tick, mark[SKID_TICK]);
        if (mark[corner < 2 ? SKID_FRONT_ON : SKID_REAR_ON] != 0 && age < PUFF_AGE && age > 0 && at[0] != INT32_MIN) {
            if (mark[SKID_SURFACE] == 1) ops->puff_1(ops->ctx, at[0], at[1], grown);
            else ops->puff_other(ops->ctx, at[0], at[1], grown);
        }
        i = previous_of(i), grown = x_add(grown, size);
    }
}

static int32_t next_of(int32_t i)
{
    return i + 1 < SKID_RECORDS ? i + 1 : 0;
}

/* A tyre pair's corners about the box's end: end + side, end - side. */
static void corners(int32_t *at, const int32_t end[3], const int32_t side[3])
{
    for (int i = 0; i < 3; i++) at[i] = x_add(end[i], side[i]), at[3 + i] = x_sub(end[i], side[i]);
}

static int32_t span(int32_t a, int32_t b)
{
    const int32_t d = x_sub(a, b);
    return d > 0 ? d : x_sub(0, d);
}

static void clear_flags(SkidMarks *m, int32_t i)
{
    m->marks[i][SKID_REAR_ON] = 0, m->marks[i][SKID_FRONT_ON] = 0;
}

/* A long step: the mark copied on, its corners then the average of it and
 * the one before, when either tyre pair skidded in both. */
static void smooth(SkidMarks *m, int32_t current, int32_t before)
{
    memcpy(m->marks[m->index], m->marks[current], sizeof m->marks[current]);
    for (int w = 0; w < MARK_SMOOTHED; w++) m->marks[current][w] = x_add(m->marks[current][w], m->marks[before][w]) / 2;
    m->index = next_of(m->index);
}

void skid_record(const SkidOps *ops, SkidMarks *m, const Car *car, const SkidInputs *in, const int32_t forward[3],
                 const int32_t side[3], const int32_t point[3])
{
    if (in->alternate != 0) return;
    int32_t front = car->skid_front, rear = car->skid_rear;
    if (front < SKID_FROM || (car->tyre_flags_front & SKID_TYRES) == 0) front = 0;
    if (rear < SKID_FROM || (car->tyre_flags_rear & SKID_TYRES) == 0) rear = 0;
    if (front != 0 || rear != 0) {
        int32_t end[3], back[3];
        for (int i = 0; i < 3; i++) end[i] = x_sub(forward[i], forward[i] / 4), back[i] = x_sub(0, end[i]);
        int32_t *mark = m->marks[m->index];
        for (int i = 0; i < 3; i++) mark[SKID_POINT + i] = point[i];
        mark[SKID_NODE] = car->node;
        mark[SKID_TICK] = in->tick;
        mark[SKID_SURFACE] = (in->surface_word != 0) + 1;
        if (front != 0) mark[SKID_FRONT_ON] = m->front, corners(mark + SKID_FRONT, end, side);
        else mark[SKID_FRONT_ON] = 0;
        if (rear != 0) mark[SKID_REAR_ON] = m->rear, corners(mark + SKID_REAR, back, side);
        else mark[SKID_REAR_ON] = 0;
        const int32_t moved = x_add(span(m->last[2], point[2]), span(m->last[0], point[0]));
        if (moved <= MARK_STEP) {
            if (rear != 0) ops->draw(ops->ctx, m->index);
        } else {
            const int32_t current = m->index;
            m->index = next_of(m->index);
            clear_flags(m, m->index);
            if (moved >= MARK_SMOOTH) {
                const int32_t before = current - 1 >= 0 ? current - 1 : BEFORE_FIRST;
                if ((m->marks[before][SKID_FRONT_ON] != 0 && m->marks[current][SKID_FRONT_ON] != 0)
                    || (m->marks[before][SKID_REAR_ON] != 0 && m->marks[current][SKID_REAR_ON] != 0))
                    smooth(m, current, before);
            }
            clear_flags(m, next_of(m->index));
            for (int i = 0; i < 3; i++) m->last[i] = point[i];
            if (rear != 0) ops->draw(ops->ctx, m->index - 1);
        }
    }
    m->front = front, m->rear = rear;
}

enum { RESET_NODE = 0x10000 };

void skid_reset(SkidMarks *m)
{
    for (int r = 0; r < SKID_RECORDS; r++) {
        m->marks[r][SKID_NODE] = RESET_NODE;
        m->marks[r][SKID_FRONT_ON] = 0;
        m->marks[r][SKID_REAR_ON] = 0;
    }
    m->index = 0;
}
