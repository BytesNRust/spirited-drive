/* The weather's picture passes (weather_draw.h): the rain's grey mood, a share of each 4x4 tile moved through a table. */
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

int main(void)
{
    test_mood_share_of_the_picture();
    test_mood_stays_in_its_box();
    return TEST_REPORT();
}
