/* BNK (the SOUNDBNK folder) tests on synthetic banks: the 128-offset table,
 * the 72-byte EACS headers, absent slots, the data extent of samples whose
 * stored length is 0, and rejection of truncation and bad headers. With CD
 * files: every sample header of every bank parses, except the one bank the
 * EXE never names (TESTBANK, and slot 70 of OSUPMB3D, 757 bytes short) whose data
 * ends past the file. */
#include "formats/bnk.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put_u32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

/* Header at `at` (also its table slot), data at `data`. */
static void put_sample(uint8_t *b, int slot, uint32_t at, uint32_t flags, int bits, int channels,
                       uint32_t length, uint32_t data)
{
    put_u32(b + 4 * slot, at);
    uint8_t *h = b + at;
    put_u32(h, flags);
    put_u32(h + 4, at + 0x28);
    h[0x15] = 0x80;
    memcpy(h + 0x28, "EACS", 4);
    put_u32(h + 0x2c, 16000);
    h[0x30] = (uint8_t)bits;
    h[0x31] = (uint8_t)channels;
    h[0x33] = 0xff;
    put_u32(h + 0x34, 3);   /* loop start */
    put_u32(h + 0x38, 40);  /* loop length */
    put_u32(h + 0x3c, length);
    put_u32(h + 0x40, data);
}

enum { BANK_SIZE = 0x400 };

/* slot 1: 16-bit stereo, 10 samples at 0x300 (40 bytes); slot 32: 8-bit mono, length 0, data at 0x340
 * running to the end of the file. */
static void build_bank(uint8_t b[BANK_SIZE])
{
    memset(b, 0, BANK_SIZE);
    put_sample(b, 1, 0x200, 0x40, 2, 2, 10, 0x300);
    put_sample(b, 32, 0x248, 0x10, 1, 1, 0, 0x340);
    for (int i = 0x300; i < BANK_SIZE; ++i) b[i] = (uint8_t)i;
}

static void test_parse(void)
{
    uint8_t b[BANK_SIZE];
    BnkSample s;
    build_bank(b);
    Bnk bnk;
    CHECK_EQ_INT(bnk_open(&bnk, b, BANK_SIZE), BNK_OK);
    CHECK_EQ_INT(bnk_sample(&bnk, 1, &s), BNK_OK);
    CHECK_EQ_INT(s.flags, 0x40);
    CHECK_EQ_INT(s.rate, 16000);
    CHECK_EQ_INT(s.bits, 16);
    CHECK_EQ_INT(s.channels, 2);
    CHECK_EQ_INT(s.loop_start, 3);
    CHECK_EQ_INT(s.loop_length, 40);
    CHECK_EQ_INT(s.length, 10);
    CHECK_EQ_INT(s.data_offset, 0x300);
    CHECK_EQ_INT(s.header_offset, 0x200);
    CHECK_EQ_INT(bnk_frame_bytes(&s), 4);
    CHECK_EQ_INT(s.header[0x15], 0x80);        /* opaque bytes kept verbatim */
    CHECK_EQ_INT(bnk_sample(&bnk, 2, &s), BNK_ABSENT);
    CHECK_EQ_INT(bnk_sample(&bnk, 128, &s), BNK_ERR_SLOT);
    CHECK_EQ_INT(bnk_sample(&bnk, -1, &s), BNK_ERR_SLOT);
}

static void test_extent(void)
{
    uint8_t b[BANK_SIZE];
    BnkSample s;
    build_bank(b);
    Bnk bnk;
    CHECK_EQ_INT(bnk_open(&bnk, b, BANK_SIZE), BNK_OK);
    bnk_sample(&bnk, 1, &s);
    CHECK_EQ_INT(bnk_data_extent(&bnk, &s), 0x40); /* up to the next data offset */
    bnk_sample(&bnk, 32, &s);
    CHECK_EQ_INT(bnk_data_extent(&bnk, &s), BANK_SIZE - 0x340); /* last one: up to the end */
    CHECK_EQ_INT(s.length, 0);
}

static void test_bad(void)
{
    uint8_t b[BANK_SIZE];
    BnkSample s;
    Bnk bnk;
    build_bank(b);
    CHECK_EQ_INT(bnk_open(&bnk, b, 511), BNK_ERR_TRUNCATED);
    CHECK_EQ_INT(bnk_open(&bnk, b, 0), BNK_ERR_TRUNCATED);

    memcpy(b + 0x200 + 0x28, "EACX", 4);
    CHECK_EQ_INT(bnk_open(&bnk, b, BANK_SIZE), BNK_OK);
    CHECK_EQ_INT(bnk_sample(&bnk, 1, &s), BNK_ERR_HEADER);

    build_bank(b);
    put_u32(b + 0x204, 0x1234); /* +4 does not point at EACS */
    CHECK_EQ_INT(bnk_open(&bnk, b, BANK_SIZE), BNK_OK);
    CHECK_EQ_INT(bnk_sample(&bnk, 1, &s), BNK_ERR_HEADER);

    build_bank(b);
    CHECK_EQ_INT(bnk_open(&bnk, b, 0x200 + 0x40), BNK_OK); /* header cut short */
    CHECK_EQ_INT(bnk_sample(&bnk, 1, &s), BNK_ERR_BOUNDS);

    build_bank(b);
    put_u32(b + 4 * 5, 0xfffffff0u); /* offset near 2^32 must not wrap */
    CHECK_EQ_INT(bnk_open(&bnk, b, BANK_SIZE), BNK_OK);
    CHECK_EQ_INT(bnk_sample(&bnk, 5, &s), BNK_ERR_BOUNDS);

    build_bank(b);
    put_u32(b + 0x200 + 0x3c, 1000); /* claims 4000 bytes, the file ends first */
    CHECK_EQ_INT(bnk_open(&bnk, b, BANK_SIZE), BNK_OK);
    CHECK_EQ_INT(bnk_sample(&bnk, 1, &s), BNK_ERR_DATA);

    build_bank(b);
    b[0x200 + 0x30] = 3; /* neither 8 nor 16 bit */
    CHECK_EQ_INT(bnk_open(&bnk, b, BANK_SIZE), BNK_OK);
    CHECK_EQ_INT(bnk_sample(&bnk, 1, &s), BNK_ERR_HEADER);
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
        Bnk bnk;
        const int short_bank = strstr(argv[a], "TESTBANK") != NULL || strstr(argv[a], "testbank") != NULL ||
                               strstr(argv[a], "OSUPMB3D") != NULL || strstr(argv[a], "osupmb3d") != NULL;
        CHECK(got && bnk_open(&bnk, buf, (size_t)size) == BNK_OK);
        int present = 0;
        for (int slot = 0; got && slot < BNK_SLOTS; ++slot) {
            BnkSample s;
            const BnkResult r = bnk_sample(&bnk, slot, &s);
            if (r == BNK_ABSENT) continue;
            ++present;
            if (short_bank && r == BNK_ERR_DATA) continue;
            CHECK_EQ_INT(r, BNK_OK);
            CHECK(s.rate == 16000 || s.rate == 16247 || s.rate == 22050);
        }
        CHECK(!got || present > 0);
        free(buf);
    }
}

int main(int argc, char **argv)
{
    test_parse();
    test_extent();
    test_bad();
    test_cd(argc, argv);
    return TEST_REPORT();
}
