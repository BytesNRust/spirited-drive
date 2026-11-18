/* dep59 step 5 (records by id + print): the digest a piece's print is made with (sha256_digest, piece_print.h): the SHA-256 of a stream
 * of bytes behind a domain's name, so a track's and a car's prints never meet. */
#include "game/sys/piece_print.h"
#include "game/sys/sha256.h"
#include "test.h"

#include <string.h>

static void test_the_digest(void)
{
    Sha256 s;
    uint8_t d[32];
    char hex[65];
    sha256_init(&s);
    sha256_update(&s, "abc", 3);
    sha256_digest(&s, d);
    sha256_init(&s);
    sha256_update(&s, "abc", 3);
    sha256_hex(&s, hex);
    static const uint8_t ABC[32] = { 0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                                      0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad };
    CHECK_MEM(d, ABC, 32);   /* FIPS 180-4's "abc" */
    CHECK(strncmp(hex, "ba7816bf8f01cfea", 16) == 0);
}

static void test_the_domains(void)
{
    uint8_t a[PIECE_PRINT_SIZE], b[PIECE_PRINT_SIZE], c[PIECE_PRINT_SIZE], zero[PIECE_PRINT_SIZE] = { 0 };
    PiecePrint p;
    piece_print_begin(&p, PIECE_PRINT_TRACK);
    piece_print_add(&p, "stream", 6);
    piece_print_end(&p, a);
    piece_print_begin(&p, PIECE_PRINT_CAR);
    piece_print_add(&p, "stream", 6);
    piece_print_end(&p, b);
    CHECK(memcmp(a, b, sizeof a) != 0);       /* the same bytes, another domain: another print */
    piece_print_begin(&p, PIECE_PRINT_TRACK);
    piece_print_add(&p, "str", 3);            /* the stream in pieces: the same print */
    piece_print_add(&p, "eam", 3);
    piece_print_end(&p, c);
    CHECK_MEM(a, c, sizeof a);
    CHECK(memcmp(a, zero, sizeof a) != 0);    /* never all zeros: that is "no print" */
    CHECK(!piece_print_is_none(a) && piece_print_is_none(zero));
    piece_print_begin(&p, PIECE_PRINT_TRACK);
    piece_print_end(&p, c);                   /* an empty stream still has a print */
    CHECK(!piece_print_is_none(c));
}

/* an update changes a piece's rules when it had a print and has another; a piece with none either side is not told as changed */
static void test_a_change_of_print(void)
{
    uint8_t a[PIECE_PRINT_SIZE], b[PIECE_PRINT_SIZE], none[PIECE_PRINT_SIZE] = { 0 };
    memset(a, 1, sizeof a), memset(b, 2, sizeof b);
    CHECK(piece_print_changed(a, b));
    CHECK(!piece_print_changed(a, a));
    CHECK(!piece_print_changed(none, b));   /* not there before: nothing to keep records of */
    CHECK(!piece_print_changed(a, none));   /* not there after: nothing new to start */
    CHECK(!piece_print_changed(none, none));
}

int main(void)
{
    test_a_change_of_print();
    test_the_digest();
    test_the_domains();
    return TEST_REPORT();
}
