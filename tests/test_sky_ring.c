/* Sky ring tests (0043abfc, 0043b298, 0043b0c8). The byte-exact check
 * against the original runs in tools/oracle_3d.py; these cover the layout
 * on hand-computed values. */
#include "game/race/sky_ring.h"
#include "test.h"

#include <math.h>
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
                       { 0x10000, 640, 0, 0, 640, 480 }, 640, 640, 0, 0, 0 };
    Count count = { 0 };
    const SkySink sink = { &count, on_triangle, on_fill, NULL };
    sky_ring_draw(&ring, &info, &frame, &h, &sink);
    /* four fills, then triangles in pairs; the first fill is the top left,
     * coloured with the first pixel of the bitmap */
    CHECK_EQ_INT(count.fills, 4);
    CHECK(count.triangles > 0 && count.triangles % 2 == 0);
    CHECK_EQ_INT(count.first_fill[0], 0);
    CHECK_EQ_INT(count.first_fill[1], 0);
    CHECK_EQ_INT(count.first_fill[4], 7);
}

typedef struct {
    int32_t left[64], right[64];
    int n;
    int32_t extreme;   /* the largest |x| drawn */
    int32_t edge_u;    /* the u of a corner drawn at x -16 (the left edge's clip), -1 none */
} Spans;

static void span_triangle(void *user, const FshEntry *texture, const int32_t uv[6], const int32_t screen[6])
{
    (void)texture, (void)uv;
    Spans *s = user;
    int32_t lo = screen[0], hi = screen[0];
    for (int i = 0; i < 3; i++) {
        const int32_t x = screen[2 * i];
        if (x == -16) s->edge_u = uv[2 * i];
        lo = x < lo ? x : lo, hi = x > hi ? x : hi;
        const int32_t a = x < 0 ? -x : x;
        s->extreme = a > s->extreme ? a : s->extreme;
    }
    if (s->n < 64) s->left[s->n] = lo, s->right[s->n] = hi, s->n++;
}

static void span_fill(void *user, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t colour)
{
    (void)user, (void)x, (void)y, (void)w, (void)h, (void)colour;
}

/* whether the triangles' x spans join up over [from, to) */
static int covers(const Spans *s, int32_t from, int32_t to)
{
    int32_t reach = from;
    for (int moved = 1; moved && reach < to;) {
        moved = 0;
        for (int i = 0; i < s->n; i++)
            if (s->left[i] <= reach && s->right[i] > reach) reach = s->right[i], moved = 1;
    }
    return reach >= to;
}

/* The u where the ring's chord crosses the ray to x = -16, worked out on the camera looking down +z: the cell's u
 * moved along with the chord from its end in view. */
static int32_t expected_edge_u(const SkyRing *ring, const View *view)
{
    const int32_t centre[3] = { 0, 0, 0x1000 }, side[3] = { 0x1000, 0, 0x1000 };
    int32_t c[2], sd[2];
    view_project(view, centre, c);
    view_project(view, side, sd);
    const double k = (-16.0 - c[0]) / (sd[0] - c[0]);   /* x / z along the ray */
    for (int i = 0; i < SKY_RING_POINTS - 1; i++) {
        const double ax = ring->points[i][0] >> 8, az = ring->points[i][2] >> 8;
        const double bx = ring->points[i + 1][0] >> 8, bz = ring->points[i + 1][2] >> 8;
        /* the end in view is the later point (the slice runs left to right); t from it towards the earlier */
        const double t = (k * bz - bx) / ((ax - bx) - k * (az - bz));
        if (!(t > 0.0 && t < 1.0) || bz <= 0.0 || ax > 0.0) continue;
        const int32_t *uv = ring->uv[0][i & 7];
        return (int32_t)lround(uv[2] + t * ((double)uv[0] - uv[2])) >> 16;
    }
    return -2;
}

/* port: a wide view (21:9 at a field of 146) reaches past the exe's seven slices; port_wide draws more, the ones
 * crossing the eye's plane or the cull limit clipped at the view's edges, so the ring covers the width */
static void test_wide(void)
{
    const FshEntry h = horz();
    TrackInfo info = { 0 };
    info.height_far = 48 << 16, info.radius_far = 96 << 16, info.ring = 1, info.ring_y_offset = -45,
    info.ring_height = 108;
    static SkyRing ring;
    static View view;
    sky_ring_build(&ring, &info, &h);
    /* 21:9's outside view with the field at +20, and at +0 (126) and +-6, where an end in front can fall past the cull
     * limit while the slice's other end is in view */
    static const int FIELDS[4][2] = { { 146, 116 }, { 126, 96 }, { 120, 90 }, { 132, 102 } };
    for (int v = 0; v < 4; v++) {
        view_init(&view, 0, 0, 1120, 480, FIELDS[v][0], FIELDS[v][1]);
        for (int32_t yaw = 0; yaw < 0x1000000; yaw += 0x1000000 / 37) {
            /* the camera turned to the yaw, as the window takes it */
            const double a = yaw * (2.0 * 3.14159265358979 / 0x1000000);
            const int32_t c = (int32_t)lround(cos(a) * 65536.0), s = (int32_t)lround(sin(a) * 65536.0);
            const int32_t matrix[9] = { c, 0, s, 0, 0x10000, 0, -s, 0, c };
            SkyFrame frame = { &view, matrix, { 0 }, yaw, 0, 0, 8, 0, { 560, 240 },
                               { 0x10000, 1120, 0, 0, 1120, 480 }, 640, 1120, 0, 0, 0 };
            Spans exe = { { 0 }, { 0 }, 0, 0, -1 }, port = { { 0 }, { 0 }, 0, 0, -1 };
            const SkySink exe_sink = { &exe, span_triangle, span_fill, NULL };
            const SkySink port_sink = { &port, span_triangle, span_fill, NULL };
            sky_ring_draw(&ring, &info, &frame, &h, &exe_sink);
            frame.port_wide = 1;
            sky_ring_draw(&ring, &info, &frame, &h, &port_sink);
            if (yaw == 0 && v == 0) CHECK(!covers(&exe, 0, 1120));   /* the exe's seven leave the edges */
            CHECK(covers(&port, 0, 1120));
            CHECK(port.extreme < 4096);
            if (yaw == 0 && v == 0) CHECK_EQ_INT(port.edge_u, expected_edge_u(&ring, &view));
        }
    }
}

int main(void)
{
    test_build();
    test_draw();
    test_wide();
    return TEST_REPORT();
}
