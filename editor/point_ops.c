/* Paste, mirror and reverse on a run of control points (point_ops.h). */
#include "point_ops.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TracePoint sub(TracePoint a, TracePoint b) { return (TracePoint){ a.x - b.x, a.z - b.z }; }
static TracePoint add(TracePoint a, TracePoint b) { return (TracePoint){ a.x + b.x, a.z + b.z }; }

/* v turned by `r` radians anticlockwise in (x, z) (from +x towards +z) */
static TracePoint turn(TracePoint v, double r)
{
    const double c = cos(r), s = sin(r);
    return (TracePoint){ v.x * c - v.z * s, v.x * s + v.z * c };
}

static double angle(TracePoint v) { return atan2(v.z, v.x); }

/* v reflected about the line along unit u */
static TracePoint reflect(TracePoint v, TracePoint u)
{
    const double t = v.x * u.x + v.z * u.z;
    return (TracePoint){ 2 * t * u.x - v.x, 2 * t * u.z - v.z };
}

TracePoint pts_heading(const TracePoint *p, int n, int i)
{
    if (n < 2 || i < 0 || i >= n) return (TracePoint){ 0, 0 };
    const TracePoint d = i == 0 ? sub(p[1], p[0]) : sub(p[i], p[i - 1]);
    const double len = hypot(d.x, d.z);
    return len > 0 ? (TracePoint){ d.x / len, d.z / len } : (TracePoint){ 0, 0 };
}

static TracePoint *copy_of(const TracePoint *p, int n)
{
    TracePoint *q = malloc(sizeof *q * (size_t)(n > 0 ? n : 1));
    if (q != NULL && n > 0) memcpy(q, p, sizeof *q * (size_t)n);
    return q;
}

/* q[e+1..n) set to p's tail, which joined p's run at p[e] arriving along `was`, turned and moved to join q[e] arriving along `now` */
static void tail_follows(TracePoint *q, const TracePoint *p, int n, int e, TracePoint was, TracePoint now)
{
    const double r = (was.x == 0 && was.z == 0) || (now.x == 0 && now.z == 0) ? 0 : angle(now) - angle(was);
    for (int i = e + 1; i < n; ++i) q[i] = add(q[e], turn(sub(p[i], p[e]), r));
}

static int run_ok(int n, int s, int e) { return n >= 2 && s >= 0 && e < n && s < e; }

TracePoint *pts_paste(const TracePoint *p, int n, int anchor, const TracePoint *block, int m)
{
    if (n < 1 || anchor < 0 || anchor >= n || m < 2) return NULL;
    TracePoint *q = malloc(sizeof *q * (size_t)(n + m - 1));
    if (q == NULL) return NULL;
    memcpy(q, p, sizeof *q * (size_t)(anchor + 1));
    const TracePoint road = pts_heading(p, n, anchor), first = pts_heading(block, m, 0);
    const double r = (road.x == 0 && road.z == 0) || (first.x == 0 && first.z == 0) ? 0 : angle(road) - angle(first);
    for (int i = 1; i < m; ++i) q[anchor + i] = add(p[anchor], turn(sub(block[i], block[0]), r));
    const int end = anchor + m - 1;   /* the tail, which left p[anchor], leaves the block's end with the same turn */
    const TracePoint now = pts_heading(q, end + 1, end);
    const double rt = (road.x == 0 && road.z == 0) || (now.x == 0 && now.z == 0) ? 0 : angle(now) - angle(road);
    for (int i = anchor + 1; i < n; ++i) q[end + i - anchor] = add(q[end], turn(sub(p[i], p[anchor]), rt));
    return q;
}

static TracePoint *mirror(const TracePoint *p, int n, int s, int e, int square)
{
    if (!run_ok(n, s, e)) return NULL;
    TracePoint *q = copy_of(p, n);
    if (q == NULL) return NULL;
    const TracePoint h = pts_heading(p, n, s), u = square ? (TracePoint){ -h.z, h.x } : h;
    for (int i = s + 1; i <= e; ++i) q[i] = add(p[s], reflect(sub(p[i], p[s]), u));
    tail_follows(q, p, n, e, pts_heading(p, n, e), pts_heading(q, n, e));
    return q;
}

TracePoint *pts_mirror_lr(const TracePoint *p, int n, int s, int e) { return mirror(p, n, s, e, 0); }
TracePoint *pts_mirror_fb(const TracePoint *p, int n, int s, int e) { return mirror(p, n, s, e, 1); }

TracePoint *pts_reverse(const TracePoint *p, int n, int s, int e)
{
    if (!run_ok(n, s, e)) return NULL;
    TracePoint *q = copy_of(p, n);
    if (q == NULL) return NULL;
    for (int i = s; i <= e; ++i) q[i] = p[s + e - i];   /* the order turned round, where the points were */
    if (s == 0 && e == n - 1) return q;
    if (s > 0) {   /* the new first on p[s], its first chord along the heading arriving at s */
        const TracePoint road = pts_heading(p, n, s), first = sub(q[s + 1], q[s]);
        const double r = angle(road) - angle(first);
        const TracePoint o = q[s];
        for (int i = s; i <= e; ++i) q[i] = add(p[s], turn(sub(q[i], o), r));
        tail_follows(q, p, n, e, pts_heading(p, n, e), pts_heading(q, n, e));
        return q;
    }
    const TracePoint road = pts_heading(p, n, e), last = sub(q[e], q[e - 1]);   /* s == 0: the new last on p[e], arriving as the road did */
    const double r = angle(road) - angle(last);
    const TracePoint o = q[e];
    for (int i = 0; i <= e; ++i) q[i] = add(p[e], turn(sub(q[i], o), r));
    return q;
}

/* the turn from heading a to heading b in degrees, positive to the right on screen (z up: anticlockwise in (x, z) is to the left) */
static double right_turn(TracePoint a, TracePoint b)
{
    if ((a.x == 0 && a.z == 0) || (b.x == 0 && b.z == 0)) return 0;
    return -remainder(angle(b) - angle(a), 2 * M_PI) * 180.0 / M_PI;
}

PointReadout pts_readout(const TracePoint *p, int n, int i)
{
    PointReadout r = { 0, 0, 0 };
    if (i < 0 || i >= n) return r;
    if (i > 0) r.to_prev_m = TRACE_METRES(hypot(p[i].x - p[i - 1].x, p[i].z - p[i - 1].z));
    if (i == 0 || i == n - 1) return r;
    const TracePoint in = sub(p[i], p[i - 1]), out = sub(p[i + 1], p[i]);
    r.turn_deg = right_turn(in, out);
    const double cross = in.x * out.z - in.z * out.x;
    if (fabs(cross) > 1e-12) {   /* the circle through the three points: a b c / (4 area) */
        const TracePoint across = sub(p[i + 1], p[i - 1]);
        r.radius_m = TRACE_METRES(hypot(in.x, in.z) * hypot(out.x, out.z) * hypot(across.x, across.z) / (2 * fabs(cross)));
    }
    return r;
}

EndGap pts_end_gap(const TracePoint *p, int n)
{
    EndGap g = { 0, 0 };
    if (n < 3) return g;
    g.gap_m = TRACE_METRES(hypot(p[n - 1].x - p[0].x, p[n - 1].z - p[0].z));
    g.turn_deg = right_turn(pts_heading(p, n, n - 1), pts_heading(p, n, 0));
    return g;
}

void pts_end_gap_text(EndGap g, int nodes, char *out, size_t cap)
{
    char turn[16];
    track_trace_turn_text(g.turn_deg, turn, sizeof turn);
    if (g.gap_m < PTS_CLOSE_NOTE_M && nodes > 0)
        snprintf(out, cap, "end: %.1f m and %s from the start; a circuit needs 512 or 1024 nodes, this road has %d", g.gap_m, turn, nodes);
    else
        snprintf(out, cap, "end: %.1f m and %s from the start", g.gap_m, turn);
}
