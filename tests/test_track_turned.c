/* track_generate_nodes_turned: the terrain rows of the template, a cross-section in the template node's own axes, are turned to each new
 * node's axes, so on a curve, a climb or a bank the ground follows the road (the TRI keeps rows in world axes: track_frame.h).
 * On a straight, flat road at heading 0 it is the plain generator's bytes. */
#include "formats/track_frame.h"
#include "formats/track_gen.h"
#include "formats/track_template.h"
#include "formats/tri.h"
#include "test.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *CURVED = "section 10\nsection 20 turn=90 grade=4 bank=5\nsection 10 turn=-60 grade=-3 bank=-4\nsection 10 grade=0 bank=0\n";

static int make_wide(const char *layout, double start_width, const uint8_t *tmpl, size_t tsize, int turned, uint8_t **out, size_t *len)
{
    TrackGenNode *nodes;
    int count;
    ExchangeError err = {0};
    if (!track_layout_nodes(layout, strlen(layout), start_width, &nodes, &count, &err)) return 0;
    const int ok = turned ? track_generate_nodes_turned(nodes, count, tmpl, tsize, out, len, &err)
                          : track_generate_nodes(nodes, count, tmpl, tsize, out, len, &err);
    free(nodes);
    return ok;
}

static int make(const char *layout, const uint8_t *tmpl, size_t tsize, int turned, uint8_t **out, size_t *len)
{
    return make_wide(layout, TRACK_TEMPLATE_HALF_WIDTH, tmpl, tsize, turned, out, len);
}

/* the row of node `n` as a vector of 11 points' deltas in the node's own axes: 0 middle, 1-5 right, 6-10 left */
static void local_row(const Tri *tri, uint32_t n, int32_t out[11][3])
{
    TriSegment seg;
    TriNode node;
    TrackFrame f;
    CHECK_EQ_INT(tri_segment(tri, n / 4, &seg), TRI_OK);
    CHECK_EQ_INT(tri_node(tri, n, &node), TRI_OK);
    track_frame_of_node(&node, &f);
    const TriRow *row = &seg.rows[n % 4];
    const TriPoint *pts[11] = { &row->middle, &row->right[0], &row->right[1], &row->right[2], &row->right[3], &row->right[4],
                                &row->left[0], &row->left[1], &row->left[2], &row->left[3], &row->left[4] };
    for (int i = 0; i < 11; ++i) {
        const int32_t world[3] = { pts[i]->x, pts[i]->y, pts[i]->z };
        track_frame_to_local(&f, world, out[i]);
    }
}

static int near3(const int32_t a[3], const int32_t b[3], int tol)
{
    return abs(a[0] - b[0]) <= tol && abs(a[1] - b[1]) <= tol && abs(a[2] - b[2]) <= tol;
}

/* every row of the new track, in its own node's axes, is the template's row in the template node's axes (the width unchanged). `all_points` 0
 * leaves out the points the generator holds inside a tight curve (the third of each side on: track_gen.h, TRACK_GEN_TERRAIN_REACH), which a CD
 * template's wide terrain reaches past in any curve of the layout; the generic template's (68 units at most) is not held in these curves */
static int rows_follow(const uint8_t *out, size_t len, const uint8_t *tmpl, size_t tsize, int tol, int all_points)
{
    Tri made, base;
    if (tri_open(&made, out, len) != TRI_OK || tri_open(&base, tmpl, tsize) != TRI_OK) return 0;
    int ok = 1;
    for (uint32_t n = 0; n < made.segment_count * 4; ++n) {
        int32_t a[11][3], b[11][3];
        local_row(&made, n, a);
        local_row(&base, 40 + n % 4, b);   /* the template's strip 10, row n % 4 */
        for (int i = 0; i < 11; ++i)
            if (all_points || i == 0 || i == 1 || i == 2 || i == 6 || i == 7) ok &= near3(a[i], b[i], tol);
    }
    return ok;
}

static void check_straight_is_plain(const uint8_t *tmpl, size_t tsize)
{
    uint8_t *plain = NULL, *turned = NULL;
    size_t plain_len = 0, turned_len = 0;
    CHECK(make("section 20\n", tmpl, tsize, 0, &plain, &plain_len));
    CHECK(make("section 20\n", tmpl, tsize, 1, &turned, &turned_len));
    CHECK_EQ_INT(turned_len, plain_len);
    if (plain != NULL && turned != NULL && plain_len == turned_len) CHECK_MEM(plain, turned, plain_len);
    free(plain);
    free(turned);
}

static void check_curve_follows(const uint8_t *tmpl, size_t tsize)
{
    uint8_t *plain = NULL, *turned = NULL;
    size_t plain_len = 0, turned_len = 0;
    CHECK(make(CURVED, tmpl, tsize, 0, &plain, &plain_len));
    CHECK(make(CURVED, tmpl, tsize, 1, &turned, &turned_len));
    if (plain == NULL || turned == NULL) { free(plain); free(turned); return; }
    CHECK(rows_follow(turned, turned_len, tmpl, tsize, 3, 1));    /* the rows follow the road, to the rounding of the turn */
    CHECK(!rows_follow(plain, plain_len, tmpl, tsize, 3, 1));     /* and the plain generator's do not: the test can tell */
    CHECK_EQ_INT(turned_len, plain_len);
    /* only the rows differ: the header, the descriptors, the scenery and each segment's bytes before its rows are the plain generator's */
    Tri a, b;
    TriParts pa, pb;
    CHECK_EQ_INT(tri_open(&a, plain, plain_len), TRI_OK);
    CHECK_EQ_INT(tri_open(&b, turned, turned_len), TRI_OK);
    CHECK_EQ_INT(tri_parts(&a, &pa), TRI_OK);
    CHECK_EQ_INT(tri_parts(&b, &pb), TRI_OK);
    CHECK_MEM(pa.header, pb.header, TRI_HEADER_SIZE);
    CHECK_EQ_INT(pa.descriptor_count, pb.descriptor_count);
    CHECK_MEM(pa.descriptors, pb.descriptors, (size_t)pa.descriptor_count * TRI_DESCRIPTOR_SIZE);
    CHECK_MEM(pa.scenery, pb.scenery, (size_t)TRI_SCENERY_MAX * TRI_SCENERY_SIZE);
    for (uint32_t s = 0; s < a.segment_count; ++s) CHECK_MEM(pa.terrain + (size_t)s * TRI_SEGMENT_SIZE, pb.terrain + (size_t)s * TRI_SEGMENT_SIZE, 0x18);
    free(plain);
    free(turned);
}

static void check_widening_is_along_the_road(const uint8_t *tmpl, size_t tsize)
{
    uint8_t *out = NULL;
    size_t len = 0;
    CHECK(make("section 10\nsection 20 turn=90 width=60\nsection 10\n", tmpl, tsize, 1, &out, &len));
    if (out == NULL) return;
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, out, len), TRI_OK);
    for (uint32_t n = 40; n < 120; n += 7) {   /* in the node's own axes the road edge is where its verge byte says, 16 deltas to a byte, right and left */
        TriNode node;
        int32_t row[11][3];
        CHECK_EQ_INT(tri_node(&tri, n, &node), TRI_OK);
        local_row(&tri, n, row);
        CHECK(abs(row[1][0] - 16 * node.left_verge) <= 3);
        CHECK(abs(row[6][0] + 16 * node.left_verge) <= 3);
        CHECK(abs(row[1][1]) <= 3 && abs(row[1][2]) <= 3);   /* the road's edge lies across the road, level with it */
    }
    free(out);
}

static void check_errors(const uint8_t *tmpl, size_t tsize)
{
    uint8_t *out = NULL;
    size_t len = 0;
    ExchangeError err = {0};
    TrackGenNode nodes[5] = {0};
    CHECK(!track_generate_nodes_turned(NULL, 5, tmpl, tsize, &out, &len, &err));
    CHECK(!track_generate_nodes_turned(nodes, 5, tmpl, tsize, &out, &len, &err));   /* 1 segment, and nodes on top of each other */
    CHECK(out == NULL);
    CHECK(!make("section 10\n", tmpl, 100, 1, &out, &len));                         /* not a TRI */
}

/* the bank (dep59 phase 2): the editor's nodes may bank TRACK_GEN_BANK_MAX_DEGREES (35, the CD's TR4 reaches 34.5), a layout still 20 */
static void check_bank_limit(const uint8_t *tmpl, size_t tsize)
{
    TrackGenNode *nodes = NULL;
    int count = 0;
    ExchangeError err = {0};
    CHECK(TRACK_GEN_BANK_MAX_DEGREES == 35.0 && TRACK_GEN_LAYOUT_BANK_MAX_DEGREES == 20.0);
    CHECK(track_layout_nodes("section 12\n", 11, TRACK_TEMPLATE_HALF_WIDTH, &nodes, &count, &err));
    if (nodes == NULL) return;
    uint8_t *out = NULL;
    size_t len = 0;
    for (int i = 0; i < count; ++i) nodes[i].bank = i % 2 == 0 ? 35.0 : -35.0;   /* the limit itself, either way */
    CHECK(track_generate_nodes_turned(nodes, count, tmpl, tsize, &out, &len, &err));
    free(out);
    out = NULL;
    CHECK(!track_generate_nodes(nodes, count, tmpl, tsize, &out, &len, &err) && strstr(err.message, "+-20") != NULL);   /* a layout's limit */
    nodes[9].bank = -35.5;
    CHECK(!track_generate_nodes_turned(nodes, count, tmpl, tsize, &out, &len, &err) && out == NULL);
    CHECK(strstr(err.message, "node 9") != NULL && strstr(err.message, "+-35") != NULL);
    free(nodes);
    TrackGenNode *layout = NULL;
    CHECK(!track_layout_nodes("section 12 bank=21\n", 19, TRACK_TEMPLATE_HALF_WIDTH, &layout, &count, &err) && layout == NULL);   /* a layout's bank: still 20 */
}

/* track_generate_check_turned: what track_generate_nodes_turned refuses before it writes, with its words, and nothing it takes (the editor
 * shows it in Problems before Build: a refusal there is a red line first) */
static void check_check_is_the_generators(const uint8_t *tmpl, size_t tsize)
{
    TrackGenNode *nodes = NULL;
    int count = 0;
    ExchangeError err = {0}, gen = {0};
    CHECK(track_layout_nodes("section 12\n", 11, TRACK_TEMPLATE_HALF_WIDTH, &nodes, &count, &err));
    if (nodes == NULL) return;
    CHECK(track_generate_check_turned(nodes, count, tmpl, tsize, &err));
    uint8_t *out = NULL;
    size_t len = 0;
    const int at = 20;   /* each breaks one rule at node 20: too near the node before, too far, a bank past the most, a barrier past the byte */
    for (int rule = 0; rule < 4; ++rule) {
        TrackGenNode was = nodes[at];
        if (rule == 0) nodes[at].position[2] = nodes[at - 1].position[2] + 30000;   /* 0.46 units */
        if (rule == 1) nodes[at].position[2] = nodes[at - 1].position[2] + (21 << 16);
        if (rule == 2) nodes[at].bank = 36.0;
        if (rule == 3) nodes[at].side[TRACK_GEN_LEFT].has |= TRACK_GEN_SIDE_BARRIER, nodes[at].side[TRACK_GEN_LEFT].barrier = 250;
        memset(&err, 0, sizeof err), memset(&gen, 0, sizeof gen);
        CHECK(!track_generate_check_turned(nodes, count, tmpl, tsize, &err));
        CHECK(!track_generate_nodes_turned(nodes, count, tmpl, tsize, &out, &len, &gen) && out == NULL);
        CHECK(strcmp(err.message, gen.message) == 0 && strstr(err.message, "node 2") != NULL);
        nodes[at] = was;
    }
    CHECK(!track_generate_check_turned(nodes, 5, tmpl, tsize, &err));   /* one segment */
    CHECK(!track_generate_check_turned(nodes, count, tmpl, 100, &err));   /* not a TRI */
    free(nodes);
}

/* a CD track as the template: its strip 10 is not at heading 0 nor flat, and the new rows still lie across the new road as the template's lie across its own */
static int check_cd_templates(int argc, char **argv)
{
    int used = 0;
    for (int a = 1; a < argc; ++a) {
        FILE *fp = fopen(argv[a], "rb");
        if (fp == NULL) continue;
        fseek(fp, 0, SEEK_END);
        const long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        uint8_t *data = malloc((size_t)size);
        Tri tri;
        TriNode model;
        if (data != NULL && fread(data, 1, (size_t)size, fp) == (size_t)size && tri_open(&tri, data, (size_t)size) == TRI_OK &&
            tri_node(&tri, 1, &model) == TRI_OK) {
            uint8_t *out = NULL;
            size_t len = 0;
            if (make_wide(CURVED, model.left_verge, data, (size_t)size, 1, &out, &len)) {
                ++used;
                CHECK(rows_follow(out, len, data, (size_t)size, 8, 0));
            } else {
                CHECK(0 && "a CD track does not make a track as the template");
            }
            free(out);
        }
        fclose(fp);
        free(data);
    }
    return used;
}

int main(int argc, char **argv)
{
    uint8_t *tmpl = NULL;
    size_t tsize = 0;
    ExchangeError err = {0};
    CHECK(track_template_generic(&tmpl, &tsize, &err));
    if (tmpl == NULL) return TEST_REPORT();
    check_straight_is_plain(tmpl, tsize);
    check_curve_follows(tmpl, tsize);
    check_widening_is_along_the_road(tmpl, tsize);
    check_errors(tmpl, tsize);
    check_bank_limit(tmpl, tsize);
    check_check_is_the_generators(tmpl, tsize);
    if (argc > 1) CHECK(check_cd_templates(argc, argv) > 0);
    free(tmpl);
    return TEST_REPORT();
}
