/* BNK exchange tests: an unedited export imports back to the original bytes (padding after the frames,
 * zero-length samples and shared data included); a longer WAV moves the following blocks and updates the
 * header; refusals carry file and line. With CD files all banks round-trip. */
#include "formats/bnk.h"
#include "formats/bnk_exchange.h"
#include "formats/wav.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put_u32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }

static void put_sample(uint8_t *b, int slot, uint32_t at, uint32_t flags, int bits, int channels, uint32_t length, uint32_t data)
{
    put_u32(b + 4 * slot, at);
    uint8_t *h = b + at;
    put_u32(h, flags);
    put_u32(h + 4, at + 0x28);
    h[0x15] = 0x80;
    h[0x1a] = 0x7f;
    memcpy(h + 0x28, "EACS", 4);
    put_u32(h + 0x2c, 16000);
    h[0x30] = (uint8_t)bits;
    h[0x31] = (uint8_t)channels;
    h[0x33] = 0xff;
    put_u32(h + 0x34, 3);
    put_u32(h + 0x38, 40);
    put_u32(h + 0x3c, length);
    put_u32(h + 0x40, data);
}

enum { SIZE = 0x400 };

/* slot 1: 16-bit stereo, 10 frames (40 bytes) + 8 bytes of padding at 0x320; slot 32: 8-bit mono, length 0,
 * 0x350..0x380; slot 33 shares slot 32's data (as 16-bit stereo); slot 40: 8-bit mono, 16 frames at 0x380 to the end (0x80 bytes, 112 of padding). */
static void build(uint8_t b[SIZE])
{
    memset(b, 0, SIZE);
    put_sample(b, 1, 0x200, 0x40, 2, 2, 10, 0x320);
    put_sample(b, 32, 0x248, 0x10, 1, 1, 0, 0x350);
    put_sample(b, 33, 0x290, 0x30, 2, 2, 0, 0x350);   /* the same bytes read as 16-bit stereo */
    put_sample(b, 40, 0x2d8, 0x10, 1, 1, 16, 0x380);
    for (int i = 0x320; i < SIZE; ++i) b[i] = (uint8_t)(i * 7);
}

static void test_identity_and_edit(void)
{
    uint8_t bank[SIZE], *back = NULL;
    size_t back_len = 0;
    build(bank);
    FileSet files;
    fileset_init(&files);
    ExchangeError err = {{0}};
    CHECK(bnk_export(bank, SIZE, &files, &err));
    CHECK(fileset_find(&files, "001.wav") && fileset_find(&files, "032.wav") && fileset_find(&files, "040.wav"));
    CHECK(fileset_find(&files, "033.wav") == NULL);                 /* shared with slot 32 */
    CHECK(fileset_find(&files, "001.tail") != NULL);                /* 8 padding bytes */
    CHECK(fileset_find(&files, "032.tail") == NULL);                /* length 0: the whole extent is the WAV */
    CHECK(fileset_find(&files, "040.tail") != NULL);
    CHECK(fileset_find(&files, "bank.tsv") != NULL);
    CHECK(bnk_import(bank, SIZE, &files, &back, &back_len, &err));
    CHECK(back_len == SIZE && memcmp(back, bank, SIZE) == 0);
    free(back);

    /* the WAV of slot 1 gets 5 more stereo frames: the blocks after it move by 20 bytes */
    uint8_t pcm[60];
    memset(pcm, 0x21, sizeof pcm);
    WavInfo info = { 22050, 16, 2, 15 };
    uint8_t *wav = NULL;
    size_t wav_len = 0;
    CHECK_EQ_INT(wav_write(pcm, &info, &wav, &wav_len), WAV_OK);
    FileSet next;
    fileset_init(&next);
    for (size_t i = 0; i < files.count; ++i) {
        if (strcmp(files.file[i].name, "001.wav") == 0) fileset_add_owned(&next, "001.wav", wav, wav_len);
        else fileset_add(&next, files.file[i].name, files.file[i].data, files.file[i].size);
    }
    CHECK(bnk_import(bank, SIZE, &next, &back, &back_len, &err));
    CHECK_EQ_INT(back_len, SIZE + 20);
    Bnk b;
    BnkSample s;
    CHECK_EQ_INT(bnk_open(&b, back, back_len), BNK_OK);
    CHECK_EQ_INT(bnk_sample(&b, 1, &s), BNK_OK);
    CHECK_EQ_INT(s.length, 15);                                     /* auto follows the WAV */
    CHECK_EQ_INT(s.rate, 22050);
    CHECK_EQ_INT(s.flags, 0x40);
    CHECK_EQ_INT(s.loop_start, 3);
    CHECK_EQ_INT(s.header[0x15], 0x80);                             /* opaque bytes kept */
    CHECK_EQ_INT(bnk_sample(&b, 32, &s), BNK_OK);
    CHECK_EQ_INT(s.data_offset, 0x350 + 20);
    CHECK_EQ_INT(s.length, 0);
    BnkSample s33;
    CHECK_EQ_INT(bnk_sample(&b, 33, &s33), BNK_OK);
    CHECK_EQ_INT(s33.data_offset, s.data_offset);                   /* still shared */
    free(back);
    fileset_free(&next);

    /* refusals */
    FileSet cut;
    fileset_init(&cut);
    for (size_t i = 0; i < files.count; ++i)
        if (strcmp(files.file[i].name, "032.wav") != 0) fileset_add(&cut, files.file[i].name, files.file[i].data, files.file[i].size);
    CHECK(!bnk_import(bank, SIZE, &cut, &back, &back_len, &err));
    CHECK(strstr(err.message, "032.wav") != NULL && strstr(err.message, "bank.tsv:") != NULL);
    fileset_free(&cut);
    fileset_free(&files);
    CHECK(!bnk_export((const uint8_t *)"short", 5, &files, &err));
    fileset_init(&files);
    CHECK(!bnk_import(bank, SIZE, &files, &back, &back_len, &err));
    fileset_free(&files);
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
    const int done = ok && bnk_export(buf, (size_t)size, &files, &err) && bnk_import(buf, (size_t)size, &files, &back, &back_len, &err);
    if (!done) fprintf(stderr, "%s: %s\n", path, err.message);
    if (!done || back_len != (size_t)size || memcmp(back, buf, (size_t)size) != 0) {
        fprintf(stderr, "%s: export then import does not reproduce the bank\n", path);
        CHECK(0);
    }
    free(back);
    free(buf);
    fileset_free(&files);
}

int main(int argc, char **argv)
{
    test_identity_and_edit();
    for (int a = 1; a < argc; ++a) check_file(argv[a]);
    return TEST_REPORT();
}
