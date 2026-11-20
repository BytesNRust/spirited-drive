/* Lanes in the generator (dep59 editor phase 2, step 4): a node's lanes (flags[0], node +4: oncoming << 4 | own, the CD's 0x11 / 0x12 / 0x21) and its
 * right half width. A 1+2 node has the CD's AL1 node 220 bytes (notes/dep59-editor-road-width.md §B: verge 40 / 80) and three 5 m strips, but not its
 * row: the middle point stays on the node line (the yellow centre) and the one-lane side is the 1+1 road's, so nothing there moves where a stretch
 * starts (AL1 moves its middle a lane out and hides the jump under transition pictures the generic art has not). On the two-lane side the inner lane
 * runs from the centre to a dashed line (slot 18: yellow inside, dashed outside), the outer lane from it to the edge (slot 15: dashed inside, edge
 * outside), and the template's strips beyond move one strip outwards. 2+1 is its mirror. A node with no lanes is the road of before, byte for byte
 * (test_track_sides pins it). */
#include "formats/track_gen.h"
#include "formats/track_template.h"
#include "formats/tri.h"
#include "test.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { COUNT = 12 * 4 + 1, LANE_FROM = 16, LANE_TO = 32, NODE = 20, SEG = NODE / 4 };

/* a straight along +z (heading 0: right is +x), 10 m wide; nodes LANE_FROM..LANE_TO-1 have `lanes` with half widths `left` / `right` (1/8 m) */
static void straight(TrackGenNode *n, int lanes, double left, double right)
{
    memset(n, 0, sizeof *n * COUNT);
    for (int i = 0; i < COUNT; ++i) {
        n[i].width = 40;
        n[i].position[2] = (int32_t)i * TRACK_GEN_NODE_LENGTH;
        if (i >= LANE_FROM && i < LANE_TO) n[i].lanes = lanes, n[i].width = left, n[i].width_r = right;
    }
}

static int generate(const TrackGenNode *n, const uint8_t *tmpl, size_t tsize, uint8_t **out, Tri *tri, TriParts *parts, char *why, size_t cap)
{
    size_t len = 0;
    ExchangeError err = {0};
    *out = NULL;
    if (!track_generate_nodes_turned(n, COUNT, tmpl, tsize, out, &len, &err)) {
        snprintf(why, cap, "%s", err.message);
        return 0;
    }
    return tri_open(tri, *out, len) == TRI_OK && tri_parts(tri, parts) == TRI_OK;
}

static const uint8_t *row_of(const TriParts *parts, int node)
{
    return parts->terrain + (size_t)(node / 4) * TRI_SEGMENT_SIZE + 0x18 + 66 * (node % 4);
}

/* the lateral place of a row point in metres, right positive: 0 the middle; 1..5 the right side's chain (1 the road edge), 6..10 the left's */
static double point_at(const uint8_t *row, int point)
{
    const uint8_t *m = row;
    double x = (int16_t)(m[0] | m[1] << 8) / 128.0;
    if (point == 0) return x;
    const int side = point > 5, last = side ? point - 6 : point - 1;
    for (int k = 0; k <= last; ++k) {
        const uint8_t *p = row + 6 * (1 + 5 * side + k);
        x += (int16_t)(p[0] | p[1] << 8) / 128.0;
    }
    return x;
}

static int near(double a, double b) { return fabs(a - b) < 0.01; }

static void check_row(const uint8_t *row, const double want[6], const char *what)
{
    static const int POINTS[6] = { 0, 1, 2, 6, 7, 8 };   /* middle, R0, R1, L0, L1, L2 */
    for (int k = 0; k < 6; ++k) {
        const double got = point_at(row, POINTS[k]);
        CHECK(near(got, want[k]));
        if (!near(got, want[k])) fprintf(stderr, "%s: point %d at %.3f m, not %.3f\n", what, POINTS[k], got, want[k]);
    }
}

/* the keyless road first: the row reading itself (middle 0, edges +-5, shoulders 3 m) and the template's texture bytes */
static void keyless(const uint8_t *tmpl, size_t tsize, uint8_t tex[10])
{
    static TrackGenNode n[COUNT];
    straight(n, 0, 40, 0);
    uint8_t *out = NULL;
    Tri tri;
    TriParts parts;
    char why[256] = "";
    CHECK(generate(n, tmpl, tsize, &out, &tri, &parts, why, sizeof why));
    if (out == NULL) return;
    const double want[6] = { 0, 5, 8, -5, -8, 0 };   /* the last is not checked */
    const uint8_t *row = row_of(&parts, NODE);
    CHECK(near(point_at(row, 0), want[0]) && near(point_at(row, 1), want[1]) && near(point_at(row, 2), want[2]));
    CHECK(near(point_at(row, 6), want[3]) && near(point_at(row, 7), want[4]));
    memcpy(tex, parts.terrain + (size_t)SEG * TRI_SEGMENT_SIZE + 0xe, 10);
    CHECK_EQ_INT(out[0x98C + 36 * NODE + 4], 0x11);   /* the template's lanes */
    free(out);
}

/* 1+2 and 2+1 at full width, and a node of the taper (the new lane half grown) */
static void lanes(const uint8_t *tmpl, size_t tsize, const uint8_t tmpl_tex[10])
{
    static const struct {
        int lanes;
        double left, right;
        uint8_t bytes[5];                 /* verge left, right, barrier left, right, flags[0] */
        double row[6];                    /* middle, R0, R1, L0, L1, L2 (0: the template's, past the one-lane side's shoulder) */
        double shoulder_end;              /* the two-lane side's third point (R2 / L2): its shoulder's end, 3 m past the edge */
    } CASES[] = {
        { 0x12, 40, 80, { 40, 80, 64, 104, 0x12 }, { 0, 5, 10, -5, -8, 0 }, 13 },
        { 0x21, 80, 40, { 80, 40, 104, 64, 0x21 }, { 0, 5, 8, -5, -10, -13 }, -13 },
        { 0x12, 40, 60, { 40, 60, 64, 84, 0x12 }, { 0, 5, 7.5, -5, -8, 0 }, 10.5 },   /* the taper: the outer lane 2.5 m */
    };
    for (size_t c = 0; c < sizeof CASES / sizeof CASES[0]; ++c) {
        static TrackGenNode n[COUNT];
        straight(n, CASES[c].lanes, CASES[c].left, CASES[c].right);
        uint8_t *out = NULL;
        Tri tri;
        TriParts parts;
        char why[256] = "";
        const int made = generate(n, tmpl, tsize, &out, &tri, &parts, why, sizeof why);
        CHECK(made);
        if (!made) {
            fprintf(stderr, "case %zu: %s\n", c, why);
            free(out);
            continue;
        }
        const uint8_t *node = out + 0x98C + 36 * NODE;
        for (int b = 0; b < 4; ++b) CHECK_EQ_INT(node[b], CASES[c].bytes[b]);
        CHECK_EQ_INT(node[4], CASES[c].bytes[4]);
        CHECK_EQ_INT(out[0x98C + 36 * (LANE_FROM - 1) + 4], 0x11);   /* before the stretch: the template's */
        CHECK_EQ_INT(out[0x98C + 36 * LANE_TO + 4], 0x11);
        double want[6];
        memcpy(want, CASES[c].row, sizeof want);
        if (CASES[c].lanes == 0x12) want[5] = point_at(row_of(&parts, NODE), 8);   /* L2 is past the one-lane side's shoulder: not this test's */
        CHECK(near(point_at(row_of(&parts, NODE), CASES[c].lanes == 0x12 ? 3 : 8), CASES[c].shoulder_end));
        char what[32];
        snprintf(what, sizeof what, "case %zu", c);
        check_row(row_of(&parts, NODE), want, what);
        /* the segment's strips: on the two-lane side the lanes' slots first, the template's beyond moved one strip out; the other side the template's */
        const uint8_t *tex = parts.terrain + (size_t)SEG * TRI_SEGMENT_SIZE + 0xe;
        uint8_t expect[10];
        if (CASES[c].lanes == 0x12) {
            const uint8_t e[10] = { 18, 15, tmpl_tex[1], tmpl_tex[2], tmpl_tex[3], tmpl_tex[5], tmpl_tex[6], tmpl_tex[7], tmpl_tex[8], tmpl_tex[9] };
            memcpy(expect, e, 10);
        } else {
            const uint8_t e[10] = { tmpl_tex[0], tmpl_tex[1], tmpl_tex[2], tmpl_tex[3], tmpl_tex[4], 18, 15, tmpl_tex[6], tmpl_tex[7], tmpl_tex[8] };
            memcpy(expect, e, 10);
        }
        CHECK(memcmp(tex, expect, 10) == 0);
        if (memcmp(tex, expect, 10) != 0)
            for (int k = 0; k < 10; ++k) fprintf(stderr, "case %zu tex[%d] = %d, want %d\n", c, k, tex[k], expect[k]);
        CHECK(memcmp(parts.terrain + (size_t)(LANE_FROM / 4 - 1) * TRI_SEGMENT_SIZE + 0xe, tmpl_tex, 10) == 0);   /* the segment before: the template's */
        free(out);
    }
}

/* the values the generator refuses, named: lanes the CD never has, a right half width out of the byte */
static void faults(const uint8_t *tmpl, size_t tsize)
{
    static const struct { int lanes; double right; const char *word; } BAD[] = {
        { 0x22, 80, "lanes" }, { 0x13, 120, "lanes" }, { 0x12, 256, "right" }, { 0x12, -1, "right" },
    };
    for (size_t b = 0; b < sizeof BAD / sizeof BAD[0]; ++b) {
        static TrackGenNode n[COUNT];
        straight(n, BAD[b].lanes, 40, BAD[b].right);
        uint8_t *out = NULL;
        Tri tri;
        TriParts parts;
        char why[256] = "";
        CHECK(!generate(n, tmpl, tsize, &out, &tri, &parts, why, sizeof why));
        CHECK(strstr(why, "node 16") != NULL && strstr(why, BAD[b].word) != NULL);
        if (strstr(why, BAD[b].word) == NULL) fprintf(stderr, "bad %zu: %s\n", b, why);
        free(out);
    }
}

/* more refusals: lanes that change inside a segment, a lane node whose two-lane half is under a lane, lanes on the flat rows, a right barrier past its byte
 * (the right half plus the wall: the 255 is the right edge's byte, not the left one's) */
static void more_faults(const uint8_t *tmpl, size_t tsize)
{
    static TrackGenNode n[COUNT];
    uint8_t *out = NULL;
    size_t len = 0;
    ExchangeError err = {0};
    straight(n, TRACK_GEN_LANES_1_2, 40, 80);
    n[17].lanes = 0, n[17].width_r = 0;   /* inside segment 4 */
    CHECK(!track_generate_nodes_turned(n, COUNT, tmpl, tsize, &out, &len, &err));
    CHECK(strstr(err.message, "node 17") != NULL && strstr(err.message, "segment") != NULL);
    free(out), out = NULL;

    straight(n, TRACK_GEN_LANES_1_2, 40, 32);
    CHECK(!track_generate_nodes_turned(n, COUNT, tmpl, tsize, &out, &len, &err));
    CHECK(strstr(err.message, "node 16") != NULL && strstr(err.message, "right half at least a lane") != NULL);
    free(out), out = NULL;
    straight(n, TRACK_GEN_LANES_2_1, 32, 80);
    CHECK(!track_generate_nodes_turned(n, COUNT, tmpl, tsize, &out, &len, &err));
    CHECK(strstr(err.message, "node 16") != NULL && strstr(err.message, "left half at least a lane") != NULL);
    free(out), out = NULL;

    straight(n, TRACK_GEN_LANES_1_2, 40, 80);
    CHECK(!track_generate_nodes(n, COUNT, tmpl, tsize, &out, &len, &err));
    CHECK(strstr(err.message, "node 16") != NULL && strstr(err.message, "turned") != NULL);
    free(out), out = NULL;

    straight(n, TRACK_GEN_LANES_1_2, 40, 100);   /* 100 + 160 = 260 on the right; the left's 40 + 160 would fit */
    for (int i = LANE_FROM; i < LANE_TO; ++i) n[i].side[TRACK_GEN_RIGHT].has = TRACK_GEN_SIDE_BARRIER, n[i].side[TRACK_GEN_RIGHT].barrier = 160;
    CHECK(!track_generate_nodes_turned(n, COUNT, tmpl, tsize, &out, &len, &err));
    CHECK(strstr(err.message, "node 16") != NULL && strstr(err.message, "right barrier") != NULL);
    if (strstr(err.message, "right barrier") == NULL) fprintf(stderr, "barrier: %s\n", err.message);
    free(out), out = NULL;
}

/* a tarmac shoulder (dev3's step 2b) on a lane segment: its picture goes on the shoulder band, one strip further out on the two-lane side (the right of
 * a 1+2: two lanes, then the shoulder), where it was on the other side */
static void tarmac_on_lanes(const uint8_t *tmpl, size_t tsize)
{
    static TrackGenNode n[COUNT];
    straight(n, TRACK_GEN_LANES_1_2, 40, 80);
    for (int i = LANE_FROM; i < LANE_TO; ++i)
        for (int side = 0; side < 2; ++side) n[i].side[side].has = TRACK_GEN_SIDE_TARMAC, n[i].side[side].tarmac = 1;
    uint8_t *out = NULL;
    Tri tri;
    TriParts parts;
    char why[256] = "";
    CHECK(generate(n, tmpl, tsize, &out, &tri, &parts, why, sizeof why));
    if (out == NULL) return;
    const uint8_t *tex = parts.terrain + (size_t)SEG * TRI_SEGMENT_SIZE + 0xe;
    CHECK_EQ_INT(tex[0], TRACK_GENERIC_SLOT_LANE_CENTRE);
    CHECK_EQ_INT(tex[1], TRACK_GENERIC_SLOT_LANE_EDGE);
    CHECK_EQ_INT(tex[2], TRACK_GENERIC_SLOT_TARMAC);        /* right: two lanes, then the shoulder */
    CHECK_EQ_INT(tex[5], TRACK_GENERIC_SLOT_ROAD);
    CHECK_EQ_INT(tex[6], TRACK_GENERIC_SLOT_TARMAC);        /* left: the road's own, then the shoulder */
    free(out);
}

/* where a stretch starts and ends nothing jumps on the one-lane side: the middle (the yellow centre) and its edge and shoulder are where the node before has
 * them, and the two-lane side's edge is not nearer the centre than before (the quads between rows join point to point: a point that moved a lane drew the
 * strips diagonally, a gravel wedge over the oncoming lane) */
static void no_jump(const uint8_t *tmpl, size_t tsize)
{
    static const int LANES[2] = { TRACK_GEN_LANES_1_2, TRACK_GEN_LANES_2_1 };
    for (int c = 0; c < 2; ++c) {
        static TrackGenNode n[COUNT];
        const int right = LANES[c] == TRACK_GEN_LANES_1_2;
        straight(n, LANES[c], right ? 40 : 45, right ? 45 : 40);   /* the taper's first node: 5 eighths more on the two-lane side */
        uint8_t *out = NULL;
        Tri tri;
        TriParts parts;
        char why[256] = "";
        CHECK(generate(n, tmpl, tsize, &out, &tri, &parts, why, sizeof why));
        if (out == NULL) continue;
        for (int k = 0; k < 2; ++k) {
            const int at = k == 0 ? LANE_FROM : LANE_TO;   /* into the stretch, and out of it */
            const uint8_t *before = row_of(&parts, at - 1), *now = row_of(&parts, at);
            const int one_edge = right ? 6 : 1, one_shoulder = right ? 7 : 2, two_edge = right ? 2 : 7, two_edge_before = right ? 1 : 6;
            CHECK(near(point_at(now, 0), point_at(before, 0)));
            CHECK(near(point_at(now, one_edge), point_at(before, one_edge)) && near(point_at(now, one_shoulder), point_at(before, one_shoulder)));
            if (k == 0) CHECK(fabs(point_at(now, two_edge)) >= fabs(point_at(before, two_edge_before)) - 0.01);
        }
        free(out);
    }
}

int main(void)
{
    uint8_t *tmpl = NULL;
    size_t tsize = 0;
    ExchangeError terr = {0};
    CHECK(track_template_generic(&tmpl, &tsize, &terr));
    if (tmpl == NULL) return TEST_REPORT();
    uint8_t tex[10] = {0};
    keyless(tmpl, tsize, tex);
    lanes(tmpl, tsize, tex);
    faults(tmpl, tsize);
    more_faults(tmpl, tsize);
    tarmac_on_lanes(tmpl, tsize);
    no_jump(tmpl, tsize);
    free(tmpl);
    return TEST_REPORT();
}
