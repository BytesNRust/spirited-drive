/* PDN (CARFAMS/<name>.PDN) tests: the 0x1cc-byte record, raw or QFS-packed,
 * and its typed words (0040fa3c, 004407b8). With CD files: every one unpacks. */
#include "formats/pdn.h"
#include "formats/qfs.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put_i32(uint8_t *p, int32_t v)
{
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)((uint32_t)v >> (8 * i));
}

static void test_words(void)
{
    uint8_t rec[PDN_SIZE] = {0};
    put_i32(rec + PDN_MASS, 1500);
    put_i32(rec + PDN_AI_SPEEDS + 4 * 99, -7);
    put_i32(rec + PDN_FACTORS + 4 * 5, 0x10000);
    CHECK_EQ_INT(pdn_i32(rec, PDN_MASS), 1500);
    CHECK_EQ_INT(pdn_i32(rec, PDN_AI_SPEEDS + 4 * 99), -7);
    CHECK_EQ_INT(pdn_i32(rec, PDN_FACTORS + 4 * 5), 0x10000);
    CHECK_EQ_INT(PDN_AI_SPEEDS + 4 * PDN_AI_SPEED_COUNT, PDN_FACTORS); /* the table ends where the factors start */
}

static void test_raw(void)
{
    uint8_t rec[PDN_SIZE], out[PDN_SIZE];
    for (int i = 0; i < PDN_SIZE; ++i) rec[i] = (uint8_t)(i * 5);
    CHECK(pdn_read(out, rec, PDN_SIZE));
    CHECK_MEM(out, rec, PDN_SIZE);
    CHECK(!pdn_read(out, rec, PDN_SIZE - 1));
}

static void test_pack(void)
{
    uint8_t rec[PDN_SIZE], back[PDN_SIZE], file[PDN_SIZE * 2];
    size_t len = 0;
    for (int i = 0; i < PDN_SIZE; ++i) rec[i] = (uint8_t)(i * 5);
    CHECK_EQ_INT(pdn_pack(rec, file, sizeof file, &len), QFS_OK);
    CHECK(pdn_read(back, file, len));
    CHECK_MEM(back, rec, PDN_SIZE);
    CHECK_EQ_INT(pdn_pack(rec, file, 8, &len), QFS_ERR_OVERFLOW);
}

static void test_exchange(void)
{
    uint8_t rec[PDN_SIZE], again[PDN_SIZE];
    for (int i = 0; i < PDN_SIZE; ++i) rec[i] = (uint8_t)(i * 5 + 1);
    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    CHECK(pdn_export(rec, &files, &err));
    memcpy(again, rec, PDN_SIZE);
    memset(again, 0, PDN_SIZE);
    CHECK(pdn_import(again, &files, &err));
    CHECK_MEM(again, rec, PDN_SIZE);                    /* every byte is covered by a row */
    const SetFile *tsv = fileset_find(&files, "pdn.tsv");
    CHECK(tsv != NULL && strstr((const char *)tsv->data, "ai_speed") != NULL);
    fileset_free(&files);

    const char *bad = "0x014\tmass\ti32\t1\t2\n";
    fileset_init(&files);
    CHECK(fileset_add(&files, "pdn.tsv", (const uint8_t *)bad, strlen(bad)));
    CHECK(!pdn_import(again, &files, &err));
    CHECK(strstr(err.message, "pdn.tsv:1") != NULL);
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
        uint8_t rec[PDN_SIZE];
        CHECK(got && pdn_read(rec, buf, (size_t)size));
        CHECK(got && pdn_i32(rec, PDN_MASS) > 0);
        uint8_t packed[PDN_SIZE * 2], again[PDN_SIZE];
        size_t packed_len = 0;
        CHECK(got && pdn_pack(rec, packed, sizeof packed, &packed_len) == QFS_OK);
        CHECK(got && pdn_read(again, packed, packed_len));
        CHECK(got && memcmp(again, rec, PDN_SIZE) == 0);
        if (got) {
            FileSet files;
            ExchangeError err = {{0}};
            uint8_t viaTsv[PDN_SIZE];
            fileset_init(&files);
            memset(viaTsv, 0, PDN_SIZE);
            CHECK(pdn_export(rec, &files, &err) && pdn_import(viaTsv, &files, &err));
            CHECK_MEM(viaTsv, rec, PDN_SIZE);
            fileset_free(&files);
        }
        free(buf);
    }
}

int main(int argc, char **argv)
{
    test_words();
    test_raw();
    test_pack();
    test_exchange();
    test_cd(argc, argv);
    return TEST_REPORT();
}
