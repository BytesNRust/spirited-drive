/* Quad and triangle mapper tests. Every expected value comes from the original
 * code (NFS_WIN.EXE 00487180, 00486950, 00487a10, 004879b0, 00486720, 004864f0, 00486400, 004858f0)
 * run under Unicorn by `tools/emu_oracle.py quad-golden`: the CRC-32 of the
 * whole 640 x 480 surface plus 8 guard rows above and below, and for two
 * cases the drawn pixels themselves. The random comparison against the
 * original is `emu_oracle.py quad-check`; this test needs no exe. */
#include "game/gfx/quad.h"
#include "test.h"

#include <stdlib.h>

enum { W = 640, H = 480, GUARD = 8, SIZE = W * (H + 2 * GUARD) };
#define FULL_CLIP 0, 0, W, H
#define UMAX(n) (((n) << 16) - 1)

static uint32_t crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static uint8_t *new_surface(void)
{
    uint8_t *s = malloc(SIZE);
    for (size_t i = 0; i < SIZE; i++) s[i] = (uint8_t)(i * 131u + 17u);
    return s;
}

static FshEntry texture(int w, int h, const uint8_t *pixels)
{
    FshEntry e = { .type = FSH_TYPE_BITMAP, .width = (uint16_t)w, .height = (uint16_t)h,
                   .payload = pixels, .payload_size = (size_t)(w * h) };
    return e;
}

static QuadTarget target(uint8_t *surface, int l, int t, int r, int b)
{
    return (QuadTarget){ surface + W * GUARD, W, l, t, r, b };
}

static const uint8_t *row(const uint8_t *surface, int x, int y)
{
    return surface + (size_t)(y + GUARD) * W + x;
}

static uint8_t tex4[16], tex6[36];

static void make_textures(void)
{
    for (int i = 0; i < 16; i++) tex4[i] = (uint8_t)(i + 1);
    for (int y = 0; y < 6; y++)
        for (int x = 0; x < 6; x++) tex6[y * 6 + x] = x == 2 ? 0xFF : (uint8_t)(10 * y + x + 1);
}

/* An exact 1:1 copy covers 8 x 8 pixels for corners (10,10) to (17,17): both
 * edges are inclusive, and the 4 x 4 texture is doubled. */
static void test_axis_aligned(void)
{
    uint8_t *s = new_surface();
    const FshEntry tex = texture(4, 4, tex4);
    const QuadTarget t = target(s, FULL_CLIP);
    const int32_t uv[8] = { 0, 0, UMAX(4), 0, UMAX(4), UMAX(4), 0, UMAX(4) };
    const int32_t screen[8] = { 10, 10, 17, 10, 17, 17, 10, 17 };
    quad_draw(&t, &tex, uv, screen, 0);
    CHECK_EQ_INT(crc32(s, SIZE), 0xf8b08d85u);

    static const uint8_t expect[8][8] = {
        { 1, 1, 1, 2, 2, 3, 3, 4 },      { 1, 1, 1, 2, 2, 3, 3, 4 },
        { 1, 1, 1, 2, 2, 3, 3, 4 },      { 5, 5, 5, 6, 6, 7, 7, 8 },
        { 5, 5, 5, 6, 6, 7, 7, 8 },      { 9, 9, 9, 10, 10, 11, 11, 12 },
        { 9, 9, 9, 10, 10, 11, 11, 12 }, { 13, 13, 13, 14, 14, 15, 15, 16 },
    };
    for (int y = 0; y < 8; y++) CHECK_MEM(row(s, 10, 10 + y), expect[y], 8);
    CHECK_EQ_INT(*row(s, 9, 10), (uint8_t)((size_t)(9 + (10 + GUARD) * W) * 131u + 17u));
    free(s);
}

/* The clip rectangle is exclusive on the right and bottom. */
static void test_clipped(void)
{
    uint8_t *s = new_surface();
    const FshEntry tex = texture(4, 4, tex4);
    const QuadTarget t = target(s, 12, 12, 20, 20);
    const int32_t uv[8] = { 0, 0, UMAX(4), 0, UMAX(4), UMAX(4), 0, UMAX(4) };
    const int32_t screen[8] = { 8, 8, 25, 8, 25, 25, 8, 25 };
    quad_draw(&t, &tex, uv, screen, 0);
    CHECK_EQ_INT(crc32(s, SIZE), 0x90a6b79du);
    static const uint8_t first[8] = { 1, 2, 2, 2, 2, 2, 3, 3 };
    static const uint8_t middle[8] = { 5, 6, 6, 6, 6, 6, 7, 7 };
    static const uint8_t last[8] = { 9, 10, 10, 10, 10, 10, 11, 11 };
    CHECK_MEM(row(s, 12, 12), first, 8);
    CHECK_MEM(row(s, 12, 13), middle, 8);
    CHECK_MEM(row(s, 12, 19), last, 8);
    free(s);
}

static void test_skewed_and_transparent(void)
{
    uint8_t *s = new_surface();
    FshEntry tex = texture(4, 4, tex4);
    QuadTarget t = target(s, FULL_CLIP);
    const int32_t uv4[8] = { 0, 0, UMAX(4), 0, UMAX(4), UMAX(4), 0, UMAX(4) };
    const int32_t skew[8] = { 20, 10, 30, 14, 24, 26, 12, 20 };
    quad_draw(&t, &tex, uv4, skew, 0);
    CHECK_EQ_INT(crc32(s, SIZE), 0x1dde942fu);
    free(s);

    /* Colour 0xFF leaves the destination alone; column 2 of tex6 is 0xFF. */
    s = new_surface();
    tex = texture(6, 6, tex6);
    t = target(s, FULL_CLIP);
    const int32_t uv6[8] = { 0, 0, UMAX(6), 0, UMAX(6), UMAX(6), 0, UMAX(6) };
    const int32_t rect[8] = { 40, 5, 51, 5, 51, 16, 40, 16 };
    quad_draw(&t, &tex, uv6, rect, 1);
    CHECK_EQ_INT(crc32(s, SIZE), 0xb9579efcu);
    /* Row 5 of the surface reads 1 1 1 2 2 then the untouched pattern at the transparent column. */
    CHECK_EQ_INT(*row(s, 44, 5), 2);
    CHECK_EQ_INT(*row(s, 45, 5), 0x98);
    free(s);
}

static void test_whole_texture(void)
{
    uint8_t *s = new_surface();
    FshEntry tex = texture(4, 4, tex4);
    QuadTarget t = target(s, FULL_CLIP);
    const int32_t a[8] = { 60, 10, 67, 10, 67, 17, 60, 17 };
    quad_draw_full(&t, &tex, a, 0);
    CHECK_EQ_INT(crc32(s, SIZE), 0x3560e95au);
    free(s);

    s = new_surface();
    tex = texture(6, 6, tex6);
    t = target(s, FULL_CLIP);
    const int32_t b[8] = { 80, 10, 91, 10, 91, 21, 80, 21 };
    quad_draw_full(&t, &tex, b, 1);
    CHECK_EQ_INT(crc32(s, SIZE), 0x56c1a002u);
    free(s);
}

static void test_two_triangles(void)
{
    uint8_t *s = new_surface();
    FshEntry tex = texture(6, 6, tex6);
    QuadTarget t = target(s, FULL_CLIP);
    const int32_t a[8] = { 100, 10, 109, 10, 109, 19, 100, 19 };
    quad_draw_full_tri(&t, &tex, a, 0);
    CHECK_EQ_INT(crc32(s, SIZE), 0x3b87a19bu);
    free(s);

    s = new_surface();
    t = target(s, FULL_CLIP);
    const int32_t b[8] = { 120, 10, 131, 14, 128, 26, 118, 20 };
    quad_draw_full_tri(&t, &tex, b, 1);
    CHECK_EQ_INT(crc32(s, SIZE), 0xd446c74fu);
    free(s);
}

/* A bare triangle, as the sky ring draws: pixel centres inside, the right
 * edge excluded, so the top row of this right triangle is 10 pixels wide. */
static void test_triangle(void)
{
    uint8_t *s = new_surface();
    FshEntry tex = texture(6, 6, tex6);
    QuadTarget t = target(s, FULL_CLIP);
    const int32_t uv[6] = { 0, 0, 5, 0, 5, 5 };
    const int32_t screen[6] = { 140, 10, 150, 10, 150, 20 };
    tri_draw(&t, &tex, uv, screen, 0);
    CHECK_EQ_INT(crc32(s, SIZE), 0x85212103u);
    static const uint8_t top[10] = { 0x01, 0x02, 0x02, 0xFF, 0xFF, 0x04, 0x04, 0x05, 0x05, 0x06 };
    CHECK_MEM(row(s, 140, 10), top, 10);
    free(s);

    s = new_surface();
    t = target(s, FULL_CLIP);
    const int32_t skew[6] = { 160, 10, 170, 14, 162, 24 };
    tri_draw(&t, &tex, uv, skew, 1);
    CHECK_EQ_INT(crc32(s, SIZE), 0xea321527u);
    free(s);

    /* all corners on one row: nothing drawn, no read past the corners */
    s = new_surface();
    t = target(s, FULL_CLIP);
    const uint32_t blank = crc32(s, SIZE);
    const int32_t flat[6] = { 140, 10, 150, 10, 145, 10 };
    tri_draw(&t, &tex, uv, flat, 0);
    CHECK_EQ_INT(crc32(s, SIZE), blank);
    free(s);
}

/* Bad targets and textures draw nothing instead of corrupting memory. */
static void test_rejected_input(void)
{
    uint8_t *s = new_surface();
    const uint32_t before = crc32(s, SIZE);
    const FshEntry tex = texture(4, 4, tex4);
    FshEntry palette = tex;
    palette.type = FSH_TYPE_PALETTE_8BIT;
    const int32_t uv[8] = { 0, 0, UMAX(4), 0, UMAX(4), UMAX(4), 0, UMAX(4) };
    const int32_t screen[8] = { 10, 10, 17, 10, 17, 17, 10, 17 };
    const QuadTarget good = target(s, FULL_CLIP);
    const QuadTarget empty = target(s, 5, 5, 5, 9);
    const QuadTarget tall = target(s, 0, 0, W, QUAD_MAX_ROWS + 1);
    quad_draw(&empty, &tex, uv, screen, 0);
    quad_draw(&tall, &tex, uv, screen, 0);
    quad_draw(&good, &palette, uv, screen, 0);
    quad_draw(&good, NULL, uv, screen, 0);
    quad_draw(NULL, &tex, uv, screen, 0);
    quad_draw_full_tri(&empty, &tex, screen, 0);
    tri_draw(&empty, &tex, uv, screen, 0);
    CHECK_EQ_INT(crc32(s, SIZE), before);
    free(s);
}

int main(void)
{
    make_textures();
    test_axis_aligned();
    test_clipped();
    test_skewed_and_transparent();
    test_whole_texture();
    test_two_triangles();
    test_triangle();
    test_rejected_input();
    return TEST_REPORT();
}
