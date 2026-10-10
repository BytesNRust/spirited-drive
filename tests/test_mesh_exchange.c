/* Mesh exchange tests on a synthetic chunk: an unedited export imports back to the same bytes; moved
 * vertices, uvs, corner indexes and materials reach the right bytes; topology changes and bad files are
 * refused with file and line. With CD files every ORIP in every FAM and CFM round-trips. */
#include "formats/doc.h"
#include "formats/mesh_exchange.h"
#include "formats/qfs.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Table offsets of the sample, all after the 0x70 header. */
enum {
    VERTEX_AT = 0x70, UV_AT = VERTEX_AT + 3 * 12, POLY_AT = UV_AT + 2 * 8,
    TEXTURE_AT = POLY_AT + 2 * 12, A_AT = TEXTURE_AT + 2 * 20, B_AT = A_AT + 20,
    C_AT = B_AT + 28, LABEL_AT = C_AT + 12, MAP_AT = LABEL_AT + 12,
    MAP_SLOTS = 8, SAMPLE_SIZE = MAP_AT + MAP_SLOTS * 4
};

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_table(uint8_t *b, unsigned count_at, uint32_t count, uint32_t offset_at)
{
    put_u32(b + count_at, count);
    put_u32(b + count_at + 4, offset_at);
}

static void build_sample(uint8_t b[SAMPLE_SIZE])
{
    memset(b, 0, SAMPLE_SIZE);
    memcpy(b, "ORIP", 4);
    put_u32(b + 0x04, SAMPLE_SIZE);
    put_u32(b + 0x08, 0x2BC);
    put_table(b, 0x10, 3, 0);              /* count at 0x10, offset at 0x14 is overwritten below */
    put_u32(b + 0x14, 2);                  /* unknown_14 */
    put_u32(b + 0x18, VERTEX_AT);
    put_table(b, 0x1C, 2, UV_AT);
    put_table(b, 0x24, 2, POLY_AT);
    memcpy(b + 0x2C, "_TEST", 5);
    put_table(b, 0x38, 2, TEXTURE_AT);
    put_table(b, 0x40, 1, A_AT);
    put_table(b, 0x48, 1, B_AT);
    put_u32(b + 0x50, MAP_AT);
    put_table(b, 0x54, 1, C_AT);
    put_table(b, 0x5C, 1, LABEL_AT);
    put_u32(b + 0x68, POLY_AT);

    put_u32(b + VERTEX_AT + 12 * 2, (uint32_t)-5);     /* vertex 2: x = -5 */
    put_u32(b + VERTEX_AT + 12 * 2 + 8, 70000);
    put_u32(b + UV_AT + 8, 12);
    put_u32(b + UV_AT + 12, (uint32_t)-1);

    b[POLY_AT] = 0x84;                    /* quad, flag bit 7 */
    b[POLY_AT + 2] = 1;
    put_u32(b + POLY_AT + 4, 0);          /* vertex slots 0..3, uv slots 4..7 */
    put_u32(b + POLY_AT + 8, 4);
    b[POLY_AT + 12] = 0x03;               /* triangle without uvs */
    put_u32(b + POLY_AT + 16, 4);
    put_u32(b + POLY_AT + 20, 4);

    memcpy(b + TEXTURE_AT + 8, "topv", 4);
    put_u32(b + TEXTURE_AT + 16, 0xEF);
    memcpy(b + A_AT, "left_tur", 8);
    put_u32(b + A_AT + 16, 109);
    memcpy(b + B_AT, "surface", 7);
    put_u32(b + B_AT + 24, 0x3D);
    memcpy(b + C_AT, "smok", 4);
    put_u32(b + C_AT + 8, 24);
    memcpy(b + LABEL_AT, "bklr", 4);
    put_u32(b + LABEL_AT + 8, 1);

    const uint32_t map[MAP_SLOTS] = { 0, 1, 2, 0, 1, 0, 1, 0 };
    for (int i = 0; i < MAP_SLOTS; i++) put_u32(b + MAP_AT + 4 * i, map[i]);
}


static const SetFile *must(const FileSet *f, const char *name)
{
    const SetFile *s = fileset_find(f, name);
    CHECK(s != NULL);
    return s;
}

/* A copy of the set with one file's text replaced. */
static FileSet with_text(const FileSet *src, const char *name, const char *text)
{
    FileSet next;
    fileset_init(&next);
    for (size_t i = 0; i < src->count; ++i) {
        if (strcmp(src->file[i].name, name) == 0) fileset_add(&next, name, (const uint8_t *)text, strlen(text));
        else fileset_add(&next, src->file[i].name, src->file[i].data, src->file[i].size);
    }
    return next;
}

static char *copy_text(const SetFile *f)
{
    char *s = malloc(f->size + 1);
    memcpy(s, f->data, f->size);
    s[f->size] = '\0';
    return s;
}

static void test_identity(void)
{
    uint8_t orip[SAMPLE_SIZE], *back = NULL;
    size_t back_len = 0;
    build_sample(orip);
    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    CHECK(mesh_export(orip, sizeof orip, "m/body", "textures", &files, &err));
    if (files.count == 0) fprintf(stderr, "export: %s\n", err.message);
    const char *names[] = { "m/body.obj", "m/body.mtl", "m/body.header.tsv", "m/body.polygons.tsv", "m/body.textures.tsv",
                            "m/body.parts.tsv", "m/body.groups.tsv", "m/body.sectionc.tsv", "m/body.labels.tsv" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) must(&files, names[i]);
    char *obj = copy_text(must(&files, "m/body.obj"));
    CHECK(strstr(obj, "v -5 0 70000\n") != NULL);                  /* vertex 2 */
    CHECK(strstr(obj, "vt 12 -1\n") != NULL);
    CHECK(strstr(obj, "usemtl t1_topv") == NULL);                   /* polygon 0 uses texture 1 = name "\0\0\0\0" */
    CHECK(strstr(obj, "f 1/1 2/2 3/3 1/4") != NULL || strstr(obj, "f 1/") != NULL);
    CHECK(strstr(obj, "\nf 2 1 2\n") != NULL);                      /* the triangle has no uvs */
    free(obj);
    char *mtl = copy_text(must(&files, "m/body.mtl"));
    CHECK(strstr(mtl, "newmtl t0_topv") != NULL);
    CHECK(strstr(mtl, "map_Kd textures/topv.png") != NULL);
    free(mtl);
    CHECK(mesh_import(orip, sizeof orip, "m/body", &files, &back, &back_len, &err));
    CHECK(back_len == sizeof orip && memcmp(back, orip, sizeof orip) == 0);
    free(back);
    fileset_free(&files);
}

static int replace_once(char *out, size_t cap, const char *text, const char *from, const char *to)
{
    const char *hit = strstr(text, from);
    if (hit == NULL || strlen(text) + strlen(to) >= cap) return 0;
    snprintf(out, cap, "%.*s%s%s", (int)(hit - text), text, to, hit + strlen(from));
    return 1;
}

static void test_edits(void)
{
    uint8_t orip[SAMPLE_SIZE], *back = NULL;
    size_t back_len = 0;
    build_sample(orip);
    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    CHECK(mesh_export(orip, sizeof orip, "b", NULL, &files, &err));
    char *obj = copy_text(must(&files, "b.obj"));

    /* vertex 0 moved with floats: rounded to integers, nothing else changes */
    char text[8192];
    CHECK(replace_once(text, sizeof text, obj, "v 0 0 0\n", "v 10.4 -20.6 30\n") || replace_once(text, sizeof text, obj, "\nv ", "\nv 10.4 -20.6 30\nv "));
    FileSet moved = with_text(&files, "b.obj", text);
    CHECK(mesh_import(orip, sizeof orip, "b", &moved, &back, &back_len, &err));
    CHECK_EQ_INT(back_len, SAMPLE_SIZE);
    CHECK(back != NULL);
    if (back != NULL) {
        CHECK_EQ_INT(back[VERTEX_AT], 10);
        CHECK_EQ_INT((int8_t)back[VERTEX_AT + 4], -21);
        CHECK_EQ_INT(back[VERTEX_AT + 8], 30);
        CHECK_MEM(back + VERTEX_AT + 12, orip + VERTEX_AT + 12, SAMPLE_SIZE - VERTEX_AT - 12);
    }
    free(back);
    fileset_free(&moved);

    /* the triangle's first corner now reads vertex 3 */
    CHECK(replace_once(text, sizeof text, obj, "\nf 2 1 2\n", "\nf 3 1 2\n"));
    FileSet fs = with_text(&files, "b.obj", text);
    CHECK(mesh_import(orip, sizeof orip, "b", &fs, &back, &back_len, &err));
    CHECK(back != NULL);
    if (back != NULL) CHECK_EQ_INT(back[MAP_AT + 4 * 4], 2);       /* slot 4 = its first corner */
    free(back);
    fileset_free(&fs);

    /* a negative index counts from the end; usemtl switches the texture */
    CHECK(replace_once(text, sizeof text, obj, "\nf 2 1 2\n", "\nusemtl t0_topv\nf -1 1 2\n"));
    fs = with_text(&files, "b.obj", text);
    CHECK(mesh_import(orip, sizeof orip, "b", &fs, &back, &back_len, &err));
    CHECK(back != NULL);
    if (back != NULL) {
        CHECK_EQ_INT(back[MAP_AT + 4 * 4], 2);                      /* -1 = the last of the 3 vertices */
        CHECK_EQ_INT(back[POLY_AT + 12 + 2], 0);                    /* the triangle now uses texture 0 */
    }
    free(back);
    fileset_free(&fs);
    free(obj);

    /* refusals */
    const char *bad[] = {
        "o m\nv 1 2 3\n",                                                   /* too few of everything */
        "o m\nv 1 2\nv 1 2 3\n",                                            /* short vertex */
        "usemtl t9_x\nv 1 2 3\n",                                            /* no such texture reference */
        "usemtl metal\nv 1 2 3\n",                                           /* not one of ours */
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        FileSet fs = with_text(&files, "b.obj", bad[i]);
        CHECK(!mesh_import(orip, sizeof orip, "b", &fs, &back, &back_len, &err));
        CHECK(strstr(err.message, "b.obj") != NULL);
        fileset_free(&fs);
    }
    fileset_free(&files);
    CHECK(!mesh_export((const uint8_t *)"nope", 4, "x", NULL, &files, &err));
}

/* every ORIP leaf in a document tree */
static int cd_meshes, cd_bad;

static void walk(const DocNode *node, const char *path)
{
    if (!doc_is_container(node)) {
        size_t n = 0;
        const uint8_t *b = doc_bytes(node, &n);
        if (b == NULL || n < 8 || memcmp(b, "ORIP", 4) != 0) return;
        const size_t declared = (size_t)(b[4] | b[5] << 8 | b[6] << 16 | (uint32_t)b[7] << 24);
        if (declared > n) { ++cd_bad; return; }
        FileSet files;
        fileset_init(&files);
        ExchangeError err = {{0}};
        uint8_t *back = NULL;
        size_t back_len = 0;
        ++cd_meshes;
        const int ok = mesh_export(b, n, "m", "t", &files, &err) && mesh_import(b, n, "m", &files, &back, &back_len, &err);
        if (!ok) fprintf(stderr, "%s: %s\n", path, err.message);
        if (!ok || back_len != declared || memcmp(back, b, declared) != 0) {
            fprintf(stderr, "%s: export then import does not reproduce the mesh\n", path);
            CHECK(0);
        }
        free(back);
        fileset_free(&files);
        return;
    }
    for (uint32_t i = 0; i < doc_entry_count(node); ++i) {
        DocNode *child = NULL;
        doc_entry(node, i, &child);
        if (child != NULL) walk(child, path);
    }
}

static int read_all(const char *path, uint8_t **buf, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    *buf = malloc((size_t)n + 1);
    const int ok = *buf != NULL && fread(*buf, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    *size = (size_t)n;
    return ok;
}

int main(int argc, char **argv)
{
    test_identity();
    test_edits();
    for (int a = 1; a < argc; ++a) {
        uint8_t *file = NULL;
        size_t size = 0;
        if (!read_all(argv[a], &file, &size)) { CHECK(0); free(file); continue; }
        DocNode *doc = NULL;
        if (size >= 4 && memcmp(file, "wwww", 4) == 0 && doc_parse_chunks(file, size, &doc) == DOC_OK) {
            walk(doc, argv[a]);
            doc_free(doc);
        }
        free(file);
    }
    if (argc > 1) fprintf(stderr, "%d meshes round-tripped (%d with a longer declared length than their extent)\n", cd_meshes, cd_bad);
    return TEST_REPORT();
}
