/* The per-car cockpit's "big + 5 small" lights (004212d4, game/race/cockpit_draw.c):
 * the four FSH shapes draw at dash_points[2..7] based on the count 004b92d4 and the
 * toggle 004f9b48. The exe matches the oracle (tools/oracle_dash.py cplights_case)
 * across 200+ cases; this test pins the port's drawing semantics:
 *
 *   count == 0:    bgdk at point 2, smdk at points 3..7 (the count is just redrawn
 *                 when *shown == 0 already).
 *   count == N:    bglt at point 2 when big_lit != 0, bgdk when big_lit == 0;
 *                 smlt at the first min(count, 5) of 3..7 and smdk at the rest.
 *   *shown:        written to count on every call.
 *
 * The shapes are made here as 4x4 (small) and 8x4 (big) 8-bit shapes of unique
 * palette indices so a drawing test reads back the byte at each point. The
 * surface is 64x32, plenty for our points.
 *
 * No FSH bank load is required: the cockpit_draw.c helpers read the shapes'
 * +0x0c/+0x0e words as the (x, y) hotspot, which is the (0, 0) layout when the
 * hotspot is at the shape's top-left; we set them explicitly. */
#include "game/gfx/blit.h"
#include "game/gfx/quad.h"
#include "game/race/cockpit.h"

#include "test.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    /* the cockpit_draw.c constants */
    BIG_LIGHT = 2, FIRST_SMALL = 3, SMALL_LIGHTS = 5,
    /* the FSH 8-bit type */
    FSH_TYPE_8BIT = 0x7b,
    /* the header + FSH entry header sizes the blit code uses */
    FSH_HEADER = 0x10,
    ENTRY_HEADER = 0x10,
    /* small smlt/smdk: 4 wide x 4 tall; big bglt/bgdk: 8 wide x 4 tall */
    SMALL_W = 4, SMALL_H = 4,
    BIG_W = 8, BIG_H = 4,
    /* unique palette indices so the readback is exact */
    PIXEL_SMLT = 0x55, PIXEL_SMDK = 0x33,
    PIXEL_BGLT = 0xaa, PIXEL_BGDK = 0x11
};

/* build an 8-bit FSH entry — the same pointer the bank hands to blit_shape. blit.c reads:
 *   type at +0, block (24-bit) +1..3, width +4, height +6, place x +0xc, place y +0xe;
 *   pixels at SHAPE_PIXELS = 0x10. So the entry header is 16 bytes. */
static uint8_t *make_shape(int32_t w, int32_t h, uint8_t fill)
{
    int32_t pixels = w * h;
    int32_t size = 0x10 + pixels;
    uint8_t *b = (uint8_t *)malloc((size_t)size);
    memset(b, 0, (size_t)size);
    b[0] = FSH_TYPE_8BIT;
    b[4] = (uint8_t)(w & 0xff); b[5] = (uint8_t)(w >> 8);
    b[6] = (uint8_t)(h & 0xff); b[7] = (uint8_t)(h >> 8);
    /* place = (0, 0) so blitting at point 0,0 keeps the shape entirely inside the surface bounds
     * at the (x, y) the cockpit draw passes. */
    b[0xc] = 0, b[0xd] = 0, b[0xe] = 0, b[0xf] = 0;
    memset(b + 0x10, fill, (size_t)pixels);
    return b;
}

static uint8_t test_surface[64 * 32];

static void clear_surface(void)
{
    memset(test_surface, 0xff, sizeof test_surface);  /* 0xff = "untouched", not a pixel index */
}

/* the byte at a dash_point: only the shape's rectangle is over-written, so any
 * untouched pixel (0xff) tells us no draw happened at that point. */
static int pixel_at(int32_t x, int32_t y)
{
    if (x < 0 || x >= 64 || y < 0 || y >= 32) return -1;
    return test_surface[y * 64 + x];
}

/* the cockpit banks its shapes as 004f9728 ('smlt'), 004f972c ('smdk'),
 * 004f9730 ('bglt'), 004f9734 ('bgdk'); the port wires them in cockpit_load.c
 * into c->small_lit, c->small_dark, c->big_lit, c->big_dark. */
static void setup_cockpit(Cockpit *c)
{
    memset(c, 0, sizeof *c);
    c->small_lit = make_shape(SMALL_W, SMALL_H, PIXEL_SMLT);
    c->small_dark = make_shape(SMALL_W, SMALL_H, PIXEL_SMDK);
    c->big_lit = make_shape(BIG_W, BIG_H, PIXEL_BGLT);
    c->big_dark = make_shape(BIG_W, BIG_H, PIXEL_BGDK);
    /* dash_points[2..7]: spread out on the 64x32 surface */
    c->dash_points[0] = (CockpitPoint){ 0, 0 };
    c->dash_points[1] = (CockpitPoint){ 0, 0 };
    c->dash_points[BIG_LIGHT] = (CockpitPoint){ 4, 2 };
    for (int32_t i = 0; i < SMALL_LIGHTS; i++)
        c->dash_points[FIRST_SMALL + i] = (CockpitPoint){ 16 + 8 * i, 16 };
    c->light_count = COCKPIT_LIGHTS;  /* unused by cockpit_lights but the field exists */
}

static void teardown_cockpit(Cockpit *c)
{
    free(c->small_lit); free(c->small_dark); free(c->big_lit); free(c->big_dark);
}

/* the dash_points[2..7] all have a non-zero x range so a drawing test reads back via pixel_at */
static void assert_draws_at(const Cockpit *c, int32_t which_small, uint8_t fill)
{
    if (which_small >= 0) {
        /* the small shape is 4x4 starting at (dash_points[FIRST_SMALL + which_small].x,
         * dash_points[FIRST_SMALL + which_small].y); the centre pixel */
        const CockpitPoint p = c->dash_points[FIRST_SMALL + which_small];
        CHECK_EQ_INT(pixel_at(p.x + 1, p.y + 1), (int)fill);
    } else {
        /* the big shape is 8x4 starting at (dash_points[2].x, dash_points[2].y) */
        const CockpitPoint p = c->dash_points[BIG_LIGHT];
        CHECK_EQ_INT(pixel_at(p.x + 1, p.y + 1), (int)fill);
    }
}

int main(void)
{
    Cockpit c;
    setup_cockpit(&c);
    QuadTarget t = { test_surface, 64, 0, 0, 64, 32 };

    /* 0. smoke test: a single known blit at (4, 2) of an 8x4 0x55 shape writes 0x55 at (5, 3) */
    clear_surface();
    fprintf(stderr, "smoke: blit ret=%d pixel(5,3)=0x%02x (expect 0x55)\n",
            blit_shape(&t, c.big_lit, 4, 2, BLIT_OPAQUE), pixel_at(5, 3));
    fprintf(stderr, "smoke: shape bytes [0..7]=%02x %02x %02x %02x %02x %02x %02x %02x\n",
            c.big_lit[0], c.big_lit[1], c.big_lit[2], c.big_lit[3],
            c.big_lit[4], c.big_lit[5], c.big_lit[6], c.big_lit[7]);
    fprintf(stderr, "smoke: shape pixels [16..24]=%02x %02x %02x %02x %02x %02x %02x %02x\n",
            c.big_lit[0x10], c.big_lit[0x11], c.big_lit[0x12], c.big_lit[0x13],
            c.big_lit[0x14], c.big_lit[0x15], c.big_lit[0x16], c.big_lit[0x17]);
    fprintf(stderr, "smoke: surface at row 2 bytes 4..16 = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            test_surface[2*64+4], test_surface[2*64+5], test_surface[2*64+6], test_surface[2*64+7],
            test_surface[2*64+8], test_surface[2*64+9], test_surface[2*64+10], test_surface[2*64+11],
            test_surface[2*64+12], test_surface[2*64+13], test_surface[2*64+14], test_surface[2*64+15]);
    fprintf(stderr, "smoke: surface at row 3 bytes 4..16 = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            test_surface[3*64+4], test_surface[3*64+5], test_surface[3*64+6], test_surface[3*64+7],
            test_surface[3*64+8], test_surface[3*64+9], test_surface[3*64+10], test_surface[3*64+11],
            test_surface[3*64+12], test_surface[3*64+13], test_surface[3*64+14], test_surface[3*64+15]);
    /* 1. count == 0 with a *shown != 0 (e.g. the previous frame had lit something,
     *    and the chase ended). count != *shown so the inner branch fires;
     *    count == 0 takes the redraw path that paints 1x bgdk + 5x smdk. */
    clear_surface();
    int32_t shown = -1;
    cockpit_lights(&t, &c, /* count */ 0, /* big_lit */ 0, &shown);
    assert_draws_at(&c, -1, PIXEL_BGDK);  /* big dark */
    for (int32_t i = 0; i < SMALL_LIGHTS; i++) assert_draws_at(&c, (int)i, PIXEL_SMDK);
    CHECK_EQ_INT(shown, 0);

    /* 2. count == 3, big_lit set: big lit, smlt at first 3 of 5, smdk at last 2. */
    clear_surface();
    shown = 0;
    cockpit_lights(&t, &c, /* count */ 3, /* big_lit */ 1, &shown);
    assert_draws_at(&c, -1, PIXEL_BGLT);
    for (int32_t i = 0; i < 3; i++) assert_draws_at(&c, (int)i, PIXEL_SMLT);
    for (int32_t i = 3; i < 5; i++) assert_draws_at(&c, (int)i, PIXEL_SMDK);
    CHECK_EQ_INT(shown, 3);

    /* 3. count == 3, big_lit unset: big dark (bgdk), same smalls. The port's
     *    'big_lit != 0 ? c->big_lit : c->big_dark' gate is what toggles the big light. */
    clear_surface();
    shown = 0;
    cockpit_lights(&t, &c, /* count */ 3, /* big_lit */ 0, &shown);
    assert_draws_at(&c, -1, PIXEL_BGDK);
    for (int32_t i = 0; i < 3; i++) assert_draws_at(&c, (int)i, PIXEL_SMLT);
    for (int32_t i = 3; i < 5; i++) assert_draws_at(&c, (int)i, PIXEL_SMDK);
    CHECK_EQ_INT(shown, 3);

    /* 4. count == 5, big_lit set: all 5 smalls lit. */
    clear_surface();
    shown = 0;
    cockpit_lights(&t, &c, /* count */ 5, /* big_lit */ 1, &shown);
    assert_draws_at(&c, -1, PIXEL_BGLT);
    for (int32_t i = 0; i < 5; i++) assert_draws_at(&c, (int)i, PIXEL_SMLT);
    CHECK_EQ_INT(shown, 5);

    /* 5. count == 100: 'lit' caps to SMALL_LIGHTS = 5 (cockpit_draw.c line 29). */
    clear_surface();
    shown = 0;
    cockpit_lights(&t, &c, /* count */ 100, /* big_lit */ 1, &shown);
    assert_draws_at(&c, -1, PIXEL_BGLT);
    for (int32_t i = 0; i < 5; i++) assert_draws_at(&c, (int)i, PIXEL_SMLT);
    CHECK_EQ_INT(shown, 100);

    /* 6. same count again — shown already equals count, so the inner branch is skipped.
     *    *shown stays equal to count; the big is still drawn (count != 0 path). */
    clear_surface();
    cockpit_lights(&t, &c, /* count */ 5, /* big_lit */ 1, &shown);
    assert_draws_at(&c, -1, PIXEL_BGLT);
    /* the previous frame already wrote smlt; with the no-redraw path, surface stays at smlt. */
    for (int32_t i = 0; i < 5; i++) assert_draws_at(&c, (int)i, PIXEL_SMLT);

    /* 7. count transitioning from 5 -> 0: clears the 5 smalls (writes smdk). */
    clear_surface();
    shown = 5;
    cockpit_lights(&t, &c, /* count */ 0, /* big_lit */ 1, &shown);
    assert_draws_at(&c, -1, PIXEL_BGDK);
    for (int32_t i = 0; i < 5; i++) assert_draws_at(&c, (int)i, PIXEL_SMDK);
    CHECK_EQ_INT(shown, 0);

    teardown_cockpit(&c);
    return TEST_REPORT();
}
