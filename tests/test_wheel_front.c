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

int main(void)
{
    test_pixels();
    test_the_frame_is_the_one_cockpit_frame_draws();
    return TEST_REPORT();
}
