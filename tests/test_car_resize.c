/* dep71 follow-up: a car's bitmap resized and its uvs following (formats/car_resize.h). A hand-made mesh: the bitmap's uvs scaled and
 * rounded, a uv shared with another bitmap's face split, a face with no uvs and another bitmap's faces untouched, tyr1 through a
 * nameless tyre reference; a hand-made CFM resized end to end; the refusals. With the CD: the Viper's rsid at twice its size. */
#include "formats/car_resize.h"
#include "formats/cfm_build.h"
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "formats/shpi_build.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_scale(void)
{
    CHECK_EQ_INT(car_resize_scale(0, 100, 200), 0);
    CHECK_EQ_INT(car_resize_scale(100, 100, 200), 200);
    CHECK_EQ_INT(car_resize_scale(17, 50, 75), 26);    /* 25.5 rounds up */
    CHECK_EQ_INT(car_resize_scale(101, 50, 75), 152);  /* 151.5 */
    CHECK_EQ_INT(car_resize_scale(-5, 100, 200), -10);
    CHECK_EQ_INT(car_resize_scale(-1, 2, 3), -2);      /* -1.5 rounds away from zero */
    CHECK_EQ_INT(car_resize_scale(268, 268, 536), 536);
    CHECK_EQ_INT(car_resize_scale(536, 536, 268), 268);
}

static void test_pixels(void)
{
    const uint8_t src[2 * 2] = { 1, 2, 3, 4 };
    uint8_t dst[4 * 4];
    car_resize_pixels(src, 2, 2, dst, 4, 4);
    const uint8_t want[16] = { 1, 1, 2, 2, 1, 1, 2, 2, 3, 3, 4, 4, 3, 3, 4, 4 };
    CHECK_MEM(dst, want, 16);
    uint8_t half[1];
    car_resize_pixels(src, 2, 2, half, 1, 1);
    CHECK_EQ_INT(half[0], 1);
}

static MeshTexture tex(const char *label, const char *name)
{
    MeshTexture t;
    memset(&t, 0, sizeof t);
    memcpy(t.label, label, strlen(label) < 8 ? strlen(label) : 8);
    if (name != NULL) memcpy(t.name, name, 4);
    return t;
}

static MeshDescPolygon poly(uint8_t texture, int corners, int has_uv, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    MeshDescPolygon p;
    memset(&p, 0, sizeof p);
    p.flags = (uint8_t)corners, p.texture = texture, p.has_uv = (uint8_t)has_uv;
    const uint32_t k[4] = { a, b, c, d };
    for (int i = 0; i < 4; i++) p.vertex[i] = k[i] % 4, p.uv[i] = has_uv ? k[i] : 0;
    if (corners == 3) p.vertex[3] = 0, p.uv[3] = 0;
    return p;
}

/* rsid 100 x 50 through references 0 and 3, door through 1, tyr1 through the nameless frntires (2) */
static void small_mesh(MeshDesc *m)
{
    memset(m, 0, sizeof *m);
    static const MeshUv uvs[6] = { { 0, 0 }, { 100, 0 }, { 100, 50 }, { 0, 50 }, { 33, 17 }, { -5, 101 } };
    m->uvs = malloc(sizeof uvs), memcpy(m->uvs, uvs, sizeof uvs), m->uv_count = 6;
    m->vertices = calloc(4, sizeof *m->vertices), m->vertex_count = 4, m->real_vertices = 4;
    for (int i = 0; i < 4; i++) m->vertices[i] = (MeshVertex){ i, i * 2, i * 3 };
    m->textures = malloc(4 * sizeof *m->textures), m->texture_count = 4;
    m->textures[0] = tex("body", "rsid"), m->textures[1] = tex("side", "door"), m->textures[2] = tex("frntires", NULL),
    m->textures[3] = tex("x", "rsid");
    m->polygons = malloc(5 * sizeof *m->polygons), m->polygon_count = 5;
    m->polygons[0] = poly(0, 4, 1, 0, 1, 2, 3);
    m->polygons[1] = poly(1, 3, 1, 4, 1, 2, 0);
    m->polygons[2] = poly(0, 4, 0, 0, 1, 2, 3);
    m->polygons[3] = poly(3, 3, 1, 4, 5, 0, 0);
    m->polygons[4] = poly(2, 3, 1, 0, 1, 2, 0);
}

static MeshUv uv_of(const MeshDesc *m, int polygon, int corner) { return m->uvs[m->polygons[polygon].uv[corner]]; }

static void test_uvs(void)
{
    MeshDesc m;
    small_mesh(&m);
    CarResizeStats st;
    memset(&st, 0xee, sizeof st);
    ExchangeError err = { { 0 } };
    CHECK(car_resize_uvs(&m, "rsid", 100, 50, 200, 75, &st, &err));
    CHECK_EQ_INT((int)st.faces, 2);
    CHECK_EQ_INT((int)st.uvs_scaled, 6);
    CHECK_EQ_INT((int)st.uvs_split, 4);   /* 0, 1, 2 (the tyre's and the door's) and 4 (the door's) */
    CHECK_EQ_INT((int)m.uv_count, 10);
    /* rsid's faces: scaled */
    const MeshUv want0[4] = { { 0, 0 }, { 200, 0 }, { 200, 75 }, { 0, 75 } };
    for (int c = 0; c < 4; c++) CHECK(uv_of(&m, 0, c).u == want0[c].u && uv_of(&m, 0, c).v == want0[c].v);
    CHECK(uv_of(&m, 3, 0).u == 66 && uv_of(&m, 3, 0).v == 26);
    CHECK(uv_of(&m, 3, 1).u == -10 && uv_of(&m, 3, 1).v == 152);
    CHECK(uv_of(&m, 3, 2).u == 0 && uv_of(&m, 3, 2).v == 0);
    CHECK(m.polygons[0].uv[0] == m.polygons[3].uv[2]);   /* one copy for both of rsid's faces */
    /* the door's and the tyre's: their records, as they were */
    CHECK(m.polygons[1].uv[0] == 4 && m.polygons[1].uv[1] == 1 && m.polygons[1].uv[2] == 2);
    CHECK(uv_of(&m, 1, 0).u == 33 && uv_of(&m, 1, 0).v == 17 && uv_of(&m, 1, 1).u == 100 && uv_of(&m, 1, 2).v == 50);
    CHECK(m.polygons[4].uv[0] == 0 && uv_of(&m, 4, 1).u == 100 && uv_of(&m, 4, 2).v == 50);
    /* the face with no uvs: none */
    CHECK(m.polygons[2].has_uv == 0 && m.polygons[2].uv[0] == 0);
    mesh_desc_free(&m);

    /* tyr1, through the nameless tyre reference: only the tyre's face */
    small_mesh(&m);
    CHECK(car_resize_uvs(&m, "tyr1", 100, 50, 50, 50, &st, &err));
    CHECK_EQ_INT((int)st.faces, 1);
    CHECK(uv_of(&m, 4, 1).u == 50 && uv_of(&m, 4, 2).u == 50 && uv_of(&m, 4, 2).v == 50);
    CHECK(uv_of(&m, 0, 1).u == 100 && uv_of(&m, 1, 1).u == 100);
    mesh_desc_free(&m);

    /* a bitmap no face uses: nothing to do, not an error */
    small_mesh(&m);
    CHECK(car_resize_uvs(&m, "none", 100, 50, 200, 100, &st, &err));
    CHECK(st.faces == 0 && st.uvs_scaled == 0 && m.uv_count == 6);
    mesh_desc_free(&m);
}

/* ---- a whole CFM ---- */

static void group_all(MeshDesc *m)
{
    m->groups = calloc(1, sizeof *m->groups), m->group_count = 1;
    memcpy(m->groups[0].name, "surface", 7);
    m->groups[0].polygons = malloc(m->polygon_count * sizeof(uint32_t)), m->groups[0].count = m->polygon_count;
    for (uint32_t i = 0; i < m->polygon_count; i++) m->groups[0].polygons[i] = i;
}

static uint8_t *small_cfm(size_t *len)
{
    static uint8_t rsid[100 * 50], door[8 * 8], tyre[16 * 16], pal[768];
    for (int i = 0; i < 100 * 50; i++) rsid[i] = (uint8_t)(i % 100 + i / 100);
    memset(door, 9, sizeof door), memset(tyre, 7, sizeof tyre);
    ShpiDescEntry e[4];
    memset(e, 0, sizeof e);
    const char *names[4] = { "!PAL", "rsid", "door", "tyr1" };
    const uint8_t *data[4] = { pal, rsid, door, tyre };
    const uint16_t w[4] = { 256, 100, 8, 16 }, h[4] = { 1, 50, 8, 16 };
    for (int i = 0; i < 4; i++) {
        memcpy(e[i].name, names[i], 4);
        e[i].type = i == 0 ? 0x24 : 0x7b, e[i].width = w[i], e[i].height = h[i], e[i].data = data[i];
        e[i].block_size = SHPI_BLOCK_AUTO, e[i].dir_index = (uint32_t)i;
    }
    MeshDesc m;
    small_mesh(&m);
    group_all(&m);
    memcpy(m.name, "_SMALL", 6);
    uint8_t *mesh = NULL, *bank = NULL, *cfm = NULL;
    size_t mesh_len = 0, bank_len = 0;
    ExchangeError err = { { 0 } };
    const ShpiHead head = { { 'A', 'B', 'C', 'D' }, 0, 0 };   /* a tag of its own: a resized texture keeps it */
    int ok = mesh_build(&m, &mesh, &mesh_len, &err) && shpi_build_head(&head, e, 4, &bank, &bank_len, &err);
    if (ok) {
        const uint8_t *const child[CFM_CHILDREN] = { mesh, bank, mesh, bank };
        const size_t clen[CFM_CHILDREN] = { mesh_len, bank_len, mesh_len, bank_len };
        ok = cfm_build(child, clen, &cfm, len, &err);
    }
    if (!ok) fprintf(stderr, "small cfm: %s\n", err.message);
    mesh_desc_free(&m);
    free(mesh), free(bank);
    return cfm;
}

static int level_of(const uint8_t *cfm, size_t len, int level, MeshDesc *m, ShpiDescEntry *e, size_t cap, size_t *n, const uint8_t **child,
                    size_t *clen)
{
    ExchangeError err = { { 0 } };
    Chunk c;
    Mesh mesh;
    return cfm_describe(cfm, len, child, clen, &err) && chunk_open(&c, child[2 * level], clen[2 * level]) == CHUNK_OK &&
           mesh_open(&mesh, &c) == MESH_OK && mesh_describe(&mesh, m, &err) && shpi_describe(child[2 * level + 1], clen[2 * level + 1], e, cap, n, &err);
}

static const ShpiDescEntry *find(const ShpiDescEntry *e, size_t n, const char *name)
{
    for (size_t i = 0; i < n; i++)
        if (memcmp(e[i].name, name, 4) == 0) return &e[i];
    return NULL;
}

static void test_cfm(void)
{
    size_t len = 0, out_len = 0;
    uint8_t *cfm = small_cfm(&len), *out = NULL;
    CHECK(cfm != NULL);
    if (cfm == NULL) return;
    CarResizeStats st;
    ExchangeError err = { { 0 } };
    CHECK(car_resize(cfm, len, 0, "rsid", 200, 75, NULL, &out, &out_len, &st, &err));
    CHECK(st.was_w == 100 && st.was_h == 50 && st.faces == 2 && st.uvs_split == 4);
    MeshDesc m;
    ShpiDescEntry e[8];
    size_t n = 0, clen[CFM_CHILDREN];
    const uint8_t *child[CFM_CHILDREN], *was[CFM_CHILDREN];
    size_t was_len[CFM_CHILDREN];
    CHECK(out != NULL && level_of(out, out_len, 0, &m, e, 8, &n, child, clen));
    CHECK(cfm_describe(cfm, len, was, was_len, &err));
    ShpiHead kept;
    CHECK(out != NULL && shpi_describe_head(child[1], clen[1], &kept) && memcmp(kept.tag, "ABCD", 4) == 0);
    if (out != NULL && n == 4) {
        const ShpiDescEntry *r = find(e, n, "rsid"), *d = find(e, n, "door");
        CHECK(r != NULL && r->width == 200 && r->height == 75);
        CHECK(r != NULL && r->data[0] == 0 && r->data[2] == 1 && r->data[3 * 200 + 2] == 3);   /* (1, 2) of the old: 1 + 2 */
        CHECK(d != NULL && d->width == 8 && d->data[0] == 9);
        CHECK(m.uv_count == 10 && uv_of(&m, 0, 2).u == 200 && uv_of(&m, 0, 2).v == 75);
        CHECK(clen[2] == was_len[2] && memcmp(child[2], was[2], clen[2]) == 0);   /* the far model's, as it was */
        CHECK(clen[3] == was_len[3] && memcmp(child[3], was[3], clen[3]) == 0);
        mesh_desc_free(&m);
    }
    free(out), out = NULL;

    /* given pixels */
    static uint8_t px[30 * 20];
    memset(px, 42, sizeof px);
    CHECK(car_resize(cfm, len, 1, "rsid", 30, 20, px, &out, &out_len, &st, &err));
    if (out != NULL && level_of(out, out_len, 1, &m, e, 8, &n, child, clen)) {
        const ShpiDescEntry *r = find(e, n, "rsid");
        CHECK(r != NULL && r->width == 30 && r->height == 20 && r->data[599] == 42);
        CHECK(uv_of(&m, 0, 2).u == 30 && uv_of(&m, 0, 2).v == 20);
        mesh_desc_free(&m);
        CHECK(level_of(out, out_len, 0, &m, e, 8, &n, child, clen) && find(e, n, "rsid")->width == 100);   /* the high level kept */
        mesh_desc_free(&m);
    }
    free(out), out = NULL;

    /* to the size it has, twice: the same bytes (the bank's length need not be a multiple of 4: the CFM's padding stays out of it) */
    uint8_t *once = NULL, *twice = NULL;
    size_t n1 = 0, n2 = 0;
    CHECK(car_resize(cfm, len, 0, "rsid", 101, 51, NULL, &once, &n1, &st, &err));
    CHECK(once != NULL && car_resize(once, n1, 0, "rsid", 101, 51, NULL, &twice, &n2, &st, &err));
    CHECK(twice != NULL && n2 == n1 && memcmp(once, twice, n1) == 0);
    free(once), free(twice);

    /* refusals: no such bitmap, a palette, a side of 0 or past the limit, no such level */
    CHECK(!car_resize(cfm, len, 0, "zzzz", 10, 10, NULL, &out, &out_len, &st, &err) && out == NULL && strstr(err.message, "zzzz") != NULL);
    CHECK(!car_resize(cfm, len, 0, "!PAL", 10, 10, NULL, &out, &out_len, &st, &err) && out == NULL);
    CHECK(!car_resize(cfm, len, 0, "rsid", 0, 10, NULL, &out, &out_len, &st, &err) && out == NULL);
    CHECK(!car_resize(cfm, len, 0, "rsid", 10, SHPI_MAX_SIDE + 1, NULL, &out, &out_len, &st, &err) && out == NULL);
    CHECK(!car_resize(cfm, len, 2, "rsid", 10, 10, NULL, &out, &out_len, &st, &err) && out == NULL);
    CHECK(!car_resize(cfm, 16, 0, "rsid", 10, 10, NULL, &out, &out_len, &st, &err) && out == NULL);
    free(cfm);
}

/* ---- the Viper ---- */

static uint8_t *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = (size_t)n;
    return d;
}

static int shows(const MeshDesc *m, const MeshDescPolygon *p, const char *name)
{
    return p->texture < m->texture_count && memcmp(m->textures[p->texture].name, name, 4) == 0;
}

static void test_viper(void)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/SIMDATA/CARFAMS/DVIPER.CFM", TNFS_SOURCE_DIR);
    size_t len = 0, out_len = 0;
    uint8_t *cfm = read_all(path, &len), *out = NULL;
    if (cfm == NULL) { printf("skip the Viper (no original/)\n"); return; }
    static ShpiDescEntry be[256], ae[256];
    size_t bn = 0, an = 0, bl[CFM_CHILDREN], al[CFM_CHILDREN];
    const uint8_t *bc[CFM_CHILDREN], *ac[CFM_CHILDREN];
    MeshDesc before, after;
    CHECK(level_of(cfm, len, 0, &before, be, 256, &bn, bc, bl));
    const ShpiDescEntry *r = find(be, bn, "rsid");
    CHECK(r != NULL);
    if (r == NULL) { free(cfm); return; }
    const uint16_t w = r->width, h = r->height;
    CarResizeStats st;
    ExchangeError err = { { 0 } };
    CHECK(car_resize(cfm, len, 0, "rsid", (uint16_t)(2 * w), (uint16_t)(2 * h), NULL, &out, &out_len, &st, &err));
    if (out == NULL) { printf("viper: %s\n", err.message); mesh_desc_free(&before); free(cfm); return; }
    CHECK(level_of(out, out_len, 0, &after, ae, 256, &an, ac, al));
    CHECK(st.was_w == w && st.was_h == h && st.faces > 0 && an == bn);
    CHECK(after.polygon_count == before.polygon_count);
    int wrong = 0, faces = 0;
    for (uint32_t i = 0; i < before.polygon_count && i < after.polygon_count; i++) {
        const MeshDescPolygon *p = &before.polygons[i], *q = &after.polygons[i];
        if (!p->has_uv) { wrong += q->has_uv; continue; }
        const int mine = shows(&before, p, "rsid");
        faces += mine;
        for (int c = 0; c < (p->flags & 7); c++) {
            const MeshUv a = before.uvs[p->uv[c]], b = after.uvs[q->uv[c]];
            wrong += mine ? (b.u != 2 * a.u || b.v != 2 * a.v) : (b.u != a.u || b.v != a.v);
        }
    }
    CHECK_EQ_INT(wrong, 0);
    CHECK_EQ_INT(faces, (int)st.faces);
    const ShpiDescEntry *n2 = find(ae, an, "rsid");
    int px = 0;
    for (int y = 0; n2 != NULL && y < h; y++)
        for (int x = 0; x < w; x++) px += n2->data[(2 * y + 1) * 2 * w + 2 * x + 1] != r->data[y * w + x];
    CHECK(n2 != NULL && n2->width == 2 * w && n2->height == 2 * h && px == 0);
    for (size_t i = 0; i < bn && i < an; i++)   /* the others as they were */
        if (memcmp(be[i].name, "rsid", 4) != 0)
            CHECK(be[i].width == ae[i].width && be[i].height == ae[i].height &&
                  memcmp(be[i].data, ae[i].data, be[i].type == 0x7b ? (size_t)be[i].width * be[i].height : 768) == 0);
    for (int k = 2; k < CFM_CHILDREN; k++) CHECK(bl[k] == al[k] && memcmp(bc[k], ac[k], bl[k]) == 0);
    printf("the Viper's rsid %dx%d -> %dx%d: %u faces, %u uvs, %u split\n", w, h, 2 * w, 2 * h, st.faces, st.uvs_scaled, st.uvs_split);
    mesh_desc_free(&before), mesh_desc_free(&after);
    free(out), free(cfm);
}

int main(void)
{
    test_scale();
    test_pixels();
    test_uvs();
    test_cfm();
    test_viper();
    return TEST_REPORT();
}
