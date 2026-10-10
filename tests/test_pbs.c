/* PBS (CARSPECS/<name>.PBS) tests: synthetic records, raw and QFS-packed, the
 * checksum at +0x774 over the first 0x758 bytes (004400f4) and, when given
 * the CD's files, the checksum of every real one. */
#include "formats/pbs.h"
#include "formats/qfs.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sealed_record(uint8_t rec[PBS_SIZE])
{
    for (int i = 0; i < PBS_SIZE; ++i) rec[i] = (uint8_t)(i * 7 + 3);
    const int32_t sum = pbs_checksum(rec);
    for (int i = 0; i < 4; ++i) rec[PBS_CHECKSUM_OFFSET + i] = (uint8_t)((uint32_t)sum >> (8 * i));
}

static void test_checksum(void)
{
    uint8_t rec[PBS_SIZE] = {0};
    CHECK_EQ_INT(pbs_checksum(rec), 0);
    rec[0] = 5;
    rec[PBS_CHECKED_BYTES - 1] = 0xff;
    CHECK_EQ_INT(pbs_checksum(rec), 5 + 0xff);
    rec[PBS_CHECKED_BYTES] = 9; /* first byte past the checked range does not count */
    CHECK_EQ_INT(pbs_checksum(rec), 5 + 0xff);
    sealed_record(rec);
    CHECK(pbs_checksum_ok(rec));
    rec[100] ^= 1;
    CHECK(!pbs_checksum_ok(rec));
}

static void test_raw(void)
{
    uint8_t rec[PBS_SIZE + 8], out[PBS_SIZE];
    sealed_record(rec);
    memset(rec + PBS_SIZE, 0xee, 8);
    CHECK(pbs_read(out, rec, PBS_SIZE));
    CHECK_MEM(out, rec, PBS_SIZE);
    CHECK(pbs_read(out, rec, PBS_SIZE + 8)); /* longer files: the record is the head */
    CHECK(!pbs_read(out, rec, PBS_SIZE - 1));
    CHECK(!pbs_read(out, rec, 0));
}

static void test_qfs_stream(void)
{
    /* RefPack header 10 FB + 24-bit size 0x000778, then literal runs ending in the stop code. */
    uint8_t out[PBS_SIZE], src[PBS_SIZE + 64];
    size_t n = 0;
    src[n++] = 0x10; src[n++] = 0xfb;
    src[n++] = 0x00; src[n++] = 0x07; src[n++] = 0x78;
    size_t left = PBS_SIZE;
    while (left > 0) {
        const size_t run = left < 112 ? left : 112; /* 0xe0..0xfb literal runs: 4..112 bytes */
        if (run < 4) break;
        src[n++] = (uint8_t)(0xe0 + (run - 4) / 4);
        for (size_t i = 0; i < run; ++i) src[n++] = 0x41;
        left -= run;
    }
    src[n++] = 0xfc; /* stop, 0 trailing literals */
    CHECK(qfs_is_compressed(src, n));
    CHECK(pbs_read(out, src, n));
    CHECK_EQ_INT(out[0], 0x41);
    CHECK_EQ_INT(out[PBS_SIZE - 1], 0x41);
    /* Same stream cut short: not a record. */
    CHECK(!pbs_read(out, src, n - 30));
}

static void test_pack(void)
{
    uint8_t rec[PBS_SIZE], back[PBS_SIZE], file[PBS_SIZE * 2];
    size_t len = 0;
    for (int i = 0; i < PBS_SIZE; ++i) rec[i] = (uint8_t)(i * 7 + 3);
    CHECK(!pbs_checksum_ok(rec));                       /* not sealed yet */
    CHECK_EQ_INT(pbs_pack(rec, file, sizeof file, &len), QFS_OK);
    CHECK(pbs_read(back, file, len));
    CHECK(pbs_checksum_ok(back));                       /* pack seals */
    CHECK_MEM(back, rec, PBS_CHECKSUM_OFFSET);          /* nothing else changes */
    pbs_seal(rec);
    CHECK_MEM(back, rec, PBS_SIZE);
    CHECK_EQ_INT(pbs_pack(rec, file, 16, &len), QFS_ERR_OVERFLOW);
}

static void test_exchange(void)
{
    uint8_t rec[PBS_SIZE], edited[PBS_SIZE];
    for (int i = 0; i < PBS_SIZE; ++i) rec[i] = (uint8_t)(i * 7 + 3);
    pbs_seal(rec);
    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    CHECK(pbs_export(rec, &files, &err));
    const SetFile *tsv = fileset_find(&files, "specs.tsv");
    CHECK(tsv != NULL);
    memcpy(edited, rec, PBS_SIZE);
    CHECK(pbs_import(edited, &files, &err));
    CHECK_MEM(edited, rec, PBS_SIZE);                   /* unedited: identical */

    /* power (+0x3c) edited in the table lands in the record and the checksum follows */
    char *text = malloc(tsv->size + 64);
    memcpy(text, tsv->data, tsv->size);
    text[tsv->size] = 0;
    char *row = strstr(text, "\tpower\ti32\t");
    CHECK(row != NULL);
    FileSet mod;
    fileset_init(&mod);
    if (row != NULL) {
        char *value = row + strlen("\tpower\ti32\t");
        char *end = strchr(value, '\n');
        char patched[16384];
        snprintf(patched, sizeof patched, "%.*s123456%s", (int)(value - text), text, end);
        CHECK(fileset_add(&mod, "specs.tsv", (const uint8_t *)patched, strlen(patched)));
        memcpy(edited, rec, PBS_SIZE);
        CHECK(pbs_import(edited, &mod, &err));
        CHECK_EQ_INT((int32_t)(edited[0x3c] | edited[0x3d] << 8 | edited[0x3e] << 16 | (uint32_t)edited[0x3f] << 24), 123456);
        CHECK(pbs_checksum_ok(edited));
        CHECK_MEM(edited, rec, 0x3c);
        CHECK_MEM(edited + 0x40, rec + 0x40, PBS_CHECKED_BYTES - 0x40);
    }
    fileset_free(&mod);

    /* refusals name the file and line */
    const char *bad[] = { "0x03c\tpowr\ti32\t5\n", "0x03c\tpower\tu32\t5\n", "0x03c\tpower\ti32\t5x\n",
                          "0x03c\tpower\ti32\t5\t6\n", "0x03d\tpower\ti32\t5\n" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        FileSet f;
        fileset_init(&f);
        CHECK(fileset_add(&f, "specs.tsv", (const uint8_t *)bad[i], strlen(bad[i])));
        memcpy(edited, rec, PBS_SIZE);
        CHECK(!pbs_import(edited, &f, &err));
        CHECK(strstr(err.message, "specs.tsv:1") != NULL);
        CHECK_MEM(edited, rec, PBS_SIZE);               /* nothing applied on failure */
        fileset_free(&f);
    }
    free(text);
    fileset_free(&files);
}

static void test_cd(int argc, char **argv)
{
    for (int a = 1; a < argc; ++a) {
        FILE *f = fopen(argv[a], "rb");
        if (f == NULL) { CHECK(0); continue; }
        fseek(f, 0, SEEK_END);
        const long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t *buf = malloc((size_t)size);
        const int got = buf != NULL && fread(buf, 1, (size_t)size, f) == (size_t)size;
        fclose(f);
        uint8_t rec[PBS_SIZE];
        CHECK(got && pbs_read(rec, buf, (size_t)size));
        CHECK(got && pbs_checksum_ok(rec));
        uint8_t packed[PBS_SIZE * 2], again[PBS_SIZE];
        size_t packed_len = 0;
        CHECK(got && pbs_pack(rec, packed, sizeof packed, &packed_len) == QFS_OK);
        CHECK(got && pbs_read(again, packed, packed_len));
        CHECK(got && memcmp(again, rec, PBS_SIZE) == 0);  /* the CD record survives pack and read */
        if (got) {
            FileSet files;
            ExchangeError err = {{0}};
            uint8_t viaTsv[PBS_SIZE];
            fileset_init(&files);
            memcpy(viaTsv, rec, PBS_SIZE);
            CHECK(pbs_export(rec, &files, &err) && pbs_import(viaTsv, &files, &err));
            CHECK_MEM(viaTsv, rec, PBS_SIZE);
            fileset_free(&files);
        }
        free(buf);
    }
}

int main(int argc, char **argv)
{
    test_checksum();
    test_raw();
    test_qfs_stream();
    test_pack();
    test_exchange();
    test_cd(argc, argv);
    return TEST_REPORT();
}
