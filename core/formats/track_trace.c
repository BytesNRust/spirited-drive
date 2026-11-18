#include "formats/track_trace.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
    SAMPLES_PER_SPAN = 64,
    ANGLE_TURN = 0x4000,     /* 14-bit headings: one full turn */
    ANGLE_HALF = 0x2000,
    POSITION_ONE = 65536,    /* 16.16 */
    MIN_SPACING_UNITS = 1,   /* the generator's smallest distance between nodes */
    NODES_PER_SEGMENT = 4    /* one TRKD chunk: 4 road nodes */
};

#define NODE_SPACING ((double)TRACK_GEN_NODE_LENGTH / POSITION_ONE)

typedef struct {
    double x, z, s;          /* a sample of the curve, with the arc length from the first point */
} Sample;

static double cr(double p0, double p1, double p2, double p3, double t)
{
    const double t2 = t * t, t3 = t2 * t;
    return 0.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
}

/* the points of the curve, `count` - 1 spans of SAMPLES_PER_SPAN samples and the last point; malloc'd into *out */
static int sample_curve(const TracePoint *p, int count, Sample **out, int *n)
{
    const int spans = count - 1;
    const int total = spans * SAMPLES_PER_SPAN + 1;
    Sample *s = malloc(sizeof *s * (size_t)total);
    if (s == NULL) return 0;
    int k = 0;
    for (int i = 0; i < spans; ++i) {
        const TracePoint *p0 = &p[i > 0 ? i - 1 : 0], *p1 = &p[i], *p2 = &p[i + 1], *p3 = &p[i + 2 < count ? i + 2 : count - 1];
        for (int j = 0; j < SAMPLES_PER_SPAN; ++j) {
            const double t = (double)j / SAMPLES_PER_SPAN;
            s[k].x = cr(p0->x, p1->x, p2->x, p3->x, t);
            s[k].z = cr(p0->z, p1->z, p2->z, p3->z, t);
            s[k].s = 0;
            ++k;
        }
    }
    s[k].x = p[count - 1].x;
    s[k].z = p[count - 1].z;
    s[k].s = 0;
    for (int m = 1; m < total; ++m) s[m].s = s[m - 1].s + hypot(s[m].x - s[m - 1].x, s[m].z - s[m - 1].z);
    *out = s;
    *n = total;
    return 1;
}

/* the curve at arc length `at` (clamped to the curve), by its samples */
static void curve_at(const Sample *s, int n, double at, double *x, double *z)
{
    if (at <= 0) { *x = s[0].x; *z = s[0].z; return; }
    if (at >= s[n - 1].s) { *x = s[n - 1].x; *z = s[n - 1].z; return; }
    int lo = 0, hi = n - 1;
    while (hi - lo > 1) {
        const int mid = (lo + hi) / 2;
        if (s[mid].s <= at) lo = mid; else hi = mid;
    }
    const double span = s[hi].s - s[lo].s;
    const double f = span > 0 ? (at - s[lo].s) / span : 0;
    *x = s[lo].x + (s[hi].x - s[lo].x) * f;
    *z = s[lo].z + (s[hi].z - s[lo].z) * f;
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
    Sample *s = NULL;
    int n = 0;
    if (!sample_curve(points, count, &s, &n)) return 0;
    const double length = s[n - 1].s;
    free(s);
    return length;
}

int track_trace_segments(double length)
{
    int segments = (int)lround(length / (NODES_PER_SEGMENT * NODE_SPACING));
    return segments < TRACK_GEN_MIN_SEGMENTS ? TRACK_GEN_MIN_SEGMENTS : segments;
}

/* the heading (14-bit, 0 along +z, positive to the right) of the curve at arc length `at`, from the curve half a node either side */
static double heading_at(const Sample *s, int n, double at, double half, double length)
{
    double x0, z0, x1, z1;
    curve_at(s, n, at - half < 0 ? 0 : at - half, &x0, &z0);
    curve_at(s, n, at + half > length ? length : at + half, &x1, &z1);
    return atan2(x1 - x0, z1 - z0) * ANGLE_TURN / (2 * M_PI);
}

int track_trace_nodes(const TracePoint *points, int count, double width, TrackGenNode **nodes, int *node_count, ExchangeError *err)
{
    *nodes = NULL;
    *node_count = 0;
    if (!check_points(points, count, err)) return 0;
    Sample *s = NULL;
    int n = 0;
    if (!sample_curve(points, count, &s, &n)) return exchange_fail(err, "out of memory"), 0;
    const double length = s[n - 1].s;
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
            const double step = length / (NODES_PER_SEGMENT * segments), half = step / 2;
            for (int k = 0; k < count_nodes; ++k) {
                const double at = k * step;
                double x, z;
                curve_at(s, n, at, &x, &z);
                out[k].heading = heading_at(s, n, at, half, length);
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
    free(s);
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
