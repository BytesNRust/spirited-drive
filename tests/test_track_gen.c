/* Track generator tests: layout errors name the line; the generated TRI opens, has the segment count of the
 * layout, nodes that turn, climb and bank as the layout says, a cross-section widened by `width`, and no scenery.
 * With CD files every track works as the template. */
#include "formats/track_gen.h"
#include "formats/track_template.h"
#include "formats/tri.h"
#include "test.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TSEGS = 20, DESCRIPTORS = 2, SCENERY_AREA = TRI_SCENERY_MAX * TRI_SCENERY_SIZE, WIDTH = 40 };

static void put_u32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
static void put_i16(uint8_t *p, int v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

static uint8_t *template_tri(size_t *size)
{
    uint8_t *header = calloc(1, TRI_HEADER_SIZE);
    uint8_t descriptors[DESCRIPTORS * TRI_DESCRIPTOR_SIZE];
    uint8_t *scenery = calloc(1, SCENERY_AREA);
    uint8_t terrain[TSEGS * TRI_SEGMENT_SIZE];
    header[0] = TRI_VERSION;
    put_i16(header + 4, 16);                          /* a circuit: the generator makes a stage */
    put_u32(header + 0x16218, 1000);
    uint8_t *node = header + 0x98C + 36;              /* node 1 is the model of every node */
    node[0] = node[1] = WIDTH;
    node[2] = node[3] = WIDTH + 24;
    node[4] = 0x22; node[7] = 3;
    for (int i = 0; i < DESCRIPTORS * TRI_DESCRIPTOR_SIZE; ++i) descriptors[i] = (uint8_t)(i * 3);
    for (int i = 0; i < TRI_SCENERY_MAX; ++i) put_u32(scenery + 16 * i, 0xffffffffu);
    put_u32(scenery, 5);                              /* one placement of the template */
    memset(terrain, 0, sizeof terrain);
    for (int s = 0; s < TSEGS; ++s) {
        uint8_t *seg = terrain + s * TRI_SEGMENT_SIZE;
        memcpy(seg, "TRKD", 4);
        put_u32(seg + 4, TRI_SEGMENT_SIZE - 12);
        seg[0x0d] = 0xff;
        memset(seg + 0x0e, 0x21, 10);
        for (int row = 0; row < 4; ++row) {
            put_i16(seg + 0x18 + 66 * row + 6, WIDTH * 16);        /* right[0].x */
            put_i16(seg + 0x18 + 66 * row + 0x24, -WIDTH * 16);    /* left[0].x */
            put_i16(seg + 0x18 + 66 * row + 12, 500 + row);        /* right[1].x, unrelated to the width */
        }
    }
    const TriParts parts = { header, descriptors, DESCRIPTORS, scenery, terrain, TSEGS, NULL, 0 };
    uint8_t *file = malloc(tri_write_size(&parts));
    tri_write(&parts, file, tri_write_size(&parts), size);
    free(header);
    free(scenery);
    return file;
}

static int generate(const char *layout, const uint8_t *tmpl, size_t tsize, uint8_t **out, size_t *len, ExchangeError *err)
{
    return track_generate(layout, strlen(layout), tmpl, tsize, out, len, err);
}

/* the turn limit is TRACK_GEN_TURN_MAX_DEGREES a node: 8 degrees is built, a little more is refused */
static void check_turn_limit(const uint8_t *tmpl, size_t tsize)
{
    uint8_t *out = NULL;
    size_t len = 0;
    ExchangeError err = {0};
    CHECK(generate("section 10 turn=320\n", tmpl, tsize, &out, &len, &err));          /* 8 degrees a node over 40 nodes */
    free(out);
    out = NULL;
    CHECK(!generate("section 10 turn=324\n", tmpl, tsize, &out, &len, &err));         /* 8.1 */
    CHECK(strstr(err.message, "more than 8 degrees") != NULL);
}

/* the terrain of a turned node, on the inside of the curve only, does not go further across the road than TRACK_GEN_TERRAIN_REACH of the curve's radius; the outside and a straight keep the
 * template's (5, 8, 18, 38, 68 units a side), and the turn limit of the nodes is TRACK_GEN_TURN_MAX_DEGREES (a layout keeps its 8) */
static void turned_nodes(TrackGenNode *n, int count, double degrees)
{
    memset(n, 0, sizeof *n * (size_t)count);
    int32_t x = 0, z = 0;
    for (int i = 0; i < count; ++i) {
        n[i].heading = i > 20 ? (i - 20) * degrees / 360 * 0x4000 : 0;   /* 20 nodes straight, then a curve that never ends */
        n[i].width = WIDTH;
        n[i].position[0] = x, n[i].position[2] = z;
        const double h = n[i].heading / 0x4000 * 2 * M_PI;
        x += (int32_t)lround(TRACK_GEN_NODE_LENGTH * sin(h)), z += (int32_t)lround(TRACK_GEN_NODE_LENGTH * cos(h));
    }
}

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

/* an S: 20 nodes straight, then `half` nodes turning `degrees` right and `half` left (abrupt) or the same turn as a sine over the two halves (smooth,
 * as the editor's spline gives it, the turn going through zero), then straight again */
static void s_nodes(TrackGenNode *n, int count, double degrees, int half, int smooth)
{
    memset(n, 0, sizeof *n * (size_t)count);
    int32_t x = 0, z = 0;
    double heading = 0;
    for (int i = 0; i < count; ++i) {
        const int k = i - 20;
        if (k > 0 && k <= 2 * half) {
            const double turn = smooth ? degrees * sin(M_PI * k / (double)half) : (k <= half ? degrees : -degrees);
            heading += turn / 360 * 0x4000;
        }
        n[i].heading = heading;
        n[i].width = WIDTH;
        n[i].position[0] = x, n[i].position[2] = z;
        const double h = heading / 0x4000 * 2 * M_PI;
        x += (int32_t)lround(TRACK_GEN_NODE_LENGTH * sin(h)), z += (int32_t)lround(TRACK_GEN_NODE_LENGTH * cos(h));
    }
}

/* S bends: the terrain edge, on either side, does not jump from one row to the next by more than a node's length (TRACK_GEN_NODE_LENGTH, 6.03 units:
 * the distance the rows themselves are apart; the straight's edge does not move at all) along an S: the curve's entry, the inflection where the inside
 * changes side, the exit. Before the easing of the caps (terrain_caps) the jump was 26 to 59 units */
static void check_s_bend_edges(void)
{
    uint8_t *tmpl = NULL;
    size_t tsize = 0;
    ExchangeError terr = {0};
    CHECK(track_template_generic(&tmpl, &tsize, &terr));
    if (tmpl == NULL) return;
    enum { HALF = 10, COUNT = 20 + 2 * HALF + 20 + 1 };
    static TrackGenNode n[COUNT];
    const double limit = TRACK_GEN_NODE_LENGTH / 65536.0 + 0.25;   /* and the rounding of three i16 steps */
    for (int t = 0; t < 4; ++t) {
        const double degrees = t % 2 == 0 ? TRACK_GEN_TURN_MAX_DEGREES - 0.5 : 8.0;
        s_nodes(n, COUNT, degrees, HALF, t / 2);
        uint8_t *out = NULL;
        size_t len = 0;
        ExchangeError err = {0};
        CHECK(track_generate_nodes_turned(n, COUNT, tmpl, tsize, &out, &len, &err));
        if (out == NULL) { fprintf(stderr, "%s\n", err.message); break; }
        Tri tri;
        TriParts parts;
        CHECK_EQ_INT(tri_open(&tri, out, len), TRI_OK);
        CHECK_EQ_INT(tri_parts(&tri, &parts), TRI_OK);
        double worst = 0;
        for (int i = 10; i + 2 < COUNT; ++i) {   /* the entry, the inflection and the exit (the closing node has no terrain row) */
            const uint8_t *a = parts.terrain + (size_t)(i / 4) * TRI_SEGMENT_SIZE + 0x18 + 66 * (i % 4);
            const uint8_t *b = parts.terrain + (size_t)((i + 1) / 4) * TRI_SEGMENT_SIZE + 0x18 + 66 * ((i + 1) % 4);
            for (int side = 0; side < 2; ++side)
                for (int k = 2; k < 5; ++k) {
                    const double jump = fabs(side_reach(a, side, k, n[i].heading) - side_reach(b, side, k, n[i + 1].heading));
                    if (jump > worst) worst = jump;
                }
        }
        CHECK(worst <= limit);
        free(out);
    }
    free(tmpl);
}

static void check_terrain_reach(void)
{
    uint8_t *tmpl = NULL;
    size_t tsize = 0;
    ExchangeError terr = {0};
    CHECK(track_template_generic(&tmpl, &tsize, &terr));   /* our own template: 5, 8, 18, 38, 68 units a side */
    if (tmpl == NULL) return;
    enum { COUNT = 12 * 4 + 1 };
    static TrackGenNode n[COUNT];
    const double step = TRACK_GEN_NODE_LENGTH / 65536.0;
    for (int t = 0; t < 7; ++t) {   /* a straight, then 4, 10 and 16.5 degrees to the right (positive) and to the left */
        const double magnitude = t == 0 ? 0 : (t - 1) % 3 == 0 ? 4 : (t - 1) % 3 == 1 ? 10 : TRACK_GEN_TURN_MAX_DEGREES;
        const double degrees = t > 3 ? -magnitude : magnitude;
        const int inner = degrees > 0 ? 0 : 1;   /* a right turn (positive) has the right points, side 0, on the inside */
        turned_nodes(n, COUNT, degrees);
        uint8_t *out = NULL;
        size_t len = 0;
        ExchangeError err = {0};
        CHECK(track_generate_nodes_turned(n, COUNT, tmpl, tsize, &out, &len, &err));
        if (out == NULL) { fprintf(stderr, "%s\n", err.message); return; }
        Tri tri;
        TriParts parts;
        CHECK_EQ_INT(tri_open(&tri, out, len), TRI_OK);
        CHECK_EQ_INT(tri_parts(&tri, &parts), TRI_OK);
        const double cap = degrees == 0 ? 1e9 : step / (fabs(degrees) * M_PI / 180) * TRACK_GEN_TERRAIN_REACH;
        for (int i = 30; i < 40; ++i) {   /* well inside the curve */
            const uint8_t *row = parts.terrain + (size_t)(i / 4) * TRI_SEGMENT_SIZE + 0x18 + 66 * (i % 4);
            for (int side = 0; side < 2; ++side) {
                CHECK(fabs(side_reach(row, side, 0, n[i].heading) - 5) < 0.2);   /* the road's edge and its verge are the road's own */
                CHECK(fabs(side_reach(row, side, 1, n[i].heading) - 8) < 0.2);
                for (int k = 2; k < 5; ++k) {
                    const double template_reach = k == 2 ? 18 : k == 3 ? 38 : 68;
                    const double want = side == inner ? fmin(template_reach, fmax(cap, 8)) : template_reach;   /* only the inside is held: the outside keeps its reach */
                    CHECK(fabs(side_reach(row, side, k, n[i].heading) - want) < 0.3);
                }
            }
        }
        free(out);
    }
    turned_nodes(n, COUNT, TRACK_GEN_TURN_MAX_DEGREES + 0.5);   /* the nodes' limit, not the layout's */
    uint8_t *out = NULL;
    size_t len = 0;
    ExchangeError err = {0};
    CHECK(!track_generate_nodes_turned(n, COUNT, tmpl, tsize, &out, &len, &err));
    CHECK(strstr(err.message, "the most is 16") != NULL);
    turned_nodes(n, COUNT, 12);
    CHECK(track_generate_nodes_turned(n, COUNT, tmpl, tsize, &out, &len, &err));
    free(out);
    out = NULL;
    CHECK(!track_generate_nodes(n, COUNT, tmpl, tsize, &out, &len, &err));   /* 12 is over a layout's 8 */
    free(tmpl);
}

static void check_errors(const uint8_t *tmpl, size_t tsize)
{
    static const struct { const char *layout; const char *needle; } bad[] = {
        { "", "no section" },
        { "section 4\n", "8" },                                 /* shorter than the minimum */
        { "section 700\n", "599" },
        { "section 10\nsection x\n", "line 2" },
        { "# c\nsection 10 turn=abc\n", "line 2" },
        { "section 10 spin=3\n", "spin" },
        { "section 10 grade=45\n", "grade" },
        { "section 10 bank=60\n", "bank" },
        { "section 10 width=3\n", "width" },
        { "corner 10\n", "corner" },
        { "section 10 turn=999\n", "turn" },
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        uint8_t *out = NULL;
        size_t len = 0;
        ExchangeError err = {0};
        CHECK(!generate(bad[i].layout, tmpl, tsize, &out, &len, &err));
        CHECK(out == NULL);
        CHECK(strstr(err.message, bad[i].needle) != NULL);
        if (strstr(err.message, bad[i].needle) == NULL) fprintf(stderr, "  layout %zu: %s\n", i, err.message);
    }
    uint8_t junk[64] = {0};
    uint8_t *out = NULL;
    size_t len = 0;
    ExchangeError err = {0};
    CHECK(!generate("section 10\n", junk, sizeof junk, &out, &len, &err));
    CHECK(strstr(err.message, "template") != NULL);
}

static void check_straight_and_curve(const uint8_t *tmpl, size_t tsize)
{
    uint8_t *out = NULL;
    size_t len = 0;
    ExchangeError err = {0};
    CHECK(generate("# a stage\nsection 10\nsection 20 turn=90 grade=4 bank=5 width=50\nsection 10 grade=0 bank=0\n", tmpl, tsize, &out, &len, &err));
    if (out == NULL) { fprintf(stderr, "%s\n", err.message); return; }
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, out, len), TRI_OK);
    CHECK_EQ_INT(tri.segment_count, 40);
    CHECK_EQ_INT(tri.circuit_segments, 0);
    CHECK_EQ_INT(tri.scenery_count, 0);
    CHECK_EQ_INT(tri.descriptor_count, DESCRIPTORS);

    TriNode n0, n1, n40, n80, n120, n160;
    CHECK_EQ_INT(tri_node(&tri, 0, &n0), TRI_OK);
    CHECK_EQ_INT(tri_node(&tri, 1, &n1), TRI_OK);
    CHECK_EQ_INT(tri_node(&tri, 40, &n40), TRI_OK);
    CHECK_EQ_INT(tri_node(&tri, 80, &n80), TRI_OK);
    CHECK_EQ_INT(tri_node(&tri, 120, &n120), TRI_OK);
    CHECK_EQ_INT(tri_node(&tri, 160, &n160), TRI_OK);   /* the closing node */

    /* the first section is straight along +z at the template's width */
    CHECK_EQ_INT(n1.position.x, 0);
    CHECK_EQ_INT(n1.position.z, TRACK_GEN_NODE_LENGTH);
    CHECK_EQ_INT(n1.orientation, 0);
    CHECK_EQ_INT(n1.left_verge, WIDTH);
    CHECK_EQ_INT(n1.left_barrier, WIDTH + 24);
    CHECK_EQ_INT(n1.property, 3);
    CHECK_EQ_INT(n1.z_orientation, 32767);
    CHECK_EQ_INT(n40.z_orientation, 32767);
    CHECK_EQ_INT(n40.orientation, 0);
    CHECK_EQ_INT(n40.left_verge, WIDTH);

    /* the curve: 90 degrees to the right over 80 nodes is a quarter of 0x4000 */
    CHECK(abs(n120.orientation - 0x1000) <= 2);
    CHECK(n80.orientation > 0 && n80.orientation < n120.orientation);
    CHECK(n120.position.x > 0);                           /* it went right */
    CHECK(n120.x_orientation < 0);                        /* the file stores -sin(heading) */
    CHECK_EQ_INT(n120.right_verge, 50);
    CHECK_EQ_INT(n120.right_barrier, 74);
    CHECK_EQ_INT(n120.slope, (int)lround(atan(0.04) / (2 * M_PI) * 16384));
    CHECK_EQ_INT(n120.slant_a, (int)lround(5.0 / 360 * 16384));
    CHECK_EQ_INT(n120.slant_b, -(int)lround(sin(5 * M_PI / 180) * 32767));
    CHECK(n120.position.y > n40.position.y);              /* it climbed */
    CHECK(n160.slope == 0 && n160.slant_a == 0);          /* ramps end at the layout's values */
    CHECK(n160.position.x > n120.position.x);

    /* heading and length: consecutive nodes are one node length apart in the horizontal plane */
    TriNode a, b;
    tri_node(&tri, 100, &a);
    tri_node(&tri, 101, &b);
    const double dx = b.position.x - a.position.x, dz = b.position.z - a.position.z;
    CHECK(fabs(hypot(dx, dz) - TRACK_GEN_NODE_LENGTH) < 4);

    /* the header copies node 0 */
    CHECK_EQ_INT(tri.node0_orientation, n0.orientation);

    /* the cross-section widens with `width`: right[0].x moves by 16 per unit, left[0].x the other way, the rest stays */
    TriSegment seg;
    CHECK_EQ_INT(tri_segment(&tri, 0, &seg), TRI_OK);
    CHECK_EQ_INT(seg.rows[0].right[0].x, WIDTH * 16);
    CHECK_EQ_INT(seg.rows[0].left[0].x, -WIDTH * 16);
    CHECK_EQ_INT(tri_segment(&tri, 35, &seg), TRI_OK);    /* width 50 reached at segment 30 */
    CHECK_EQ_INT(seg.rows[1].right[0].x, 50 * 16);
    CHECK_EQ_INT(seg.rows[1].left[0].x, -50 * 16);
    CHECK_EQ_INT(seg.rows[1].right[1].x, 501);
    CHECK_EQ_INT(seg.fence, 0xff);
    CHECK_EQ_INT(seg.texture[0], 0x21);
    free(out);

    /* determinism */
    uint8_t *again = NULL, *first = NULL;
    size_t again_len = 0, first_len = 0;
    CHECK(generate("section 12 turn=-30\n", tmpl, tsize, &first, &first_len, &err));
    CHECK(generate("section 12 turn=-30\n", tmpl, tsize, &again, &again_len, &err));
    CHECK(first_len == again_len && memcmp(first, again, first_len) == 0);
    Tri left;
    TriNode l;
    CHECK_EQ_INT(tri_open(&left, first, first_len), TRI_OK);
    tri_node(&left, 48, &l);
    CHECK(l.position.x < 0);                              /* a negative turn goes left */
    CHECK(l.orientation > 0x3000);                        /* headings are 14-bit: -30 degrees is 0x4000 - 1365 */
    free(first);
    free(again);
}

/* the bytes the generator makes from a layout, as hashes taken before it was split into nodes and a builder: any change of its output is a change here */
static uint64_t fnv64(const uint8_t *p, size_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

static void check_golden(const uint8_t *tmpl, size_t tsize)
{
    static const struct { const char *layout; uint64_t hash; } golden[] = {   /* one hash for every machine: track_gen is built without fused multiply-adds */
        { "section 10\n", 0xcdb2635430d481bfull },
        { "section 10\nsection 20 turn=90 grade=4 bank=5 width=50\nsection 10 grade=0 bank=0\n", 0x0cea70a33687b805ull },
        { "section 30 turn=-100 grade=-6 bank=-8 width=70\nsection 12 turn=40 grade=12 bank=3 width=30\n", 0xf1e24a91cf337e38ull },
    };
    for (size_t i = 0; i < sizeof golden / sizeof golden[0]; ++i) {
        uint8_t *out = NULL;
        size_t len = 0;
        ExchangeError err = {0};
        CHECK(generate(golden[i].layout, tmpl, tsize, &out, &len, &err));
        if (out == NULL) continue;
        if (fnv64(out, len) != golden[i].hash) fprintf(stderr, "golden %zu: 0x%llxull\n", i, (unsigned long long)fnv64(out, len));
        CHECK(fnv64(out, len) == golden[i].hash);
        free(out);
    }
}

/* explicit nodes: what a layout describes as nodes, and nodes made by hand (an editor's) */
static int nodes_of(const char *layout, TrackGenNode **nodes, int *count, ExchangeError *err)
{
    return track_layout_nodes(layout, strlen(layout), WIDTH, nodes, count, err);
}

static void check_layout_nodes(void)
{
    TrackGenNode *n = NULL;
    int count = 0;
    ExchangeError err = {0};
    CHECK(nodes_of("section 10\nsection 20 turn=90 grade=4 bank=5 width=50\n", &n, &count, &err));
    CHECK_EQ_INT(count, 30 * 4 + 1);                      /* the closing node too */
    if (n == NULL) return;
    CHECK(n[0].position[0] == 0 && n[0].position[1] == 0 && n[0].position[2] == 0);
    CHECK_EQ_INT(n[1].position[2], TRACK_GEN_NODE_LENGTH);   /* straight along +z */
    CHECK_EQ_INT((int)n[0].width, WIDTH);
    CHECK_EQ_INT((int)lround(n[count - 1].width), 50);
    CHECK_EQ_INT((int)lround(n[count - 1].grade), 4);
    CHECK_EQ_INT((int)lround(n[count - 1].bank), 5);
    CHECK(fabs(n[count - 1].heading - 0x1000) < 1e-6);    /* 90 degrees to the right */
    free(n);
    CHECK(!nodes_of("section 3\n", &n, &count, &err) && n == NULL);   /* the layout's own errors */
    CHECK(strstr(err.message, "8") != NULL);
}

/* a quarter circle of 40 segments and a straight of 10 made by hand: positions as given, angles as the formulas say */
static void hand_nodes(TrackGenNode *n, int count)
{
    memset(n, 0, sizeof *n * (size_t)count);
    int32_t x = 0, y = 0, z = 0;
    for (int i = 0; i < count; ++i) {
        n[i].heading = i <= 160 ? 0x1000 * (double)i / 160 : 0x1000;
        n[i].grade = 2, n[i].bank = i <= 160 ? 3 : 0, n[i].width = WIDTH;
        n[i].position[0] = x, n[i].position[1] = y, n[i].position[2] = z;
        const double h = n[i].heading / 0x4000 * 2 * M_PI;
        x += (int32_t)lround(300000 * sin(h)), y += 7000, z += (int32_t)lround(300000 * cos(h));   /* not the layout's 395000 */
    }
}

static void check_explicit_nodes(const uint8_t *tmpl, size_t tsize)
{
    enum { COUNT = 50 * 4 + 1 };
    static TrackGenNode n[COUNT];
    hand_nodes(n, COUNT);
    uint8_t *out = NULL;
    size_t len = 0;
    ExchangeError err = {0};
    CHECK(track_generate_nodes(n, COUNT, tmpl, tsize, &out, &len, &err));
    if (out == NULL) { fprintf(stderr, "%s\n", err.message); return; }
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, out, len), TRI_OK);
    CHECK_EQ_INT(tri.segment_count, 50);
    CHECK_EQ_INT(tri.circuit_segments, 0);
    for (int i = 0; i < COUNT; i += 17) {                 /* the positions are the given ones, the angles the formulas' */
        TriNode t;
        CHECK_EQ_INT(tri_node(&tri, (uint32_t)i, &t), TRI_OK);
        CHECK(t.position.x == n[i].position[0] && t.position.y == n[i].position[1] && t.position.z == n[i].position[2]);
        const unsigned want = (unsigned)((long)lround(n[i].heading) & 0x3fff);
        CHECK_EQ_INT((unsigned)t.orientation & 0x3fff, want);
        CHECK_EQ_INT(t.slope, (int)lround(atan(0.02) / (2 * M_PI) * 16384));
        CHECK_EQ_INT(t.left_verge, WIDTH);
    }
    uint8_t *again = NULL;
    size_t again_len = 0;
    CHECK(track_generate_nodes(n, COUNT, tmpl, tsize, &again, &again_len, &err));
    CHECK(again_len == len && memcmp(again, out, len) == 0);   /* deterministic */
    free(again);
    free(out);

    /* what the layout path makes is what the nodes of the layout make */
    const char *layout = "section 12 turn=30 grade=2\nsection 20 turn=-50 bank=4 width=60\n";
    TrackGenNode *ln = NULL;
    int lcount = 0;
    uint8_t *a = NULL, *b = NULL;
    size_t alen = 0, blen = 0;
    CHECK(nodes_of(layout, &ln, &lcount, &err));
    CHECK(generate(layout, tmpl, tsize, &a, &alen, &err));
    CHECK(track_generate_nodes(ln, lcount, tmpl, tsize, &b, &blen, &err));
    CHECK(alen == blen && memcmp(a, b, alen) == 0);
    free(ln), free(a), free(b);

    /* a template whose node 1 has a verge of 0 (CY1..3) or 123 (TR5), outside what a layout asks for: layouts from it still build */
    for (int w = 0; w <= 123; w += 123) {
        CHECK(track_layout_nodes("section 10\n", 11, w, &ln, &lcount, &err));
        CHECK(ln != NULL && (int)ln[0].width == w);
        CHECK(track_generate_nodes(ln, lcount, tmpl, tsize, &a, &alen, &err));
        free(ln), free(a);
    }
}

static void check_explicit_errors(const uint8_t *tmpl, size_t tsize)
{
    enum { COUNT = 12 * 4 + 1 };
    static TrackGenNode n[COUNT + 8];
    static const struct { const char *name; const char *needle; } bad[] = {
        { "count", "nodes" }, { "short", "8" }, { "long", "599" }, { "nan", "node 7" }, { "width", "node 5" }, { "grade", "node 6" },
        { "bank", "node 9" }, { "turn", "node 20" }, { "still", "node 11" }, { "far", "node 13" }, { "heading", "node 3" },
    };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; ++k) {
        int count = COUNT;
        hand_nodes(n, COUNT);
        for (int i = 0; i < COUNT; ++i) n[i].heading = 0, n[i].bank = 0, n[i].position[0] = 0, n[i].position[2] = 395000 * i;
        const char *w = bad[k].name;
        if (!strcmp(w, "count")) count = COUNT + 1;
        else if (!strcmp(w, "short")) count = 7 * 4 + 1;
        else if (!strcmp(w, "long")) count = 600 * 4 + 1;
        else if (!strcmp(w, "nan")) n[7].grade = NAN;
        else if (!strcmp(w, "width")) n[5].width = 300;
        else if (!strcmp(w, "grade")) n[6].grade = 31;
        else if (!strcmp(w, "bank")) n[9].bank = -21;
        else if (!strcmp(w, "turn")) for (int i = 20; i < COUNT; ++i) n[i].heading = n[19].heading + 0x4000 / 360.0 * 9 + (i - 20) * 0;   /* 9 degrees in one step: over the 8 a node may turn */
        else if (!strcmp(w, "still")) n[11].position[2] = n[10].position[2];
        else if (!strcmp(w, "far")) n[13].position[2] = n[12].position[2] + 25 * 65536;
        else if (!strcmp(w, "heading")) for (int i = 3; i < COUNT; ++i) n[i].heading = 1e30;
        uint8_t *out = NULL;
        size_t len = 0;
        ExchangeError err = {0};
        static TrackGenNode big[600 * 4 + 1];
        if (!strcmp(w, "long")) { memcpy(big, n, sizeof n); for (int i = COUNT; i < count; ++i) big[i] = n[COUNT - 1], big[i].position[2] = 395000 * i; }
        CHECK(!track_generate_nodes(!strcmp(w, "long") ? big : n, count, tmpl, tsize, &out, &len, &err));
        CHECK(out == NULL);
        CHECK(strstr(err.message, bad[k].needle) != NULL);
        if (strstr(err.message, bad[k].needle) == NULL) fprintf(stderr, "  %s: %s\n", w, err.message);
    }
}

int main(int argc, char **argv)
{
    size_t tsize = 0;
    uint8_t *tmpl = template_tri(&tsize);
    check_errors(tmpl, tsize);
    check_turn_limit(tmpl, tsize);
    check_terrain_reach();
    check_s_bend_edges();
    check_straight_and_curve(tmpl, tsize);
    check_golden(tmpl, tsize);
    check_layout_nodes();
    check_explicit_nodes(tmpl, tsize);
    check_explicit_errors(tmpl, tsize);
    free(tmpl);

    for (int i = 1; i < argc; ++i) {                      /* CD tracks as templates */
        FILE *f = fopen(argv[i], "rb");
        if (f == NULL) continue;
        fseek(f, 0, SEEK_END);
        const long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t *data = malloc((size_t)n);
        CHECK(fread(data, 1, (size_t)n, f) == (size_t)n);
        fclose(f);
        uint8_t *out = NULL;
        size_t len = 0;
        ExchangeError err = {0};
        const int ok = generate("section 16\nsection 32 turn=120 grade=3 bank=4 width=60\nsection 16 turn=-60 grade=-2 bank=0\nsection 16\n",
                                data, (size_t)n, &out, &len, &err);
        CHECK(ok);
        if (!ok) fprintf(stderr, "%s: %s\n", argv[i], err.message);
        if (ok) {
            Tri tri;
            CHECK_EQ_INT(tri_open(&tri, out, len), TRI_OK);
            CHECK_EQ_INT(tri.segment_count, 80);
            free(out);
        }
        free(data);
    }
    return TEST_REPORT();
}
