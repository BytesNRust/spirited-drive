/* the shade table's re-match after the car colours fit (shade_fit.h) */
#include "tests/test.h"

#include "game/viewer/shade_fit.h"

#include <string.h>

static void set(uint8_t *rgbx, int e, int r, int g, int b)
{
    rgbx[4 * e] = (uint8_t)r, rgbx[4 * e + 1] = (uint8_t)g, rgbx[4 * e + 2] = (uint8_t)b;
}

static void test_refit(void)
{
    uint8_t before[1024] = { 0 }, shade[256], hidden[256] = { 0 }, enabled[256];
    memset(enabled, 1, sizeof enabled);
    for (int i = 0; i < 256; i++) shade[i] = (uint8_t)i;
    set(before, 10, 12, 16, 16);   /* road */
    set(before, 20, 8, 8, 8);      /* the darker road the shade table named */
    set(before, 21, 10, 10, 12);   /* near black, usable */
    set(before, 22, 240, 240, 240);
    hidden[20] = 1;                /* the fit gave 20 to a car (now blue paint) */
    shade[10] = 20;
    shade[30] = 22;                /* an entry the fit left alone */
    shade[20] = 20;                /* a hidden row: left as is */
    CHECK_EQ_INT(shade_refit(shade, before, hidden, enabled), 1);
    CHECK(shade[10] != 20);
    CHECK_EQ_INT(shade[10], 21);   /* the nearest usable to the old colour, not the paint */
    CHECK_EQ_INT(shade[30], 22);
    CHECK_EQ_INT(shade[20], 20);
    CHECK_EQ_INT(shade_refit(shade, before, hidden, enabled), 0);   /* again: nothing to do */
}

static void test_no_fit(void)
{
    uint8_t before[1024] = { 0 }, shade[256], hidden[256] = { 0 }, enabled[256], copy[256];
    memset(enabled, 1, sizeof enabled);
    for (int i = 0; i < 256; i++) shade[i] = (uint8_t)((i * 7) & 0xff);
    memcpy(copy, shade, sizeof copy);
    CHECK_EQ_INT(shade_refit(shade, before, hidden, enabled), 0);   /* car_colours off: no hidden entry */
    CHECK(memcmp(shade, copy, sizeof copy) == 0);
}

static void test_disabled_skipped(void)
{
    uint8_t before[1024] = { 0 }, shade[256], hidden[256] = { 0 }, enabled[256] = { 0 };
    for (int i = 0; i < 256; i++) shade[i] = (uint8_t)i;
    set(before, 20, 8, 8, 8), set(before, 40, 9, 9, 9), set(before, 50, 200, 0, 0);
    hidden[20] = 1, enabled[50] = 1, enabled[40] = 0;   /* 40 is set aside: only 50 is usable */
    shade[10] = 20;
    shade_refit(shade, before, hidden, enabled);
    CHECK_EQ_INT(shade[10], 50);
}

int main(void)
{
    test_refit();
    test_no_fit();
    test_disabled_skipped();
    return TEST_REPORT();
}
