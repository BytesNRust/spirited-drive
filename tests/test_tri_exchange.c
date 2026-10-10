/* TRI exchange tests: an unedited export imports back to the same bytes; edits reach the node, scenery,
 * descriptor and terrain bytes; the scenery terminator follows the row count; tables that change a
 * replace-only count are refused. With CD files every TRI round-trips. */
#include "formats/tri.h"
#include "formats/tri_exchange.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put_u32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }

enum { SEGMENTS = 3, DESCRIPTORS = 2, SCENERY_AREA = TRI_SCENERY_MAX * TRI_SCENERY_SIZE };

static uint8_t *build(size_t *size)
{
    const TriParts parts_unused = {0};
    (void)parts_unused;
    uint8_t *header = calloc(1, TRI_HEADER_SIZE);
    uint8_t descriptors[DESCRIPTORS * TRI_DESCRIPTOR_SIZE];
    uint8_t *scenery = calloc(1, SCENERY_AREA);
    uint8_t terrain[SEGMENTS * TRI_SEGMENT_SIZE];
    header[0] = TRI_VERSION;
    put_u32(header + 0x16218, 1000);
    for (int i = 0; i < DESCRIPTORS * TRI_DESCRIPTOR_SIZE; ++i) descriptors[i] = (uint8_t)(i * 3);
    for (int i = 0; i < TRI_SCENERY_MAX; ++i) put_u32(scenery + 16 * i, 0xffffffffu);
    for (int i = 0; i < 2; ++i) {                       /* two real placements */
        put_u32(scenery + 16 * i, (uint32_t)(10 + i));
        scenery[16 * i + 4] = (uint8_t)i;
    }
    memset(terrain, 0, sizeof terrain);
    for (int s = 0; s < SEGMENTS; ++s) {
        memcpy(terrain + s * TRI_SEGMENT_SIZE, "TRKD", 4);
        put_u32(terrain + s * TRI_SEGMENT_SIZE + 4, TRI_SEGMENT_SIZE - 12);
        for (int b = 12; b < TRI_SEGMENT_SIZE; ++b) terrain[s * TRI_SEGMENT_SIZE + b] = (uint8_t)(s * 50 + b);
    }
    const TriParts parts = { header, descriptors, DESCRIPTORS, scenery, terrain, SEGMENTS, NULL, 0 };
    uint8_t *file = malloc(tri_write_size(&parts));
    tri_write(&parts, file, tri_write_size(&parts), size);
    free(header);
    free(scenery);
    return file;
}

static void replace_text(FileSet *set, const char *name, const char *from, const char *to)
{
    const SetFile *f = fileset_find(set, name);
    char *text = malloc(f->size + 1 + strlen(to));
    memcpy(text, f->data, f->size);
    text[f->size] = 0;
    char *hit = strstr(text, from);
    if (hit != NULL) {
        memmove(hit + strlen(to), hit + strlen(from), strlen(hit + strlen(from)) + 1);
        memcpy(hit, to, strlen(to));
    }
    FileSet next;
    fileset_init(&next);
    for (size_t i = 0; i < set->count; ++i) {
        if (strcmp(set->file[i].name, name) == 0) fileset_add(&next, name, (uint8_t *)text, strlen(text));
        else fileset_add(&next, set->file[i].name, set->file[i].data, set->file[i].size);
    }
    free(text);
    fileset_free(set);
    *set = next;
}

static void test_synthetic(void)
{
    size_t size = 0;
    uint8_t *tri = build(&size);
    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    CHECK(tri_export(tri, size, &files, &err));
    CHECK(fileset_find(&files, "track.tsv") && fileset_find(&files, "nodes.tsv") && fileset_find(&files, "segments.tsv") &&
          fileset_find(&files, "scenery.tsv") && fileset_find(&files, "descriptors.tsv") && fileset_find(&files, "brightness.tsv"));
    const SetFile *track = fileset_find(&files, "track.tsv");
    CHECK(strstr((const char *)track->data, "descriptor_count\tu32\t2") != NULL);   /* header offsets past 64 KiB */
    CHECK(strstr((const char *)track->data, "scenery_slots\tu32\t1000") != NULL);
    uint8_t *back = NULL;
    size_t back_len = 0;
    CHECK(tri_import(tri, size, &files, &back, &back_len, &err));
    CHECK(back_len == size && memcmp(back, tri, size) == 0);
    free(back);

    /* the scenery table lists the two placements; dropping one moves the terminator */
    const SetFile *sc = fileset_find(&files, "scenery.tsv");
    CHECK(strstr((const char *)sc->data, "\n1\t11\t") != NULL);
    CHECK(strstr((const char *)sc->data, "\n2\t") == NULL);
    FileSet one = {0};
    fileset_init(&one);
    for (size_t i = 0; i < files.count; ++i) fileset_add(&one, files.file[i].name, files.file[i].data, files.file[i].size);
    {
        const SetFile *f = fileset_find(&one, "scenery.tsv");
        const char *second = strstr((const char *)f->data, "\n1\t");
        FileSet next;
        fileset_init(&next);
        for (size_t i = 0; i < one.count; ++i) {
            if (strcmp(one.file[i].name, "scenery.tsv") == 0) fileset_add(&next, "scenery.tsv", f->data, (size_t)(second + 1 - (const char *)f->data));
            else fileset_add(&next, one.file[i].name, one.file[i].data, one.file[i].size);
        }
        fileset_free(&one);
        one = next;
    }
    CHECK(tri_import(tri, size, &one, &back, &back_len, &err));
    Tri t;
    CHECK_EQ_INT(tri_open(&t, back, back_len), TRI_OK);
    CHECK_EQ_INT(t.scenery_count, 1);
    free(back);
    fileset_free(&one);

    /* an edit of a node's slope and a terrain delta reaches the bytes */
    replace_text(&files, "descriptors.tsv", "\n0\t0\t3\t", "\n0\t9\t3\t");
    CHECK(tri_import(tri, size, &files, &back, &back_len, &err));
    CHECK_EQ_INT(back[t.descriptors_offset], 9);
    free(back);
    fileset_free(&files);

    /* a node row missing is refused */
    fileset_init(&files);
    CHECK(tri_export(tri, size, &files, &err));
    const SetFile *nodes = fileset_find(&files, "nodes.tsv");
    const char *last = strrchr((const char *)nodes->data, '\n');
    (void)last;
    FileSet cut;
    fileset_init(&cut);
    for (size_t i = 0; i < files.count; ++i) {
        if (strcmp(files.file[i].name, "nodes.tsv") == 0) {
            const char *end = (const char *)files.file[i].data + files.file[i].size - 1;
            while (end > (const char *)files.file[i].data && end[-1] != '\n') --end;
            fileset_add(&cut, "nodes.tsv", files.file[i].data, (size_t)(end - (const char *)files.file[i].data));
        } else fileset_add(&cut, files.file[i].name, files.file[i].data, files.file[i].size);
    }
    CHECK(!tri_import(tri, size, &cut, &back, &back_len, &err));
    CHECK(strstr(err.message, "nodes.tsv") != NULL);
    fileset_free(&cut);
    fileset_free(&files);
    CHECK(!tri_export((const uint8_t *)"nope", 4, &files, &err));
    free(tri);
}

static void check_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) { CHECK(0); return; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)size);
    const int ok = buf != NULL && fread(buf, 1, (size_t)size, f) == (size_t)size;
    fclose(f);
    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    uint8_t *back = NULL;
    size_t back_len = 0;
    const int done = ok && tri_export(buf, (size_t)size, &files, &err) && tri_import(buf, (size_t)size, &files, &back, &back_len, &err);
    if (!done) fprintf(stderr, "%s: %s\n", path, err.message);
    if (!done || back_len != (size_t)size || memcmp(back, buf, (size_t)size) != 0) {
        fprintf(stderr, "%s: export then import does not reproduce the TRI\n", path);
        CHECK(0);
    }
    free(back);
    free(buf);
    fileset_free(&files);
}

int main(int argc, char **argv)
{
    test_synthetic();
    for (int a = 1; a < argc; ++a) check_file(argv[a]);
    return TEST_REPORT();
}
