/* dep71 follow-up 2: a bitmap of the car's own (formats/car_texture_add.h). The name and label rules; a hand-made CFM: the bitmap after
 * the bank's last entry, a reference to it after the mesh's last with no face on it, every other entry, reference, polygon and chunk
 * as it was; the far level alone with level 1; the refusals. With the CD: the Viper with "badg" added near and far. */
#include "formats/car_texture_add.h"
#include "formats/cfm_build.h"
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "formats/mesh_build.h"
#include "formats/shpi_build.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_names(void)
{
    CHECK(car_texture_name_refused("badg") == NULL);
    CHECK(car_texture_name_refused("rr02") == NULL);
    const char *bad[] = { "bad", "badge", "Badg", "ba-g", "ba g", "!new", "tyr1", "tyr4", "shad", "circ", "topv", "rsid", "bott", "wing",
                          "frnt", "rear", "side", "back", "J000", "j001", "" };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        const char *why = car_texture_name_refused(bad[i]);
        CHECK(why != NULL && why[0] != '\0');
        if (why == NULL) printf("the name '%s' was taken\n", bad[i]);
    }
    CHECK(car_texture_label_refused("badge") == NULL);
    CHECK(car_texture_label_refused("rear_2") == NULL);
    const char *bad_labels[] = { "", "ninechars", "Badge", "a b", "frntires", "rertires", "tire1", "tire4", "pal" };
    for (size_t i = 0; i < sizeof bad_labels / sizeof *bad_labels; i++) CHECK(car_texture_label_refused(bad_labels[i]) != NULL);
}

/* ---- a hand-made CFM: !PAL and rsid; two faces on reference 0 (rsid) ---- */

static uint8_t *small_cfm(size_t *len)
{
    static uint8_t rsid[10 * 5], pal[768];
    memset(rsid, 3, sizeof rsid);
    for (int i = 0; i < 768; i++) pal[i] = (uint8_t)i;
    ShpiDescEntry e[2];
    memset(e, 0, sizeof e);
    memcpy(e[0].name, "!PAL", 4), e[0].type = 0x24, e[0].width = 256, e[0].height = 1, e[0].data = pal;
    memcpy(e[1].name, "rsid", 4), e[1].type = 0x7b, e[1].width = 10, e[1].height = 5, e[1].data = rsid, e[1].dir_index = 1;
    e[0].block_size = e[1].block_size = SHPI_BLOCK_AUTO;
    MeshDesc m;
    memset(&m, 0, sizeof m);
    memcpy(m.name, "_SMALL", 6);
    m.vertices = calloc(4, sizeof *m.vertices), m.vertex_count = m.real_vertices = 4;
    for (int i = 0; i < 4; i++) m.vertices[i] = (MeshVertex){ i, 2 * i, 3 * i };
    m.uvs = calloc(4, sizeof *m.uvs), m.uv_count = 4;
    m.textures = calloc(1, sizeof *m.textures), m.texture_count = 1;
    memcpy(m.textures[0].name, "rsid", 4), m.textures[0].unknown_1[1] = 0xef;
    m.polygons = calloc(2, sizeof *m.polygons), m.polygon_count = 2;
    for (int p = 0; p < 2; p++) {
        m.polygons[p].flags = 3, m.polygons[p].has_uv = (uint8_t)(p == 0);
        for (int c = 0; c < 3; c++) m.polygons[p].vertex[c] = (uint32_t)(c + p), m.polygons[p].uv[c] = p == 0 ? (uint32_t)c : 0;
    }
    m.groups = calloc(1, sizeof *m.groups), m.group_count = 1;
    memcpy(m.groups[0].name, "surface", 7);
    m.groups[0].polygons = calloc(2, sizeof(uint32_t)), m.groups[0].count = 2, m.groups[0].polygons[1] = 1;
    uint8_t *mesh = NULL, *bank = NULL, *cfm = NULL;
    size_t mesh_len = 0, bank_len = 0;
    ExchangeError err = { { 0 } };
    int ok = mesh_build(&m, &mesh, &mesh_len, &err) && shpi_build(e, 2, &bank, &bank_len, &err);
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

typedef struct {
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    MeshDesc mesh;
    ShpiDescEntry bank[260];
    size_t n;
} Level;

static int open_level(const uint8_t *cfm, size_t size, int level, Level *l)
{
    ExchangeError err = { { 0 } };
    Chunk c;
    Mesh m;
    memset(&l->mesh, 0, sizeof l->mesh);
    return cfm_describe(cfm, size, l->child, l->len, &err) && chunk_open(&c, l->child[2 * level], l->len[2 * level]) == CHUNK_OK &&
           mesh_open(&m, &c) == MESH_OK && mesh_describe(&m, &l->mesh, &err) &&
           shpi_describe(l->child[2 * level + 1], l->len[2 * level + 1], l->bank, 260, &l->n, &err);
}

static int same_chunk(const Level *a, const Level *b, int k) { return a->len[k] == b->len[k] && memcmp(a->child[k], b->child[k], a->len[k]) == 0; }

/* `after` is `before` with `name` (w x h, `px`) and a reference labelled `label` added at `level`; the other level untouched */
static void check_added(const uint8_t *before, size_t bn, const uint8_t *after, size_t an, int level, const char *name, const char *label,
                        const uint8_t *px, uint16_t w, uint16_t h, uint32_t ref)
{
    static Level was, now;
    CHECK(open_level(before, bn, level, &was) && open_level(after, an, level, &now));
    CHECK_EQ_INT((int)now.n, (int)was.n + 1);
    for (size_t i = 0; i < was.n && i < now.n; i++)   /* the bank's entries as they were */
        CHECK(memcmp(was.bank[i].name, now.bank[i].name, 4) == 0 && was.bank[i].type == now.bank[i].type &&
              was.bank[i].width == now.bank[i].width && was.bank[i].height == now.bank[i].height &&
              memcmp(was.bank[i].data, now.bank[i].data, was.bank[i].type == 0x7b ? (size_t)was.bank[i].width * was.bank[i].height : 768) == 0);
    const ShpiDescEntry *e = now.n > 0 ? &now.bank[now.n - 1] : NULL;
    CHECK(e != NULL && memcmp(e->name, name, 4) == 0 && e->type == 0x7b && e->width == w && e->height == h &&
          memcmp(e->data, px, (size_t)w * h) == 0 && e->dir_index == now.n - 1);
    CHECK_EQ_INT((int)now.mesh.texture_count, (int)was.mesh.texture_count + 1);
    CHECK_EQ_INT((int)ref, (int)was.mesh.texture_count);
    CHECK(memcmp(now.mesh.textures, was.mesh.textures, was.mesh.texture_count * sizeof *was.mesh.textures) == 0);
    const MeshTexture *t = &now.mesh.textures[ref];
    char want[8] = { 0 };
    memcpy(want, label, strlen(label));
    CHECK(memcmp(t->label, want, 8) == 0 && memcmp(t->name, name, 4) == 0);
    CHECK(was.mesh.texture_count == 0 || memcmp(t->unknown_1, was.mesh.textures[0].unknown_1, sizeof t->unknown_1) == 0 ||
          t->unknown_1[1] != 0);   /* a flat colour word from the mesh's own */
    CHECK(now.mesh.polygon_count == was.mesh.polygon_count && now.mesh.uv_count == was.mesh.uv_count &&
          now.mesh.vertex_count == was.mesh.vertex_count);
    int on_it = 0;
    for (uint32_t p = 0; p < now.mesh.polygon_count; p++) on_it += now.mesh.polygons[p].texture == ref;
    CHECK_EQ_INT(on_it, 0);
    const int other = 1 - level;   /* the other level's chunks as they were */
    CHECK(same_chunk(&was, &now, 2 * other) && same_chunk(&was, &now, 2 * other + 1));
    mesh_desc_free(&was.mesh), mesh_desc_free(&now.mesh);
}

static void test_small(void)
{
    size_t len = 0, n1 = 0, n2 = 0;
    uint8_t *cfm = small_cfm(&len), *out = NULL, *again = NULL;
    CHECK(cfm != NULL);
    if (cfm == NULL) return;
    static uint8_t px[6 * 4];
    for (int i = 0; i < 24; i++) px[i] = (uint8_t)(i * 7);
    uint32_t ref = 99;
    ExchangeError err = { { 0 } };
    CHECK(car_texture_add(cfm, len, 0, "badg", "badge", px, 6, 4, &out, &n1, &ref, &err));
    if (out != NULL) check_added(cfm, len, out, n1, 0, "badg", "badge", px, 6, 4, ref);
    /* a second one, on the far level */
    CHECK(out != NULL && car_texture_add(out, n1, 1, "rr02", "rr02", px, 4, 6, &again, &n2, &ref, &err));
    if (again != NULL) check_added(out, n1, again, n2, 1, "rr02", "rr02", px, 4, 6, ref);
    /* refusals: nothing made */
    uint8_t *no = NULL;
    size_t nn = 0;
    CHECK(!car_texture_add(out, n1, 0, "badg", "x", px, 6, 4, &no, &nn, &ref, &err) && no == NULL && strstr(err.message, "badg") != NULL);
    CHECK(!car_texture_add(cfm, len, 0, "rsid", "x", px, 6, 4, &no, &nn, &ref, &err) && no == NULL);
    CHECK(!car_texture_add(cfm, len, 0, "BAD!", "x", px, 6, 4, &no, &nn, &ref, &err) && no == NULL);
    CHECK(!car_texture_add(cfm, len, 0, "good", "tire1", px, 6, 4, &no, &nn, &ref, &err) && no == NULL);
    CHECK(!car_texture_add(cfm, len, 0, "good", "x", px, 0, 4, &no, &nn, &ref, &err) && no == NULL);
    CHECK(!car_texture_add(cfm, len, 0, "good", "x", px, CAR_TEXTURE_PNG_MAX_SIDE + 1, 4, &no, &nn, &ref, &err) && no == NULL);
    CHECK(!car_texture_add(cfm, len, 2, "good", "x", px, 6, 4, &no, &nn, &ref, &err) && no == NULL);
    free(cfm), free(out), free(again);
}

/* a bank of 256 entries takes no more (the mesh, 1 reference to start with, is not full yet) */
static void test_full(void)
{
    size_t len = 0, n = 0;
    uint8_t *cfm = small_cfm(&len), *cur = cfm, *next = NULL;
    static uint8_t px[4];
    ExchangeError err = { { 0 } };
    uint32_t ref = 0;
    int added = 0;
    char name[8];
    for (int i = 0; cur != NULL && i < 300; i++) {
        snprintf(name, sizeof name, "n%03d", i);
        if (!car_texture_add(cur, len, 0, name, name, px, 2, 2, &next, &n, &ref, &err)) break;
        if (cur != cfm) free(cur);
        cur = next, len = n, added++;
    }
    CHECK_EQ_INT(added, CAR_TEXTURE_ENTRIES_MAX - 2);   /* the bank fills first: !PAL, rsid and 254 of its own */
    CHECK(strstr(err.message, "full") != NULL && strstr(err.message, "256") != NULL);
    if (cur != cfm) free(cur);
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

static void test_viper(void)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/SIMDATA/CARFAMS/DVIPER.CFM", TNFS_SOURCE_DIR);
    size_t len = 0, n1 = 0, n2 = 0;
    uint8_t *cfm = read_all(path, &len), *near = NULL, *both = NULL;
    if (cfm == NULL) { printf("skip the Viper (no original/)\n"); return; }
    static uint8_t px[64 * 32];
    for (int i = 0; i < 64 * 32; i++) px[i] = (uint8_t)(i % 0xfa);
    uint32_t ref = 0;
    ExchangeError err = { { 0 } };
    CHECK(car_texture_add(cfm, len, 0, "badg", "badge", px, 64, 32, &near, &n1, &ref, &err));
    if (near != NULL) check_added(cfm, len, near, n1, 0, "badg", "badge", px, 64, 32, ref);
    CHECK(near != NULL && car_texture_add(near, n1, 1, "badg", "badge", px, 32, 16, &both, &n2, &ref, &err));
    if (both != NULL) check_added(near, n1, both, n2, 1, "badg", "badge", px, 32, 16, ref);
    if (near == NULL || both == NULL) printf("viper: %s\n", err.message);
    free(cfm), free(near), free(both);
}

int main(void)
{
    test_names();
    test_small();
    test_full();
    test_viper();
    return TEST_REPORT();
}
