#include "formats/track_trace.h"

#include "formats/race_stage.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
            return exchange_fail(err, "point %d is more than %g m from the origin", i, TRACE_METRES(TRACE_COORD_MAX)), 0;
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

/* ---- the road's width: the project's at the start, a key's at its point, a smoothstep between ---- */

typedef struct {
    int point;
    double d;   /* arc length along the curve at the point, units */
    double w;   /* the width there, the verge byte */
} WidthStop;

static double smoothstep(double t)
{
    return t * t * (3 - 2 * t);
}

/* The stops of the width, ascending by point: the start (point 0 with the project's width, unless a key is on it) and every key that sets a width.
 * Fills stops[0..*n) (room for key_count + 1); 0 with err when a key is off the points, off the road's range, or twice on a point. */
static int width_stops(const Curve *c, double width, const TraceKey *keys, int key_count, WidthStop *stops, int *n, ExchangeError *err)
{
    int used = 0;
    for (int k = 0; k < key_count; ++k) {
        if (!(keys[k].has & TRACE_KEY_WIDTH)) continue;
        const int point = keys[k].point;
        if (point < 0 || point >= c->count) return exchange_fail(err, "a road key is on point %d, and the trace has points 0..%d", point, c->count - 1), 0;
        if (!(keys[k].width_m >= TRACE_ROAD_MIN_M && keys[k].width_m <= TRACE_ROAD_MAX_M))
            return exchange_fail(err, "the road key on point %d has width %g m, outside %g..%g m", point, keys[k].width_m, TRACE_ROAD_MIN_M, TRACE_ROAD_MAX_M), 0;
        int at = used;
        while (at > 0 && stops[at - 1].point > point) --at;
        if (at > 0 && stops[at - 1].point == point) return exchange_fail(err, "point %d has a road key twice", point), 0;
        for (int j = used; j > at; --j) stops[j] = stops[j - 1];
        stops[at].point = point;
        stops[at].d = c->s[point * SAMPLES_PER_SPAN];
        stops[at].w = keys[k].width_m * TRACE_WIDTH_PER_METRE;
        ++used;
    }
    if (used == 0 || stops[0].point != 0) {   // the start is a stop too: the project's width, from the first point
        for (int j = used; j > 0; --j) stops[j] = stops[j - 1];
        stops[0].point = 0;
        stops[0].d = 0;
        stops[0].w = width;
        ++used;
    }
    *n = used;
    return 1;
}

/* the width (byte) at arc length `at`: the stop before's, to the stop after's by a smoothstep; the last one's after the last stop */
static double width_at(const WidthStop *stops, int n, double at)
{
    if (at >= stops[n - 1].d) return stops[n - 1].w;
    for (int j = 0; j + 1 < n; ++j) {
        if (at >= stops[j + 1].d) continue;
        const double span = stops[j + 1].d - stops[j].d;
        const double t = span > 0 ? (at - stops[j].d) / span : 1;
        return stops[j].w + (stops[j + 1].w - stops[j].w) * smoothstep(t < 0 ? 0 : t);
    }
    return stops[n - 1].w;
}

int track_trace_nodes_keyed(const TracePoint *points, int count, double width, const TraceKey *keys, int key_count, TrackGenNode **nodes,
                            int *node_count, ExchangeError *err)
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
    WidthStop *stops = malloc(sizeof *stops * (size_t)(key_count > 0 ? key_count + 1 : 1));
    int stop_count = 0;
    if (stops == NULL) {
        exchange_fail(err, "out of memory");
    } else if (!width_stops(&c, width, keys, key_count, stops, &stop_count, err)) {
        // err names the key
    } else if (segments > TRACK_GEN_MAX_SEGMENTS) {
        exchange_fail(err, "the road is %d segments; the most is %d (about %g m)", segments, TRACK_GEN_MAX_SEGMENTS,
                      TRACE_METRES((double)TRACK_GEN_MAX_SEGMENTS * NODES_PER_SEGMENT * NODE_SPACING));
    } else if (length / (NODES_PER_SEGMENT * segments) < MIN_SPACING_UNITS) {
        exchange_fail(err, "the road is too short: %g m for %d nodes", TRACE_METRES(length), count_nodes);
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
                out[k].width = stop_count == 1 ? stops[0].w : width_at(stops, stop_count, k * step);   // one stop: that width as it is (the project's with no key), not a computed one
                out[k].position[0] = (int32_t)lround(x * POSITION_ONE);
                out[k].position[1] = 0;
                out[k].position[2] = (int32_t)lround(z * POSITION_ONE);
            }
            *nodes = out;
            *node_count = count_nodes;
            ok = 1;
        }
    }
    free(stops);
    curve_free(&c);
    return ok;
}

int track_trace_nodes(const TracePoint *points, int count, double width, TrackGenNode **nodes, int *node_count, ExchangeError *err)
{
    return track_trace_nodes_keyed(points, count, width, NULL, 0, nodes, node_count, err);
}

void track_trace_point_widths(const TracePoint *points, int count, double width, const TraceKey *keys, int key_count, double *metres)
{
    for (int i = 0; i < count; ++i) metres[i] = 0;
    Curve c;
    ExchangeError err;
    if (points == NULL || count < 2 || !curve_make(points, count, &c)) return;
    WidthStop *stops = malloc(sizeof *stops * (size_t)(key_count > 0 ? key_count + 1 : 1));
    int n = 0;
    if (stops != NULL && width_stops(&c, width, keys, key_count, stops, &n, &err)) {
        for (int i = 0; i < count; ++i) metres[i] = width_at(stops, n, c.s[i * SAMPLES_PER_SPAN]) / TRACE_WIDTH_PER_METRE;
    }
    free(stops);
    curve_free(&c);
}

/* ---- the key list's edits ---- */

int track_trace_key_find(const TraceKey *keys, int count, int point)
{
    for (int i = 0; i < count; ++i)
        if (keys[i].point == point) return i;
    return -1;
}

int track_trace_key_set_width(TraceKey **keys, int *count, int *cap, int point, double metres)
{
    int at = track_trace_key_find(*keys, *count, point);
    if (at < 0) {
        if (*count == *cap) {
            const int grown = *cap == 0 ? 8 : *cap * 2;
            TraceKey *more = realloc(*keys, sizeof *more * (size_t)grown);
            if (more == NULL) return 0;
            *keys = more;
            *cap = grown;
        }
        at = *count;
        while (at > 0 && (*keys)[at - 1].point > point) {
            (*keys)[at] = (*keys)[at - 1];
            --at;
        }
        (*keys)[at].point = point;
        (*keys)[at].has = 0;
        (*keys)[at].width_m = 0;
        ++*count;
    }
    (*keys)[at].has |= TRACE_KEY_WIDTH;
    (*keys)[at].width_m = metres;
    return 1;
}

static void key_remove_at(TraceKey *keys, int *count, int at)
{
    for (int i = at; i + 1 < *count; ++i) keys[i] = keys[i + 1];
    --*count;
}

void track_trace_key_clear_width(TraceKey *keys, int *count, int point)
{
    const int at = track_trace_key_find(keys, *count, point);
    if (at < 0) return;
    keys[at].has &= ~(unsigned)TRACE_KEY_WIDTH;
    keys[at].width_m = 0;
    if (keys[at].has == 0) key_remove_at(keys, count, at);   // no other value set (the next steps' bits will say): the key is nothing
}

void track_trace_keys_insert_point(TraceKey *keys, int count, int index)
{
    for (int i = 0; i < count; ++i)
        if (keys[i].point >= index) ++keys[i].point;
}

void track_trace_keys_remove_point(TraceKey *keys, int *count, int index)
{
    const int at = track_trace_key_find(keys, *count, index);
    if (at >= 0) key_remove_at(keys, count, at);
    for (int i = 0; i < *count; ++i)
        if (keys[i].point > index) --keys[i].point;
}

double track_trace_turn_degrees(double from, double to)
{
    double d = fmod(to - from, ANGLE_TURN);
    if (d > ANGLE_HALF) d -= ANGLE_TURN;
    if (d < -ANGLE_HALF) d += ANGLE_TURN;
    return d * 360.0 / ANGLE_TURN;
}

/* a node's turn from the node before (degrees, unsigned) and the radius of that turn (units; 0 on a straight) */
static void node_turn_radius(const TrackGenNode *nodes, int i, double *turn, double *radius)
{
    *turn = fabs(track_trace_turn_degrees(nodes[i - 1].heading, nodes[i].heading));
    *radius = 0;
    if (*turn > 1e-9) {
        const double dx = ((double)nodes[i].position[0] - nodes[i - 1].position[0]) / POSITION_ONE;
        const double dz = ((double)nodes[i].position[2] - nodes[i - 1].position[2]) / POSITION_ONE;
        *radius = hypot(dx, dz) / (*turn * M_PI / 180);
    }
}

void track_trace_flags(const TrackGenNode *nodes, int count, uint8_t *flags)
{
    if (count > 0) flags[0] = 0;
    for (int i = 1; i < count; ++i) {
        uint8_t f = 0;
        double turn, radius;
        node_turn_radius(nodes, i, &turn, &radius);
        if (turn > TRACE_TURN_MAX_DEGREES + 1e-6) f |= TRACE_FLAG_TURN;
        else if (turn > TRACE_TURN_WARN_DEGREES + 1e-6) f |= TRACE_FLAG_TURN_WARN;
        if (turn > 1e-9) {   // a turn with no distance (radius 0) is under every limit
            if (radius < TRACE_RADIUS_MIN) f |= TRACE_FLAG_RADIUS;
            else if (radius < TRACE_RADIUS_WARN) f |= TRACE_FLAG_RADIUS_WARN;
        }
        if (fabs(nodes[i].width - nodes[i - 1].width) > TRACE_WIDTH_STEP_WARN_M * TRACE_WIDTH_PER_METRE + 1e-6) f |= TRACE_FLAG_WIDTH_WARN;
        flags[i] = f;
    }
}

#define RED_BITS (TRACE_FLAG_TURN | TRACE_FLAG_RADIUS)

int track_trace_problems(const TrackGenNode *nodes, int count, const uint8_t *flags, TraceProblem *out, int cap)
{
    int total = 0;
    for (int pass = 0; pass < 2; ++pass) {   /* the red nodes, then the amber ones */
        for (int i = 1; i < count; ++i) {
            const int red = (flags[i] & RED_BITS) != 0;
            if (flags[i] == 0 || red != (pass == 0)) continue;
            if (total < cap) {
                out[total].node = i;
                out[total].red = red;
                out[total].flags = flags[i];
                node_turn_radius(nodes, i, &out[total].turn, &out[total].radius);
                out[total].width_step = (flags[i] & TRACE_FLAG_WIDTH_WARN) ? fabs(nodes[i].width - nodes[i - 1].width) / TRACE_WIDTH_PER_METRE : 0;
            }
            ++total;
        }
    }
    return total;
}

#define DEGREE "\xc2\xb0"

int track_trace_stage_min_nodes(void)
{
    return RACE_STAGE_END + RACE_GRID_FRONT + 2;
}

int track_trace_stage_short(int node_count, char *out, size_t cap)
{
    if (cap > 0) out[0] = '\0';
    if (node_count >= track_trace_stage_min_nodes()) return 0;
    const double node_km = TRACE_METRES(TRACK_GEN_NODE_LENGTH / 65536.0) / 1000.0;   /* the spacing, 16.16 */
    const int road = node_count > 0 ? node_count - 1 : 0, needs = track_trace_stage_min_nodes() - 1;
    if (cap > 0)   /* a cut text is still the reason: snprintf ends it within cap */
        (void)snprintf(out, cap, "too short for a stage: %.2f km of road (%d nodes); a stage needs %.2f km (%d nodes): the race ends %d nodes (%.2f km) before the "
                                 "road's last node, and the front row starts at node %d", road * node_km, road, needs * node_km, needs, RACE_STAGE_END,
                       RACE_STAGE_END * node_km, RACE_GRID_FRONT);
    return 1;
}

void track_trace_problem_text(const TraceProblem *p, char *out, size_t cap)
{
    const int turn_red = (p->flags & TRACE_FLAG_TURN) != 0, turn = turn_red || (p->flags & TRACE_FLAG_TURN_WARN);
    const int radius_red = (p->flags & TRACE_FLAG_RADIUS) != 0, radius = radius_red || (p->flags & TRACE_FLAG_RADIUS_WARN);
    const int width = (p->flags & TRACE_FLAG_WIDTH_WARN) != 0;
    char clauses[3][64];
    int n = 0;
    if (turn) snprintf(clauses[n++], sizeof clauses[0], "turns %.1f" DEGREE " (more than %g" DEGREE ")", p->turn, turn_red ? TRACE_TURN_MAX_DEGREES : TRACE_TURN_WARN_DEGREES);
    if (radius) snprintf(clauses[n++], sizeof clauses[0], "radius %.0f m (under %g m)", TRACE_METRES(p->radius), TRACE_METRES(radius_red ? TRACE_RADIUS_MIN : TRACE_RADIUS_WARN));
    if (width) snprintf(clauses[n++], sizeof clauses[0], "width changes %.2f m (more than %g m)", p->width_step, TRACE_WIDTH_STEP_WARN_M);
    snprintf(out, cap, "node %d: ", p->node);
    for (int i = 0; i < n; ++i) {
        const size_t at = strlen(out);
        snprintf(out + at, cap > at ? cap - at : 0, "%s%s", i > 0 ? " and " : "", clauses[i]);
    }
    const size_t at = strlen(out);
    snprintf(out + at, cap > at ? cap - at : 0, ", %s", p->red ? "can't be built" : "may not look right");
}

int track_trace_summary(const uint8_t *flags, int count, TraceSummaryLine lines[TRACE_SUMMARY_LINES])
{
    enum { RULE_TURN, RULE_RADIUS, RULE_WIDTH };
    static const struct { uint8_t bit; int red, kind; } RULES[TRACE_SUMMARY_LINES] = {
        { TRACE_FLAG_TURN, 1, RULE_TURN }, { TRACE_FLAG_RADIUS, 1, RULE_RADIUS }, { TRACE_FLAG_TURN_WARN, 0, RULE_TURN },
        { TRACE_FLAG_RADIUS_WARN, 0, RULE_RADIUS }, { TRACE_FLAG_WIDTH_WARN, 0, RULE_WIDTH }
    };
    int used = 0;
    for (int r = 0; r < TRACE_SUMMARY_LINES; ++r) {
        int nodes = 0;
        for (int i = 0; i < count; ++i) nodes += (flags[i] & RULES[r].bit) != 0;
        if (nodes == 0) continue;
        const char *plural = nodes == 1 ? "" : "s";
        const char *verdict = RULES[r].red ? "the track can't be built" : (nodes == 1 ? "it may not look right in the game" : "they may not look right in the game");
        lines[used].red = RULES[r].red;
        lines[used].nodes = nodes;
        if (RULES[r].kind == RULE_TURN)
            snprintf(lines[used].text, TRACE_SUMMARY_TEXT, "%d node%s turn%s more than %g" DEGREE " per node: %s", nodes, plural, nodes == 1 ? "s" : "",
                     RULES[r].red ? TRACE_TURN_MAX_DEGREES : TRACE_TURN_WARN_DEGREES, verdict);
        else if (RULES[r].kind == RULE_RADIUS)
            snprintf(lines[used].text, TRACE_SUMMARY_TEXT, "%d node%s %s on a curve of radius under %g m: %s", nodes, plural, nodes == 1 ? "is" : "are",
                     TRACE_METRES(RULES[r].red ? TRACE_RADIUS_MIN : TRACE_RADIUS_WARN), verdict);
        else
            snprintf(lines[used].text, TRACE_SUMMARY_TEXT, "%d node%s change%s width by more than %g m per node: %s", nodes, plural, nodes == 1 ? "s" : "",
                     TRACE_WIDTH_STEP_WARN_M, verdict);
        ++used;
    }
    return used;
}

void track_trace_distances(const TrackGenNode *nodes, int count, double *metres)
{
    double at = 0;
    for (int i = 0; i < count; ++i) {
        at += i > 0 ? track_trace_step(nodes, i) : 0;
        metres[i] = at;
    }
}

double track_trace_signed_turn(const TrackGenNode *nodes, int i)
{
    return i > 0 ? track_trace_turn_degrees(nodes[i - 1].heading, nodes[i].heading) : 0;
}

double track_trace_step(const TrackGenNode *nodes, int i)
{
    if (i <= 0) return 0;
    const double dx = ((double)nodes[i].position[0] - nodes[i - 1].position[0]) / POSITION_ONE;
    const double dy = ((double)nodes[i].position[1] - nodes[i - 1].position[1]) / POSITION_ONE;
    const double dz = ((double)nodes[i].position[2] - nodes[i - 1].position[2]) / POSITION_ONE;
    return TRACE_METRES(sqrt(dx * dx + dy * dy + dz * dz));
}

void track_trace_point_distances(const TracePoint *points, int count, double *metres)
{
    Curve c;
    if (points == NULL || count < 2 || !curve_make(points, count, &c)) {
        for (int i = 0; i < count; ++i) metres[i] = 0;
        return;
    }
    for (int i = 0; i < count; ++i) metres[i] = TRACE_METRES(c.s[i * SAMPLES_PER_SPAN]);
    curve_free(&c);
}

void track_trace_turn_text(double degrees, char *out, size_t cap)
{
    if (fabs(degrees) < 0.05) snprintf(out, cap, "-");
    else snprintf(out, cap, "%c %.1f" DEGREE, degrees > 0 ? 'R' : 'L', fabs(degrees));
}
