/* Track trace tests: control points become nodes (a straight stays straight, an arc turns by its angle, a tight arc is flagged for its
 * radius, a corner for its turn), the faults are named, and the nodes go through track_generate_nodes into a TRI that opens. */
#include "formats/track_trace.h"
#include "formats/track_gen.h"
#include "formats/track_template.h"
#include "formats/tri.h"
#include "test.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum { TSEGS = 20, DESCRIPTORS = 2, SCENERY_AREA = TRI_SCENERY_MAX * TRI_SCENERY_SIZE, WIDTH = 40 };

static void put_u32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
static void put_i16(uint8_t *p, int v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/* a synthetic template (no CD files needed): a circuit header, one model node and TSEGS terrain strips */
static uint8_t *template_tri(size_t *size)
{
    uint8_t *header = calloc(1, TRI_HEADER_SIZE);
    uint8_t descriptors[DESCRIPTORS * TRI_DESCRIPTOR_SIZE];
    uint8_t *scenery = calloc(1, SCENERY_AREA);
    uint8_t terrain[TSEGS * TRI_SEGMENT_SIZE];
    header[0] = TRI_VERSION;
    put_i16(header + 4, 16);
    put_u32(header + 0x16218, 1000);
    uint8_t *node = header + 0x98C + 36;
    node[0] = node[1] = WIDTH;
    node[2] = node[3] = WIDTH + 24;
    node[4] = 0x22; node[7] = 3;
    for (int i = 0; i < DESCRIPTORS * TRI_DESCRIPTOR_SIZE; ++i) descriptors[i] = (uint8_t)(i * 3);
    for (int i = 0; i < TRI_SCENERY_MAX; ++i) put_u32(scenery + 16 * i, 0xffffffffu);
    memset(terrain, 0, sizeof terrain);
    for (int s = 0; s < TSEGS; ++s) {
        uint8_t *seg = terrain + s * TRI_SEGMENT_SIZE;
        memcpy(seg, "TRKD", 4);
        put_u32(seg + 4, TRI_SEGMENT_SIZE - 12);
        seg[0x0d] = 0xff;
        memset(seg + 0x0e, 0x21, 10);
        for (int row = 0; row < 4; ++row) put_i16(seg + 0x18 + 66 * row + 6, WIDTH * 16);
    }
    const TriParts parts = { header, descriptors, DESCRIPTORS, scenery, terrain, TSEGS, NULL, 0 };
    uint8_t *file = malloc(tri_write_size(&parts));
    tri_write(&parts, file, tri_write_size(&parts), size);
    free(header);
    free(scenery);
    return file;
}

static double unit(int32_t v) { return v / 65536.0; }

static int nodes_of(const TracePoint *p, int count, double width, TrackGenNode **nodes, int *n)
{
    ExchangeError err;
    memset(&err, 0, sizeof err);
    return track_trace_nodes(p, count, width, nodes, n, &err);
}

static int count_flags(const uint8_t *flags, int n, uint8_t bit)
{
    int c = 0;
    for (int i = 0; i < n; ++i) if (flags[i] & bit) ++c;
    return c;
}

static void test_straight(void)
{
    const TracePoint p[] = { { 0, 0 }, { 0, 96 }, { 0, 192 }, { 0, 288 } };
    TrackGenNode *nodes = NULL;
    int n = 0;
    CHECK(nodes_of(p, 4, WIDTH, &nodes, &n));
    CHECK_EQ_INT(n, 49);                                   /* 288 units = 12 segments of 24 */
    CHECK_EQ_INT(track_trace_segments(288), 12);
    CHECK_EQ_INT(track_trace_segments(10), TRACK_GEN_MIN_SEGMENTS);
    int straight = 1;
    for (int k = 0; k < n; ++k) {
        if (nodes[k].heading != 0.0 || nodes[k].position[0] != 0 || nodes[k].position[1] != 0) straight = 0;
        if (k > 0 && fabs((unit(nodes[k].position[2]) - unit(nodes[k - 1].position[2])) - 6.0) > 0.01) straight = 0;
        CHECK_EQ_INT(nodes[k].width == WIDTH, 1);
    }
    CHECK(straight);
    CHECK(fabs(unit(nodes[n - 1].position[2]) - 288.0) < 0.01);
    uint8_t *flags = calloc((size_t)n, 1);
    track_trace_flags(nodes, n, flags);
    CHECK_EQ_INT(count_flags(flags, n, TRACE_FLAG_TURN | TRACE_FLAG_RADIUS), 0);
    free(flags);
    free(nodes);
}

/* a quarter circle of radius r from heading 0 (+z) to heading -90 (a left turn), `steps` control points */
static void arc_points(double r, int steps, double degrees, TracePoint *out)
{
    for (int i = 0; i <= steps; ++i) {
        const double a = degrees * M_PI / 180 * i / steps;
        out[i].x = r * cos(a);
        out[i].z = r * sin(a);
    }
}

static void test_arc(void)
{
    TracePoint p[32];
    arc_points(100, 12, 90, p);
    TrackGenNode *nodes = NULL;
    int n = 0;
    CHECK(nodes_of(p, 13, WIDTH, &nodes, &n));
    const double length = track_trace_length(p, 13);
    CHECK(fabs(length - 100 * M_PI / 2) < 1.0);             /* the arc's length, the Catmull curve within a unit */
    CHECK_EQ_INT(n, 4 * track_trace_segments(length) + 1);
    /* the road starts along its first chord and ends along its last: the curve has the chords' directions at its ends */
    const double first = atan2(p[1].x - p[0].x, p[1].z - p[0].z) * 8192 / M_PI;
    CHECK(fabs(track_trace_turn_degrees(first, nodes[0].heading)) < 0.3);
    const double last = atan2(p[12].x - p[11].x, p[12].z - p[11].z) * 8192 / M_PI;
    CHECK(fabs(track_trace_turn_degrees(last, nodes[n - 1].heading)) < 0.3);
    CHECK(track_trace_turn_degrees(nodes[0].heading, nodes[n - 1].heading) < 0);   /* a left turn: negative */
    uint8_t *flags = calloc((size_t)n, 1);
    track_trace_flags(nodes, n, flags);
    CHECK_EQ_INT(count_flags(flags, n, TRACE_FLAG_TURN | TRACE_FLAG_RADIUS), 0);   /* about 3 degrees a node on radius 100 */
    free(flags);
    free(nodes);
}

static void test_tight_arc_flags_radius(void)
{
    TracePoint p[32];
    TrackGenNode *nodes = NULL;
    int n = 0;
    arc_points(30, 24, 180, p);                             /* a half circle of radius 30: about 11.5 degrees a node: amber, not red */
    CHECK(nodes_of(p, 25, WIDTH, &nodes, &n));
    uint8_t *flags = calloc((size_t)n, 1);
    track_trace_flags(nodes, n, flags);
    CHECK(count_flags(flags, n, TRACE_FLAG_RADIUS_WARN) > 0);
    CHECK_EQ_INT(count_flags(flags, n, TRACE_FLAG_RADIUS | TRACE_FLAG_TURN), 0);
    CHECK_EQ_INT(flags[0], 0);
    free(flags);
    free(nodes);
    arc_points(15, 24, 180, p);                             /* radius 15: about 23 degrees a node: red */
    CHECK(nodes_of(p, 25, WIDTH, &nodes, &n));
    flags = calloc((size_t)n, 1);
    track_trace_flags(nodes, n, flags);
    CHECK(count_flags(flags, n, TRACE_FLAG_RADIUS) > 0);
    free(flags);
    free(nodes);
}

static void test_corner_flags_turn(void)
{
    const TracePoint p[] = { { 0, 0 }, { 0, 100 }, { 100, 100 } };
    TrackGenNode *nodes = NULL;
    int n = 0;
    CHECK(nodes_of(p, 3, WIDTH, &nodes, &n));
    uint8_t *flags = calloc((size_t)n, 1);
    track_trace_flags(nodes, n, flags);
    CHECK(count_flags(flags, n, TRACE_FLAG_TURN) > 0);
    free(flags);
    free(nodes);
}


/* nodes that turn `degrees` each from the one before, TRACK_GEN_NODE_LENGTH apart: the limits' levels by flags alone */
static uint8_t flags_at_turn(double degrees)
{
    TrackGenNode n[3];
    const double step = TRACK_GEN_NODE_LENGTH / 65536.0, back = degrees * 0x4000 / 360.0;
    double x = 0, z = 0;
    for (int i = 0; i < 3; ++i) {
        n[i].heading = back * i;
        n[i].grade = n[i].bank = 0;
        n[i].width = WIDTH;
        n[i].position[0] = (int32_t)lround(x * 65536);
        n[i].position[1] = 0;
        n[i].position[2] = (int32_t)lround(z * 65536);
        x += step * sin(back * i * 2 * M_PI / 0x4000);
        z += step * cos(back * i * 2 * M_PI / 0x4000);
    }
    uint8_t flags[3];
    track_trace_flags(n, 3, flags);
    return flags[2];
}

/* the limits' levels (notes/dep59-editor-limits-vs-cd.md): up to 8 degrees a node nothing is marked, over it amber (the road is built), over 16 red;
 * the editor's red limit is the generator's own */
static void test_limit_levels(void)
{
    CHECK_EQ_INT(flags_at_turn(5.0), 0);
    CHECK_EQ_INT(flags_at_turn(7.9), 0);                                                      /* radius 43.7 */
    CHECK_EQ_INT(flags_at_turn(8.0), 0);                                                      /* the amber starts over it */
    CHECK_EQ_INT(flags_at_turn(8.2), TRACE_FLAG_TURN_WARN | TRACE_FLAG_RADIUS_WARN);          /* radius 42.1 */
    CHECK_EQ_INT(flags_at_turn(12.0), TRACE_FLAG_TURN_WARN | TRACE_FLAG_RADIUS_WARN);
    CHECK_EQ_INT(flags_at_turn(16.0), TRACE_FLAG_TURN_WARN | TRACE_FLAG_RADIUS_WARN);         /* Alpine 3's 15.8 and the CD's 16.2 (TR6) are amber */
    CHECK_EQ_INT(flags_at_turn(16.4), TRACE_FLAG_TURN_WARN | TRACE_FLAG_RADIUS_WARN);         /* radius 21.1 */
    CHECK_EQ_INT(flags_at_turn(16.7), TRACE_FLAG_TURN | TRACE_FLAG_RADIUS);                   /* red: no warning bits besides */
    CHECK(TRACE_TURN_MAX_DEGREES == TRACK_GEN_TURN_MAX_DEGREES);                              /* track_gen refuses what the editor shows red */
    CHECK(TRACE_TURN_WARN_DEGREES == TRACK_GEN_TURN_WARN_DEGREES);
}

/* a curve drawn at 16 degrees a node (a control point on every node, as an Alpine 3 curve would be) is built: amber, never red, though the spline's
 * joint with the straight gives a little over 16 */
static void test_drawn_16_is_amber(void)
{
    enum { STRAIGHT = 20, ARC = 12, COUNT = 2 * STRAIGHT + ARC + 1 };
    const double step = TRACK_GEN_NODE_LENGTH / 65536.0;
    TracePoint p[COUNT];
    double x = 0, z = 0, heading = 0;
    for (int i = 0; i < COUNT; ++i) {
        p[i].x = x, p[i].z = z;
        if (i >= STRAIGHT && i < STRAIGHT + ARC) heading += 16.0 * M_PI / 180;
        x += step * sin(heading), z += step * cos(heading);
    }
    TrackGenNode *nodes = NULL;
    int n = 0;
    CHECK(nodes_of(p, COUNT, WIDTH, &nodes, &n));
    uint8_t *flags = calloc((size_t)n, 1);
    track_trace_flags(nodes, n, flags);
    CHECK_EQ_INT(count_flags(flags, n, TRACE_FLAG_TURN | TRACE_FLAG_RADIUS), 0);
    CHECK(count_flags(flags, n, TRACE_FLAG_TURN_WARN) > 0);
    free(flags);
    free(nodes);
}

/* nodes whose turns from the one before are `turns[1..count-1]` degrees, TRACK_GEN_NODE_LENGTH apart */
static void nodes_turning(const double *turns, int count, TrackGenNode *n)
{
    const double step = TRACK_GEN_NODE_LENGTH / 65536.0;
    double x = 0, z = 0, heading = 0;
    for (int i = 0; i < count; ++i) {
        heading += i > 0 ? turns[i] : 0;
        n[i].heading = heading * 0x4000 / 360.0;
        n[i].grade = n[i].bank = 0;
        n[i].width = WIDTH;
        n[i].position[0] = (int32_t)lround(x * 65536);
        n[i].position[1] = 0;
        n[i].position[2] = (int32_t)lround(z * 65536);
        x += step * sin(heading * M_PI / 180), z += step * cos(heading * M_PI / 180);
    }
}

/* the Problems panel's rows: red nodes first, then amber, each by node number, with the text the limits make; the summary has a line per rule broken */
static void test_problems_and_summary(void)
{
    enum { N = 7 };
    const double turns[N] = { 0, 9.4, 17.0, 3.0, 9.0, 16.0, 1.0 };   /* node 1 and 4 and 5 amber (5 is 16.0: over 8, under 16.5), node 2 red */
    TrackGenNode n[N];
    nodes_turning(turns, N, n);
    uint8_t flags[N];
    track_trace_flags(n, N, flags);
    TraceProblem rows[N];
    CHECK_EQ_INT(track_trace_problems(n, N, flags, rows, N), 4);
    CHECK_EQ_INT(rows[0].node, 2);                    /* the red one first */
    CHECK(rows[0].red && !rows[1].red && !rows[2].red && !rows[3].red);
    CHECK_EQ_INT(rows[1].node, 1);                    /* then the amber ones by node */
    CHECK_EQ_INT(rows[2].node, 4);
    CHECK_EQ_INT(rows[3].node, 5);
    char text[160];
    track_trace_problem_text(&rows[0], text, sizeof text);
    CHECK(strcmp(text, "node 2: turns 17.0\xc2\xb0 (more than 16.5\xc2\xb0) and radius 20 m (under 20.9 m), can't be built") == 0);
    track_trace_problem_text(&rows[1], text, sizeof text);
    CHECK(strcmp(text, "node 1: turns 9.4\xc2\xb0 (more than 8\xc2\xb0) and radius 37 m (under 43 m), may not look right") == 0);
    TraceProblem few[2];
    CHECK_EQ_INT(track_trace_problems(n, N, flags, few, 2), 4);   /* the total, though only 2 rows fit: the red one and the first amber */
    CHECK_EQ_INT(few[0].node, 2);
    CHECK_EQ_INT(few[1].node, 1);

    TraceSummaryLine lines[TRACE_SUMMARY_LINES];
    CHECK_EQ_INT(track_trace_summary(flags, N, lines), 4);
    CHECK(lines[0].red && lines[1].red && !lines[2].red && !lines[3].red);
    CHECK(strcmp(lines[0].text, "1 node turns more than 16.5\xc2\xb0 per node: the track can't be built") == 0);
    CHECK(strcmp(lines[1].text, "1 node is on a curve of radius under 20.9 m: the track can't be built") == 0);
    CHECK(strcmp(lines[2].text, "3 nodes turn more than 8\xc2\xb0 per node: they may not look right in the game") == 0);
    CHECK(strcmp(lines[3].text, "3 nodes are on a curve of radius under 43 m: they may not look right in the game") == 0);

    const double calm[N] = { 0, 1, 2, 3, 2, 1, 0 };    /* nothing breaks: no rows and no lines */
    nodes_turning(calm, N, n);
    track_trace_flags(n, N, flags);
    CHECK_EQ_INT(track_trace_problems(n, N, flags, rows, N), 0);
    CHECK_EQ_INT(track_trace_summary(flags, N, lines), 0);
}

/* a turn with no distance from the node before (a hand-made node array) has radius 0: red for the radius, as it always was */
static void test_zero_distance_turn(void)
{
    TrackGenNode n[2] = {{ 0 }};
    n[0].width = n[1].width = WIDTH;
    n[1].heading = 100;
    uint8_t flags[2];
    track_trace_flags(n, 2, flags);
    CHECK_EQ_INT(flags[1] & TRACE_FLAG_RADIUS, TRACE_FLAG_RADIUS);
}

/* metres: the distance from node 0 is the sum of the steps, the turn is signed (right positive), a control point's distance is along the curve */
static void test_metres(void)
{
    enum { N = 5 };
    const double turns[N] = { 0, 5.0, -3.0, 0, 2.0 };
    TrackGenNode n[N];
    nodes_turning(turns, N, n);
    double m[N];
    track_trace_distances(n, N, m);
    const double step = TRACK_GEN_NODE_LENGTH / 65536.0 * TRACE_METRES_PER_UNIT;
    CHECK(m[0] == 0);
    for (int i = 1; i < N; ++i) CHECK(fabs(m[i] - step * i) < 0.001);
    CHECK(fabs(track_trace_step(n, 3) - step) < 0.001);
    CHECK(track_trace_step(n, 0) == 0);
    CHECK(fabs(track_trace_signed_turn(n, 1) - 5.0) < 0.01);     /* right */
    CHECK(fabs(track_trace_signed_turn(n, 2) + 3.0) < 0.01);     /* left */
    CHECK(track_trace_signed_turn(n, 0) == 0);
    const TracePoint line[] = { { 0, 0 }, { 0, 100 }, { 0, 300 } };
    double pd[3];
    track_trace_point_distances(line, 3, pd);
    CHECK(fabs(pd[0]) < 0.001 && fabs(pd[1] - 100) < 0.5 && fabs(pd[2] - 300) < 0.5);   /* along the curve: a straight, so the sums of its spans */
    CHECK(TRACE_METRES_PER_UNIT == 1.0);
    char t[16];
    track_trace_turn_text(5.26, t, sizeof t);
    CHECK(strcmp(t, "R 5.3\xc2\xb0") == 0);
    track_trace_turn_text(-3.14, t, sizeof t);
    CHECK(strcmp(t, "L 3.1\xc2\xb0") == 0);
    track_trace_turn_text(0.01, t, sizeof t);
    CHECK(strcmp(t, "-") == 0);
}

static void test_faults(void)
{
    TrackGenNode *nodes = NULL;
    int n = 0;
    const TracePoint one[] = { { 0, 0 } };
    CHECK(!nodes_of(one, 1, WIDTH, &nodes, &n));
    const TracePoint nan_point[] = { { 0, 0 }, { NAN, 10 } };
    CHECK(!nodes_of(nan_point, 2, WIDTH, &nodes, &n));
    const TracePoint twins[] = { { 0, 0 }, { 0, 0 }, { 0, 200 } };
    CHECK(!nodes_of(twins, 3, WIDTH, &nodes, &n));
    const TracePoint short_road[] = { { 0, 0 }, { 0, 10 } };   /* 8 segments would put the nodes 0.3 units apart */
    CHECK(!nodes_of(short_road, 2, WIDTH, &nodes, &n));
    TracePoint long_road[61];
    for (int i = 0; i <= 60; ++i) { long_road[i].x = 0; long_road[i].z = 300.0 * i; }   /* 18000 units: 750 segments */
    CHECK(!nodes_of(long_road, 61, WIDTH, &nodes, &n));
    CHECK(nodes == NULL);
}

static void test_generates_tri(void)
{
    TracePoint p[32];
    arc_points(100, 6, 90, p);
    TrackGenNode *nodes = NULL;
    int n = 0;
    CHECK(nodes_of(p, 7, WIDTH, &nodes, &n));
    size_t tsize = 0, out_len = 0;
    uint8_t *tmpl = template_tri(&tsize);
    uint8_t *out = NULL;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    CHECK(track_generate_nodes(nodes, n, tmpl, tsize, &out, &out_len, &err));
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, out, out_len), TRI_OK);
    CHECK_EQ_INT(tri.segment_count, (n - 1) / 4);
    free(out);
    free(tmpl);
    free(nodes);
}

/* the road made on the generic template (no CD byte) through the turned writer: a curve comes out as a TRI that opens */
static void test_turned_generic(void)
{
    TracePoint p[32];
    arc_points(100, 12, 90, p);
    TrackGenNode *nodes = NULL;
    int n = 0;
    CHECK(nodes_of(p, 13, WIDTH, &nodes, &n));
    uint8_t *tmpl = NULL, *out = NULL;
    size_t tmpl_len = 0, out_len = 0;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    CHECK(track_template_generic(&tmpl, &tmpl_len, &err));
    CHECK(track_generate_nodes_turned(nodes, n, tmpl, tmpl_len, &out, &out_len, &err));
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, out, out_len), TRI_OK);
    CHECK_EQ_INT(tri.segment_count, (n - 1) / 4);
    free(out);
    free(tmpl);
    free(nodes);
}

static void test_turn_degrees(void)
{
    CHECK(fabs(track_trace_turn_degrees(0, 4096) - 90) < 1e-9);
    CHECK(fabs(track_trace_turn_degrees(4096, 0) + 90) < 1e-9);
    CHECK(fabs(track_trace_turn_degrees(16000, 100) - 10.6348) < 0.001);   /* across the wrap */
}


/* ---- road keys: the width of the road per control point (the plan's step 1) ---- */

static double smooth(double t) { return t * t * (3 - 2 * t); }

/* a straight of `n` points `gap` m apart */
static void straight_points(TracePoint *p, int n, double gap)
{
    for (int i = 0; i < n; ++i) p[i].x = 0, p[i].z = gap * i;
}

/* no keys: the same nodes as ever, byte for byte; and a key list of nothing is the same */
static void test_keys_none_is_old(void)
{
    TracePoint p[6];
    straight_points(p, 6, 60);
    TrackGenNode *a = NULL, *b = NULL, *c = NULL;
    int na = 0, nb = 0, nc = 0;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    CHECK(track_trace_nodes(p, 6, WIDTH, &a, &na, &err));
    CHECK(track_trace_nodes_keyed(p, 6, WIDTH, NULL, 0, &b, &nb, &err));
    const TraceKey none[1] = { { 2, 0, 0 } };   /* a key with no value (no bit set) says nothing */
    CHECK(track_trace_nodes_keyed(p, 6, WIDTH, none, 1, &c, &nc, &err));
    CHECK_EQ_INT(na, nb);
    CHECK_EQ_INT(na, nc);
    if (a != NULL && b != NULL && c != NULL && na == nb && na == nc) {
        for (int i = 0; i < na; ++i) {   /* field by field: the struct's tail padding is not written */
            CHECK(a[i].heading == b[i].heading && a[i].grade == b[i].grade && a[i].bank == b[i].bank && a[i].width == b[i].width);
            CHECK(memcmp(a[i].position, b[i].position, sizeof a[i].position) == 0);
            CHECK(a[i].heading == c[i].heading && a[i].width == c[i].width && memcmp(a[i].position, c[i].position, sizeof a[i].position) == 0);
        }
    }
    free(a);
    free(b);
    free(c);
}

/* the width along the road: the project's at the start, then a key's value at its point, and between two keys a smoothstep over the arc length */
static void test_keys_interpolate(void)
{
    TracePoint p[6];
    straight_points(p, 6, 60);                    /* 300 m: 12 segments, a node every 6.25 m */
    const TraceKey keys[2] = { { 1, TRACE_KEY_WIDTH, 8.0 }, { 4, TRACE_KEY_WIDTH, 14.0 } };   /* 32 and 56 bytes at s = 60 and 240 */
    TrackGenNode *n = NULL;
    int count = 0;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    CHECK(track_trace_nodes_keyed(p, 6, WIDTH, keys, 2, &n, &count, &err));
    CHECK_EQ_INT(count, 49);
    if (n == NULL) return;
    int exact = 1;
    for (int k = 0; k < count; ++k) {
        const double s = k * 6.25;
        double want = 56;
        if (s <= 60) want = 40 + (32 - 40) * smooth(s / 60);                 /* from the project's width (the implicit key of point 0) to the first key */
        else if (s < 240) want = 32 + (56 - 32) * smooth((s - 60) / 180);    /* between the two keys */
        if (fabs(n[k].width - want) > 1e-6) exact = 0;
    }
    CHECK(exact);
    CHECK(n[0].width == 40);
    CHECK(fabs(n[24].width - 44.0) < 1e-9);       /* s = 150: the middle of 60..240 */
    CHECK(n[48].width == 56);                     /* after the last key it holds */
    free(n);

    const TraceKey first = { 0, TRACE_KEY_WIDTH, 6.0 };   /* a key on point 0 replaces the project's width as the start */
    CHECK(track_trace_nodes_keyed(p, 6, WIDTH, &first, 1, &n, &count, &err));
    CHECK(n != NULL && n[0].width == 24 && n[count - 1].width == 24);
    free(n);

    double w[6];                                   /* what each control point is, in metres: the dimmed values of the Points table */
    track_trace_point_widths(p, 6, WIDTH, keys, 2, w);
    CHECK(fabs(w[0] - 10.0) < 1e-9 && fabs(w[1] - 8.0) < 1e-9 && fabs(w[4] - 14.0) < 1e-9 && fabs(w[5] - 14.0) < 1e-9);
    CHECK(fabs(w[2] - (32 + 24 * smooth(60.0 / 180)) / 4) < 1e-6);
    CHECK(fabs(w[3] - (32 + 24 * smooth(120.0 / 180)) / 4) < 1e-6);
}

/* a key list that is wrong is refused with its point named */
static void test_keys_faults(void)
{
    TracePoint p[4];
    straight_points(p, 4, 100);
    TrackGenNode *n = NULL;
    int count = 0;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    const TraceKey off[1] = { { 4, TRACE_KEY_WIDTH, 10.0 } };
    CHECK(!track_trace_nodes_keyed(p, 4, WIDTH, off, 1, &n, &count, &err));
    CHECK(strstr(err.message, "point 4") != NULL);
    const TraceKey narrow[1] = { { 1, TRACE_KEY_WIDTH, 1.75 } };
    CHECK(!track_trace_nodes_keyed(p, 4, WIDTH, narrow, 1, &n, &count, &err));
    CHECK(strstr(err.message, "2") != NULL && strstr(err.message, "30") != NULL);
    const TraceKey wide[1] = { { 1, TRACE_KEY_WIDTH, 30.25 } };
    CHECK(!track_trace_nodes_keyed(p, 4, WIDTH, wide, 1, &n, &count, &err));
    const TraceKey twice[2] = { { 1, TRACE_KEY_WIDTH, 9.0 }, { 1, TRACE_KEY_WIDTH, 11.0 } };
    CHECK(!track_trace_nodes_keyed(p, 4, WIDTH, twice, 2, &n, &count, &err));
    CHECK(strstr(err.message, "twice") != NULL);
    CHECK(n == NULL && count == 0);
}

/* the list's edits: set, update, clear, and the keys follow their points when one is inserted or removed */
static void test_keys_edits(void)
{
    TraceKey *k = NULL;
    int n = 0, cap = 0;
    CHECK(track_trace_key_set_width(&k, &n, &cap, 5, 12.0));
    CHECK(track_trace_key_set_width(&k, &n, &cap, 2, 8.0));
    CHECK(track_trace_key_set_width(&k, &n, &cap, 9, 14.0));
    CHECK_EQ_INT(n, 3);
    CHECK(k != NULL && k[0].point == 2 && k[1].point == 5 && k[2].point == 9);   /* by point */
    CHECK(track_trace_key_set_width(&k, &n, &cap, 5, 13.0));
    CHECK_EQ_INT(n, 3);                                                          /* an update, not a new key */
    CHECK(k[1].width_m == 13.0 && (k[1].has & TRACE_KEY_WIDTH));
    CHECK_EQ_INT(track_trace_key_find(k, n, 5), 1);
    CHECK_EQ_INT(track_trace_key_find(k, n, 4), -1);
    track_trace_keys_insert_point(k, n, 5);                                      /* a point inserted at 5: the keys from 5 on move up */
    CHECK(k[0].point == 2 && k[1].point == 6 && k[2].point == 10);
    track_trace_keys_remove_point(k, &n, 6);                                     /* the point of a key removed: the key goes with it */
    CHECK_EQ_INT(n, 2);
    CHECK(k[0].point == 2 && k[1].point == 9);                                   /* the later ones move down */
    track_trace_key_clear_width(k, &n, 2);
    CHECK_EQ_INT(n, 1);
    CHECK(k[0].point == 9);
    track_trace_key_clear_width(k, &n, 7);                                       /* none there: nothing happens */
    CHECK_EQ_INT(n, 1);
    free(k);
}

/* the limit on the road width: more than 5 bytes (1.25 m) a node is amber, wording and summary included; a calm ramp is clean */
static void test_keys_width_limit(void)
{
    TracePoint q[12];                                   /* evenly spaced, so the curve is straight: only the width is amber */
    straight_points(q, 12, 15);
    TracePoint p[4] = { { 0, 0 }, { 0, 100 }, { 0, 115 }, { 0, 300 } };
    const TraceKey sudden[2] = { { 2, TRACE_KEY_WIDTH, 10.0 }, { 3, TRACE_KEY_WIDTH, 14.0 } };   /* 10 to 14 m (16 bytes) over 15 m */
    TrackGenNode *n = NULL;
    int count = 0;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    CHECK(track_trace_nodes_keyed(q, 12, WIDTH, sudden, 2, &n, &count, &err));
    if (n == NULL) return;
    uint8_t *flags = calloc((size_t)count, 1);
    track_trace_flags(n, count, flags);
    int flagged = 0, right = 1;
    for (int i = 1; i < count; ++i) {
        const int over = fabs(n[i].width - n[i - 1].width) > 5.0 + 1e-9;
        if ((flags[i] & TRACE_FLAG_WIDTH_WARN) != 0) ++flagged;
        if (((flags[i] & TRACE_FLAG_WIDTH_WARN) != 0) != over) right = 0;
        CHECK((flags[i] & (TRACE_FLAG_TURN | TRACE_FLAG_RADIUS)) == 0);   /* amber for the width, red for nothing */
    }
    CHECK(right);
    CHECK(flagged > 0);
    CHECK(flags[0] == 0);
    TraceProblem rows[8];
    const int total = track_trace_problems(n, count, flags, rows, 8);
    CHECK_EQ_INT(total, flagged);
    CHECK(total > 0 && !rows[0].red && rows[0].width_step > 1.25);
    char text[160];
    track_trace_problem_text(&rows[0], text, sizeof text);
    CHECK(strstr(text, "width changes") != NULL && strstr(text, "more than 1.25 m") != NULL && strstr(text, "may not look right") != NULL);
    TraceSummaryLine lines[TRACE_SUMMARY_LINES];
    const int used = track_trace_summary(flags, count, lines);
    CHECK_EQ_INT(used, 1);
    CHECK(!lines[0].red && lines[0].nodes == flagged && strstr(lines[0].text, "change width by more than 1.25 m per node") != NULL);
    free(flags);
    free(n);

    const TraceKey calm[2] = { { 0, TRACE_KEY_WIDTH, 8.0 }, { 3, TRACE_KEY_WIDTH, 14.0 } };   /* 6 m over 300 m: 0.2 m a node at the most */
    CHECK(track_trace_nodes_keyed(p, 4, WIDTH, calm, 2, &n, &count, &err));
    flags = calloc((size_t)count, 1);
    track_trace_flags(n, count, flags);
    CHECK_EQ_INT(count_flags(flags, count, TRACE_FLAG_WIDTH_WARN), 0);
    free(flags);
    free(n);

    /* the longest row: both amber turn rules and the width one on a node of the last digits; the text is whole, not cut at the buffer */
    const TraceProblem all = { 2396, 0, TRACE_FLAG_TURN_WARN | TRACE_FLAG_RADIUS_WARN | TRACE_FLAG_WIDTH_WARN, 16.4, 21.0, 1.55 };
    char longest[TRACE_PROBLEM_TEXT];
    track_trace_problem_text(&all, longest, sizeof longest);
    CHECK(strlen(longest) > 128 - 1 && strlen(longest) < TRACE_PROBLEM_TEXT - 1);
    CHECK(strstr(longest, "width changes 1.55 m (more than 1.25 m), may not look right") != NULL);
    CHECK(strncmp(longest, "node 2396: turns 16.4", 21) == 0);
}

/* the ramp's steepest step: the smoothstep's slope is 1.5 times the average one, so a ramp of D bytes over L metres on 6.25 m nodes steps 1.5 x D x 6.25 / L
 * bytes at the most: 8 to 14 m (24 bytes) is at the limit at 45 m (5.00 bytes), clean from about there and amber under it */
static void test_keys_ramp_steepness(void)
{
    const double lengths[3] = { 60, 45, 40 };
    const int amber[3] = { 0, 0, 1 };
    for (int t = 0; t < 3; ++t) {
        TracePoint p[4] = { { 0, 0 }, { 0, 100 }, { 0, 100 + lengths[t] }, { 0, 300 } };
        const TraceKey keys[2] = { { 1, TRACE_KEY_WIDTH, 8.0 }, { 2, TRACE_KEY_WIDTH, 14.0 } };
        TrackGenNode *n = NULL;
        int count = 0;
        ExchangeError err;
        memset(&err, 0, sizeof err);
        CHECK(track_trace_nodes_keyed(p, 4, 32, keys, 2, &n, &count, &err));   /* the project's width 8 m too: the ramp is the only change */
        if (n == NULL) return;
        double most = 0;
        for (int i = 1; i < count; ++i) most = fmax(most, fabs(n[i].width - n[i - 1].width));
        CHECK(most <= 1.5 * 24 * 6.25 / lengths[t] + 0.05);                      /* the slope's own bound */
        CHECK_EQ_INT(most > 5.0, amber[t]);
        free(n);
    }
}

int main(void)
{
    test_straight();
    test_arc();
    test_tight_arc_flags_radius();
    test_corner_flags_turn();
    test_limit_levels();
    test_drawn_16_is_amber();
    test_problems_and_summary();
    test_zero_distance_turn();
    test_metres();
    test_faults();
    test_generates_tri();
    test_turn_degrees();
    test_turned_generic();
    test_keys_none_is_old();
    test_keys_interpolate();
    test_keys_faults();
    test_keys_edits();
    test_keys_width_limit();
    test_keys_ramp_steepness();
    return TEST_REPORT();
}
