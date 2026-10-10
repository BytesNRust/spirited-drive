/* Document tests: untouched trees and directories write back byte for byte;
 * replacing a child moves the offsets after it and nothing else; broken tables
 * are refused or kept as leaves. With CD files (FAM, CFM, FSH, QFS): every
 * container unpacks, writes back and compares equal, SHPI leaves expanded. */
#include "formats/chunk.h"
#include "formats/doc.h"
#include "formats/qfs.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void put_u32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

/* root wwww [24, 0 (absent), 40, 24 (shares child 0)] with 4 pad bytes after the table,
 * @24 an ORIP-tagged leaf of 16 bytes, @40 a wwww [12] holding 8 bytes of "SHPI" start. */
enum { SAMPLE = 40 + 12 + 8 };

static void build(uint8_t b[SAMPLE])
{
    memset(b, 0, SAMPLE);
    memcpy(b, "wwww", 4);
    put_u32(b + 4, 4);
    put_u32(b + 8, 24);
    put_u32(b + 12, 0);
    put_u32(b + 16, 40);
    put_u32(b + 20, 24);
    memcpy(b + 24, "ORIP", 4);
    put_u32(b + 28, 16);
    memset(b + 32, 0x5a, 8);
    memcpy(b + 40, "wwww", 4);
    put_u32(b + 44, 1);
    put_u32(b + 48, 12);
    memcpy(b + 52, "SHPIabcd", 8);
}

static void test_identity(void)
{
    uint8_t in[SAMPLE], out[SAMPLE + 8];
    size_t len = 0;
    build(in);
    DocNode *root = NULL;
    CHECK_EQ_INT(doc_parse_chunks(in, sizeof in, &root), DOC_OK);
    if (root == NULL) return;
    CHECK_EQ_INT(doc_entry_count(root), 4);
    CHECK_EQ_INT(doc_size(root), SAMPLE);
    CHECK_EQ_INT(doc_write(root, out, sizeof out, &len), DOC_OK);
    CHECK_EQ_INT(len, SAMPLE);
    CHECK_MEM(out, in, SAMPLE);
    DocNode *a = NULL, *b = NULL, *c = NULL, *d = NULL;
    CHECK_EQ_INT(doc_entry(root, 0, &a), DOC_OK);
    CHECK_EQ_INT(doc_entry(root, 1, &b), DOC_OK);
    CHECK_EQ_INT(doc_entry(root, 2, &c), DOC_OK);
    CHECK_EQ_INT(doc_entry(root, 3, &d), DOC_OK);
    CHECK(a != NULL && a == d);          /* shared offset, one child */
    CHECK(b == NULL);                    /* absent stays absent */
    CHECK(c != NULL && doc_is_container(c));
    CHECK(a != NULL && !doc_is_container(a));
    CHECK_EQ_INT(doc_entry(root, 4, &a), DOC_ERR_INDEX);
    CHECK_EQ_INT(doc_write(root, out, SAMPLE - 1, &len), DOC_ERR_CAPACITY);
    doc_free(root);
}

static void test_replace(void)
{
    uint8_t in[SAMPLE], out[SAMPLE + 64];
    size_t len = 0;
    build(in);
    DocNode *root = NULL, *a = NULL;
    CHECK_EQ_INT(doc_parse_chunks(in, sizeof in, &root), DOC_OK);
    if (root == NULL) return;
    doc_entry(root, 0, &a);
    uint8_t big[40];
    memset(big, 0x77, sizeof big);
    memcpy(big, "ORIP", 4);
    put_u32(big + 4, sizeof big);
    CHECK_EQ_INT(doc_set_bytes(a, big, sizeof big), DOC_OK);
    CHECK_EQ_INT(doc_size(root), SAMPLE + 24);
    CHECK_EQ_INT(doc_write(root, out, sizeof out, &len), DOC_OK);
    CHECK_EQ_INT(len, SAMPLE + 24);
    Chunk root_chunk, child;
    CHECK_EQ_INT(chunk_open(&root_chunk, out, len), CHUNK_OK);
    CHECK_EQ_INT(chunk_child(&root_chunk, 2, &child), CHUNK_OK);
    CHECK_EQ_INT(child.kind, CHUNK_KIND_HEADER);
    CHECK_MEM(child.data, "wwww", 4);
    CHECK_EQ_INT((size_t)(child.data - out), 24 + 40);        /* moved by the 24 extra bytes */
    CHECK_EQ_INT(chunk_child(&root_chunk, 1, &child), CHUNK_OK);
    CHECK_EQ_INT(child.kind, CHUNK_KIND_ABSENT);
    CHECK_EQ_INT(chunk_child(&root_chunk, 3, &child), CHUNK_OK);
    CHECK_EQ_INT((size_t)(child.data - out), 24);              /* still shares child 0 */
    CHECK_MEM(out + 24, big, sizeof big);
    CHECK_EQ_INT(doc_set_bytes(root, big, 1), DOC_ERR_INDEX);  /* a container has no bytes */
    doc_free(root);
}

static void test_bad(void)
{
    uint8_t in[SAMPLE];
    DocNode *root = NULL;
    build(in);
    CHECK_EQ_INT(doc_parse_chunks(in, 7, &root), DOC_ERR_HEADER);
    memcpy(in, "wwwx", 4);
    CHECK_EQ_INT(doc_parse_chunks(in, sizeof in, &root), DOC_ERR_HEADER);
    build(in);
    put_u32(in + 4, 100000);                                   /* table past the data */
    CHECK_EQ_INT(doc_parse_chunks(in, sizeof in, &root), DOC_ERR_BOUNDS);
    build(in);
    put_u32(in + 16, 400);                                     /* child offset past the data */
    CHECK_EQ_INT(doc_parse_chunks(in, sizeof in, &root), DOC_ERR_BOUNDS);
    build(in);
    put_u32(in + 8, 4);                                        /* child offset inside the table */
    CHECK_EQ_INT(doc_parse_chunks(in, sizeof in, &root), DOC_ERR_BOUNDS);

    /* A nested wwww with a broken table stays a leaf and still round-trips. */
    build(in);
    put_u32(in + 44, 9999);
    uint8_t out[SAMPLE];
    size_t len = 0;
    CHECK_EQ_INT(doc_parse_chunks(in, sizeof in, &root), DOC_OK);
    if (root == NULL) return;
    DocNode *c = NULL;
    doc_entry(root, 2, &c);
    CHECK(c != NULL && !doc_is_container(c));
    CHECK_EQ_INT(doc_write(root, out, sizeof out, &len), DOC_OK);
    CHECK_MEM(out, in, SAMPLE);
    doc_free(root);
}

/* SHPI with two entries: "pal0" at 0x20 (8 bytes), "img0" at 0x28 (to the end); size word = file size. */
enum { SHPI_FILE = 0x38 };

static void build_shpi(uint8_t b[SHPI_FILE])
{
    memset(b, 0, SHPI_FILE);
    memcpy(b, "SHPI", 4);
    put_u32(b + 4, SHPI_FILE);
    put_u32(b + 8, 2);
    memcpy(b + 12, "GIMX", 4);
    memcpy(b + 16, "pal0", 4);
    put_u32(b + 20, 0x20);
    memcpy(b + 24, "img0", 4);
    put_u32(b + 28, 0x28);
    memset(b + 0x20, 0x11, 8);
    memset(b + 0x28, 0x22, 16);
}

static void test_shpi(void)
{
    uint8_t in[SHPI_FILE], out[SHPI_FILE + 32];
    size_t len = 0;
    build_shpi(in);
    DocNode *doc = NULL, *img = NULL;
    CHECK_EQ_INT(doc_parse_shpi(in, sizeof in, &doc), DOC_OK);
    if (doc == NULL) return;
    CHECK_EQ_INT(doc_write(doc, out, sizeof out, &len), DOC_OK);
    CHECK_EQ_INT(len, SHPI_FILE);
    CHECK_MEM(out, in, SHPI_FILE);
    doc_entry(doc, 1, &img);
    uint8_t bigger[32];
    memset(bigger, 0x33, sizeof bigger);
    CHECK_EQ_INT(doc_set_bytes(img, bigger, sizeof bigger), DOC_OK);
    CHECK_EQ_INT(doc_write(doc, out, sizeof out, &len), DOC_OK);
    CHECK_EQ_INT(len, 0x28 + 32);
    CHECK_EQ_INT(out[4], len);                                  /* size word follows the file size */
    CHECK_EQ_INT(out[20], 0x20);
    CHECK_EQ_INT(out[28], 0x28);
    doc_free(doc);

    /* A leaf expands to an SHPI in place; one that is not an SHPI is refused. */
    uint8_t wrap[12 + SHPI_FILE];
    memset(wrap, 0, sizeof wrap);
    memcpy(wrap, "wwww", 4);
    put_u32(wrap + 4, 1);
    put_u32(wrap + 8, 12);
    build_shpi(wrap + 12);
    CHECK_EQ_INT(doc_parse_chunks(wrap, 12 + SHPI_FILE, &doc), DOC_OK);
    if (doc == NULL) return;
    DocNode *leaf = NULL;
    doc_entry(doc, 0, &leaf);
    CHECK(leaf != NULL && !doc_is_container(leaf));
    CHECK_EQ_INT(doc_expand_shpi(leaf), DOC_OK);
    CHECK(doc_is_container(leaf));
    CHECK_EQ_INT(doc_entry_count(leaf), 2);
    CHECK_EQ_INT(doc_expand_shpi(leaf), DOC_ERR_INDEX);
    uint8_t whole[12 + SHPI_FILE];
    CHECK_EQ_INT(doc_write(doc, whole, sizeof whole, &len), DOC_OK);
    CHECK_MEM(whole, wrap, 12 + SHPI_FILE);
    doc_free(doc);
    DocNode *not_shpi = NULL;
    uint8_t tree[SAMPLE];
    build(tree);
    CHECK_EQ_INT(doc_parse_shpi(tree, sizeof tree, &not_shpi), DOC_ERR_HEADER);
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

static void expand_all(DocNode *node)
{
    if (!doc_is_container(node)) return;
    for (uint32_t i = 0; i < doc_entry_count(node); ++i) {
        DocNode *child = NULL;
        doc_entry(node, i, &child);
        if (child == NULL) continue;
        size_t size = 0;
        const uint8_t *bytes = doc_is_container(child) ? NULL : doc_bytes(child, &size);
        if (bytes != NULL && size >= 4 && memcmp(bytes, "SHPI", 4) == 0) doc_expand_shpi(child);
        expand_all(child);
    }
}

static int compare_files;

static void check_file(const char *path)
{
    uint8_t *file = NULL;
    size_t size = 0;
    if (!read_all(path, &file, &size)) { CHECK(0); free(file); return; }
    uint8_t *plain = file;
    size_t plain_size = size;
    size_t unpacked = 0;
    if (qfs_is_compressed(file, size) && qfs_decompressed_size(file, size, &unpacked) == QFS_OK) {
        plain = malloc(unpacked + 1);
        plain_size = unpacked;
        if (plain == NULL || qfs_decompress(file, size, plain, unpacked) != QFS_OK) { CHECK(0); free(plain); free(file); return; }
    }
    DocNode *doc = NULL;
    DocResult r = DOC_ERR_HEADER;
    if (plain_size >= 4 && memcmp(plain, "wwww", 4) == 0) r = doc_parse_chunks(plain, plain_size, &doc);
    else if (plain_size >= 4 && memcmp(plain, "SHPI", 4) == 0) r = doc_parse_shpi(plain, plain_size, &doc);
    if (doc != NULL) {
        ++compare_files;
        expand_all(doc);
        uint8_t *out = malloc(plain_size + 1);
        size_t len = 0;
        const DocResult w = out == NULL ? DOC_ERR_MEMORY : doc_write(doc, out, plain_size, &len);
        if (w != DOC_OK || len != plain_size || memcmp(out, plain, plain_size) != 0) {
            fprintf(stderr, "%s: does not write back identically (%s, %zu of %zu bytes)\n", path, doc_result_str(w), len, plain_size);
            CHECK(0);
        }
        free(out);
        doc_free(doc);
    } else if (r != DOC_ERR_HEADER) {
        fprintf(stderr, "%s: %s\n", path, doc_result_str(r));
        CHECK(0);
    }
    if (plain != file) free(plain);
    free(file);
}

int main(int argc, char **argv)
{
    test_identity();
    test_replace();
    test_bad();
    test_shpi();
    for (int a = 1; a < argc; ++a) check_file(argv[a]);
    if (argc > 1) fprintf(stderr, "%d containers compared\n", compare_files);
    return TEST_REPORT();
}
