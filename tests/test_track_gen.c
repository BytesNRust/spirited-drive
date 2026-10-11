/* Track generator tests: layout errors name the line; the generated TRI opens, has the segment count of the
 * layout, nodes that turn, climb and bank as the layout says, a cross-section widened by `width`, and no scenery.
 * With CD files every track works as the template. */
#include "formats/track_gen.h"
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

int main(int argc, char **argv)
{
    size_t tsize = 0;
    uint8_t *tmpl = template_tri(&tsize);
    check_errors(tmpl, tsize);
    check_straight_and_curve(tmpl, tsize);
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
