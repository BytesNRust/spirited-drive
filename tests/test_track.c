/* Race track loaders over the CD data: segment decoding (0043dfd0) and the
 * terrain texture table (0044c36c). Argument: the CD's SIMDATA directory. */
#include "formats/chunk.h"
#include "formats/tri.h"
#include "game/gfx/object3d.h"
#include "game/race/model_draw.h"
#include "game/race/track_art.h"
#include "game/race/track_geometry.h"
#include "game/race/track_info.h"
#include "game/race/track_objects.h"
#include "game/sys/fixed_math.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static uint8_t *read_file(const char *dir, const char *name, size_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return NULL;
    uint8_t *data = NULL;
    if (fseek(fp, 0, SEEK_END) == 0) {
        const long n = ftell(fp);
        data = n > 0 ? malloc((size_t)n) : NULL;
        if (data != NULL && (fseek(fp, 0, SEEK_SET) != 0 || fread(data, 1, (size_t)n, fp) != (size_t)n)) {
            free(data);
            data = NULL;
        }
        *size = (size_t)n;
    }
    fclose(fp);
    return data;
}

static int same_point(TrackPoint p, int32_t x, int32_t y, int32_t z)
{
    return p.x == x && p.y == y && p.z == z;
}

/* Every point is the node plus the chain of deltas from the centre, << 9. */
static void check_segment(const Tri *tri, uint32_t s)
{
    TrackSegment seg;
    TriSegment raw;
    CHECK_EQ_INT(track_decode_segment(tri, s, &seg), TRI_OK);
    CHECK_EQ_INT(tri_segment(tri, s, &raw), TRI_OK);
    CHECK_MEM(seg.texture, raw.texture, sizeof seg.texture);
    for (int r = 0; r < TRACK_SEGMENT_ROWS; r++) {
        TriNode n;
        CHECK_EQ_INT(tri_node(tri, s * 4 + (uint32_t)r, &n), TRI_OK);
        const TriRow *row = &raw.rows[r];
        int32_t x = n.position.x + row->middle.x * 512, y = n.position.y + row->middle.y * 512;
        int32_t z = n.position.z + row->middle.z * 512;
        CHECK(same_point(seg.rows[r][0], x, y, z));
        int32_t rx = x, ry = y, rz = z, lx = x, ly = y, lz = z;
        for (int i = 0; i < 5; i++) {
            rx += row->right[i].x * 512, ry += row->right[i].y * 512, rz += row->right[i].z * 512;
            lx += row->left[i].x * 512, ly += row->left[i].y * 512, lz += row->left[i].z * 512;
        }
        CHECK(same_point(seg.rows[r][5], rx, ry, rz));
        CHECK(same_point(seg.rows[r][10], lx, ly, lz));
    }
}

static void test_geometry(const char *dir)
{
    size_t size = 0;
    Tri tri;
    uint8_t *circuit = read_file(dir, "MISC/TR1.TRI", &size);
    if (circuit != NULL && tri_open(&tri, circuit, size) == TRI_OK) {
        check_segment(&tri, 3);
        TrackSegment a, b;
        CHECK_EQ_INT(track_decode_segment(&tri, tri.segment_count, &a), TRI_OK);  /* wraps on a circuit */
        CHECK_EQ_INT(track_decode_segment(&tri, 0, &b), TRI_OK);
        CHECK(same_point(a.rows[1][4], b.rows[1][4].x, b.rows[1][4].y, b.rows[1][4].z));
    } else {
        CHECK(!"TR1.TRI");
    }
    free(circuit);

    uint8_t *stage = read_file(dir, "MISC/CY1.TRI", &size);
    if (stage != NULL && tri_open(&tri, stage, size) == TRI_OK) {
        check_segment(&tri, 0);
        check_segment(&tri, tri.segment_count - 1u);
        TrackSegment past;
        CHECK(track_decode_segment(&tri, tri.segment_count, &past) != TRI_OK);  /* a stage has an end */
    } else {
        CHECK(!"CY1.TRI");
    }
    free(stage);
}

static void test_art(const char *dir)
{
    size_t size = 0;
    Chunk root;
    TrackArt *art = malloc(sizeof *art);
    uint8_t *circuit = read_file(dir, "NTRACKFM/TR1_T01.FAM", &size);
    if (art != NULL && circuit != NULL && chunk_open(&root, circuit, size) == CHUNK_OK) {
        CHECK(track_art_load(art, &root));
        CHECK(art->has_palette && art->has_horizon);
        CHECK(!art->slots[0][0].present);                      /* "00A0" does not exist */
        const TrackTexture *a = art->slots[1 * 3 + 0];          /* "01A0" */
        CHECK(a[0].present);
        CHECK(a[3].bitmap.payload == a[0].bitmap.payload);      /* "01A1".."01A3" reuse "01A0" */
        CHECK(a[4].present && a[4].bitmap.payload != a[0].bitmap.payload);  /* "01A4" */
        CHECK_EQ_INT(art->horizon.width, 960);
    } else {
        CHECK(!"TR1_T01.FAM");
    }
    free(circuit);

    uint8_t *stage = read_file(dir, "NTRACKFM/CY1_T01.FAM", &size);
    if (art != NULL && stage != NULL && chunk_open(&root, stage, size) == CHUNK_OK) {
        CHECK(track_art_load(art, &root));
        CHECK(art->has_palette);
        CHECK(art->slots[0][0].present);                        /* "A000" of the first SHPI */
    } else {
        CHECK(!"CY1_T01.FAM");
    }
    free(stage);
    free(art);
}

static void test_fixed_math(void)
{
    CHECK_EQ_INT(fx_sin(0), 0);
    CHECK_EQ_INT(fx_sin(0x100), 0x10000);      /* quarter turn */
    CHECK_EQ_INT(fx_sin(0x200), 0);
    CHECK_EQ_INT(fx_sin(0x300), -0x10000);
    CHECK_EQ_INT(fx_cos(0), 0x10000);
    CHECK_EQ_INT(fx_sin(8), 3215);              /* the table truncates: 3216 when rounded */
    CHECK_EQ_INT(fx_sin(0x100 + 8), fx_sin(0x100 - 8));
    CHECK_EQ_INT(fx_sin(0x400 + 5), fx_sin(5)); /* wraps per turn */
}

/* TR1 record 1: a two-sided billboard (descriptor 6, 10 x 5 m, side 7 m) at
 * node 1, offset (8543, 20, 209) << 8. */
static void test_objects(const char *dir)
{
    size_t size = 0;
    Tri tri;
    uint8_t *data = read_file(dir, "MISC/TR1.TRI", &size);
    if (data == NULL || tri_open(&tri, data, size) != TRI_OK) {
        CHECK(!"TR1.TRI");
        free(data);
        return;
    }
    TrackObject o;
    TriNode n;
    CHECK_EQ_INT(track_object(&tri, 1, &o), TRI_OK);
    CHECK_EQ_INT(tri_node(&tri, 1, &n), TRI_OK);
    CHECK_EQ_INT(o.node, 1);
    CHECK_EQ_INT(o.descriptor.type, TRACK_OBJECT_TWO_SIDED);
    CHECK_EQ_INT(o.corner_count, 6);
    CHECK(same_point(o.origin, n.position.x + 8543 * 256, n.position.y + 20 * 256, n.position.z + 209 * 256));
    CHECK_EQ_INT(o.corners[0].y - o.corners[3].y, 5 * 65536);  /* height */
    CHECK_EQ_INT(o.corners[2].y, o.origin.y);                  /* bottom on the origin */
    /* the bottom edge is centred on the origin */
    CHECK_EQ_INT(o.corners[2].x + o.corners[3].x, 2 * o.origin.x);
    CHECK_EQ_INT(o.corners[2].z + o.corners[3].z, 2 * o.origin.z);
    CHECK_EQ_INT(o.corners[4].y - o.corners[5].y, 5 * 65536);
    /* the side face starts at the bottom-right corner, at a right angle */
    {
        const long long fx = o.corners[2].x - o.corners[3].x, fz = o.corners[2].z - o.corners[3].z;
        const long long sx = o.corners[5].x - o.corners[2].x, sz = o.corners[5].z - o.corners[2].z;
        CHECK(llabs(fx * sx + fz * sz) < (llabs(fx) + llabs(fz)) * 2048);
        CHECK(sx != 0 || sz != 0);
    }

    CHECK_EQ_INT(track_object(&tri, 0, &o), TRI_OK);            /* record 0: a 3D object */
    CHECK_EQ_INT(o.descriptor.type, TRACK_OBJECT_MODEL);
    CHECK_EQ_INT(o.corner_count, 0);
    CHECK(track_object(&tri, tri.scenery_count, &o) != TRI_OK);
    free(data);
}

static void test_info(const char *dir)
{
    CHECK_EQ_INT(track_info_decimal("-45"), -45 * 65536);
    CHECK_EQ_INT(track_info_decimal(" 0.25"), 16384);
    CHECK_EQ_INT(track_info_decimal("1.5"), 98304);
    CHECK_EQ_INT(track_info_decimal("-0.5"), -32768);
    CHECK_EQ_INT(track_info_decimal("0.0001"), 6);       /* 65536 / 10000, truncated */

    size_t size = 0;
    TrackInfo info;
    char *text = (char *)read_file(dir, "MISC/TR1INFO", &size);
    CHECK(text != NULL && track_info_parse(&info, text, size));
    if (text != NULL) {
        CHECK_EQ_INT(info.height_far, 48 * 65536);
        CHECK_EQ_INT(info.radius_far, 96 * 65536);
        CHECK_EQ_INT(info.colour_top, (0 << 10) | (1 << 5) | 2);
        CHECK_EQ_INT(info.ring, 1);
        CHECK_EQ_INT(info.ring_y_offset, -45);
        CHECK_EQ_INT(info.ring_height, 108);
        CHECK_EQ_INT(info.time_of_day[0][0], 292);
        CHECK_EQ_INT(info.time_of_day[0][4], 255 * 256);
    }
    free(text);
    text = (char *)read_file(dir, "MISC/AL1INFO", &size);
    CHECK(text != NULL && track_info_parse(&info, text, size));
    if (text != NULL) CHECK_EQ_INT(info.ring, 0);
    free(text);
}

typedef struct {
    int textured, flat;
    int32_t first[8];
} Drawn;

static void on_textured(void *user, Object3DTable table, int mode, const FshEntry *texture, const Object3DCorners *c)
{
    Drawn *d = user;
    (void)texture;
    if (d->textured++ == 0) memcpy(d->first, c->screen, sizeof d->first);
    CHECK_EQ_INT((int)table * 3 + mode, 10);
}

static void on_flat(void *user, uint32_t rgb, const Object3DCorners *c)
{
    (void)rgb, (void)c;
    ((Drawn *)user)->flat++;
}

/* TR1 model 0 (three textured quads) 50 m straight ahead, as the original
 * draws it (tools/oracle_object3d.py checks the general case). */
static void test_object3d(const char *dir)
{
    size_t size = 0;
    uint8_t *data = read_file(dir, "NTRACKFM/TR1_T01.FAM", &size);
    Chunk root, objects, pair, orip, shpi;
    Mesh mesh;
    Fsh fsh;
    if (data == NULL || chunk_open(&root, data, size) != CHUNK_OK ||
        chunk_child(&root, FAM_CHILD_OBJECTS, &objects) != CHUNK_OK || chunk_child(&objects, 0, &pair) != CHUNK_OK ||
        chunk_child(&pair, 0, &orip) != CHUNK_OK || mesh_open(&mesh, &orip) != MESH_OK) {
        CHECK(!"TR1_T01.FAM model 0");
        free(data);
        return;
    }
    Object3D obj;
    CHECK(object3d_create(&obj, &mesh));
    CHECK(chunk_child(&pair, 1, &shpi) == CHUNK_OK && chunk_bitmaps(&shpi, &fsh) == FSH_OK);
    CHECK_EQ_INT(object3d_bind(&obj, &fsh), 3);
    static View view;
    view_init(&view, 0, 0, 640, 480, 96, 96);
    const int32_t cam[3] = { 0, 0, 0 }, pos[3] = { 0, 0, 50 * 65536 };
    const int32_t m[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };
    const ModelView mv = { &view, cam, m, 0 };
    Drawn d = { 0 };
    const Object3DSink sink = { &d, on_textured, on_flat };
    CHECK_EQ_INT(model_draw_object(&obj, pos, 0, &mv, &sink), 1);
    CHECK_EQ_INT(d.textured, 3);
    CHECK_EQ_INT(d.flat, 0);
    static const int32_t first[8] = { 303, 73, 320, 66, 320, 240, 303, 240 };
    CHECK_MEM(d.first, first, sizeof first);
    const int32_t behind[3] = { 0, 0, -50 * 65536 };
    CHECK_EQ_INT(model_draw_object(&obj, behind, 0, &mv, &sink), 0);
    object3d_free(&obj);
    free(data);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s SIMDATA_DIR\n", argv[0]);
        return 1;
    }
    test_fixed_math();
    test_geometry(argv[1]);
    test_objects(argv[1]);
    test_info(argv[1]);
    test_art(argv[1]);
    test_object3d(argv[1]);
    return TEST_REPORT();
}
