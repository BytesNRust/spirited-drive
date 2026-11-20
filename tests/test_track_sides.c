/* The road's sides in the generator (dep59 editor phase 2, step 3): per node and per side the shoulder's width, the wall's distance, its kind and the fence.
 * First of all: a road that says nothing of its sides is byte for byte the road of before (the pins below were taken before step 3 changed the generator). */
#include "formats/track_gen.h"
#include "formats/track_template.h"
#include "formats/tri.h"
#include "test.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH = 40, HALF = 10, COUNT = 20 + 2 * HALF + 20 + 1 };

static uint64_t fnv64(const uint8_t *p, size_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

/* an S as the editor's spline gives it: 20 nodes straight, a turn of `degrees` a node as a sine over 2 x HALF nodes, 20 straight; `width` wide */
static void s_nodes(TrackGenNode *n, double degrees, double width)
{
    memset(n, 0, sizeof *n * COUNT);
    int32_t x = 0, z = 0;
    double heading = 0;
    for (int i = 0; i < COUNT; ++i) {
        const int k = i - 20;
        if (k > 0 && k <= 2 * HALF) heading += degrees * sin(M_PI * k / (double)HALF) / 360 * 0x4000;
        n[i].heading = heading;
        n[i].width = width;
        n[i].position[0] = x, n[i].position[2] = z;
        const double h = heading / 0x4000 * 2 * M_PI;
        x += (int32_t)lround(TRACK_GEN_NODE_LENGTH * sin(h)), z += (int32_t)lround(TRACK_GEN_NODE_LENGTH * cos(h));
    }
}

static uint64_t generate_hash(const TrackGenNode *n, const uint8_t *tmpl, size_t tsize)
{
    uint8_t *out = NULL;
    size_t len = 0;
    ExchangeError err = {0};
    if (!track_generate_nodes_turned(n, COUNT, tmpl, tsize, &out, &len, &err)) {
        fprintf(stderr, "%s\n", err.message);
        return 0;
    }
    const uint64_t h = fnv64(out, len);
    free(out);
    return h;
}


/* the generated TRI of `n` (COUNT nodes), opened: 1, or 0 with the error printed */
static int generate_tri(const TrackGenNode *n, const uint8_t *tmpl, size_t tsize, uint8_t **out, Tri *tri, TriParts *parts)
{
    size_t len = 0;
    ExchangeError err = {0};
    *out = NULL;
    if (!track_generate_nodes_turned(n, COUNT, tmpl, tsize, out, &len, &err)) {
        fprintf(stderr, "%s\n", err.message);
        return 0;
    }
    return tri_open(tri, *out, len) == TRI_OK && tri_parts(tri, parts) == TRI_OK;
}

static const uint8_t *row_of(const TriParts *parts, int node)
{
    return parts->terrain + (size_t)(node / 4) * TRI_SEGMENT_SIZE + 0x18 + 66 * (node % 4);
}

/* how far across the road the row's point `upto` (0 the road edge, 1 the shoulder's end, ...) of a side (0 the right, 1 the left) reaches, in units */
static double side_reach(const uint8_t *row, int side, int upto, double heading)
{
    const double a = heading / 0x4000 * 2 * M_PI;
    double across = 0;
    for (int k = 0; k <= upto; ++k) {
        const uint8_t *p = row + 6 * (1 + 5 * side + k);
        const double dx = (int16_t)(p[0] | p[1] << 8) / 128.0, dz = (int16_t)(p[4] | p[5] << 8) / 128.0;
        across += dx * cos(a) - dz * sin(a);
    }
    return fabs(across);
}

static void set_side(TrackGenNode *n, int side, unsigned has, double shoulder, double barrier, int wall, int fence)
{
    n->side[side].has = has;
    n->side[side].shoulder = shoulder;
    n->side[side].barrier = barrier;
    n->side[side].wall = wall;
    n->side[side].fence = fence;
}

enum { ALL = TRACK_GEN_SIDE_SHOULDER | TRACK_GEN_SIDE_BARRIER | TRACK_GEN_SIDE_WALL | TRACK_GEN_SIDE_FENCE };

/* the editor's road (turned rows, the generic template) as it was before step 3: a straight one and two S bends, 10 m and 14 m wide */
static void check_no_sides_is_the_road_of_before(const uint8_t *tmpl, size_t tsize)
{
    static const struct { double degrees, width; uint64_t hash; } PINS[] = {
        { 0.0, WIDTH, 0x0f716e1d735c19bdull }, { 8.0, WIDTH, 0x657c3ddefebb1d2cull }, { 15.0, 56, 0xea96bf57d18ba178ull },
    };
    static TrackGenNode n[COUNT];
    for (size_t i = 0; i < sizeof PINS / sizeof PINS[0]; ++i) {
        s_nodes(n, PINS[i].degrees, PINS[i].width);
        const uint64_t h = generate_hash(n, tmpl, tsize);
        if (h != PINS[i].hash) fprintf(stderr, "pin %zu: 0x%llxull\n", i, (unsigned long long)h);
        CHECK(h == PINS[i].hash);
    }
}


/* every side value given as the generic template has it (shoulder and wall 3 m, soft walls, a fence each side) is the same road too */
static void check_template_values_are_the_road_of_before(const uint8_t *tmpl, size_t tsize)
{
    static TrackGenNode n[COUNT];
    s_nodes(n, 8.0, WIDTH);
    for (int i = 0; i < COUNT; ++i)
        for (int side = 0; side < 2; ++side) set_side(&n[i], side, ALL, 24, 24, TRACK_GEN_WALL_SOFT, 1);
    CHECK(generate_hash(n, tmpl, tsize) == 0x657c3ddefebb1d2cull);
}

/* the wall's distance from the road edge, each side its own: the node's barrier bytes are the verge byte plus it (1/8 m) */
static void check_barriers(const uint8_t *tmpl, size_t tsize)
{
    static TrackGenNode n[COUNT];
    s_nodes(n, 0.0, WIDTH);
    for (int i = 0; i < COUNT; ++i) {
        set_side(&n[i], 0, TRACK_GEN_SIDE_BARRIER, 0, i < 20 ? 48 : 96, 0, 0);   /* left: 6 m, then 12 m */
        if (i >= 20) set_side(&n[i], 1, TRACK_GEN_SIDE_BARRIER, 0, 8, 0, 0);    /* right: the template's 3 m, then 1 m */
    }
    uint8_t *out;
    Tri tri;
    TriParts parts;
    if (!generate_tri(n, tmpl, tsize, &out, &tri, &parts)) { CHECK(0); return; }
    int ok = 1;
    for (int i = 0; i < COUNT; ++i) {
        TriNode node;
        CHECK_EQ_INT(tri_node(&tri, (uint32_t)i, &node), TRI_OK);
        if (node.left_barrier != WIDTH + (i < 20 ? 48 : 96) || node.right_barrier != WIDTH + (i < 20 ? 24 : 8)) ok = 0;
        if (node.left_verge != WIDTH || node.right_verge != WIDTH) ok = 0;   /* the road is as it was */
    }
    CHECK(ok);
    free(out);
}

/* the wall's kind: node byte 5, the left side's in the high nibble, the right's in the low; 0 hard (the game's hit with its sound), 1 soft */
static void check_walls(const uint8_t *tmpl, size_t tsize)
{
    static TrackGenNode n[COUNT];
    s_nodes(n, 0.0, WIDTH);
    for (int i = 10; i < 30; ++i) set_side(&n[i], 0, TRACK_GEN_SIDE_WALL, 0, 0, TRACK_GEN_WALL_HARD, 0);
    for (int i = 20; i < COUNT; ++i) set_side(&n[i], 1, TRACK_GEN_SIDE_WALL, 0, 0, TRACK_GEN_WALL_HARD, 0);
    uint8_t *out;
    Tri tri;
    TriParts parts;
    if (!generate_tri(n, tmpl, tsize, &out, &tri, &parts)) { CHECK(0); return; }
    int ok = 1;
    for (int i = 0; i < COUNT; ++i) {
        TriNode node;
        CHECK_EQ_INT(tri_node(&tri, (uint32_t)i, &node), TRI_OK);
        const int left = i >= 10 && i < 30 ? 0 : 1, right = i >= 20 ? 0 : 1;   /* the template's walls are soft (0x11) */
        if (node.flags[1] != (left << 4 | right)) ok = 0;
        if (node.flags[0] != 0x11 || node.flags[2] != 0x22) ok = 0;   /* the lanes and the surfaces are not the sides' */
    }
    CHECK(ok);
    free(out);
}

/* the fence: one byte a segment (4 nodes), bit 7 the left fence and bit 6 the right one (as the game draws them: the captures fence-bit-0x80-left and
 * fence-bit-0x40-right); a segment takes its first node's; the other bits (low, the texture) stay the template's */
static void check_fences(const uint8_t *tmpl, size_t tsize)
{
    static TrackGenNode n[COUNT];
    s_nodes(n, 0.0, WIDTH);
    for (int i = 0; i < COUNT; ++i) {
        if (i >= 8 && i < 24) set_side(&n[i], 0, TRACK_GEN_SIDE_FENCE, 0, 0, 0, 0);   /* no left fence on segments 2..5 */
        if (i >= 21) set_side(&n[i], 1, TRACK_GEN_SIDE_FENCE, 0, 0, 0, 0);          /* nor a right one from segment 6 (its node 24 is the first after 21) */
    }
    uint8_t *out;
    Tri tri;
    TriParts parts;
    if (!generate_tri(n, tmpl, tsize, &out, &tri, &parts)) { CHECK(0); return; }
    int ok = 1;
    for (int seg = 0; seg < (COUNT - 1) / 4; ++seg) {
        const uint8_t fence = parts.terrain[(size_t)seg * TRI_SEGMENT_SIZE + 0xd];
        const int left = !(seg >= 2 && seg < 6), right = seg < 6;
        const uint8_t want = (uint8_t)((TRACK_TEMPLATE_FENCE & ~(TRI_FENCE_LEFT | TRI_FENCE_RIGHT)) | (left ? TRI_FENCE_LEFT : 0) | (right ? TRI_FENCE_RIGHT : 0));
        if (fence != want) {
            fprintf(stderr, "segment %d: fence 0x%02x, expected 0x%02x\n", seg, fence, want);
            ok = 0;
        }
    }
    CHECK(ok);
    free(out);
}

/* the shoulder: the row's second point of a side (the shoulder's end) is the road edge plus it; the points past it keep their own steps (the verge 10 m,
 * the grass 20 and 30), so they move out with it; the other side is as it was */
static void check_shoulders(const uint8_t *tmpl, size_t tsize)
{
    static TrackGenNode n[COUNT];
    s_nodes(n, 0.0, WIDTH);
    for (int i = 0; i < COUNT; ++i) set_side(&n[i], 1, TRACK_GEN_SIDE_SHOULDER, i < 20 ? 0 : 48, 0, 0, 0);   /* right: none, then 6 m */
    uint8_t *out;
    Tri tri;
    TriParts parts;
    if (!generate_tri(n, tmpl, tsize, &out, &tri, &parts)) { CHECK(0); return; }
    int ok = 1;
    for (int i = 0; i + 1 < COUNT; ++i) {
        const uint8_t *row = row_of(&parts, i);
        const double s = i < 20 ? 0 : 6, edge = WIDTH / 8.0;
        const double right[5] = { edge, edge + s, edge + s + 10, edge + s + 30, edge + s + 60 };
        const double left[5] = { edge, edge + 3, edge + 13, edge + 33, edge + 63 };
        for (int k = 0; k < 5; ++k) {
            if (fabs(side_reach(row, 0, k, 0) - right[k]) > 0.05) ok = 0;
            if (fabs(side_reach(row, 1, k, 0) - left[k]) > 0.05) ok = 0;
        }
    }
    CHECK(ok);
    free(out);
}

/* shoulders and walls on an S, both sides: every row still goes outward point by point (the terrain cap holds the outer points, never back over the
 * shoulder), and the barrier bytes follow */
static void check_sides_on_a_bend(const uint8_t *tmpl, size_t tsize)
{
    static TrackGenNode n[COUNT];
    s_nodes(n, 15.0, WIDTH);
    for (int i = 0; i < COUNT; ++i) {
        set_side(&n[i], 0, TRACK_GEN_SIDE_SHOULDER | TRACK_GEN_SIDE_BARRIER, 40, 64, 0, 0);
        set_side(&n[i], 1, TRACK_GEN_SIDE_SHOULDER | TRACK_GEN_SIDE_BARRIER, 8, 16, 0, 0);
    }
    uint8_t *out;
    Tri tri;
    TriParts parts;
    if (!generate_tri(n, tmpl, tsize, &out, &tri, &parts)) { CHECK(0); return; }
    int outward = 1;
    for (int i = 0; i + 1 < COUNT; ++i) {
        const uint8_t *row = row_of(&parts, i);
        for (int side = 0; side < 2; ++side)
            for (int k = 1; k < 5; ++k)
                if (side_reach(row, side, k, n[i].heading) + 0.05 < side_reach(row, side, k - 1, n[i].heading)) outward = 0;
        if (fabs(side_reach(row, 0, 1, n[i].heading) - (WIDTH + 8) / 8.0) > 0.3 || fabs(side_reach(row, 1, 1, n[i].heading) - (WIDTH + 40) / 8.0) > 0.3) outward = 0;
    }
    CHECK(outward);
    free(out);
}

/* the values the generator refuses, named: a shoulder or a wall past 20 m, a barrier byte past 255 (the wall further than 31.9 m from the centre), a kind
 * that is not hard or soft */
static void check_side_errors(const uint8_t *tmpl, size_t tsize)
{
    static TrackGenNode n[COUNT];
    static const struct { unsigned has; double shoulder, barrier; int wall; const char *word; } BAD[] = {
        { TRACK_GEN_SIDE_SHOULDER, 161, 0, 0, "shoulder" }, { TRACK_GEN_SIDE_SHOULDER, -1, 0, 0, "shoulder" },
        { TRACK_GEN_SIDE_BARRIER, 161, 0, 0, "barrier" },   /* (barrier set below) */
        { TRACK_GEN_SIDE_BARRIER, 0, 0, 0, "centre" },
        { TRACK_GEN_SIDE_WALL, 0, 0, 2, "barrier" },
    };
    for (size_t b = 0; b < sizeof BAD / sizeof BAD[0]; ++b) {
        s_nodes(n, 0.0, WIDTH);
        const double barrier = b == 2 ? 161 : b == 3 ? 100 : 24;
        if (b == 3) n[7].width = 200;   /* 25 m from the centre plus 12.5 m: past the byte */
        set_side(&n[7], 1, BAD[b].has, BAD[b].shoulder, barrier, BAD[b].wall, 0);
        uint8_t *out = NULL;
        size_t len = 0;
        ExchangeError err = {0};
        CHECK(!track_generate_nodes_turned(n, COUNT, tmpl, tsize, &out, &len, &err));
        CHECK(strstr(err.message, "node 7") != NULL && strstr(err.message, BAD[b].word) != NULL);
        free(out);
    }
}

/* the 255 is the wall's byte itself, each side: the right one adds the wall to the right edge's byte, which is the width plus what the template's right
 * edge has more than its left. A template with a wider right half refuses a right wall that the left side may have; the generic template is symmetric,
 * so the editor's TRACE_FLAG_WALL_FAR (width plus wall) is the generator's own check there */
static void check_wall_byte_per_side(const uint8_t *tmpl, size_t tsize)
{
    enum { NODE1 = 0x98C + 36 };
    CHECK_EQ_INT(tmpl[NODE1 + 0], tmpl[NODE1 + 1]);   /* the generic template: left edge == right edge */
    uint8_t *wide = malloc(tsize);
    if (wide == NULL) return;
    memcpy(wide, tmpl, tsize);
    wide[NODE1 + 1] = (uint8_t)(wide[NODE1 + 1] + 10);   /* the right edge 1.25 m farther out */
    static TrackGenNode n[COUNT];
    for (int side = 0; side < 2; ++side) {
        s_nodes(n, 0.0, WIDTH);
        n[7].width = 95;                                   /* 95 + 160 = 255: the left wall's byte is full, the right one's is past it */
        set_side(&n[7], side, TRACK_GEN_SIDE_BARRIER, 0, 160, 0, 0);
        uint8_t *out = NULL;
        size_t len = 0;
        ExchangeError err = {0};
        const int built = track_generate_nodes_turned(n, COUNT, wide, tsize, &out, &len, &err);
        if (side == TRACK_GEN_LEFT) {
            CHECK(built);
        } else {
            CHECK(!built);
            CHECK(strstr(err.message, "node 7") != NULL && strstr(err.message, "right") != NULL && strstr(err.message, "centre") != NULL);
            if (strstr(err.message, "right") == NULL) fprintf(stderr, "%s\n", err.message);
        }
        free(out);
    }
    free(wide);
}

int main(void)
{
    uint8_t *tmpl = NULL;
    size_t tsize = 0;
    ExchangeError terr = {0};
    CHECK(track_template_generic(&tmpl, &tsize, &terr));
    if (tmpl == NULL) return TEST_REPORT();
    check_no_sides_is_the_road_of_before(tmpl, tsize);
    check_template_values_are_the_road_of_before(tmpl, tsize);
    check_barriers(tmpl, tsize);
    check_walls(tmpl, tsize);
    check_fences(tmpl, tsize);
    check_shoulders(tmpl, tsize);
    check_sides_on_a_bend(tmpl, tsize);
    check_side_errors(tmpl, tsize);
    check_wall_byte_per_side(tmpl, tsize);
    free(tmpl);
    return TEST_REPORT();
}
