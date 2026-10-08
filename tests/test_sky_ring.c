/* Sky ring tests (0043abfc, 0043b298, 0043b0c8). The byte-exact check
 * against the original runs in tools/oracle_3d.py; these cover the layout
 * on hand-computed values. */
#include "game/race/sky_ring.h"
#include "test.h"

#include <stdint.h>

enum { HORZ_W = 640, HORZ_H = 120 };

static uint8_t pixels[HORZ_W * HORZ_H];

typedef struct {
    int triangles, fills;
    int32_t first_fill[5];
} Count;

static void on_triangle(void *user, const FshEntry *texture, const int32_t uv[6], const int32_t screen[6])
{
    (void)texture, (void)uv, (void)screen;
    ((Count *)user)->triangles++;
}

static void on_fill(void *user, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t colour)
{
    Count *c = user;
    if (c->fills++ == 0) {
        const int32_t first[5] = { x, y, w, h, colour };
        for (int i = 0; i < 5; i++) c->first_fill[i] = first[i];
    }
}

static FshEntry horz(void)
{
    pixels[0] = 7;
    pixels[HORZ_W * HORZ_H - 1] = 9;
    return (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, HORZ_W, HORZ_H, { 0 }, pixels, sizeof pixels };
}

static void test_build(void)
{
    const FshEntry h = horz();
    TrackInfo info = { 0 };
    info.height_far = 48 << 16, info.radius_far = 96 << 16, info.vert_offset_far = 5, info.z_offset_far = 100;
    info.ring = 1;
    static SkyRing ring;
    sky_ring_build(&ring, &info, &h);
    CHECK_EQ_INT(ring.enabled, 1);
    CHECK_EQ_INT(ring.slices, SKY_RING_SLICES);
    /* point 0 faces +z; point 8 (half a turn) -z; points 16.. repeat 0.. */
    CHECK_EQ_INT(ring.points[0][0], 0);
    CHECK_EQ_INT(ring.points[0][1], 5);
    CHECK_EQ_INT(ring.points[0][2], (96 << 16) + 100);
    CHECK_EQ_INT(ring.points[8][2], -(96 << 16) + 100);
    CHECK_EQ_INT(ring.points[16][0], 0);
    /* cells are 79 texels wide (640 / 8 - 1); v runs from row 1 to H - 2 */
    CHECK_EQ_INT(ring.uv[0][0][0], 0);
    CHECK_EQ_INT(ring.uv[0][0][2], 79 << 16);
    CHECK_EQ_INT(ring.uv[0][1][0], 80 << 16);
    CHECK_EQ_INT(ring.uv[0][0][1], 0x10000);
    CHECK_EQ_INT(ring.uv[0][0][5], (HORZ_H - 2) << 16);
    /* the second set runs backwards: cell 0 starts on the last cell's end */
    CHECK_EQ_INT(ring.uv[1][0][0], (7 * 80 + 79) << 16);
    CHECK_EQ_INT(ring.uv[1][0][2], (7 * 80) << 16);
    info.ring = 0;
    sky_ring_build(&ring, &info, &h);
    CHECK_EQ_INT(ring.enabled, 1);
    CHECK_EQ_INT(ring.slices, 0);
    info.ring = 1, info.height_far = 0;
    sky_ring_build(&ring, &info, &h);
    CHECK_EQ_INT(ring.enabled, 0);
}

static void test_draw(void)
{
    const FshEntry h = horz();
    TrackInfo info = { 0 };
    info.height_far = 48 << 16, info.radius_far = 96 << 16, info.ring = 1, info.ring_y_offset = -45,
    info.ring_height = 108;
    static SkyRing ring;
    static View view;
    sky_ring_build(&ring, &info, &h);
    view_init(&view, 0, 0, 640, 480, 96, 96);
    const int32_t matrix[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };
    SkyFrame frame = { &view, matrix, { 0 }, 0x320000, 0, 0, 8, 0, { 320, 240 },
                       { 0x10000, 640, 0, 0, 640, 480 }, 640, 640, 0 };
    Count count = { 0 };
    const SkySink sink = { &count, on_triangle, on_fill };
    sky_ring_draw(&ring, &info, &frame, &h, &sink);
    /* four fills, then triangles in pairs; the first fill is the top left,
     * coloured with the first pixel of the bitmap */
    CHECK_EQ_INT(count.fills, 4);
    CHECK(count.triangles > 0 && count.triangles % 2 == 0);
    CHECK_EQ_INT(count.first_fill[0], 0);
    CHECK_EQ_INT(count.first_fill[1], 0);
    CHECK_EQ_INT(count.first_fill[4], 7);
}

int main(void)
{
    test_build();
    test_draw();
    return TEST_REPORT();
}
