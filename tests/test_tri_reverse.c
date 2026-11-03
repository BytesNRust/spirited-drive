/* A circuit reversed (formats/tri_reverse.h): every CD circuit still opens, reversing twice gives the file back
 * (the scenery in the order and the nodes the reversal keeps them in), the road is where it was (every node's
 * terrain points, from the other side), and the fields that turn with the direction do. The CD's stages are refused. */
#include "formats/tri.h"
#include "formats/tri_reverse.h"
#include "test.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SCENERY_AREA = TRI_SCENERY_MAX * TRI_SCENERY_SIZE };

static void world_points(const Tri *tri, uint32_t node, int64_t out[11][3])
{
    TriNode n;
    TriSegment seg;
    CHECK_EQ_INT(tri_node(tri, node, &n), TRI_OK);
    CHECK_EQ_INT(tri_segment(tri, node / TRI_NODES_PER_SEGMENT, &seg), TRI_OK);
    const TriRow *row = &seg.rows[node % TRI_NODES_PER_SEGMENT];
    const int64_t base[3] = { n.position.x, n.position.y, n.position.z };
    const int64_t mid[3] = { row->middle.x, row->middle.y, row->middle.z };
    for (int k = 0; k < 3; k++) out[0][k] = base[k] + mid[k] * 512;
    for (int chain = 0; chain < 2; chain++) {
        const TriPoint *p = chain == 0 ? row->right : row->left;
        int64_t at[3] = { mid[0], mid[1], mid[2] };
        for (int i = 0; i < 5; i++) {
            at[0] += p[i].x, at[1] += p[i].y, at[2] += p[i].z;
            for (int k = 0; k < 3; k++) out[1 + 5 * chain + i][k] = base[k] + at[k] * 512;
        }
    }
}

static void check_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) { CHECK(0); return; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)size), *rev = malloc((size_t)size), *back = malloc((size_t)size), *again = malloc((size_t)size);
    const int ok = buf != NULL && rev != NULL && back != NULL && again != NULL && fread(buf, 1, (size_t)size, f) == (size_t)size;
    fclose(f);
    Tri tri;
    CHECK(ok && tri_open(&tri, buf, (size_t)size) == TRI_OK);
    if (!ok) return;
    if (!tri_is_circuit(&tri)) {
        CHECK_EQ_INT(tri_reverse(buf, (size_t)size, rev), TRI_ERR_HEADER);
        goto done;
    }
    CHECK_EQ_INT(tri_reverse(buf, (size_t)size, rev), TRI_OK);
    Tri r;
    CHECK_EQ_INT(tri_open(&r, rev, (size_t)size), TRI_OK);
    CHECK_EQ_INT(tri_reverse(rev, (size_t)size, back), TRI_OK);
    CHECK_EQ_INT(tri_reverse(back, (size_t)size, again), TRI_OK);
    /* all but the scenery comes back as it was; the scenery's nodes are reduced to the ring and kept in order, so
     * a second round trip leaves it where the first did */
    const size_t scenery = tri.scenery_offset;
    /* node 0's own slope and heading are covered by the header's at load (0043e4e0): only those are not kept */
    memcpy(back + 0x98C + 0x14, buf + 0x98C + 0x14, 2);
    memcpy(back + 0x98C + 0x18, buf + 0x98C + 0x18, 2);
    for (size_t i = 0; i < scenery; i++)
        if (buf[i] != back[i]) { fprintf(stderr, "%s: first difference at 0x%zx (%02x, %02x)\n", path, i, buf[i], back[i]); CHECK(0); break; }
    CHECK(memcmp(buf + scenery + SCENERY_AREA, back + scenery + SCENERY_AREA, (size_t)size - scenery - SCENERY_AREA) == 0);
    CHECK(memcmp(rev, again, (size_t)size) == 0);

    const uint32_t ring = tri.ring_nodes;
    for (uint32_t j = 0; j < ring; j += 7) {
        const uint32_t old = (ring - j) % ring;
        TriNode a, b;
        CHECK_EQ_INT(tri_node(&tri, old, &a), TRI_OK);
        CHECK_EQ_INT(tri_node(&r, j, &b), TRI_OK);
        CHECK(a.position.x == b.position.x && a.position.y == b.position.y && a.position.z == b.position.z);
        CHECK_EQ_INT(b.left_verge, a.right_verge);
        CHECK_EQ_INT(b.right_barrier, a.left_barrier);
        CHECK_EQ_INT(b.flags[0], (a.flags[0] << 4 | a.flags[0] >> 4) & 0xff);
        TriNode before;
        CHECK_EQ_INT(tri_node(&tri, (old + ring - 1) % ring, &before), TRI_OK);
        if (old > 1) CHECK_EQ_INT(b.orientation, (before.orientation + 0x2000) & 0x3fff);
        if (old > 1) CHECK_EQ_INT((before.slope + b.slope) & 0x3fff, 0);
        if (j > 0 && j + 1 < ring) {   /* the way to the next node, as the file's own values are (to a unit or two); the last one leads to node 0, whose slope the header sets */
            TriNode next;
            CHECK_EQ_INT(tri_node(&r, j + 1 < ring ? j + 1 : 0, &next), TRI_OK);
            const double dx = (double)next.position.x - b.position.x, dy = (double)next.position.y - b.position.y,
                         dz = (double)next.position.z - b.position.z;
            const double turn = 0x4000;
            int heading = (int)lround(atan2(dx, dz) / (2 * M_PI) * turn) - b.orientation;
            int slope = (int)lround(atan2(dy, hypot(dx, dz)) / (2 * M_PI) * turn) - ((b.slope + 0x2000) % 0x4000 - 0x2000);
            heading = (heading % 0x4000 + 0x6000) % 0x4000 - 0x2000;
            if (!(abs(heading) <= 2 && abs(slope) <= 2))
                fprintf(stderr, "%s node %u: heading off %d, slope off %d\n", path, j, heading, slope);
            CHECK(abs(heading) <= 2 && abs(slope) <= 2);
        }
        int64_t was[11][3], now[11][3];
        world_points(&tri, old, was);
        world_points(&r, j, now);
        CHECK(memcmp(was[0], now[0], sizeof was[0]) == 0);
        CHECK(memcmp(was[1], now[6], 5 * sizeof was[1]) == 0);    /* the old right chain is the new left one */
        CHECK(memcmp(was[6], now[1], 5 * sizeof was[6]) == 0);
    }
    for (uint32_t t = 0; t < tri.segment_count; t += 5) {
        TriSegment a, b;
        CHECK_EQ_INT(tri_segment(&tri, tri.segment_count - 1 - t, &a), TRI_OK);
        CHECK_EQ_INT(tri_segment(&r, t, &b), TRI_OK);
        for (int s = 0; s < 10; s++) CHECK_EQ_INT(b.texture[s], a.texture[(s + 5) % 10]);
        CHECK_EQ_INT((b.fence & TRI_FENCE_LEFT) != 0, (a.fence & TRI_FENCE_RIGHT) != 0);
        CHECK_EQ_INT(b.fence & TRI_FENCE_TEXTURE_MASK, a.fence & TRI_FENCE_TEXTURE_MASK);
    }
    CHECK_EQ_INT(r.scenery_count, tri.scenery_count);
    int64_t last = -1;
    for (uint32_t i = 0; i < r.scenery_count; i++) {
        TriScenery s;
        CHECK_EQ_INT(tri_scenery(&r, i, &s), TRI_OK);
        CHECK(s.node >= 0 && (uint32_t)s.node < ring && s.node >= last);   /* ascending, as the walk needs */
        last = s.node;
    }
done:
    free(buf), free(rev), free(back), free(again);
}

/* a model turns half a turn about the centre of its footprint (the pivots), not about its corner: its centre stays where it
 * stood, so reversing twice with the pivots gives the offsets back (up to the offsets' rounding), and once they have moved
 * the models, never the boards */
static void check_pivots(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) { CHECK(0); return; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)size), *once = malloc((size_t)size), *twice = malloc((size_t)size), *plain = malloc((size_t)size);
    uint8_t *plain2 = malloc((size_t)size);
    const int ok = buf != NULL && once != NULL && twice != NULL && plain != NULL && plain2 != NULL &&
                   fread(buf, 1, (size_t)size, f) == (size_t)size;
    fclose(f);
    Tri tri, a, b, c, d2;
    CHECK(ok && tri_open(&tri, buf, (size_t)size) == TRI_OK);
    if (!ok || !tri_is_circuit(&tri)) goto done;
    TriPivot pivots[64];
    for (int i = 0; i < 64; i++) pivots[i] = (TriPivot){ 100 + 7 * i, -30 + 3 * i };
    const TriReverseOptions opt = { 0, pivots, 64 };
    CHECK_EQ_INT(tri_reverse(buf, (size_t)size, plain), TRI_OK);
    CHECK_EQ_INT(tri_reverse(plain, (size_t)size, plain2), TRI_OK);
    CHECK_EQ_INT(tri_reverse_with(buf, (size_t)size, once, &opt), TRI_OK);
    CHECK_EQ_INT(tri_reverse_with(once, (size_t)size, twice, &opt), TRI_OK);
    CHECK_EQ_INT(tri_open(&a, plain, (size_t)size), TRI_OK);
    CHECK_EQ_INT(tri_open(&b, once, (size_t)size), TRI_OK);
    CHECK_EQ_INT(tri_open(&c, twice, (size_t)size), TRI_OK);
    CHECK_EQ_INT(tri_open(&d2, plain2, (size_t)size), TRI_OK);
    int moved = 0, models = 0;
    for (uint32_t i = 0; i < a.scenery_count; i++) {
        TriScenery sa, sb, sc, sd;
        TriDescriptor d;
        CHECK_EQ_INT(tri_scenery(&a, i, &sa), TRI_OK);
        CHECK_EQ_INT(tri_scenery(&b, i, &sb), TRI_OK);
        CHECK_EQ_INT(tri_scenery(&c, i, &sc), TRI_OK);
        CHECK_EQ_INT(tri_scenery(&d2, i, &sd), TRI_OK);
        CHECK_EQ_INT(tri_descriptor(&a, sa.descriptor, &d), TRI_OK);
        CHECK_EQ_INT(sa.node, sb.node);
        CHECK_EQ_INT(sa.orientation, sb.orientation);
        CHECK_EQ_INT(sa.y, sb.y);
        if (d.type != 1 || d.resource_id >= 64) {   /* a board is left alone */
            CHECK_EQ_INT(sa.x, sb.x);
            CHECK_EQ_INT(sa.z, sb.z);
            continue;
        }
        models++;
        if (sa.x != sb.x || sa.z != sb.z) moved++;
        CHECK(abs(sc.x - sd.x) <= 4 && abs(sc.z - sd.z) <= 4);
    }
    CHECK(moved > 0 || models == 0);
done:
    free(buf), free(once), free(twice), free(plain), free(plain2);
}

/* the property byte (tunnel, LOD, sound) belongs to the strip from a node to the next: the new node j has that of the old strip
 * it now starts, the one that ended at old node N - j; and reversing twice gives every node's own back */
static void check_strip_property(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) { CHECK(0); return; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)size), *once = malloc((size_t)size), *twice = malloc((size_t)size);
    const int ok = buf != NULL && once != NULL && twice != NULL && fread(buf, 1, (size_t)size, f) == (size_t)size;
    fclose(f);
    Tri tri, a, b;
    CHECK(ok && tri_open(&tri, buf, (size_t)size) == TRI_OK);
    if (!ok || !tri_is_circuit(&tri)) goto done;
    const TriReverseOptions opt = { 1, NULL, 0 };
    CHECK_EQ_INT(tri_reverse_with(buf, (size_t)size, once, &opt), TRI_OK);
    CHECK_EQ_INT(tri_reverse_with(once, (size_t)size, twice, &opt), TRI_OK);
    CHECK_EQ_INT(tri_open(&a, once, (size_t)size), TRI_OK);
    CHECK_EQ_INT(tri_open(&b, twice, (size_t)size), TRI_OK);
    const uint32_t ring = tri.ring_nodes;
    for (uint32_t j = 0; j < ring; j++) {
        const uint32_t old = (ring - j) % ring, before = (old + ring - 1) % ring;
        TriNode n, o, t, own;
        CHECK_EQ_INT(tri_node(&a, j, &n), TRI_OK);
        CHECK_EQ_INT(tri_node(&tri, before, &o), TRI_OK);
        CHECK_EQ_INT(tri_node(&b, j, &t), TRI_OK);
        CHECK_EQ_INT(tri_node(&tri, j, &own), TRI_OK);
        CHECK_EQ_INT(n.property, o.property);
        CHECK_EQ_INT(t.property, own.property);
    }
done:
    free(buf), free(once), free(twice);
}

int main(int argc, char **argv)
{
    for (int a = 1; a < argc; ++a) check_file(argv[a]), check_pivots(argv[a]), check_strip_property(argv[a]);
    return TEST_REPORT();
}
