/* port (gauge_wheel): which frame is in front of the gauges, and which of its pixels (wheel_front.h). */
#include "game/race/wheel_front.h"
#include "test.h"

#include <string.h>

/* a shape 4 x 3 at (10, 20): the first row see-through, a 7 at (11, 21) and (13, 22), the rest see-through */
static void make(uint8_t s[0x10 + 12])
{
    memset(s, 0xff, 0x10 + 12);
    memset(s, 0, 0x10);
    s[4] = 4, s[6] = 3, s[0xc] = 10, s[0xe] = 20;
    s[0x10 + 1 * 4 + 1] = 7;
    s[0x10 + 2 * 4 + 3] = 7;
}

static void test_pixels(void)
{
    uint8_t s[0x10 + 12];
    make(s);
    CHECK(wheel_front_pixel(s, 11, 21));
    CHECK(wheel_front_pixel(s, 13, 22));
    CHECK(!wheel_front_pixel(s, 10, 20));   /* see-through */
    CHECK(!wheel_front_pixel(s, 12, 21));
    CHECK(!wheel_front_pixel(s, 9, 21));    /* outside the box */
    CHECK(!wheel_front_pixel(s, 14, 21));
    CHECK(!wheel_front_pixel(s, 11, 23));
    CHECK(!wheel_front_pixel(NULL, 11, 21));
}

static void test_the_frame_is_the_one_cockpit_frame_draws(void)
{
    static Cockpit c;
    memset(&c, 0, sizeof c);
    static uint8_t mark[4][COCKPIT_FRAMES];
    for (int row = 2; row < 6; row++)
        for (int f = 0; f < COCKPIT_FRAMES; f++) c.frames[row][f] = &mark[row - 2][f];
    /* plain track: steering negative is the left ('t'l row 2, 's'l row 3), positive the right (4, 5) */
    CHECK(wheel_front_shape(&c, 0, -3, 1, 0) == &mark[0][3]);
    CHECK(wheel_front_shape(&c, 1, -3, 1, 0) == &mark[1][3]);
    CHECK(wheel_front_shape(&c, 0, 3, 0, 0) == &mark[2][3]);
    CHECK(wheel_front_shape(&c, 1, 3, 0, 0) == &mark[3][3]);
    /* a mirrored track swaps them */
    CHECK(wheel_front_shape(&c, 0, -3, 1, 1) == &mark[2][3]);
    CHECK(wheel_front_shape(&c, 1, 3, 0, 1) == &mark[1][3]);
    /* frame 0 has no sign: the steering's tells the side */
    CHECK(wheel_front_shape(&c, 0, 0, 1, 0) == &mark[0][0]);
    CHECK(wheel_front_shape(&c, 0, 0, 0, 0) == &mark[2][0]);
    /* out of range, none */
    CHECK(wheel_front_shape(&c, 0, COCKPIT_FRAMES, 0, 0) == NULL);
    CHECK(wheel_front_shape(NULL, 0, 0, 0, 0) == NULL);
}

/* a 'w' shape 3 x 2 at (5, 4): colours 1..6, one see-through */
static void make_w(uint8_t s[0x10 + 6])
{
    memset(s, 0, 0x10);
    s[4] = 3, s[6] = 2, s[0xc] = 5, s[0xe] = 4;
    const uint8_t px[6] = { 1, 2, 3, 4, 0xff, 6 };
    memcpy(s + 0x10, px, 6);
}

static uint8_t art_pic[640 * 480];
static uint8_t zone_pic[640 * 480];

static void test_the_wheel_in_the_shown_frame(void)
{
    uint8_t w[0x10 + 6], t[0x10 + 12];
    make_w(w), make(t);
    memset(art_pic, 9, sizeof art_pic);
    art_pic[4 * 640 + 5] = 1;    /* the dash already holds this pixel: not the wheel */
    CHECK(!wheel_front_now(w, NULL, NULL, art_pic, 5, 4));
    CHECK(wheel_front_now(w, NULL, NULL, art_pic, 6, 4));    /* 2 over the dash's 9 */
    CHECK(!wheel_front_now(w, NULL, NULL, art_pic, 6, 5));   /* the see-through one */
    CHECK(wheel_front_now(NULL, t, NULL, art_pic, 11, 21));  /* the 't' frame is the wheel whatever the art holds */
    CHECK(!wheel_front_now(NULL, t, NULL, art_pic, 12, 21));
    CHECK(!wheel_front_now(NULL, NULL, NULL, art_pic, 6, 4));
}

static void test_the_zone_is_the_wheel_in_any_pose(void)
{
    static Cockpit c;
    memset(&c, 0, sizeof c);
    uint8_t w0[0x10 + 6], w1[0x10 + 6], t[0x10 + 12];
    make_w(w0), make_w(w1), make(t);
    w1[0x10 + 5] = 7;            /* in this pose the last pixel differs */
    c.frames[0][0] = w0, c.frames[0][1] = w1, c.frames[2][0] = t;
    memset(art_pic, 0, sizeof art_pic);
    for (int i = 0; i < 6; i++) art_pic[(4 + i / 3) * 640 + 5 + i % 3] = w0[0x10 + i] == 0xff ? 0 : w0[0x10 + i];   /* the straight pose is in the dash */
    wheel_front_zone(&c, art_pic, zone_pic);
    CHECK(zone_pic[5 * 640 + 7] == 1);    /* the pixel that changes with the steering */
    CHECK(zone_pic[4 * 640 + 5] == 0);    /* same in every pose: the dash's */
    CHECK(zone_pic[21 * 640 + 11] == 1);  /* the 't' frame's pixels */
    CHECK(zone_pic[22 * 640 + 13] == 1);
    CHECK(zone_pic[0] == 0);
}

/* shapes of every kind (boxes partly or wholly outside the view, empty ones, see-through pixels, pixels equal to the art's) and a NULL here and there: the mask is the per-pixel answers of
 * wheel_front_now and wheel_front_pixel at every pixel of the 640 x 480 view, for each way drive_wide.c asks (with the 'w' frame, without it) */
static uint32_t rng_state;
static int rnd(int n) { rng_state = rng_state * 1664525u + 1013904223u; return (int)((rng_state >> 8) % (uint32_t)n); }

static uint8_t *random_shape(uint8_t *buf)
{
    if (rnd(6) == 0) return NULL;
    const int w = rnd(8) == 0 ? 0 : rnd(70), h = rnd(8) == 0 ? 0 : rnd(50);
    const int left = rnd(700) - 30, top = rnd(520) - 30;
    memset(buf, 0, 0x10);
    buf[4] = (uint8_t)w, buf[5] = (uint8_t)(w >> 8), buf[6] = (uint8_t)h, buf[7] = (uint8_t)(h >> 8);
    buf[0xc] = (uint8_t)left, buf[0xd] = (uint8_t)(left >> 8), buf[0xe] = (uint8_t)top, buf[0xf] = (uint8_t)(top >> 8);
    for (int i = 0; i < w * h; i++) buf[0x10 + i] = rnd(3) == 0 ? 0xff : (uint8_t)rnd(4);
    return buf;
}

static void test_the_mask_is_the_per_pixel_answers(void)
{
    static uint8_t bw[0x10 + 70 * 50], bt[0x10 + 70 * 50], bs[0x10 + 70 * 50], mask[WHEEL_MASK_SIZE];
    long on = 0, off = 0;
    for (int seed = 0; seed < 60; seed++) {
        rng_state = 12345u + (uint32_t)seed * 7919u;
        const uint8_t *w = random_shape(bw), *t = random_shape(bt), *sh = random_shape(bs);
        const uint8_t *art = NULL;
        if (rnd(5) != 0) {
            for (int i = 0; i < 640 * 480; i++) art_pic[i] = (uint8_t)rnd(4);
            art = art_pic;
        }
        wheel_front_mask(w, t, sh, art, mask);
        int bad = 0;
        for (int y = 0; y < 480; y++)
            for (int x = 0; x < 640; x++) {
                const int m = mask[y * 640 + x];
                bad += ((m & WHEEL_MASK_FRONT) != 0) != (wheel_front_now(NULL, t, sh, art, x, y) != 0);
                bad += ((m & WHEEL_MASK_FRONT) != 0) != (wheel_front_pixel(t, x, y) || wheel_front_pixel(sh, x, y));
                bad += (m != 0) != (wheel_front_now(w, t, sh, art, x, y) != 0);
                if (m != 0) on++;
                else off++;
            }
        CHECK_EQ_INT(bad, 0);
    }
    CHECK(on > 10000 && off > 10000);   /* pixels on and off, or the comparison proves nothing */
}

int main(void)
{
    test_pixels();
    test_the_frame_is_the_one_cockpit_frame_draws();
    test_the_wheel_in_the_shown_frame();
    test_the_zone_is_the_wheel_in_any_pose();
    test_the_mask_is_the_per_pixel_answers();
    return TEST_REPORT();
}
