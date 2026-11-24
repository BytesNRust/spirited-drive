/* portal_faces_add (game/race/portal_faces.h): a tunnel's portal, a model on the node before its first tunnel node, gets its second face on the node after the
 * last one: the same descriptor, turned half a turn, its centre across the road where the first one's is; a board is not copied, a model inside the tunnel
 * is not, a mouth that has the model already is left alone, a second call adds nothing, and the records stay in node order. */
#include "formats/tri.h"
#include "game/race/portal_faces.h"
#include "test.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { SEGMENTS = 4, NODES = 16, FIRST = 2, LAST = 13, SCENERY_BYTES = TRI_SCENERY_MAX * TRI_SCENERY_SIZE, NODE_AT = 0x98C };

typedef struct {
    uint8_t header[TRI_HEADER_SIZE];
    uint8_t descriptors[3 * TRI_DESCRIPTOR_SIZE];
    uint8_t scenery[SCENERY_BYTES];
    uint8_t terrain[SEGMENTS * TRI_SEGMENT_SIZE];
} Synthetic;

/* the file the parts make, and the parts' scenery the placements are written into */
static uint8_t file[sizeof(Synthetic) + 64];
static size_t file_size;

static void write_file(const Synthetic *s)
{
    const TriParts p = { s->header, s->descriptors, 3, s->scenery, s->terrain, SEGMENTS, NULL, 0 };
    CHECK_EQ_INT(tri_write(&p, file, sizeof file, &file_size), TRI_OK);
}

static void put16(uint8_t *p, int v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }

static void record(Synthetic *s, int index, int node, int descriptor, int orientation, int x, int y, int z)
{
    uint8_t *r = s->scenery + index * TRI_SCENERY_SIZE;
    memset(r, 0, TRI_SCENERY_SIZE);
    put32(r, (uint32_t)node);
    r[4] = (uint8_t)descriptor, r[5] = (uint8_t)orientation;
    put16(r + 0x0a, x), put16(r + 0x0c, y), put16(r + 0x0e, z);
}

/* `bend` turns every node's heading a little more than the one before: the far mouth faces another way */
static void fill(Synthetic *s, int bend)
{
    memset(s, 0, sizeof *s);
    s->header[0] = TRI_VERSION;
    put32(s->header + 0x16218, 1000);
    uint8_t *d = s->descriptors;
    d[1] = 1, d[2] = 0;                       /* descriptor 0: model 0 */
    d[TRI_DESCRIPTOR_SIZE + 1] = 4;           /* 1: a board */
    d[2 * TRI_DESCRIPTOR_SIZE + 1] = 1, d[2 * TRI_DESCRIPTOR_SIZE + 2] = 1;   /* 2: model 1 */
    for (int i = 0; i < SEGMENTS; i++) {
        uint8_t *seg = s->terrain + i * TRI_SEGMENT_SIZE;
        memcpy(seg, "TRKD", 4);
        put32(seg + 4, TRI_SEGMENT_SIZE - 12);
    }
    for (int n = 0; n < NODES; n++) {
        uint8_t *p = s->header + NODE_AT + 36 * n;
        put32(p + 8, (uint32_t)(n * 0x60000 + (bend ? n * n * 0x1000 : 0)));
        put32(p + 16, (uint32_t)(n * 0x60000));
        p[7] = n >= FIRST && n <= LAST ? 4 : 0;
        put16(p + 0x18, bend ? n * 0x100 : 0);
    }
    put16(s->header + 0x20, 0);
    record(s, 0, 1, 0, 0, 0, 5, 0);          /* the portal: model 0, a corner on the road's axis */
    record(s, 1, 1, 1, 0, 100, 0, 0);        /* a board on the same node */
    record(s, 2, 7, 2, 0, 0, 0, 0);          /* a model inside */
    put32(s->scenery + 3 * TRI_SCENERY_SIZE, 0xffffffffu);
}

static const TriPivot PIVOTS[2] = { { 192, 0 }, { 0, 0 } };

static void centre(const Tri *tri, int index, double out[2])
{
    TriScenery s;
    TriNode n;
    CHECK_EQ_INT(tri_scenery(tri, (uint32_t)index, &s), TRI_OK);
    CHECK_EQ_INT(tri_node(tri, (uint32_t)s.node, &n), TRI_OK);
    const double a = ((s.orientation * 0x10000 + (n.orientation & 0x3fff) * 0x400) / 16777216.0) * 2 * 3.14159265358979323846;
    out[0] = n.position.x / 256.0 + s.x + 16 * (cos(a) * PIVOTS[0].x), out[1] = n.position.z / 256.0 + s.z + 16 * (sin(a) * PIVOTS[0].x);
}

static void test_the_second_face(int bend)
{
    static Synthetic s;
    fill(&s, bend);
    write_file(&s);
    CHECK_EQ_INT(portal_faces_add(file, file_size, PIVOTS, 2), 1);
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, file, file_size), TRI_OK);
    CHECK_EQ_INT(tri.scenery_count, 4);
    int32_t last = -1;
    int copy = -1;
    for (uint32_t i = 0; i < tri.scenery_count; i++) {
        TriScenery r;
        CHECK_EQ_INT(tri_scenery(&tri, i, &r), TRI_OK);
        CHECK(r.node >= last);
        last = r.node;
        if (r.node == LAST + 1) copy = (int)i;
    }
    CHECK(copy >= 0);
    if (copy < 0) return;
    TriScenery c, o;
    CHECK_EQ_INT(tri_scenery(&tri, (uint32_t)copy, &c), TRI_OK);
    CHECK_EQ_INT(tri_scenery(&tri, 0, &o), TRI_OK);
    CHECK_EQ_INT(c.descriptor, 0);
    CHECK_EQ_INT(c.orientation, 128);
    CHECK_EQ_INT(c.y, o.y);
    /* the board and the inner model stayed single */
    int boards = 0, inner = 0;
    for (uint32_t i = 0; i < tri.scenery_count; i++) {
        TriScenery r;
        CHECK_EQ_INT(tri_scenery(&tri, i, &r), TRI_OK);
        boards += r.descriptor == 1, inner += r.descriptor == 2;
    }
    CHECK_EQ_INT(boards, 1);
    CHECK_EQ_INT(inner, 1);
    /* the centre across the road: the first one's offset from its node turned by what the heading changed between the nodes */
    double co[2], cc[2];
    centre(&tri, 0, co);
    centre(&tri, copy, cc);
    TriNode n0, n1;
    CHECK_EQ_INT(tri_node(&tri, 1, &n0), TRI_OK);
    CHECK_EQ_INT(tri_node(&tri, LAST + 1, &n1), TRI_OK);
    const double turn = ((n1.orientation - n0.orientation) & 0x3fff) / 16384.0 * 2 * 3.14159265358979323846;
    const double rx = co[0] - n0.position.x / 256.0, rz = co[1] - n0.position.z / 256.0;
    const double wx = cos(turn) * rx - sin(turn) * rz, wz = sin(turn) * rx + cos(turn) * rz;
    CHECK(fabs(cc[0] - n1.position.x / 256.0 - wx) < 40);
    CHECK(fabs(cc[1] - n1.position.z / 256.0 - wz) < 40);
    /* a second call adds nothing */
    CHECK_EQ_INT(portal_faces_add(file, file_size, PIVOTS, 2), 0);
    CHECK_EQ_INT(tri_open(&tri, file, file_size), TRI_OK);
    CHECK_EQ_INT(tri.scenery_count, 4);
}

static void test_a_mouth_that_has_the_model_is_left_alone(void)
{
    static Synthetic s;
    fill(&s, 0);
    record(&s, 3, LAST + 1, 0, 128, 0, 5, 0);
    put32(s.scenery + 4 * TRI_SCENERY_SIZE, 0xffffffffu);
    write_file(&s);
    CHECK_EQ_INT(portal_faces_add(file, file_size, PIVOTS, 2), 0);
}

static void test_a_portal_two_nodes_out_is_a_portal(void)
{
    static Synthetic s;   /* a reversed circuit's portal lands one node further from the run */
    fill(&s, 0);
    put32(s.scenery, LAST + 2);
    write_file(&s);
    CHECK_EQ_INT(portal_faces_add(file, file_size, PIVOTS, 2), 1);
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, file, file_size), TRI_OK);
    int at = 0;
    for (uint32_t i = 0; i < tri.scenery_count; i++) {
        TriScenery r;
        CHECK_EQ_INT(tri_scenery(&tri, i, &r), TRI_OK);
        at += r.node == FIRST - 2 && r.descriptor == 0 && r.orientation == 128;
    }
    CHECK_EQ_INT(at, 1);
}

static void test_no_tunnel_no_copy(void)
{
    static Synthetic s;
    fill(&s, 0);
    for (int n = 0; n < NODES; n++) s.header[NODE_AT + 36 * n + 7] = 0;
    write_file(&s);
    CHECK_EQ_INT(portal_faces_add(file, file_size, PIVOTS, 2), 0);
}

int main(void)
{
    test_the_second_face(0);
    test_the_second_face(1);
    test_a_mouth_that_has_the_model_is_left_alone();
    test_a_portal_two_nodes_out_is_a_portal();
    test_no_tunnel_no_copy();
    return TEST_REPORT();
}
