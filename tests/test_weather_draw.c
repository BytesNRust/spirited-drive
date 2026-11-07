/* The weather's picture passes (weather_draw.h): the rain's grey mood, a share of each 4x4 tile moved through a table. */
#include "game/race/weather_snow.h"
#include "game/viewer/weather_draw.h"
#include "tests/test.h"

#include <string.h>

static void test_mood_share_of_the_picture(void)
{
    uint8_t table[256], px[16 * 16], expect[16 * 16];
    for (int c = 0; c < 256; c++) table[c] = (uint8_t)(255 - c);   /* a table that moves every value */
    const int32_t box[4] = { 0, 0, 16, 16 };
    for (int share = 0; share <= 16; share += 4) {
        memset(px, 10, sizeof px);
        weather_mood_rows(px, 16, box, table, share);
        int moved = 0;
        for (int i = 0; i < 16 * 16; i++) moved += px[i] != 10;
        CHECK_EQ_INT(moved, share * 16);                               /* share of 16 in every 4x4 tile */
    }
    memset(px, 10, sizeof px), memset(expect, 10, sizeof expect);
    weather_mood_rows(px, 16, box, table, 7);
    uint8_t again[16 * 16];
    memset(again, 10, sizeof again);
    weather_mood_rows(again, 16, box, table, 7);
    CHECK(memcmp(px, again, sizeof px) == 0);                          /* the same picture each time: no noise from frame to frame */
}

static void test_mood_stays_in_its_box(void)
{
    uint8_t table[256], px[16 * 16];
    for (int c = 0; c < 256; c++) table[c] = 99;
    memset(px, 10, sizeof px);
    const int32_t box[4] = { 4, 5, 12, 9 };
    weather_mood_rows(px, 16, box, table, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            const int in = x >= 4 && x < 12 && y >= 5 && y < 9;
            CHECK_EQ_INT(px[y * 16 + x], in ? 99 : 10);                /* all of the box, nothing outside */
        }
}

static int snow_pixels(int level, int open, int scale, uint32_t tick, uint8_t *px, int w, int h, const int32_t box[4])
{
    uint8_t table[256];
    for (int c = 0; c < 256; c++) table[c] = 200;
    memset(px, 10, (size_t)w * (size_t)h);
    weather_snow_draw(px, w, box, scale, tick, level, open, table);
    int on = 0;
    for (int i = 0; i < w * h; i++) on += px[i] == 200;
    return on;
}

/* port (snow_weather): the flakes are drawn through the table, only inside the box, the storm's more than the snow's, none in a closed zone, the same picture at the same tick */
static void test_snow_flakes(void)
{
    enum { W = 320, H = 240 };
    static uint8_t a[W * H], b[W * H];
    const int32_t box[4] = { 20, 30, 300, 220 };
    const int light = snow_pixels(SNOW_LIGHT, 16, 1, 40, a, W, H, box), heavy = snow_pixels(SNOW_HEAVY, 16, 1, 40, b, W, H, box);
    CHECK(light > 10 && heavy > 2 * light);                    /* more flakes in the storm */
    CHECK_EQ_INT(snow_pixels(SNOW_NONE, 16, 1, 40, a, W, H, box), 0);
    CHECK_EQ_INT(snow_pixels(SNOW_HEAVY, 0, 1, 40, a, W, H, box), 0);   /* no snow in a tunnel */
    const int mouth = snow_pixels(SNOW_HEAVY, 8, 1, 40, a, W, H, box);
    CHECK(mouth > 0 && mouth < heavy);                         /* thinned out at its mouths */
    snow_pixels(SNOW_HEAVY, 16, 1, 40, a, W, H, box), snow_pixels(SNOW_HEAVY, 16, 1, 40, b, W, H, box);
    CHECK(memcmp(a, b, sizeof a) == 0);                        /* the same picture each time */
    snow_pixels(SNOW_HEAVY, 16, 1, 41, b, W, H, box);
    CHECK(memcmp(a, b, sizeof a) != 0);                        /* and it moves */
    snow_pixels(SNOW_HEAVY, 16, 2, 40, a, W, H, box);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (a[y * W + x] != 10) CHECK(x >= box[0] && x < box[2] && y >= box[1] && y < box[3]);   /* nothing outside the box, at any render scale */
    CHECK(snow_pixels(SNOW_LIGHT, 16, 2, 40, a, W, H, box) > light);   /* at a render scale each flake is `scale` pixels across */
}

int main(void)
{
    test_mood_share_of_the_picture();
    test_mood_stays_in_its_box();
    test_snow_flakes();
    return TEST_REPORT();
}
