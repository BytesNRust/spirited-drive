/* The track editor's selected control points (selection.h). */
#include "selection.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sel_init(Selection *s)
{
    s->on = NULL, s->points = 0, s->anchor = -1;
}

void sel_free(Selection *s)
{
    free(s->on);
    sel_init(s);
}

int sel_resize(Selection *s, int points)
{
    if (points < 0) points = 0;
    unsigned char *on = realloc(s->on, (size_t)(points > 0 ? points : 1));
    if (on == NULL) return 0;
    if (points > s->points) memset(on + s->points, 0, (size_t)(points - s->points));
    s->on = on, s->points = points;
    if (s->anchor >= points) s->anchor = -1;
    return 1;
}

int sel_points(const Selection *s) { return s->points; }

void sel_clear(Selection *s)
{
    if (s->points > 0) memset(s->on, 0, (size_t)s->points);
    s->anchor = -1;
}

void sel_click(Selection *s, int i, SelClick how)
{
    if (i < 0 || i >= s->points) return;
    if (how == SEL_TOGGLE) {
        s->on[i] = !s->on[i];
        s->anchor = i;
        return;
    }
    const int from = how == SEL_RANGE && s->anchor >= 0 ? s->anchor : i;
    memset(s->on, 0, (size_t)s->points);
    for (int k = from < i ? from : i; k <= (from < i ? i : from); ++k) s->on[k] = 1;
    if (how == SEL_ONE || s->anchor < 0) s->anchor = i;
}

void sel_box(Selection *s, const TracePoint *p, int n, double x0, double z0, double x1, double z1, int add)
{
    const double lx = fmin(x0, x1), hx = fmax(x0, x1), lz = fmin(z0, z1), hz = fmax(z0, z1);
    if (!add) sel_clear(s);
    for (int i = 0; i < n && i < s->points; ++i)
        if (p[i].x >= lx && p[i].x <= hx && p[i].z >= lz && p[i].z <= hz) s->on[i] = 1;
    if (s->anchor < 0) s->anchor = sel_first(s);
}

int sel_has(const Selection *s, int i) { return i >= 0 && i < s->points && s->on[i]; }

int sel_count(const Selection *s)
{
    int count = 0;
    for (int i = 0; i < s->points; ++i) count += s->on[i] != 0;
    return count;
}

int sel_first(const Selection *s)
{
    for (int i = 0; i < s->points; ++i)
        if (s->on[i]) return i;
    return -1;
}

int sel_last(const Selection *s)
{
    for (int i = s->points - 1; i >= 0; --i)
        if (s->on[i]) return i;
    return -1;
}

void sel_describe(const Selection *s, char *out, size_t cap)
{
    if (cap == 0) return;
    out[0] = '\0';
    size_t at = 0;
    for (int i = 0; i < s->points; ++i) {
        if (!s->on[i]) continue;
        int j = i;
        while (j + 1 < s->points && s->on[j + 1]) ++j;
        char part[32];
        const int n = j > i ? snprintf(part, sizeof part, "%s%d-%d", at ? ", " : "", i, j) : snprintf(part, sizeof part, "%s%d", at ? ", " : "", i);
        if (at + (size_t)n + 1 > cap) {   /* no room: end in "..." */
            const size_t keep = cap > 4 ? (at < cap - 4 ? at : cap - 4) : 0;
            snprintf(out + keep, cap - keep, "...");
            return;
        }
        memcpy(out + at, part, (size_t)n + 1);
        at += (size_t)n;
        i = j;
    }
}

void sel_inserted(Selection *s, int at)
{
    if (at < 0 || at > s->points || !sel_resize(s, s->points + 1)) return;
    memmove(s->on + at + 1, s->on + at, (size_t)(s->points - 1 - at));
    s->on[at] = 0;
    if (s->anchor >= at) s->anchor++;
}

void sel_removed(Selection *s, int at)
{
    if (at < 0 || at >= s->points) return;
    memmove(s->on + at, s->on + at + 1, (size_t)(s->points - 1 - at));
    s->points--;
    if (s->anchor == at) s->anchor = -1;
    else if (s->anchor > at) s->anchor--;
}

void sel_move(const Selection *s, TracePoint *p, int n, double dx, double dz)
{
    for (int i = 0; i < n && i < s->points; ++i)
        if (s->on[i]) p[i].x += dx, p[i].z += dz;
}

void sel_rotate(const Selection *s, TracePoint *p, int n, const TracePoint *from, double degrees)
{
    const int pivot = sel_first(s);
    if (pivot < 0 || pivot >= n) return;
    const double r = degrees * M_PI / 180.0, c = cos(r), sn = sin(r);
    const TracePoint o = from[pivot];
    for (int i = 0; i < n && i < s->points; ++i) {
        if (!s->on[i]) continue;
        const double dx = from[i].x - o.x, dz = from[i].z - o.z;   /* clockwise with z up: (0, 1) turns to (1, 0) */
        double x = o.x + dx * c + dz * sn, z = o.z - dx * sn + dz * c;
        if (fabs(x - round(x)) < 1e-9) x = round(x);   /* a right angle lands on the grid, not a hair beside it */
        if (fabs(z - round(z)) < 1e-9) z = round(z);
        p[i].x = x, p[i].z = z;
    }
}

int sel_can_remove(const Selection *s, char *why, size_t cap)
{
    const int count = sel_count(s);
    if (count == 0) return snprintf(why, cap, "select the points to remove first (click one)"), 0;
    if (s->points - count < TRACE_POINTS_MIN)
        return snprintf(why, cap, "a track keeps at least %d points: %d of %d are selected", TRACE_POINTS_MIN, count, s->points), 0;
    return 1;
}
