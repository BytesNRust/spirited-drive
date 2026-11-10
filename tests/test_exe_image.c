/* The exe image (game/exe/exe_image.h): a PE parsed from a byte buffer and read by virtual address, and its check against the
 * PC SE's size and SHA-256. The images here are built by the test (a tiny PE with invented bytes); with original/ there,
 * the real NFS_WIN.EXE is checked as well. */
#include "game/exe/exe_image.h"
#include "game/exe/exe_tables.h"
#include "game/sys/data_check.h"
#include "game/sys/sha256.h"
#include "tests/test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PE_AT = 0x80, OPT_SIZE = 0xE0, BASE = 0x400000, FILE_SIZE = 0x600 };

static void put16(uint8_t *b, size_t at, uint16_t v) { b[at] = (uint8_t)v; b[at + 1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *b, size_t at, uint32_t v) { put16(b, at, (uint16_t)v); put16(b, at + 2, (uint16_t)(v >> 16)); }

static void section(uint8_t *b, int i, const char *name, uint32_t vsize, uint32_t vaddr, uint32_t raw_size, uint32_t raw_ptr)
{
    size_t at = PE_AT + 24 + OPT_SIZE + 40 * (size_t)i;
    memcpy(b + at, name, strlen(name));
    put32(b, at + 8, vsize);
    put32(b, at + 12, vaddr);
    put32(b, at + 16, raw_size);
    put32(b, at + 20, raw_ptr);
}

/* .text at 0x401000 (raw 0x200, 0x100 bytes), .data at 0x402000 (raw 0x300, 0x100 bytes, 0x180 in memory), .bss at 0x403000 (no bytes) */
static uint8_t *make_exe(void)
{
    uint8_t *b = calloc(1, FILE_SIZE);
    b[0] = 'M'; b[1] = 'Z';
    put32(b, 0x3C, PE_AT);
    memcpy(b + PE_AT, "PE\0\0", 4);
    put16(b, PE_AT + 6, 3);
    put16(b, PE_AT + 20, OPT_SIZE);
    put32(b, PE_AT + 24 + 28, BASE);
    section(b, 0, ".text", 0, 0x1000, 0x100, 0x200);
    section(b, 2, ".bss", 0x80, 0x3000, 0x80, 0);
    section(b, 1, ".data", 0x180, 0x2000, 0x100, 0x300);
    for (int i = 0; i < 0x100; i++) b[0x300 + i] = (uint8_t)(i * 3 + 1);
    memcpy(b + 0x300 + 0x40, "Alpha\0Beta", 10);
    return b;
}

static void test_open_and_read(void)
{
    uint8_t *b = make_exe();
    ExeImage img;
    char err[256] = "";
    CHECK_EQ_INT(exe_image_open(&img, b, FILE_SIZE, err, sizeof err), 0);
    CHECK_EQ_INT(img.sections, 3);
    CHECK_EQ_INT(img.base, BASE);
    uint8_t got[4];
    CHECK_EQ_INT(exe_va_read(&img, BASE + 0x2000 + 4, got, 4), 0);
    CHECK_EQ_INT(got[0], 13); CHECK_EQ_INT(got[3], 22);
    memset(got, 0xEE, sizeof got);
    CHECK_EQ_INT(exe_va_read(&img, BASE + 0x3000 + 8, got, 4), 0);   /* .bss: zeros, the file has none there */
    CHECK_EQ_INT(got[0] | got[1] | got[2] | got[3], 0);
    uint8_t tail[8];
    CHECK_EQ_INT(exe_va_read(&img, BASE + 0x2000 + 0xFC, tail, 8), 0);   /* 4 bytes in the raw data, 4 past it: zeros */
    CHECK_EQ_INT(tail[0], (0xFC * 3 + 1) & 0xFF);
    CHECK_EQ_INT(tail[3], (0xFF * 3 + 1) & 0xFF);
    CHECK_EQ_INT(tail[4] | tail[5] | tail[6] | tail[7], 0);
    CHECK_EQ_INT(exe_va_read(&img, BASE + 0x2000 + 0x17C, tail, 4), 0);   /* in memory (vsize 0x180) but past the raw data */
    CHECK_EQ_INT(tail[0] | tail[3], 0);
    CHECK(exe_va_read(&img, BASE + 0x2000 + 0x180, tail, 4) != 0);   /* the first byte past the section's memory */
    CHECK(exe_va_read(&img, BASE + 0x500, tail, 4) != 0);           /* before every section */
    CHECK(exe_va_read(&img, 0x10, tail, 4) != 0);                   /* below the image base */
    CHECK(exe_va_read(&img, 0xFFFFFFFCu, tail, 8) != 0);            /* the wrap at 2^32 */
    char s[16];
    CHECK_EQ_INT(exe_va_cstr(&img, BASE + 0x2000 + 0x40, s, sizeof s), 0);
    CHECK(strcmp(s, "Alpha") == 0);
    CHECK_EQ_INT(exe_va_cstr(&img, BASE + 0x2000 + 0x46, s, 3), 0);   /* cut to the buffer, still NUL terminated */
    CHECK(strcmp(s, "Be") == 0);
    CHECK(exe_va_cstr(&img, BASE + 0x500, s, sizeof s) != 0);
    free(b);
}

static void test_rejects(void)
{
    ExeImage img;
    char err[256];
    uint8_t *b = make_exe();
    err[0] = 0;
    CHECK(exe_image_open(&img, b, 0x30, err, sizeof err) != 0);
    CHECK(strstr(err, "short") != NULL);
    b[0] = 'X';
    err[0] = 0;
    CHECK(exe_image_open(&img, b, FILE_SIZE, err, sizeof err) != 0);
    CHECK(strstr(err, "MZ") != NULL);
    b[0] = 'M';
    b[PE_AT] = 'Q';
    err[0] = 0;
    CHECK(exe_image_open(&img, b, FILE_SIZE, err, sizeof err) != 0);
    CHECK(strstr(err, "PE") != NULL);
    b[PE_AT] = 'P';
    put32(b, 0x3C, FILE_SIZE - 8);   /* the PE header off the end of the file */
    CHECK(exe_image_open(&img, b, FILE_SIZE, err, sizeof err) != 0);
    put32(b, 0x3C, PE_AT);
    put16(b, PE_AT + 6, 40);         /* more sections than the image keeps */
    err[0] = 0;
    CHECK(exe_image_open(&img, b, FILE_SIZE, err, sizeof err) != 0);
    CHECK(strstr(err, "sections") != NULL);
    put16(b, PE_AT + 6, 3);
    section(b, 0, ".text", 0, 0x1000, 0x1000, 0x400);   /* raw data past the end of the file */
    err[0] = 0;
    CHECK(exe_image_open(&img, b, FILE_SIZE, err, sizeof err) != 0);
    CHECK(strstr(err, "section") != NULL);
    free(b);
    CHECK(exe_image_open(NULL, NULL, 0, err, sizeof err) != 0);
    CHECK(exe_image_open(&img, NULL, 0, NULL, 0) != 0);   /* no message wanted: still refused */
}

static void test_verify(void)
{
    uint8_t *b = make_exe();
    Sha256 s;
    char hex[65], err[512] = "";
    sha256_init(&s);
    sha256_update(&s, b, FILE_SIZE);
    sha256_hex(&s, hex);
    CHECK_EQ_INT(exe_image_verify_against(b, FILE_SIZE, FILE_SIZE, hex, "dir", err, sizeof err), 0);
    b[0x310] ^= 1;   /* one bit of the data */
    CHECK(exe_image_verify_against(b, FILE_SIZE, FILE_SIZE, hex, "/some/dir", err, sizeof err) != 0);
    CHECK(strstr(err, "/some/dir") != NULL);
    CHECK(strstr(err, "NFS_WIN.EXE") != NULL);
    CHECK(strstr(err, "sha256") != NULL);
    CHECK(strstr(err, hex) != NULL);            /* the expected digest is in the message */
    CHECK(strstr(err, "another release") != NULL);
    b[0x310] ^= 1;
    CHECK(exe_image_verify_against(b, FILE_SIZE - 1, FILE_SIZE, hex, "dir", err, sizeof err) != 0);   /* the size alone */
    CHECK(strstr(err, "size") != NULL);
    /* the real manifest: the invented image is not the PC SE */
    CHECK(exe_image_verify(b, FILE_SIZE, "dir", err, sizeof err) != 0);
    CHECK(strstr(err, "1069056") != NULL);
    free(b);
}

#ifdef TNFS_SOURCE_DIR
static void test_real_exe(void)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/original/NFS_WIN.EXE", TNFS_SOURCE_DIR);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "no original/NFS_WIN.EXE: the real exe's check skipped\n"); return; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n);
    CHECK(fread(b, 1, (size_t)n, f) == (size_t)n);
    fclose(f);
    char err[512] = "";
    CHECK_EQ_INT(exe_image_verify(b, (size_t)n, "original", err, sizeof err), 0);
    ExeImage img;
    CHECK_EQ_INT(exe_image_open(&img, b, (size_t)n, err, sizeof err), 0);
    CHECK_EQ_INT(img.base, 0x400000);
    uint8_t row[16];
    CHECK_EQ_INT(exe_va_read(&img, 0x4C2850, row, sizeof row), 0);   /* DGROUP */
    CHECK_EQ_INT(exe_va_read(&img, 0x520000, row, sizeof row), 0);   /* .bss */
    CHECK_EQ_INT(row[0] | row[15], 0);
    free(b);
}
#endif

/* the table set the extractors fill (exe_tables.h): its members are the arrays' own sizes */
static void test_tables_shape(void)
{
    ExeTables *t = calloc(1, sizeof *t);
    CHECK_EQ_INT(sizeof t->ai_driver_data, AI_DRIVER_DATA_SIZE);
    CHECK_EQ_INT(sizeof t->track_polygons / sizeof t->track_polygons[0], TRACK_POLYGON_COUNT);
    CHECK_EQ_INT(sizeof t->config_row_number / sizeof t->config_row_number[0], CONFIG_RECORD_ROW_COUNT);
    CHECK(sizeof *t < 200000);   /* about 41 KB of data and the names' room */
    free(t);
}

int main(void)
{
    test_tables_shape();
    test_open_and_read();
    test_rejects();
    test_verify();
#ifdef TNFS_SOURCE_DIR
    test_real_exe();
#endif
    return TEST_REPORT();
}
