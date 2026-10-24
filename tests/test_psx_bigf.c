/* The PS1 car archive's reader on a synthetic archive: the BIGF member lookup, and the colours of a SHPP texture
 * directory counted through each 4bpp texture's CLUT (the "shad" texture and the non-4bpp ones are left out). No CD data:
 * every byte here is written by the test. */
#include "formats/psx_bigf.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static void put24(uint8_t *p, uint32_t v) { put16(p, v), p[2] = (uint8_t)(v >> 16); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v), put16(p + 2, v >> 16); }
static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v; }

enum {
    TEX_LEN = 66,            /* an image block of 18 bytes (16 + one 2-pixel row) and a CLUT block of 48 */
    CLUT_TYPE = 0x23, IMAGE_4BPP = 0x40, IMAGE_8BPP = 0x41,
    SHPP_HEAD = 16, SHPP_STRIDE = 8, SHPP_DIR = SHPP_HEAD + 3 * SHPP_STRIDE
};

/* A texture of two pixels, 1 and 2, through a CLUT with colour `c1` at index 1 and `c2` at index 2. */
static void texture(uint8_t *p, uint8_t type, uint16_t c1, uint16_t c2)
{
    memset(p, 0, TEX_LEN);
    p[0] = type, put24(p + 1, 18), put16(p + 4, 2), put16(p + 6, 1);
    p[16] = 0x21;   /* pixel 0 is the low nibble: 1; pixel 1 the high: 2 */
    uint8_t *c = p + 18;
    c[0] = CLUT_TYPE, put24(c + 1, 48), put16(c + 4, 16), put16(c + 6, 1);
    put16(c + 16 + 2 * 1, c1), put16(c + 16 + 2 * 2, c2);
}

/* A directory of three textures: J000 (colours 0x001f and 0x83e0, the STP bit set), shad (ignored), and J001 (8bpp,
 * skipped). Returns its size. */
static size_t directory(uint8_t *f)
{
    memset(f, 0, SHPP_DIR + 3 * TEX_LEN);
    memcpy(f, "SHPP", 4), put32(f + 8, 3);   /* the count sits at 8 */
    memcpy(f + 12, "GIMX", 4);
    const char *names[3] = { "J000", "shad", "J001" };
    for (int i = 0; i < 3; i++) {
        memcpy(f + SHPP_HEAD + SHPP_STRIDE * i, names[i], 4);
        put32(f + SHPP_HEAD + SHPP_STRIDE * i + 4, SHPP_DIR + TEX_LEN * i);
    }
    texture(f + SHPP_DIR, IMAGE_4BPP, 0x001f, 0x83e0);
    texture(f + SHPP_DIR + TEX_LEN, IMAGE_4BPP, 0x7c00, 0x7c00);
    texture(f + SHPP_DIR + 2 * TEX_LEN, IMAGE_8BPP, 0x0200, 0x0400);
    return SHPP_DIR + 3 * TEX_LEN;
}

static void test_member(void)
{
    uint8_t a[64];
    memset(a, 0, sizeof a);
    memcpy(a, "BIGF", 4), be32(a + 8, 1);   /* one member, its entry at 16: be32 offset, be32 size, the name */
    be32(a + 16, 30), be32(a + 20, 5);
    memcpy(a + 24, "x.psh", 6);
    memcpy(a + 30, "HELLO", 5);
    const uint8_t *data = NULL;
    size_t length = 0;
    CHECK(psx_bigf_member(a, 35, "x.psh", &data, &length));
    CHECK_EQ_INT(length, 5);
    CHECK(data == a + 30);
    CHECK(!psx_bigf_member(a, 35, "y.psh", &data, &length));
    CHECK(!psx_bigf_member(a, 35, "x.ps", &data, &length));   /* a prefix is not the name */
    CHECK(!psx_bigf_member(a, 20, "x.psh", &data, &length));  /* the entries run past the buffer */
}

static void test_colours(void)
{
    uint8_t f[SHPP_DIR + 3 * TEX_LEN];
    const size_t size = directory(f);
    uint32_t *counts = calloc(PSX_COLOURS, sizeof *counts);
    CHECK(counts != NULL);
    CHECK(psx_shpp_colours(f, size, counts));
    CHECK_EQ_INT(counts[0x001f], 1);    /* J000's pixel 1 */
    CHECK_EQ_INT(counts[0x03e0], 1);    /* J000's pixel 2, the STP bit cleared */
    CHECK_EQ_INT(counts[0x7c00], 0);    /* shad is not body */
    CHECK_EQ_INT(counts[0x0200], 0);    /* the 8bpp texture is skipped */
    CHECK(!psx_shpp_colours(f, 40, counts));   /* the directory is cut short */
    free(counts);
}

int main(void)
{
    test_member();
    test_colours();
    return TEST_REPORT();
}
