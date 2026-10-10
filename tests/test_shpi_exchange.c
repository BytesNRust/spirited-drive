/* SHPI exchange tests: an unedited export imports back to the original bytes; edited PNGs land in the
 * entry (indexed ones directly, RGBA through the palette, with the reassigned pixels counted); mismatched
 * tables are refused with a message. With CD files every SHPI (FSH and QFS payloads) round-trips. */
#include "formats/png.h"
#include "formats/qfs.h"
#include "formats/shpi_exchange.h"
#include "formats/tsv.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put_u32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }

/* SHPI: "pal0" 8-bit palette (grey ramp but colour 5 red), "img0" 4x2 with a 3-byte tail, "img1" 2x2. */
enum { PAL_AT = 0x28, IMG0_AT = PAL_AT + 16 + 768, IMG1_AT = IMG0_AT + 16 + 8 + 3, TOTAL = IMG1_AT + 16 + 4 };

static void entry_header(uint8_t *e, uint8_t type, uint32_t block, uint16_t w, uint16_t h)
{
    e[0] = type;
    e[1] = (uint8_t)block; e[2] = (uint8_t)(block >> 8); e[3] = (uint8_t)(block >> 16);
    e[4] = (uint8_t)w; e[5] = (uint8_t)(w >> 8); e[6] = (uint8_t)h; e[7] = (uint8_t)(h >> 8);
    e[8] = 0; e[9] = 0; e[10] = 0; e[11] = 0; e[12] = 7; e[13] = 0; e[14] = 0xfe; e[15] = 0xff;   /* misc: 0,0,7,-2 */
}

static void build(uint8_t b[TOTAL])
{
    memset(b, 0, TOTAL);
    memcpy(b, "SHPI", 4);
    put_u32(b + 4, TOTAL);
    put_u32(b + 8, 3);
    memcpy(b + 12, "GIMX", 4);
    memcpy(b + 16, "pal0", 4); put_u32(b + 20, PAL_AT);
    memcpy(b + 24, "img0", 4); put_u32(b + 28, IMG0_AT);
    memcpy(b + 32, "im.1", 4); put_u32(b + 36, IMG1_AT);
    entry_header(b + PAL_AT, 0x24, 0, 0, 0);
    for (int i = 0; i < 256; ++i) b[PAL_AT + 16 + 3 * i] = b[PAL_AT + 16 + 3 * i + 1] = b[PAL_AT + 16 + 3 * i + 2] = (uint8_t)i;
    b[PAL_AT + 16 + 15] = 255; b[PAL_AT + 16 + 16] = 0; b[PAL_AT + 16 + 17] = 0;     /* colour 5 = (255,0,0) */
    entry_header(b + IMG0_AT, 0x7b, 16 + 8, 4, 2);
    for (int i = 0; i < 8; ++i) b[IMG0_AT + 16 + i] = (uint8_t)(i * 30);
    b[IMG0_AT + 24] = 0xaa; b[IMG0_AT + 25] = 0xbb; b[IMG0_AT + 26] = 0xcc;          /* tail */
    entry_header(b + IMG1_AT, 0x7b, 16 + 4, 2, 2);
    b[IMG1_AT + 16] = 1; b[IMG1_AT + 17] = 2; b[IMG1_AT + 18] = 3; b[IMG1_AT + 19] = 4;
}

static void test_identity(void)
{
    uint8_t shpi[TOTAL], *back = NULL;
    size_t back_len = 0;
    build(shpi);
    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    CHECK(shpi_export(shpi, sizeof shpi, NULL, &files, &err));
    CHECK(fileset_find(&files, "entries.tsv") != NULL);
    CHECK(fileset_find(&files, "0000_pal0.bin") != NULL);
    CHECK(fileset_find(&files, "0001_img0.png") != NULL);
    CHECK(fileset_find(&files, "0002_im_1.png") != NULL);
    ImportStats stats;
    CHECK(shpi_import(shpi, sizeof shpi, NULL, &files, &back, &back_len, &stats, &err));
    CHECK_EQ_INT(back_len, TOTAL);
    CHECK(back != NULL && memcmp(back, shpi, TOTAL) == 0);
    CHECK_EQ_INT(stats.reassigned, 0);
    CHECK_EQ_INT(stats.pixels, 12);
    free(back);

    /* the PNG carries the SHPI's own palette */
    PngImage img;
    const SetFile *png = fileset_find(&files, "0001_img0.png");
    CHECK_EQ_INT(png_read(png->data, png->size, &img), PNG_OK);
    CHECK_EQ_INT(img.indexed, 1);
    CHECK_EQ_INT(img.palette[15], 255);
    CHECK_EQ_INT(img.pixels[7], 210);
    png_free(img.pixels);
    fileset_free(&files);
}

static void replace(FileSet *f, const char *name, uint8_t *data, size_t size)
{
    FileSet next;
    fileset_init(&next);
    for (size_t i = 0; i < f->count; ++i) {
        if (strcmp(f->file[i].name, name) == 0) fileset_add_owned(&next, name, data, size);
        else fileset_add(&next, f->file[i].name, f->file[i].data, f->file[i].size);
    }
    fileset_free(f);
    *f = next;
}

static void test_edits(void)
{
    uint8_t shpi[TOTAL], *back = NULL;
    size_t back_len = 0;
    build(shpi);
    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    CHECK(shpi_export(shpi, sizeof shpi, NULL, &files, &err));
    ImportStats stats;

    /* an indexed edit: pixel 0 of img0 becomes index 5 */
    uint8_t px[8], pal[768], *png = NULL;
    size_t png_len = 0;
    for (int i = 0; i < 8; ++i) px[i] = (uint8_t)(i * 30);
    px[0] = 5;
    for (int i = 0; i < 256; ++i) pal[3 * i] = pal[3 * i + 1] = pal[3 * i + 2] = (uint8_t)i;
    pal[15] = 255; pal[16] = 0; pal[17] = 0;
    CHECK_EQ_INT(png_write_indexed(px, 4, 2, pal, &png, &png_len), PNG_OK);
    replace(&files, "0001_img0.png", png, png_len);
    CHECK(shpi_import(shpi, sizeof shpi, NULL, &files, &back, &back_len, &stats, &err));
    CHECK_EQ_INT(stats.reassigned, 0);
    CHECK_EQ_INT(back_len, TOTAL);
    CHECK_EQ_INT(back[IMG0_AT + 16], 5);
    CHECK_EQ_INT(back[IMG0_AT + 24], 0xaa);                  /* the tail is kept */
    CHECK_MEM(back + IMG1_AT, shpi + IMG1_AT, 20);           /* the other entry untouched */
    free(back);

    /* an RGBA edit: red is colour 5 exactly, (100,100,101) is nearest to grey 100, transparent is 0 */
    uint8_t rgba[2 * 2 * 4] = { 255, 0, 0, 255,   100, 100, 101, 255,   9, 9, 9, 0,   0, 0, 0, 255 };
    CHECK_EQ_INT(png_write_rgba(rgba, 2, 2, &png, &png_len), PNG_OK);
    replace(&files, "0002_im_1.png", png, png_len);
    CHECK(shpi_import(shpi, sizeof shpi, NULL, &files, &back, &back_len, &stats, &err));
    CHECK_EQ_INT(back[IMG1_AT + 16], 5);
    CHECK_EQ_INT(back[IMG1_AT + 17], 100);
    CHECK_EQ_INT(back[IMG1_AT + 18], 0);
    CHECK_EQ_INT(back[IMG1_AT + 19], 0);
    CHECK_EQ_INT(stats.reassigned, 1);                       /* only the (100,100,101) pixel */
    free(back);
    fileset_free(&files);
}

static void test_refusals(void)
{
    uint8_t shpi[TOTAL], *back = NULL;
    size_t back_len = 0;
    ImportStats stats;
    ExchangeError err = {{0}};
    build(shpi);
    FileSet files;
    fileset_init(&files);
    CHECK(!shpi_import(shpi, sizeof shpi, NULL, &files, &back, &back_len, &stats, &err));
    CHECK(strstr(err.message, "entries.tsv") != NULL);

    CHECK(shpi_export(shpi, sizeof shpi, NULL, &files, &err));
    /* a table that names another entry */
    const SetFile *t = fileset_find(&files, "entries.tsv");
    char *text = malloc(t->size + 1);
    memcpy(text, t->data, t->size);
    text[t->size] = 0;
    char *hit = strstr(text, "696d6730");              /* "img0" */
    CHECK(hit != NULL);
    if (hit) hit[7] = '1';
    FileSet bad;
    fileset_init(&bad);
    for (size_t i = 0; i < files.count; ++i) {
        if (strcmp(files.file[i].name, "entries.tsv") == 0) fileset_add(&bad, "entries.tsv", (uint8_t *)text, t->size);
        else fileset_add(&bad, files.file[i].name, files.file[i].data, files.file[i].size);
    }
    CHECK(!shpi_import(shpi, sizeof shpi, NULL, &bad, &back, &back_len, &stats, &err));
    CHECK(strstr(err.message, "entries.tsv:") != NULL);      /* carries the line */
    fileset_free(&bad);
    free(text);

    /* a PNG of another size than the table says */
    uint8_t px[4 * 4] = {0}, pal[768] = {0}, *png = NULL;
    size_t png_len = 0;
    CHECK_EQ_INT(png_write_indexed(px, 4, 4, pal, &png, &png_len), PNG_OK);
    replace(&files, "0001_img0.png", png, png_len);
    CHECK(!shpi_import(shpi, sizeof shpi, NULL, &files, &back, &back_len, &stats, &err));
    CHECK(strstr(err.message, "0001_img0.png") != NULL);

    /* not an SHPI at all, and a truncated one */
    CHECK(!shpi_export((const uint8_t *)"nothing here at all", 19, NULL, &files, &err));
    FileSet empty;
    fileset_init(&empty);
    CHECK(!shpi_export(shpi, 30, NULL, &empty, &err));
    fileset_free(&empty);
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

static int cd_files;

static void check_file(const char *path)
{
    uint8_t *file = NULL, *plain = NULL;
    size_t size = 0, plain_size = 0;
    if (!read_all(path, &file, &size)) { CHECK(0); free(file); return; }
    plain = file;
    plain_size = size;
    size_t unpacked = 0;
    if (qfs_is_compressed(file, size) && qfs_decompressed_size(file, size, &unpacked) == QFS_OK) {
        plain = malloc(unpacked + 1);
        plain_size = unpacked;
        if (plain == NULL || qfs_decompress(file, size, plain, unpacked) != QFS_OK) { CHECK(0); free(plain); free(file); return; }
    }
    if (plain_size >= 4 && memcmp(plain, "SHPI", 4) == 0) {
        FileSet files;
        fileset_init(&files);
        ExchangeError err = {{0}};
        uint8_t *back = NULL;
        size_t back_len = 0;
        ImportStats stats;
        ++cd_files;
        const int ok = shpi_export(plain, plain_size, NULL, &files, &err) &&
                       shpi_import(plain, plain_size, NULL, &files, &back, &back_len, &stats, &err);
        if (!ok) fprintf(stderr, "%s: %s\n", path, err.message);
        if (!ok || back_len != plain_size || memcmp(back, plain, plain_size) != 0 || stats.reassigned != 0) {
            fprintf(stderr, "%s: export then import does not reproduce the SHPI\n", path);
            CHECK(0);
        }
        free(back);
        fileset_free(&files);
    }
    if (plain != file) free(plain);
    free(file);
}

int main(int argc, char **argv)
{
    test_identity();
    test_edits();
    test_refusals();
    for (int a = 1; a < argc; ++a) check_file(argv[a]);
    if (argc > 1) fprintf(stderr, "%d SHPI files round-tripped\n", cd_files);
    return TEST_REPORT();
}
