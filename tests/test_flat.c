/* Flat-colour polygon and line tests. Every checksum comes from the original
 * code (NFS_WIN.EXE 0048434e, 00484377, 00484389, 00484254) run under Unicorn
 * by `tools/flat_oracle.py golden`: the CRC-32 of the whole 640 x 480 surface
 * plus 8 guard rows above and below. The random comparison against the
 * original is `flat_oracle.py check`; this test needs no exe. */
#include "game/gfx/flat.h"
#include "test.h"

#include <stdlib.h>

enum { W = 640, H = 480, GUARD = 8, SIZE = W * (H + 2 * GUARD), MAX_POINTS = 8 };

typedef struct {
    int kind;  /* 0 flat_polygon, 2 flat_polygon_merge, 3 flat_line */
    int clip[4];
    uint32_t colour;
    int n;
    int32_t pts[2 * MAX_POINTS];
    uint32_t crc;
} Golden;

#define FULL 0, 0, W, H

/* tools/flat_cases.py fixed_cases(), in order. */
static const Golden golden[] = {
    { 0, { FULL }, 0x55, 4, { 20, 20, 40, 20, 40, 40, 20, 40 }, 0x65eba5feu },
    { 0, { FULL }, 0x66, 3, { 100, 10, 130, 50, 90, 40 }, 0xc4865a90u },
    { 2, { FULL }, 0x77, 5, { 160, 20, 200, 30, 190, 70, 150, 50, 140, 30 }, 0xf039bfa6u },
    { 0, { 30, 30, 50, 50 }, 0x88, 4, { 10, 10, 70, 20, 60, 80, 15, 70 }, 0xd5d1cabbu },
    { 0, { FULL }, 0x99, 2, { 250, 10, 250, 40 }, 0x2411c51du },
    { 0, { FULL }, 0xAA, 3, { 300, 5, 340, 5, 340, 5 }, 0x5bee6339u },
    { 0, { FULL }, 0xBB, 1, { 400, 400 }, 0xdf73f371u },
    { 3, { FULL }, 0xC1, 2, { 10, 100, 50, 100 }, 0xf86370b4u },
    { 3, { FULL }, 0xC2, 2, { 60, 100, 60, 140 }, 0x77ce1c66u },
    { 3, { FULL }, 0xC3, 2, { 70, 100, 110, 140 }, 0xcb1bb42au },
    { 3, { FULL }, 0xC4, 2, { 120, 100, 170, 112 }, 0xaae02670u },
    { 3, { FULL }, 0xC5, 2, { 180, 100, 192, 150 }, 0x8e166bbbu },
    { 3, { FULL }, 0xC6, 2, { 260, 100, 200, 112 }, 0xa31362f1u },
    { 3, { FULL }, 0xC7, 2, { 330, 100, 320, 160 }, 0x0e83a5eau },
    { 3, { 350, 100, 360, 110 }, 0xC8, 2, { 340, 95, 372, 120 }, 0x5107dacau },
    { 3, { FULL }, 0xC9, 2, { -30, -20, 30000, 40000 }, 0xc30d541eu },
    { 0, { FULL }, 0xD1, 5, { 64, 319, 60, 329, 90, 308, 95, 307, 72, 322 }, 0xd96acdd5u },
    { 2, { FULL }, 0xD1, 5, { 64, 319, 60, 329, 90, 308, 95, 307, 72, 322 }, 0xf18100cbu },
    { 0, { FULL }, 0xD2, 3, { 639, 10, 700, 20, 650, 40 }, 0x5bee6339u },
    { 0, { 100, 100, 200, 200 }, 0xD3, 3, { 50, 150, 250, 120, 180, 260 }, 0x24f5656eu },
    { 0, { 100, 100, 200, 200 }, 0xD4, 4, { 150, 50, 250, 150, 150, 250, 50, 150 }, 0x67035d2bu },
    { 0, { FULL }, 0xD5, 4, { 300, 300, 302, 360, 303, 360, 301, 300 }, 0x38210b10u },
    { 0, { FULL }, 0xD6, 4, { 320, 300, 400, 303, 400, 305, 320, 302 }, 0x7eca424fu },
    { 2, { FULL }, 0xD7, 4, { 320, 320, 400, 323, 400, 325, 320, 322 }, 0x5bc72712u },
    { 0, { FULL }, 0xD8, 8, { 450, 300, 470, 300, 485, 315, 485, 335, 470, 350, 450, 350, 435, 335, 435, 315 },
      0xc6d02844u },
};

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

static QuadTarget target(uint8_t *surface, int l, int t, int r, int b)
{
    return (QuadTarget){ surface + W * GUARD, W, l, t, r, b };
}

static const uint8_t *row(const uint8_t *surface, int x, int y)
{
    return surface + (size_t)(y + GUARD) * W + x;
}

static void draw(const Golden *g, const QuadTarget *t)
{
    switch (g->kind) {
    case 0: flat_polygon(t, g->colour, g->n, g->pts); break;
    case 2: flat_polygon_merge(t, g->colour, g->n, g->pts); break;
    default: flat_line(t, g->pts[0], g->pts[1], g->pts[2], g->pts[3], g->colour); break;
    }
}

static void test_against_original(void)
{
    for (size_t i = 0; i < sizeof golden / sizeof golden[0]; i++) {
        const Golden *g = &golden[i];
        uint8_t *s = new_surface();
        const QuadTarget t = target(s, g->clip[0], g->clip[1], g->clip[2], g->clip[3]);
        draw(g, &t);
        if (crc32(s, SIZE) != g->crc) fprintf(stderr, "golden case %zu\n", i);
        CHECK_EQ_INT(crc32(s, SIZE), g->crc);
        free(s);
    }
}

/* Both outline edges are drawn, so a square from (20,20) to (40,40) fills
 * 21 x 21 pixels, and one pixel outside each side keeps the pattern. */
static void test_square(void)
{
    uint8_t *s = new_surface();
    const QuadTarget t = target(s, FULL);
    const int32_t sq[8] = { 20, 20, 40, 20, 40, 40, 20, 40 };
    flat_polygon(&t, 0x55, 4, sq);
    for (int y = 20; y <= 40; y++)
        for (int x = 20; x <= 40; x++) CHECK_EQ_INT(*row(s, x, y), 0x55);
    CHECK_EQ_INT(*row(s, 19, 30), (uint8_t)((size_t)(19 + (30 + GUARD) * W) * 131u + 17u));
    CHECK_EQ_INT(*row(s, 41, 30), (uint8_t)((size_t)(41 + (30 + GUARD) * W) * 131u + 17u));
    CHECK_EQ_INT(*row(s, 30, 19), (uint8_t)((size_t)(30 + (19 + GUARD) * W) * 131u + 17u));
    CHECK_EQ_INT(*row(s, 30, 41), (uint8_t)((size_t)(30 + (41 + GUARD) * W) * 131u + 17u));
    free(s);
}

/* A point draws one pixel; a line draws both ends; only the low colour byte
 * is used. */
static void test_point_and_line(void)
{
    uint8_t *s = new_surface();
    const QuadTarget t = target(s, FULL);
    const int32_t p[2] = { 400, 400 };
    flat_polygon(&t, 0x1234AB, 1, p);
    CHECK_EQ_INT(*row(s, 400, 400), 0xAB);
    flat_line(&t, 10, 100, 50, 100, 0x11);
    CHECK_EQ_INT(*row(s, 10, 100), 0x11);
    CHECK_EQ_INT(*row(s, 50, 100), 0x11);
    CHECK(*row(s, 51, 100) != 0x11);
    flat_line(&t, 70, 100, 110, 140, 0x22);
    CHECK_EQ_INT(*row(s, 70, 100), 0x22);
    CHECK_EQ_INT(*row(s, 110, 140), 0x22);
    free(s);
}

/* The clip rectangle is exclusive on the right and bottom; nothing outside it
 * (guard rows included) changes however far the shape reaches. */
static void test_clip_bounds(void)
{
    uint8_t *s = new_surface();
    const uint8_t *before = new_surface();
    const QuadTarget t = target(s, 100, 100, 200, 200);
    const int32_t big[8] = { -100000, -100000, 100000, -100000, 100000, 100000, -100000, 100000 };
    flat_polygon(&t, 0x42, 4, big);
    flat_line(&t, -50, -50, 5000, 4000, 0x43);
    for (int y = -GUARD; y < H + GUARD; y++)
        for (int x = 0; x < W; x++) {
            const int inside = x >= 100 && x < 200 && y >= 100 && y < 200;
            if (inside) CHECK(*row(s, x, y) == 0x42 || *row(s, x, y) == 0x43);
            else CHECK_EQ_INT(*row(s, x, y), *row(before, x, y));
        }
    free((void *)before);
    free(s);
}

/* Targets the scan tables cannot hold, and empty input, draw nothing. */
static void test_rejected_input(void)
{
    uint8_t *s = new_surface();
    const uint32_t before = crc32(s, SIZE);
    const int32_t sq[8] = { 20, 20, 40, 20, 40, 40, 20, 40 };
    const QuadTarget good = target(s, FULL);
    const QuadTarget tall = target(s, 0, 0, W, FLAT_MAX_ROWS + 1);
    const QuadTarget empty = target(s, 5, 5, 5, 9);
    flat_polygon(&tall, 1, 4, sq);
    flat_polygon(&empty, 1, 4, sq);
    flat_polygon(&good, 1, 0, sq);
    flat_polygon(&good, 1, 4, NULL);
    flat_polygon(NULL, 1, 4, sq);
    flat_polygon_merge(&tall, 1, 4, sq);
    flat_line(&tall, 0, 0, 10, 10, 1);
    flat_line(NULL, 0, 0, 10, 10, 1);
    CHECK_EQ_INT(crc32(s, SIZE), before);
    free(s);
}

int main(void)
{
    test_against_original();
    test_square();
    test_point_and_line();
    test_clip_bounds();
    test_rejected_input();
    return TEST_REPORT();
}
