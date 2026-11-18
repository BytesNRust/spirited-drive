/* track_frame: the node's axes from its three angles, and the integer turn between the world and the node. */
#include "formats/track_frame.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "test.h"

enum { ONE = 0x10000, QUARTER = 0x1000, TURN = 0x4000 };

static int near(int32_t a, int32_t b, int32_t tol)
{
    return abs(a - b) <= tol;
}

static int axis_is(const TrackFrame *f, int axis, int32_t x, int32_t y, int32_t z, int32_t tol)
{
    return near(f->axis[axis][0], x, tol) && near(f->axis[axis][1], y, tol) && near(f->axis[axis][2], z, tol);
}

static void check_known_frames(void)
{
    TrackFrame f;
    track_frame_make(0, 0, 0, &f);   /* heading 0: along is z, right is x, up is y */
    CHECK(axis_is(&f, 0, ONE, 0, 0, 0));
    CHECK(axis_is(&f, 1, 0, ONE, 0, 0));
    CHECK(axis_is(&f, 2, 0, 0, ONE, 0));

    track_frame_make(QUARTER, 0, 0, &f);   /* a quarter turn: along is x (as the node positions advance), right is -z */
    CHECK(axis_is(&f, 2, ONE, 0, 0, 2));
    CHECK(axis_is(&f, 0, 0, 0, -ONE, 2));
    CHECK(axis_is(&f, 1, 0, ONE, 0, 2));

    track_frame_make(0, QUARTER / 2, 0, &f);   /* climbing 45 degrees: along gains height, up leans back */
    CHECK(f.axis[2][1] > 0 && near(f.axis[2][1], f.axis[2][2], 4));
    CHECK(f.axis[1][2] < 0 && near(f.axis[1][1], f.axis[2][2], 4));
    CHECK(axis_is(&f, 0, ONE, 0, 0, 2));

    TrackFrame g;
    track_frame_make(5 + TURN, -7 - TURN, 9 + 3 * TURN, &f);   /* the angles wrap */
    track_frame_make(5, -7, 9, &g);
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) CHECK(f.axis[i][j] == g.axis[i][j]);
}

static void check_node_frame(void)
{
    TriNode node = {0};
    node.orientation = 0x0555;
    node.slope = 0x0123;
    node.slant_a = 0x0042;
    TrackFrame a, b;
    track_frame_of_node(&node, &a);
    track_frame_make(0x0555, 0x0123, 0x0042, &b);
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) CHECK(a.axis[i][j] == b.axis[i][j]);
}

static int64_t dot(const int32_t *a, const int32_t *b)
{
    return ((int64_t)a[0] * b[0] + (int64_t)a[1] * b[1] + (int64_t)a[2] * b[2]) >> 16;
}

/* the axes stay unit length and square to each other over a sweep of every angle */
static void check_orthonormal(void)
{
    for (int h = 0; h < TURN; h += 331) for (int s = -0x0600; s <= 0x0600; s += 211) for (int b = -0x0300; b <= 0x0300; b += 97) {
        TrackFrame f;
        track_frame_make(h, s, b, &f);
        for (int i = 0; i < 3; ++i) for (int j = i; j < 3; ++j) {
            const int64_t want = i == j ? ONE : 0;
            CHECK(llabs((long long)(dot(f.axis[i], f.axis[j]) - want)) <= 64);
        }
    }
}

/* to the world and back keeps a vector to the rounding of the 16.16 axes */
static void check_round_trip(void)
{
    uint32_t seed = 12345;
    for (int n = 0; n < 4000; ++n) {
        seed = seed * 1664525u + 1013904223u;
        const int32_t heading = (int32_t)(seed >> 8) & 0x3fff;
        seed = seed * 1664525u + 1013904223u;
        const int32_t slope = ((int32_t)(seed >> 8) & 0x7ff) - 0x400;
        seed = seed * 1664525u + 1013904223u;
        const int32_t bank = ((int32_t)(seed >> 8) & 0x3ff) - 0x200;
        TrackFrame f;
        track_frame_make(heading, slope, bank, &f);
        int32_t v[3];
        for (int k = 0; k < 3; ++k) {
            seed = seed * 1664525u + 1013904223u;
            v[k] = ((int32_t)((seed >> 8) & 0x3ffff) - 0x20000) * (n % 2 ? 1 : 128);   /* up to 2 u either way, and up to 256 u (rows reach 460) */
        }
        int32_t w[3], back[3];
        track_frame_to_world(&f, v, w);
        track_frame_to_local(&f, w, back);
        const int32_t big = abs(v[0]) > abs(v[1]) ? (abs(v[0]) > abs(v[2]) ? abs(v[0]) : abs(v[2])) : (abs(v[1]) > abs(v[2]) ? abs(v[1]) : abs(v[2]));
        for (int k = 0; k < 3; ++k) CHECK(near(back[k], v[k], 2 + big / 5000));   /* measured worst: 1.4e-4 of the biggest component */
    }
}

static void check_exact_turns(void)
{
    TrackFrame f;
    track_frame_make(QUARTER, 0, 0, &f);
    const int32_t local[3] = { 5 * 0x200, 7 * 0x200, 11 * 0x200 };   /* right 5, up 7, along 11 units of 0x200 */
    int32_t w[3];
    track_frame_to_world(&f, local, w);
    CHECK(near(w[0], local[2], 2) && near(w[1], local[1], 2) && near(w[2], -local[0], 2));
    int32_t back[3];
    track_frame_to_local(&f, w, back);
    for (int k = 0; k < 3; ++k) CHECK(near(back[k], local[k], 2));

    int32_t zero[3] = { 0, 0, 0 };
    track_frame_to_world(&f, zero, w);
    CHECK(w[0] == 0 && w[1] == 0 && w[2] == 0);
}

/* The first step of a row from the node: on every road of the CD it goes to the right (right[0]) and to the left (left[0]) of the
 * node in the node's own axes, whatever the heading; and on the banked ones the two edges are level in the banked frame
 * and not in the flat one: the frame and the TRI agree on which way is right and which way the bank leans. */
static void edges_local(const TriRow *row, const TrackFrame *f, int32_t right[3], int32_t left[3])
{
    const int32_t r[3] = { row->right[0].x * 0x200, row->right[0].y * 0x200, row->right[0].z * 0x200 };
    const int32_t l[3] = { row->left[0].x * 0x200, row->left[0].y * 0x200, row->left[0].z * 0x200 };
    track_frame_to_local(f, r, right);
    track_frame_to_local(f, l, left);
}

typedef struct { int tracks, banked_tracks; } CdStats;

/* the along axis points at the next node: the cosine of the angle to the step between the two positions */
static int along_follows(const TrackFrame *f, const TriNode *a, const TriNode *b)
{
    const double step[3] = { (double)b->position.x - a->position.x, (double)b->position.y - a->position.y, (double)b->position.z - a->position.z };
    const double len = sqrt(step[0] * step[0] + step[1] * step[1] + step[2] * step[2]);
    if (len < 1) return 1;
    const double dotp = (step[0] * f->axis[2][0] + step[1] * f->axis[2][1] + step[2] * f->axis[2][2]) / 65536.0 / len;
    return dotp > 0.98;
}

static void check_cd_track(const Tri *tri, CdStats *stats)
{
    int total = 0, right_ok = 0, left_ok = 0, banked = 0, along_ok = 0, along_total = 0;
    double level_banked = 0, level_flat = 0;
    for (uint32_t s = 0; s + 1 < tri->segment_count; ++s) {
        TriSegment seg;
        if (tri_segment(tri, s, &seg) != TRI_OK) continue;
        for (int r = 0; r < TRI_NODES_PER_SEGMENT; ++r) {
            TriNode node;
            if (tri_node(tri, s * TRI_NODES_PER_SEGMENT + (uint32_t)r, &node) != TRI_OK) continue;
            TrackFrame f, flat;
            track_frame_of_node(&node, &f);
            track_frame_make(node.orientation, node.slope, 0, &flat);
            int32_t right[3], left[3];
            edges_local(&seg.rows[r], &f, right, left);
            ++total;
            TriNode next;
            if (node.orientation != 0 && tri_node(tri, s * TRI_NODES_PER_SEGMENT + (uint32_t)r + 1, &next) == TRI_OK) {
                ++along_total;
                if (along_follows(&f, &node, &next)) ++along_ok;
            }
            if (right[0] > 0) ++right_ok;
            if (left[0] < 0) ++left_ok;
            if (node.slant_a == 0) continue;
            ++banked;
            level_banked += abs(right[1] - left[1]);
            edges_local(&seg.rows[r], &flat, right, left);
            level_flat += abs(right[1] - left[1]);
        }
    }
    CHECK(total > 0);
    if (total == 0) return;
    ++stats->tracks;
    CHECK(along_ok * 100 >= along_total * 98);
    CHECK(right_ok * 100 >= total * 99);
    CHECK(left_ok * 100 >= total * 99);
    if (banked >= 50) {
        ++stats->banked_tracks;
        CHECK(level_banked * 4 < level_flat);
    }
}

static void check_cd_tracks(int argc, char **argv)
{
    CdStats stats = {0, 0};
    for (int a = 1; a < argc; ++a) {
        FILE *fp = fopen(argv[a], "rb");
        if (fp == NULL) continue;
        fseek(fp, 0, SEEK_END);
        const long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        uint8_t *data = malloc((size_t)size);
        if (data != NULL && fread(data, 1, (size_t)size, fp) == (size_t)size) {
            Tri tri;
            if (tri_open(&tri, data, (size_t)size) == TRI_OK) check_cd_track(&tri, &stats);
        }
        fclose(fp);
        free(data);
    }
    if (argc > 1) {   /* with CD files, they were read: a test that loaded none proves nothing */
        CHECK(stats.tracks > 0);
        CHECK(stats.banked_tracks > 0);
    }
}

int main(int argc, char **argv)
{
    check_known_frames();
    check_node_frame();
    check_orthonormal();
    check_round_trip();
    check_exact_turns();
    check_cd_tracks(argc, argv);
    return TEST_REPORT();
}
