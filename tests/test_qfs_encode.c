/* RefPack encoder tests: every stream must decode back to the input with the
 * port's decoder (the game's 004a823c), however it was cut into opcodes. The
 * in-place check follows 004a2520 (packed data moved to the end of a buffer of
 * unpacked + 0x400 bytes and unpacked from the front). With CD files: every
 * packed file there is unpacked, re-packed and compared. */
#include "formats/qfs.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t rng_state = 12345;

static uint8_t rnd(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return (uint8_t)(rng_state >> 24);
}

/* Compresses, checks the bound and the header, decodes and compares. Returns the packed size. */
static size_t round_trip(const uint8_t *in, size_t n)
{
    const size_t cap = qfs_compress_bound(n);
    uint8_t *packed = malloc(cap);
    uint8_t *back = malloc(n + 1);
    size_t packed_len = 0;
    CHECK(packed != NULL && back != NULL);
    if (packed == NULL || back == NULL) { free(packed); free(back); return 0; }
    CHECK_EQ_INT(qfs_compress(in, n, packed, cap, &packed_len), QFS_OK);
    CHECK(packed_len <= cap);
    CHECK(qfs_is_compressed(packed, packed_len));
    size_t size = 0;
    CHECK_EQ_INT(qfs_decompressed_size(packed, packed_len, &size), QFS_OK);
    CHECK_EQ_INT(size, n);
    CHECK_EQ_INT(qfs_decompress(packed, packed_len, back, n), QFS_OK);
    CHECK(n == 0 || memcmp(in, back, n) == 0);
    free(packed);
    free(back);
    return packed_len;
}

static void test_small(void)
{
    uint8_t data[300];
    for (size_t n = 0; n < 40; ++n) {
        for (size_t i = 0; i < n; ++i) data[i] = rnd();
        round_trip(data, n);
    }
    for (size_t n = 0; n < 40; ++n) {
        memset(data, 'x', n);
        round_trip(data, n);
    }
}

static void test_repeats_compress(void)
{
    enum { N = 100000 };
    uint8_t *zero = calloc(N, 1);
    CHECK(zero != NULL);
    if (zero == NULL) return;
    CHECK(round_trip(zero, N) < 400);  /* an overlapping copy of length 1028 per 4-byte opcode */
    free(zero);
}

/* Matches whose offset needs each of the three copy forms (<=1 KiB, <=16 KiB, <=128 KiB) and
 * whose length passes each form's maximum (10, 67, 1028). */
static void test_offsets_and_lengths(void)
{
    enum { N = 140000 };
    uint8_t *data = malloc(N);
    CHECK(data != NULL);
    if (data == NULL) return;
    for (size_t gap = 100; gap <= 131072; gap = gap < 1000 ? gap * 3 : gap * 8) {
        for (size_t i = 0; i < N; ++i) data[i] = rnd();
        const size_t len = gap + 3000 < N ? 3000 : 700;
        for (size_t i = 0; i < len && gap + i < (size_t)N; ++i) data[gap + i] = data[i];
        round_trip(data, N);
    }
    free(data);
}

static void test_random_mixed(void)
{
    enum { N = 70000 };
    uint8_t *data = malloc(N);
    CHECK(data != NULL);
    if (data == NULL) return;
    for (int round = 0; round < 6; ++round) {
        size_t i = 0;
        while (i < N) {
            const size_t run = 1 + rnd() % 40;
            if (rnd() & 1 && i > 50) {
                const size_t back = 1 + rnd() % (i < 20000 ? i : 20000);
                for (size_t k = 0; k < run && i < N; ++k, ++i) data[i] = data[i - back];
            } else {
                for (size_t k = 0; k < run && i < N; ++k, ++i) data[i] = rnd();
            }
        }
        round_trip(data, N);
    }
    free(data);
}

static void test_capacity(void)
{
    uint8_t data[64] = {0};
    uint8_t out[8];
    size_t len = 0;
    CHECK_EQ_INT(qfs_compress(data, sizeof data, out, sizeof out, &len), QFS_ERR_OVERFLOW);
    CHECK_EQ_INT(qfs_compress(data, 0x1000000, out, sizeof out, &len), QFS_ERR_OVERFLOW);  /* 24-bit size field */
}

/* 004a2520 unpacks in place. Rule 1: packed <= unpacked + 0x400. Rule 2: output written never
 * passes the input still unread. */
static void test_inplace(void)
{
    uint8_t zero[4096] = {0};
    uint8_t packed[600];
    size_t len = 0;
    CHECK_EQ_INT(qfs_compress(zero, sizeof zero, packed, sizeof packed, &len), QFS_OK);
    CHECK(qfs_inplace_ok(packed, len, sizeof zero));
    CHECK(!qfs_inplace_ok(packed, len, 10));           /* the stream does not unpack to that size */
    CHECK(!qfs_inplace_ok(zero, sizeof zero, 100));    /* not a QFS stream */

    enum { HEAD = 100000, TAIL = 140000 };
    uint8_t *data = malloc(HEAD + TAIL);
    CHECK(data != NULL);
    if (data == NULL) return;
    uint8_t *out = malloc(qfs_compress_bound(HEAD + TAIL));
    CHECK(out != NULL);
    if (out == NULL) { free(data); return; }
    for (int i = 0; i < TAIL; ++i) data[i] = rnd();
    CHECK_EQ_INT(qfs_compress(data, TAIL, out, qfs_compress_bound(TAIL), &len), QFS_OK);
    CHECK(len > TAIL + 0x400);                         /* random data grows by 1 byte per 112 */
    CHECK(!qfs_inplace_ok(out, len, TAIL));            /* rule 1 */

    memset(data, 0, HEAD);
    for (int i = 0; i < TAIL; ++i) data[HEAD + i] = rnd();
    CHECK_EQ_INT(qfs_compress(data, HEAD + TAIL, out, qfs_compress_bound(HEAD + TAIL), &len), QFS_OK);
    CHECK(len <= HEAD + TAIL + 0x400);                 /* rule 1 holds ... */
    CHECK(!qfs_inplace_ok(out, len, HEAD + TAIL));     /* ... rule 2 does not */
    free(data);
    free(out);
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

static void test_cd(int argc, char **argv)
{
    for (int a = 1; a < argc; ++a) {
        uint8_t *file = NULL;
        size_t size = 0;
        if (!read_all(argv[a], &file, &size)) { CHECK(0); free(file); continue; }
        size_t unpacked = 0;
        if (qfs_is_compressed(file, size) && qfs_decompressed_size(file, size, &unpacked) == QFS_OK) {
            uint8_t *plain = malloc(unpacked + 1);
            if (plain != NULL && qfs_decompress(file, size, plain, unpacked) == QFS_OK) round_trip(plain, unpacked);
            else CHECK(0);
            free(plain);
        } else {
            round_trip(file, size);  /* plain files compress too */
        }
        free(file);
    }
}

int main(int argc, char **argv)
{
    test_small();
    test_repeats_compress();
    test_offsets_and_lengths();
    test_random_mixed();
    test_capacity();
    test_inplace();
    test_cd(argc, argv);
    return TEST_REPORT();
}
