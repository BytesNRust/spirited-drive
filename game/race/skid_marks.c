/* The followed car's skid marks (004596a8). */
#include "game/race/skid_marks.h"

#include "game/gfx/thick_line.h"
#include "game/gfx/x86_int.h"

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
