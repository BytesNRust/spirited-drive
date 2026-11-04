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

int main(void)
{
    test_pixels();
    test_the_frame_is_the_one_cockpit_frame_draws();
    test_the_wheel_in_the_shown_frame();
    test_the_zone_is_the_wheel_in_any_pose();
    return TEST_REPORT();
}
