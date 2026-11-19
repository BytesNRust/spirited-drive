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

int main(void)
{
    test_straight();
    test_arc();
    test_tight_arc_flags_radius();
    test_corner_flags_turn();
    test_limit_levels();
    test_drawn_16_is_amber();
    test_faults();
    test_generates_tri();
    test_turn_degrees();
    test_turned_generic();
    return TEST_REPORT();
}
