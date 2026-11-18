/* The track template of our own: it opens as a TRI, is a straight stage with the cross-section and texture slots the header names, is the
 * same bytes every time, and track_gen makes a track from it. */
#include "formats/track_gen.h"
#include "formats/track_template.h"
#include "formats/tri.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t fnv64(const uint8_t *p, size_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

static void check_structure(const uint8_t *file, size_t size)
{
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, file, size), TRI_OK);
    CHECK(!tri_is_circuit(&tri));
    CHECK_EQ_INT(tri.segment_count, TRACK_TEMPLATE_SEGMENTS);
    CHECK_EQ_INT(tri.descriptor_count, 64);
    CHECK_EQ_INT(tri.scenery_count, 0);   /* the list ends at once: no scenery */
    CHECK_EQ_INT(tri.node_count, TRACK_TEMPLATE_SEGMENTS * 4);

    for (uint32_t i = 0; i <= TRACK_TEMPLATE_SEGMENTS * 4; ++i) {
        TriNode n;
        CHECK_EQ_INT(tri_node(&tri, i, &n), TRI_OK);
        CHECK_EQ_INT(n.left_verge, TRACK_TEMPLATE_HALF_WIDTH);
        CHECK_EQ_INT(n.right_verge, TRACK_TEMPLATE_HALF_WIDTH);
        CHECK_EQ_INT(n.left_barrier, TRACK_TEMPLATE_BARRIER);
        CHECK_EQ_INT(n.right_barrier, TRACK_TEMPLATE_BARRIER);
        CHECK(n.flags[0] == 0x11 && n.flags[1] == 0x11 && n.flags[2] == 0x22);   /* lanes, walls: what the AI and the crash code read */
        CHECK_EQ_INT(n.property, 3);                                                /* the plain road */
        CHECK_EQ_INT(n.position.x, 0);
        CHECK_EQ_INT(n.position.y, 0);
        CHECK_EQ_INT(n.position.z, (long long)i * TRACK_GEN_NODE_LENGTH);
        CHECK_EQ_INT(n.orientation, 0);
        CHECK_EQ_INT(n.slope, 0);
        CHECK_EQ_INT(n.slant_a, 0);
        CHECK_EQ_INT(n.z_orientation, 32767);
        CHECK_EQ_INT(n.x_orientation, 0);
    }
    TriNode past;
    CHECK(tri_node(&tri, TRACK_TEMPLATE_SEGMENTS * 4 + 1, &past) != TRI_OK);   /* nothing after the closing node */

    for (uint32_t i = 0; i < TRACK_TEMPLATE_SEGMENTS; ++i) {
        TriSegment seg;
        CHECK_EQ_INT(tri_segment(&tri, i, &seg), TRI_OK);
        CHECK_EQ_INT(seg.fence, TRACK_TEMPLATE_FENCE);
        CHECK_EQ_INT(seg.fence & (TRI_FENCE_LEFT | TRI_FENCE_RIGHT), TRI_FENCE_LEFT | TRI_FENCE_RIGHT);   /* a fence each side */
        CHECK_EQ_INT(TRACK_GENERIC_FENCE_SLOT + (seg.fence & 0x1f), 32);   /* the texture slot the FAM's bank 10 letter C fills */
        const uint8_t want[10] = { TRACK_GENERIC_SLOT_ROAD, TRACK_GENERIC_SLOT_SHOULDER, TRACK_GENERIC_SLOT_VERGE, TRACK_GENERIC_SLOT_GRASS,
                                   TRACK_GENERIC_SLOT_GRASS, TRACK_GENERIC_SLOT_ROAD, TRACK_GENERIC_SLOT_SHOULDER, TRACK_GENERIC_SLOT_VERGE,
                                   TRACK_GENERIC_SLOT_GRASS, TRACK_GENERIC_SLOT_GRASS };
        CHECK_MEM(seg.texture, want, 10);
        for (int r = 0; r < 4; ++r) {
            const TriRow *row = &seg.rows[r];
            CHECK(row->middle.x == 0 && row->middle.y == 0 && row->middle.z == 0);
            for (int k = 0; k < 5; ++k) {   /* symmetric: the left chain is the right one mirrored across the road */
                CHECK_EQ_INT(row->left[k].x, -row->right[k].x);
                CHECK_EQ_INT(row->left[k].y, row->right[k].y);
                CHECK_EQ_INT(row->left[k].z, row->right[k].z);
            }
            /* the road's edge is where the verge byte says: a byte is 1/8 of a unit, a delta 1/128 of one, so 16 deltas to the byte */
            CHECK_EQ_INT(row->right[0].x, TRACK_TEMPLATE_HALF_WIDTH * 16);
            CHECK_EQ_INT(row->right[0].y, 0);
            long long reach = 0;
            for (int k = 0; k < 5; ++k) { CHECK(row->right[k].x > 0); reach += row->right[k].x; }
            CHECK(reach * 512 / 65536 >= 40);   /* at least 40 units of ground on each side */
        }
    }
    for (uint32_t i = 0; i < TRACK_TEMPLATE_SEGMENTS; ++i) {
        uint8_t brightness[3];
        CHECK_EQ_INT(tri_brightness(&tri, i, brightness), TRI_OK);
        CHECK(brightness[0] == 32 && brightness[1] == 27 && brightness[2] == 34);   /* the entry track_gen copies to every strip */
    }
}

static void check_same_bytes_each_time(const uint8_t *a, size_t asize)
{
    uint8_t *b = NULL;
    size_t bsize = 0;
    ExchangeError err = {0};
    CHECK(track_template_generic(&b, &bsize, &err));
    CHECK_EQ_INT(bsize, asize);
    if (b != NULL && bsize == asize) CHECK_MEM(a, b, asize);
    free(b);
}

static void check_generate_from_it(const uint8_t *tmpl, size_t tsize)
{
    uint8_t *out = NULL;
    size_t len = 0;
    ExchangeError err = {0};
    const char *layout = "section 10\nsection 20 turn=90 grade=4 bank=5 width=50\nsection 10 grade=0 bank=0\n";
    CHECK(track_generate(layout, strlen(layout), tmpl, tsize, &out, &len, &err));
    if (out == NULL) return;
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, out, len), TRI_OK);
    CHECK_EQ_INT(tri.segment_count, 40);
    TriSegment seg;
    CHECK_EQ_INT(tri_segment(&tri, 20, &seg), TRI_OK);   /* the generated strips keep the template's textures and are widened by width */
    CHECK_EQ_INT(seg.texture[0], TRACK_GENERIC_SLOT_ROAD);
    CHECK_EQ_INT(seg.texture[9], TRACK_GENERIC_SLOT_GRASS);
    TriNode node;
    CHECK_EQ_INT(tri_node(&tri, 80, &node), TRI_OK);   /* the node bytes the generator copies from the template's keep the lanes, walls and property */
    CHECK(node.flags[0] == 0x11 && node.flags[1] == 0x11 && node.flags[2] == 0x22);
    CHECK_EQ_INT(node.property, 3);
    CHECK(node.left_verge > TRACK_TEMPLATE_HALF_WIDTH);   /* widened by the layout */
    CHECK_EQ_INT(seg.rows[0].right[0].x, 16 * node.left_verge);   /* the road's edge is at the node's verge byte (rows 0 of strip 20 is node 80) */
    free(out);
}

int main(void)
{
    uint8_t *file = NULL;
    size_t size = 0;
    ExchangeError err = {0};
    CHECK(track_template_generic(&file, &size, &err));
    if (file == NULL) return TEST_REPORT();
    check_structure(file, size);
    check_same_bytes_each_time(file, size);
    check_generate_from_it(file, size);
    CHECK(fnv64(file, size) == 0x626720e610226d90ull);   /* the golden: whole numbers all the way, so one hash for every machine */
    free(file);
    return TEST_REPORT();
}
