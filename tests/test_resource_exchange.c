/* Whole-file exchange tests: every kind exports to a tree and imports back; an unedited import returns the
 * original bytes (a packed file keeps its stream); an edited packed file is packed again and unpacks to the
 * edit; wrong kinds and missing files are refused. With CD files every game file round-trips. */
#include "formats/pbs.h"
#include "formats/png.h"
#include "formats/qfs.h"
#include "formats/resource_exchange.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put_u32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }

static FileSet replaced(const FileSet *src, const char *name, const char *from, const char *to)
{
    FileSet next;
    fileset_init(&next);
    for (size_t i = 0; i < src->count; ++i) {
        if (strcmp(src->file[i].name, name) != 0) { fileset_add(&next, src->file[i].name, src->file[i].data, src->file[i].size); continue; }
        char *text = malloc(src->file[i].size + strlen(to) + 1);
        memcpy(text, src->file[i].data, src->file[i].size);
        text[src->file[i].size] = '\0';
        char *hit = strstr(text, from);
        CHECK(hit != NULL);
        if (hit != NULL) {
            char *out = malloc(strlen(text) + strlen(to) + 1);
            snprintf(out, strlen(text) + strlen(to) + 1, "%.*s%s%s", (int)(hit - text), text, to, hit + strlen(from));
            fileset_add(&next, name, (uint8_t *)out, strlen(out));
            free(out);
        }
        free(text);
    }
    return next;
}

static void test_pbs_packed(void)
{
    uint8_t rec[PBS_SIZE], packed[PBS_SIZE * 2], *out = NULL;
    size_t packed_len = 0, out_len = 0;
    for (int i = 0; i < PBS_SIZE; ++i) rec[i] = (uint8_t)(i * 7 + 3);
    pbs_seal(rec);
    CHECK_EQ_INT(pbs_pack(rec, packed, sizeof packed, &packed_len), QFS_OK);

    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    ImportStats stats;
    CHECK(resource_export(packed, packed_len, "CZR1.PBS", NULL, &files, &err));
    CHECK(fileset_find(&files, "resource.tsv") && fileset_find(&files, "specs.tsv"));
    CHECK(resource_import(packed, packed_len, "CZR1.PBS", NULL, &files, &out, &out_len, &stats, &err));
    CHECK(out_len == packed_len && memcmp(out, packed, packed_len) == 0);       /* untouched: the original stream */
    free(out);

    /* an edit: power (+0x3c) = 777; the file is packed again and unpacks to the edit with a valid checksum */
    const char *row = "\tpower\ti32\t";
    const SetFile *t = fileset_find(&files, "specs.tsv");
    char *text = malloc(t->size + 1);
    memcpy(text, t->data, t->size);
    text[t->size] = 0;
    const char *at = strstr(text, row);
    CHECK(at != NULL);
    char old_value[16] = {0};
    if (at) sscanf(at + strlen(row), "%15[^\n]", old_value);
    char from[64];
    snprintf(from, sizeof from, "%s%s", row, old_value);
    FileSet edited = replaced(&files, "specs.tsv", from, "\tpower\ti32\t777");
    free(text);
    CHECK(resource_import(packed, packed_len, "CZR1.PBS", NULL, &edited, &out, &out_len, &stats, &err));
    CHECK(out != NULL && qfs_is_compressed(out, out_len));
    uint8_t back[PBS_SIZE];
    CHECK(out != NULL && pbs_read(back, out, out_len));
    CHECK_EQ_INT((int32_t)(back[0x3c] | back[0x3d] << 8 | back[0x3e] << 16 | (uint32_t)back[0x3f] << 24), 777);
    CHECK(pbs_checksum_ok(back));
    free(out);
    fileset_free(&edited);

    /* wrong kind and missing manifest */
    FileSet other;
    fileset_init(&other);
    for (size_t i = 0; i < files.count; ++i)
        if (strcmp(files.file[i].name, "resource.tsv") != 0) fileset_add(&other, files.file[i].name, files.file[i].data, files.file[i].size);
    CHECK(!resource_import(packed, packed_len, "CZR1.PBS", NULL, &other, &out, &out_len, &stats, &err));
    CHECK(strstr(err.message, "resource.tsv") != NULL);
    fileset_free(&other);
    fileset_free(&files);
    fileset_init(&files);
    CHECK(resource_export(rec, PBS_SIZE, "thing.bin", NULL, &files, &err));      /* no .pbs name: raw */
    CHECK(fileset_find(&files, "raw.bin") != NULL);
    CHECK(!resource_import(rec, PBS_SIZE, "X.PBS", NULL, &files, &out, &out_len, &stats, &err));   /* manifest says raw */
    CHECK(strstr(err.message, "raw") != NULL);
    fileset_free(&files);
}

/* wwww [SHPI(1 bitmap), absent, same SHPI, raw bytes] */
static void test_tree(void)
{
    uint8_t shpi[0x28 + 16 + 4 + 4];
    memset(shpi, 0, sizeof shpi);
    memcpy(shpi, "SHPI", 4);
    put_u32(shpi + 4, sizeof shpi);
    put_u32(shpi + 8, 1);
    memcpy(shpi + 12, "GIMX", 4);
    memcpy(shpi + 16, "tex0", 4);
    put_u32(shpi + 20, 0x18);
    shpi[0x18] = 0x7b;
    shpi[0x19] = 20;
    shpi[0x1c] = 2;
    shpi[0x1e] = 2;
    shpi[0x28 - 0x10 + 16] = 1;
    uint8_t tree[8 + 16 + sizeof shpi + 6];
    memset(tree, 0, sizeof tree);
    memcpy(tree, "wwww", 4);
    put_u32(tree + 4, 4);
    put_u32(tree + 8, 24);
    put_u32(tree + 12, 0);
    put_u32(tree + 16, 24);
    put_u32(tree + 20, 24 + sizeof shpi);
    memcpy(tree + 24, shpi, sizeof shpi);
    memcpy(tree + 24 + sizeof shpi, "rawbit", 6);

    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    ImportStats stats;
    uint8_t *out = NULL;
    size_t out_len = 0;
    CHECK(resource_export(tree, sizeof tree, "x.fam", NULL, &files, &err));
    if (files.count == 0) fprintf(stderr, "%s\n", err.message);
    CHECK(fileset_find(&files, "tree.tsv") != NULL);
    CHECK(fileset_find(&files, "c0/entries.tsv") != NULL);
    CHECK(fileset_find(&files, "c0/0000_tex0.png") != NULL);
    CHECK(fileset_find(&files, "c3/raw.bin") != NULL);
    CHECK(fileset_find(&files, "c2/entries.tsv") == NULL);                      /* shared with c0 */
    const SetFile *manifest = fileset_find(&files, "tree.tsv");
    CHECK(manifest != NULL && strstr((const char *)manifest->data, "c1\tabsent") != NULL);
    CHECK(manifest != NULL && strstr((const char *)manifest->data, "c2\tsame\tc0") != NULL);
    CHECK(resource_import(tree, sizeof tree, "x.fam", NULL, &files, &out, &out_len, &stats, &err));
    CHECK(out_len == sizeof tree && memcmp(out, tree, sizeof tree) == 0);
    free(out);

    /* the raw child edited to a longer value moves nothing else out of place */
    FileSet raw;
    fileset_init(&raw);
    for (size_t i = 0; i < files.count; ++i) {
        if (strcmp(files.file[i].name, "c3/raw.bin") == 0) fileset_add(&raw, "c3/raw.bin", (const uint8_t *)"longer raw data", 15);
        else fileset_add(&raw, files.file[i].name, files.file[i].data, files.file[i].size);
    }
    CHECK(resource_import(tree, sizeof tree, "x.fam", NULL, &raw, &out, &out_len, &stats, &err));
    CHECK_EQ_INT(out_len, sizeof tree + 9);
    CHECK(out != NULL && memcmp(out + 24, shpi, sizeof shpi) == 0);
    CHECK(out != NULL && memcmp(out + 24 + sizeof shpi, "longer raw data", 15) == 0);
    free(out);
    fileset_free(&raw);
    fileset_free(&files);
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

static int cd_files, cd_trees;

static void check_file(const char *path)
{
    uint8_t *file = NULL, *back = NULL;
    size_t size = 0, back_len = 0;
    if (!read_all(path, &file, &size)) { CHECK(0); free(file); return; }
    const char *name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    ImportStats stats;
    ++cd_files;
    const int ok = resource_export(file, size, name, NULL, &files, &err) &&
                   resource_import(file, size, name, NULL, &files, &back, &back_len, &stats, &err);
    if (!ok) fprintf(stderr, "%s: %s\n", path, err.message);
    if (!ok || back_len != size || memcmp(back, file, size) != 0 || stats.reassigned != 0) {
        fprintf(stderr, "%s: export then import does not give the file back\n", path);
        CHECK(0);
    }
    if (fileset_find(&files, "tree.tsv") != NULL) ++cd_trees;
    free(back);
    free(file);
    fileset_free(&files);
}

int main(int argc, char **argv)
{
    test_pbs_packed();
    test_tree();
    for (int a = 1; a < argc; ++a) check_file(argv[a]);
    if (argc > 1) fprintf(stderr, "%d game files round-tripped (%d chunk trees)\n", cd_files, cd_trees);
    return TEST_REPORT();
}
