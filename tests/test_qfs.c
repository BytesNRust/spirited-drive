/* QFS decoder tests. Expected behaviour follows the game's own decoders:
 * dispatcher 004a2520, RefPack 004a823c, BTree 004a96f4, Huffman 004a83b0. */
#include "formats/qfs.h"
#include "test.h"

static void test_header(void)
{
    const uint8_t plain[] = { 0x10, 0xFB, 0x01, 0x02, 0x03, 0xFC };
    size_t size = 0;
    CHECK(qfs_is_compressed(plain, sizeof plain));
    CHECK_EQ_INT(qfs_decompressed_size(plain, sizeof plain, &size), QFS_OK);
    CHECK_EQ_INT(size, 0x010203);

    /* Bit 0 of the first byte: a 3-byte field precedes the size (game reads 5..7). */
    const uint8_t flagged[] = { 0x11, 0xFB, 0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x07, 0xFC };
    CHECK_EQ_INT(qfs_decompressed_size(flagged, sizeof flagged, &size), QFS_OK);
    CHECK_EQ_INT(size, 7);

    const uint8_t not_qfs[] = { 'S', 'H', 'P', 'I', 0, 0 };
    CHECK(!qfs_is_compressed(not_qfs, sizeof not_qfs));
    CHECK_EQ_INT(qfs_decompressed_size(not_qfs, sizeof not_qfs, &size), QFS_ERR_HEADER);
    CHECK(!qfs_is_compressed(plain, 4));
}

/* 0xE0..0xFB: (code & 0x1f) * 4 + 4 literal bytes. Stop code 0xFC..0xFF:
 * (code & 3) trailing literals. */
static void test_literals_and_stop(void)
{
    const uint8_t src[] = { 0x10, 0xFB, 0x00, 0x00, 0x06,
                            0xE0, 'a', 'b', 'c', 'd',
                            0xFE, 'e', 'f' };
    uint8_t out[6];
    CHECK_EQ_INT(qfs_decompress(src, sizeof src, out, sizeof out), QFS_OK);
    CHECK_MEM(out, "abcdef", 6);
}

/* 0x00..0x7F: 2 bytes. literals = code & 3, copy len = ((code >> 2) & 7) + 3,
 * offset = ((code >> 5) << 8) + b1 + 1. */
static void test_short_copy(void)
{
    const uint8_t src[] = { 0x10, 0xFB, 0x00, 0x00, 0x05,
                            0x02, 0x01, 'x', 'y',   /* 2 literals, copy 3 from offset 2 */
                            0xFC };
    uint8_t out[5];
    CHECK_EQ_INT(qfs_decompress(src, sizeof src, out, sizeof out), QFS_OK);
    CHECK_MEM(out, "xyxyx", 5);
}

/* 0x80..0xBF: 3 bytes. literals = b1 >> 6, copy len = (code & 0x3f) + 4,
 * offset = ((b1 & 0x3f) << 8) + b2 + 1. */
static void test_medium_copy(void)
{
    const uint8_t src[] = { 0x10, 0xFB, 0x00, 0x00, 0x06,
                            0x80, 0x40, 0x00, 'z',  /* 1 literal, copy 4 from offset 1 */
                            0xFD, '!' };
    uint8_t out[6];
    CHECK_EQ_INT(qfs_decompress(src, sizeof src, out, sizeof out), QFS_OK);
    CHECK_MEM(out, "zzzzz!", 6);
}

/* 0xC0..0xDF: 4 bytes. literals = code & 3, copy len = ((code >> 2) & 3) * 256 + b3 + 5,
 * offset = ((code & 0x10) << 12) + (b1 << 8) + b2 + 1. */
static void test_long_copy(void)
{
    const uint8_t src[] = { 0x10, 0xFB, 0x00, 0x00, 0x07,
                            0xC2, 0x00, 0x01, 0x00, 'p', 'q',  /* 2 literals, copy 5 from offset 2 */
                            0xFC };
    uint8_t out[7];
    CHECK_EQ_INT(qfs_decompress(src, sizeof src, out, sizeof out), QFS_OK);
    CHECK_MEM(out, "pqpqpqp", 7);

    /* (code >> 2) & 3 adds 256 to the length: 0xC4 -> 256 + 0 + 5 = 261 bytes. */
    const uint8_t src_long[] = { 0x10, 0xFB, 0x00, 0x01, 0x09,  /* 265 bytes */
                                 0x01, 0x00, 'k',              /* 1 literal + copy 3 from offset 1 */
                                 0xC4, 0x00, 0x00, 0x00,       /* copy 261 from offset 1 */
                                 0xFC };
    uint8_t out_long[265];
    CHECK_EQ_INT(qfs_decompress(src_long, sizeof src_long, out_long, sizeof out_long), QFS_OK);
    CHECK(out_long[0] == 'k' && out_long[132] == 'k' && out_long[264] == 'k');
}

/* BTree (46FB): clue byte, N pair nodes (node, left, right), then data where a
 * node expands recursively, clue+X emits X literally and clue+0 ends. */
static void test_btree(void)
{
    const uint8_t src[] = { 0x46, 0xFB, 0x00, 0x00, 0x07,
                            0xF0,                     /* clue */
                            0x02,                     /* 2 nodes */
                            0x80, 'a', 'b',
                            0x81, 0x80, 'c',          /* nested: 0x81 -> 0x80 c -> a b c */
                            'x', 0x80, 0x81, 0xF0, 0x80,
                            0xF0, 0x00 };
    uint8_t out[7];
    size_t size = 0;
    CHECK(qfs_is_compressed(src, sizeof src));
    CHECK_EQ_INT(qfs_decompressed_size(src, sizeof src, &size), QFS_OK);
    CHECK_EQ_INT(size, 7);
    CHECK_EQ_INT(qfs_decompress(src, sizeof src, out, sizeof out), QFS_OK);
    CHECK_MEM(out, "xababc\x80", 7);

    /* A node referring to itself must not recurse forever. */
    const uint8_t cyclic[] = { 0x46, 0xFB, 0x00, 0x00, 0x04, 0xF0, 0x01, 0x80, 0x80, 'a', 0x80, 0xF0, 0x00 };
    uint8_t out_cyclic[4];
    CHECK_EQ_INT(qfs_decompress(cyclic, sizeof cyclic, out_cyclic, sizeof out_cyclic), QFS_ERR_CORRUPT);

    /* Missing terminator. */
    const uint8_t truncated[] = { 0x46, 0xFB, 0x00, 0x00, 0x02, 0xF0, 0x00, 'a', 'b' };
    CHECK_EQ_INT(qfs_decompress(truncated, sizeof truncated, out, sizeof out), QFS_ERR_TRUNCATED);
}

/* Huffman (30FB): clue byte, per-length symbol counts (getnum) until the code
 * space is full, symbols as skips over unused byte values, then canonical
 * codes. clue + getnum n>0 repeats the last byte n times; clue + 0 + bit 0 +
 * 8 bits is a literal; clue + 0 + bit 1 ends. Built by hand: clue F0, symbols
 * 'A' (code 0) and F0 (code 1), data "AAA", run x3, literal 'z', end. */
static void test_huffman(void)
{
    uint8_t src[] = { 0x30, 0xFB, 0x00, 0x00, 0x07,
                      0xF0, 0xC1, 0x14, 0x16, 0x43, 0xF0, 0xF5, 0x90 };
    uint8_t out[7];
    CHECK(qfs_is_compressed(src, sizeof src));
    CHECK_EQ_INT(qfs_decompress(src, sizeof src, out, sizeof out), QFS_OK);
    CHECK_MEM(out, "AAAAAAz", 7);

    /* 32FB: same stream, then a running sum over the output. */
    src[0] = 0x32;
    const uint8_t delta[] = { 0x41, 0x82, 0xC3, 0x04, 0x45, 0x86, 0x00 };
    CHECK_EQ_INT(qfs_decompress(src, sizeof src, out, sizeof out), QFS_OK);
    CHECK_MEM(out, delta, 7);

    /* 34FB: running sum of the running sum. */
    src[0] = 0x34;
    const uint8_t delta2[] = { 0x41, 0xC3, 0x86, 0x8A, 0xCF, 0x55, 0x55 };
    CHECK_EQ_INT(qfs_decompress(src, sizeof src, out, sizeof out), QFS_OK);
    CHECK_MEM(out, delta2, 7);

    /* Stream cut before the end marker. */
    src[0] = 0x30;
    CHECK_EQ_INT(qfs_decompress(src, 10, out, sizeof out), QFS_ERR_TRUNCATED);

    /* A run with nothing decoded yet has no byte to repeat. */
    const uint8_t run_first[] = { 0x30, 0xFB, 0x00, 0x00, 0x03,
                                  0xF0, 0xC1, 0x14, 0x16, 0x5F, 0xFF, 0xFF };
    CHECK_EQ_INT(qfs_decompress(run_first, sizeof run_first, out, sizeof out), QFS_ERR_CORRUPT);
}

static void test_unsupported_codes(void)
{
    const uint8_t unknown[] = { 0x20, 0xFB, 0x00, 0x00, 0x01, 0xFC };
    uint8_t out[1];
    CHECK(!qfs_is_compressed(unknown, sizeof unknown));
    CHECK_EQ_INT(qfs_decompress(unknown, sizeof unknown, out, sizeof out), QFS_ERR_HEADER);
}

static void test_corrupt_input(void)
{
    uint8_t out[16];

    /* Copy reaching before the start of the output. */
    const uint8_t bad_offset[] = { 0x10, 0xFB, 0x00, 0x00, 0x03, 0x00, 0x05, 0xFC };
    CHECK_EQ_INT(qfs_decompress(bad_offset, sizeof bad_offset, out, sizeof out), QFS_ERR_OFFSET);

    /* Literal run longer than the remaining input. */
    const uint8_t truncated[] = { 0x10, 0xFB, 0x00, 0x00, 0x08, 0xE1, 'a', 'b' };
    CHECK_EQ_INT(qfs_decompress(truncated, sizeof truncated, out, sizeof out), QFS_ERR_TRUNCATED);

    /* Missing stop code. */
    const uint8_t no_stop[] = { 0x10, 0xFB, 0x00, 0x00, 0x04, 0xE0, 'a', 'b', 'c', 'd' };
    CHECK_EQ_INT(qfs_decompress(no_stop, sizeof no_stop, out, sizeof out), QFS_ERR_TRUNCATED);

    /* Output larger than the destination buffer. */
    const uint8_t big[] = { 0x10, 0xFB, 0x00, 0x00, 0x06, 0xE0, 'a', 'b', 'c', 'd', 0xFE, 'e', 'f' };
    CHECK_EQ_INT(qfs_decompress(big, sizeof big, out, 4), QFS_ERR_OVERFLOW);

    /* Stream produces fewer bytes than the header announces. */
    const uint8_t short_out[] = { 0x10, 0xFB, 0x00, 0x00, 0x09, 0xE0, 'a', 'b', 'c', 'd', 0xFC };
    CHECK_EQ_INT(qfs_decompress(short_out, sizeof short_out, out, sizeof out), QFS_ERR_SIZE);
}

int main(void)
{
    test_header();
    test_literals_and_stop();
    test_short_copy();
    test_medium_copy();
    test_long_copy();
    test_btree();
    test_huffman();
    test_unsupported_codes();
    test_corrupt_input();
    return TEST_REPORT();
}
