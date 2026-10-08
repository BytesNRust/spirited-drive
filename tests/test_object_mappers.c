/* Object-engine mapper tests. Every expected CRC comes from the original code
 * (NFS_WIN.EXE 004aa510, 004aa600, 004ab060, 00486804, 004865d4, 004aaf14 and
 * the 15 table thunks 004a34e7 to 004a4205 that 00491a82 calls) run under
 * Unicorn by `tools/emu_oracle.py object-golden`: the CRC-32 of the whole
 * 640 x 480 surface plus 8 guard rows above and below. The random comparison
 * is `emu_oracle.py quad-check`; this test needs no exe. */
#include "game/gfx/object_mappers.h"
#include "test.h"

#include <stdlib.h>

enum { W = 640, H = 480, GUARD = 8, SIZE = W * (H + 2 * GUARD) };

static uint8_t tex6[36], shade[256];

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

static QuadTarget target(uint8_t *surface)
{
    return (QuadTarget){ surface + W * GUARD, W, 0, 0, W, H };
}

static void make_data(void)
{
    /* column 2 transparent (0xFF), column 3 shadow (0xFE) */
    for (int y = 0; y < 6; y++)
        for (int x = 0; x < 6; x++) tex6[y * 6 + x] = x == 2 ? 0xFF : x == 3 ? 0xFE : (uint8_t)(10 * y + x + 1);
    for (int i = 0; i < 256; i++) shade[i] = (uint8_t)(i * 7 + 3);
}

#define RECT(x0, y0, x1, y1) { x0, y0, x1, y0, x1, y1, x0, y1 }
static const int32_t skew[8] = { 20, 10, 31, 14, 26, 26, 12, 22 };
static const int32_t uv_tex[8] = { 0, 0, 5, 0, 5, 5, 0, 5 };
static const int32_t identity[8] = { 0, 1, 2, 3, 0, 1, 2, 3 };

static void check_surface(uint8_t *s, uint32_t crc, int line)
{
    test_checks++;
    if (crc32(s, SIZE) != crc) {
        test_failures++;
        fprintf(stderr, "%s:%d: surface crc %08x, expected %08x\n", __FILE__, line, crc32(s, SIZE), crc);
    }
    free(s);
}
#define CHECK_SURFACE(s, crc) check_surface((s), (crc), __LINE__)

/* The direct entry points: 004aa510, 004aa600, 004ab060, then the three inset
 * mappers with the index list reordered for the last. */
static void test_mappers(void)
{
    const FshEntry tex = texture(6, 6, tex6);
    uint8_t *s = new_surface();
    QuadTarget t = target(s);
    const int32_t tri_screen[6] = { 140, 10, 150, 10, 150, 20 };
    tri_draw_mode(&t, &tex, uv_tex, tri_screen, MAP_SHADE, shade);
    CHECK_SURFACE(s, 0x7b33b2dau);

    s = new_surface();
    t = target(s);
    const int32_t umax = (6 << 16) - 1;
    const int32_t uv16[8] = { 0, 0, umax, 0, umax, umax, 0, umax };
    const int32_t a[8] = RECT(40, 5, 51, 16);
    quad_draw_mode(&t, &tex, uv16, a, MAP_SHADE, shade);
    CHECK_SURFACE(s, 0x6f89fa50u);

    s = new_surface();
    t = target(s);
    const int32_t b[8] = RECT(60, 10, 71, 21);
    obj_draw_full(&t, &tex, b, MAP_SHADE, shade);
    CHECK_SURFACE(s, 0x304d6a08u);

    s = new_surface();
    t = target(s);
    const int32_t c[8] = RECT(80, 10, 89, 19);
    obj_draw_inset(&t, &tex, c, MAP_OPAQUE, NULL);
    CHECK_SURFACE(s, 0xb15a875bu);

    s = new_surface();
    t = target(s);
    obj_draw_inset(&t, &tex, skew, MAP_TRANSPARENT, NULL);
    CHECK_SURFACE(s, 0xbaa0d871u);

    s = new_surface();
    t = target(s);
    const int32_t d[8] = RECT(100, 30, 109, 39);
    const int32_t d_rev[8] = { d[6], d[7], d[4], d[5], d[2], d[3], d[0], d[1] };
    obj_draw_inset(&t, &tex, d_rev, MAP_SHADE, shade);
    CHECK_SURFACE(s, 0xf5229031u);
}

#define FLAGS_QUAD 0x1000
#define FLAGS_SPLIT 0x2000
#define MODE(m) ((m) << OBJ_MODE_SHIFT)

/* One case per table of the object engine, each mode, the object flag, and
 * mode 3 of the triangle table running into the quad table. */
static void test_polygons(void)
{
    const FshEntry tex = texture(6, 6, tex6);
    const struct { uint32_t flags; int32_t screen[8]; int object_flag; int32_t vi[4]; uint32_t crc; } poly[] = {
        { 3 | MODE(2), { 20, 10, 31, 14, 26, 26, 12, 22 }, 0, { 0, 1, 2, 3 }, 0x8b736e88u },
        { 4 | FLAGS_QUAD | MODE(1), RECT(40, 40, 51, 51), 0, { 0, 1, 2, 3 }, 0x609762fau },
        { 4 | FLAGS_QUAD | MODE(2), { 20, 10, 31, 14, 26, 26, 12, 22 }, 0, { 0, 1, 2, 3 }, 0x516d0d04u },
        { 4 | FLAGS_QUAD | FLAGS_SPLIT | MODE(2), { 20, 10, 31, 14, 26, 26, 12, 22 }, 0, { 0, 1, 2, 3 },
          0xbaa56b84u },
        { 4 | FLAGS_QUAD | FLAGS_SPLIT | MODE(1), RECT(60, 40, 71, 51), 0, { 3, 2, 1, 0 }, 0xf3b8be71u },
        { 4 | MODE(1), RECT(80, 40, 91, 51), 0, { 0, 1, 2, 3 }, 0x6fe70825u },
        { 4 | MODE(2), { 20, 10, 31, 14, 26, 26, 12, 22 }, 0, { 0, 1, 2, 3 }, 0x75093300u },
        { 4 | FLAGS_SPLIT | MODE(1), RECT(100, 40, 109, 49), 0, { 0, 1, 2, 3 }, 0xecac7a84u },
        { 4 | FLAGS_QUAD | MODE(0), RECT(120, 40, 131, 51), 1, { 0, 1, 2, 3 }, 0x62607900u },
        { 3 | MODE(3), RECT(140, 40, 151, 51), 0, { 0, 1, 2, 3 }, 0x7cd5932du },
    };
    for (size_t i = 0; i < sizeof poly / sizeof poly[0]; i++) {
        uint8_t *s = new_surface();
        const QuadTarget t = target(s);
        /* the uv list is the identity; the golden cases that reverse the vertex list
         * did so with the uv list 0 1 2 3 as well */
        const int32_t uv_index[4] = { 0, 1, 2, 3 };
        const int drawn = obj_draw_polygon(&t, &tex, poly[i].flags, poly[i].object_flag, uv_tex, 4,
                                           uv_index, poly[i].screen, 4, poly[i].vi, shade);
        CHECK_EQ_INT(drawn, 1);
        check_surface(s, poly[i].crc, __LINE__);
    }
}

/* 00491a82's table choice and its overflow into the next table. */
static void test_select(void)
{
    ObjMapper m = { 0 };
    CHECK(obj_select(3 | MODE(1), 0, &m) && m.shape == OBJ_TRIANGLE && m.mode == MAP_TRANSPARENT);
    CHECK(obj_select(4 | FLAGS_QUAD | MODE(2), 0, &m) && m.shape == OBJ_QUAD && m.mode == MAP_SHADE);
    CHECK(obj_select(4 | FLAGS_QUAD | FLAGS_SPLIT, 0, &m) && m.shape == OBJ_QUAD_TRIS && m.mode == MAP_OPAQUE);
    CHECK(obj_select(4 | FLAGS_QUAD, 1, &m) && m.shape == OBJ_QUAD_TRIS);
    CHECK(obj_select(4, 0, &m) && m.shape == OBJ_FULL);
    CHECK(obj_select(4 | FLAGS_SPLIT, 0, &m) && m.shape == OBJ_INSET);
    CHECK(obj_select(4, 1, &m) && m.shape == OBJ_INSET);
    /* mode 4 of the triangle table is entry 4 of 15: the quad table's mode 1 */
    CHECK(obj_select(3 | MODE(4), 0, &m) && m.shape == OBJ_QUAD && m.mode == MAP_TRANSPARENT);
    CHECK(obj_select(4 | FLAGS_SPLIT | MODE(2), 0, &m) && m.shape == OBJ_INSET && m.mode == MAP_SHADE);
    /* past the last table the original calls address 0 */
    CHECK(obj_select(4 | FLAGS_SPLIT | MODE(3), 0, &m) == 0);
    CHECK(obj_select(3 | MODE(15), 0, &m) == 0);
}

/* Colour 0xFE becomes shade[pixel under it], 0xFF leaves the pixel alone and
 * anything else is written, over a 3 x 3 patch. */
static void test_shade_semantics(void)
{
    const uint8_t fill[3] = { 0xFE, 0xFF, 0x07 };
    for (int f = 0; f < 3; f++) {
        const uint8_t texels[4] = { fill[f], fill[f], fill[f], fill[f] };
        const FshEntry tex = texture(2, 2, texels);
        uint8_t *s = new_surface();
        const QuadTarget t = target(s);
        const int32_t screen[8] = RECT(10, 10, 12, 12);
        uint8_t before[3][3];
        for (int y = 0; y < 3; y++)
            for (int x = 0; x < 3; x++) before[y][x] = s[(size_t)(y + 10 + GUARD) * W + 10 + x];
        obj_draw_full(&t, &tex, screen, MAP_SHADE, shade);
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                const uint8_t want = f == 0 ? shade[before[y][x]] : f == 1 ? before[y][x] : 0x07;
                CHECK_EQ_INT(s[(size_t)(y + 10 + GUARD) * W + 10 + x], want);
            }
        }
        free(s);
    }
}

/* Bad input draws nothing instead of corrupting memory. */
static void test_rejected_input(void)
{
    uint8_t *s = new_surface();
    const uint32_t before = crc32(s, SIZE);
    const FshEntry tex = texture(6, 6, tex6);
    FshEntry other = tex;
    other.type = FSH_TYPE_PALETTE_8BIT;
    const QuadTarget t = target(s);
    const int32_t screen[8] = RECT(20, 20, 40, 40);
    const int32_t bad_index[4] = { 0, 1, 2, 4 };
    const int32_t negative[4] = { 0, 1, -1, 3 };
    obj_draw_full(&t, &tex, screen, MAP_SHADE, NULL);                      /* no shadow table */
    obj_draw_inset(&t, &other, screen, MAP_OPAQUE, NULL);                   /* other texture type */
    obj_draw_quad(&t, &tex, uv_tex, screen, (MapMode)3, shade);             /* no such mode */
    CHECK_EQ_INT(obj_draw_polygon(&t, &tex, 4 | FLAGS_QUAD, 0, uv_tex, 4, identity, screen, 4, bad_index, shade), 0);
    CHECK_EQ_INT(obj_draw_polygon(&t, &tex, 4 | FLAGS_QUAD, 0, uv_tex, 4, identity, screen, 4, negative, shade), 0);
    CHECK_EQ_INT(obj_draw_polygon(&t, &tex, 4 | FLAGS_QUAD, 0, uv_tex, 4, bad_index, screen, 4, identity, shade), 0);
    CHECK_EQ_INT(obj_draw_polygon(&t, &tex, 4 | FLAGS_QUAD, 0, NULL, 0, NULL, screen, 4, identity, shade), 0);
    CHECK_EQ_INT(obj_draw_polygon(&t, &tex, 4 | FLAGS_SPLIT | MODE(9), 0, uv_tex, 4, identity, screen, 4, identity, shade), 0);
    CHECK_EQ_INT(crc32(s, SIZE), before);
    free(s);
}

int main(void)
{
    make_data();
    test_mappers();
    test_polygons();
    test_select();
    test_shade_semantics();
    test_rejected_input();
    return TEST_REPORT();
}
