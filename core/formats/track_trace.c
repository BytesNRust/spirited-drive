#include "formats/track_trace.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
    SAMPLES_PER_SPAN = 64,   /* the arc-length table: its samples map a length to the curve's parameter */
    ANGLE_TURN = 0x4000,     /* 14-bit headings: one full turn */
    ANGLE_HALF = 0x2000,
    POSITION_ONE = 65536,    /* 16.16 */
    MIN_SPACING_UNITS = 1,   /* the generator's smallest distance between nodes */
    NODES_PER_SEGMENT = 4    /* one TRKD chunk: 4 road nodes */
};

#define NODE_SPACING ((double)TRACK_GEN_NODE_LENGTH / POSITION_ONE)

/* The curve: point i to i+1 is span i, the parameter u runs from 0 at the first point to count-1 at the last. */
typedef struct {
    const TracePoint *p;
    int count;
    double *s;               /* arc length at each sample, malloc'd */
    double *u;               /* the curve's parameter at each sample */
    int n;                   /* samples */
    double length;
} Curve;

static double cr(double p0, double p1, double p2, double p3, double t)
{
    const double t2 = t * t, t3 = t2 * t;
    return 0.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
}

static double cr_d(double p0, double p1, double p2, double p3, double t)
{
    const double t2 = t * t;
    return 0.5 * ((-p0 + p2) + 2 * (2 * p0 - 5 * p1 + 4 * p2 - p3) * t + 3 * (-p0 + 3 * p1 - 3 * p2 + p3) * t2);
}

/* the point (or, with `deriv`, the derivative) of the curve at parameter u, span i = floor(u) (the last span at the end) */
static void eval(const Curve *c, double u, int deriv, double *x, double *z)
{
    int i = (int)floor(u);
    if (i > c->count - 2) i = c->count - 2;
    if (i < 0) i = 0;
    const double t = u - i;
    const TracePoint *p0 = &c->p[i > 0 ? i - 1 : 0], *p1 = &c->p[i], *p2 = &c->p[i + 1];
    const TracePoint *p3 = &c->p[i + 2 < c->count ? i + 2 : c->count - 1];
    *x = deriv ? cr_d(p0->x, p1->x, p2->x, p3->x, t) : cr(p0->x, p1->x, p2->x, p3->x, t);
    *z = deriv ? cr_d(p0->z, p1->z, p2->z, p3->z, t) : cr(p0->z, p1->z, p2->z, p3->z, t);
}

static void curve_free(Curve *c)
{
    free(c->s);
    free(c->u);
    c->s = NULL;
    c->u = NULL;
}

/* the arc-length table of the curve through `p`; 1 on success (curve_free frees it) */
static int curve_make(const TracePoint *p, int count, Curve *c)
{
    const int spans = count - 1, n = spans * SAMPLES_PER_SPAN + 1;
    c->p = p;
    c->count = count;
    c->n = n;
    c->s = malloc(sizeof *c->s * (size_t)n);
    c->u = malloc(sizeof *c->u * (size_t)n);
    if (c->s == NULL || c->u == NULL) {
        curve_free(c);
        return 0;
    }
    for (int k = 0; k < n; ++k) c->u[k] = (double)k / SAMPLES_PER_SPAN;
    c->s[0] = 0;
    double px, pz;
    eval(c, 0, 0, &px, &pz);
    for (int k = 1; k < n; ++k) {
        double x, z;
        eval(c, c->u[k], 0, &x, &z);
        c->s[k] = c->s[k - 1] + hypot(x - px, z - pz);
        px = x;
        pz = z;
    }
    c->length = c->s[n - 1];
    return 1;
}

/* the curve's parameter at arc length `at` (clamped to the curve), by the table */
static double param_at(const Curve *c, double at)
{
    if (at <= 0) return c->u[0];
    if (at >= c->length) return c->u[c->n - 1];
    int lo = 0, hi = c->n - 1;
    while (hi - lo > 1) {
        const int mid = (lo + hi) / 2;
        if (c->s[mid] <= at) lo = mid; else hi = mid;
    }
    const double span = c->s[hi] - c->s[lo];
    const double f = span > 0 ? (at - c->s[lo]) / span : 0;
    return c->u[lo] + (c->u[hi] - c->u[lo]) * f;
}

static int check_points(const TracePoint *points, int count, ExchangeError *err)
{
    if (points == NULL || count < 2) return exchange_fail(err, "a trace has 2 points or more, not %d", count), 0;
    for (int i = 0; i < count; ++i) {
        if (!isfinite(points[i].x) || !isfinite(points[i].z)) return exchange_fail(err, "point %d is not a number", i), 0;
        if (fabs(points[i].x) > TRACE_COORD_MAX || fabs(points[i].z) > TRACE_COORD_MAX)
            return exchange_fail(err, "point %d is more than %g units from the origin", i, TRACE_COORD_MAX), 0;
        if (i > 0 && hypot(points[i].x - points[i - 1].x, points[i].z - points[i - 1].z) < TRACE_STEP_MIN)
            return exchange_fail(err, "point %d is on top of point %d", i, i - 1), 0;
    }
    return 1;
}

double track_trace_length(const TracePoint *points, int count)
{
    if (points == NULL || count < 2) return 0;
    Curve c;
    if (!curve_make(points, count, &c)) return 0;
    const double length = c.length;
    curve_free(&c);
    return length;
}

int track_trace_segments(double length)
{
    const int segments = (int)lround(length / (NODES_PER_SEGMENT * NODE_SPACING));
    return segments < TRACK_GEN_MIN_SEGMENTS ? TRACK_GEN_MIN_SEGMENTS : segments;
}

/* the heading (14-bit, 0 along +z, positive to the right) of the curve at parameter u: the direction of its derivative */
static double heading_of(const Curve *c, double u)
{
    double dx, dz;
    eval(c, u, 1, &dx, &dz);
    return atan2(dx, dz) * ANGLE_TURN / (2 * M_PI);
}

int track_trace_nodes(const TracePoint *points, int count, double width, TrackGenNode **nodes, int *node_count, ExchangeError *err)
{
    *nodes = NULL;
    *node_count = 0;
    if (!check_points(points, count, err)) return 0;
    Curve c;
    if (!curve_make(points, count, &c)) return exchange_fail(err, "out of memory"), 0;
    const double length = c.length;
    const int segments = track_trace_segments(length);
    const int count_nodes = NODES_PER_SEGMENT * segments + 1;
    int ok = 0;
    if (segments > TRACK_GEN_MAX_SEGMENTS) {
        exchange_fail(err, "the road is %d segments; the most is %d (about %g units)", segments, TRACK_GEN_MAX_SEGMENTS,
                      (double)TRACK_GEN_MAX_SEGMENTS * NODES_PER_SEGMENT * NODE_SPACING);
    } else if (length / (NODES_PER_SEGMENT * segments) < MIN_SPACING_UNITS) {
        exchange_fail(err, "the road is too short: %g units for %d nodes", length, count_nodes);
    } else {
        TrackGenNode *out = malloc(sizeof *out * (size_t)count_nodes);
        if (out == NULL) {
            exchange_fail(err, "out of memory");
        } else {
            const double step = length / (NODES_PER_SEGMENT * segments);
            for (int k = 0; k < count_nodes; ++k) {
                const double u = param_at(&c, k * step);
                double x, z;
                eval(&c, u, 0, &x, &z);
                out[k].heading = heading_of(&c, u);
                out[k].grade = 0;
                out[k].bank = 0;
                out[k].width = width;
                out[k].position[0] = (int32_t)lround(x * POSITION_ONE);
                out[k].position[1] = 0;
                out[k].position[2] = (int32_t)lround(z * POSITION_ONE);
            }
            *nodes = out;
            *node_count = count_nodes;
            ok = 1;
        }
    }
    curve_free(&c);
    return ok;
}

double track_trace_turn_degrees(double from, double to)
{
    double d = fmod(to - from, ANGLE_TURN);
    if (d > ANGLE_HALF) d -= ANGLE_TURN;
    if (d < -ANGLE_HALF) d += ANGLE_TURN;
    return d * 360.0 / ANGLE_TURN;
}

void track_trace_flags(const TrackGenNode *nodes, int count, uint8_t *flags)
{
    if (count > 0) flags[0] = 0;
    for (int i = 1; i < count; ++i) {
        uint8_t f = 0;
        const double turn = fabs(track_trace_turn_degrees(nodes[i - 1].heading, nodes[i].heading));
        if (turn > TRACE_TURN_MAX_DEGREES + 1e-6) f |= TRACE_FLAG_TURN;
        if (turn > 1e-9) {
            const double dx = ((double)nodes[i].position[0] - nodes[i - 1].position[0]) / POSITION_ONE;
            const double dz = ((double)nodes[i].position[2] - nodes[i - 1].position[2]) / POSITION_ONE;
            const double radius = hypot(dx, dz) / (turn * M_PI / 180);
            if (radius < TRACE_RADIUS_MIN) f |= TRACE_FLAG_RADIUS;
        }
        flags[i] = f;
    }
}
