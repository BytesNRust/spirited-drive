/* dep59 step 4b: the CONTENT screen's colours matched in its background's own palette (game/frontend/content_backdrop.h): every entry but
 * the pointer's and the see-through one, whatever palette_usable() holds (it is the last picture's set, not this palette's: matched
 * through it, the darkened background came out in the wrong colours). */
#include "game/frontend/content_backdrop.h"
#include "game/sys/palette.h"
#include "test.h"

#include <stdint.h>
#include <string.h>

/* a grey ramp: entry i is (i, i, i), the pointer's and the see-through ones bright red */
static void ramp(uint8_t pal[768])
{
    for (int i = 0; i < 256; i++) pal[3 * i] = pal[3 * i + 1] = pal[3 * i + 2] = (uint8_t)i;
    for (int i = CONTENT_BACKDROP_POINTER; i < 256; i++) pal[3 * i] = 255, pal[3 * i + 1] = pal[3 * i + 2] = 0;
}

/* the global set as another picture leaves it: two entries alone */
static void narrow_usable(void)
{
    uint8_t usable[256] = { 0 };
    usable[0] = usable[250] = 1;
    palette_usable_set(usable);
}

static int distance(int a, int b) { return a > b ? a - b : b - a; }

/* each entry to the grey nearest its colour 3/10 of the way to the brown, not to what the narrow global set allows */
static void test_darken_in_own_palette(void)
{
    uint8_t pal[768], table[256];
    ramp(pal);
    narrow_usable();
    content_backdrop_darken(pal, 0x120e09, 3, 10, table);
    /* 100 toward (18, 14, 9): (70+5, 70+4, 70+3) = (75, 74, 73), whose nearest grey is 74 */
    CHECK(distance(table[100], 74) <= 1);
    CHECK(distance(table[200], 144) <= 1);   /* (140+5, 140+4, 140+3) */
    CHECK_EQ_INT(table[CONTENT_BACKDROP_POINTER], CONTENT_BACKDROP_POINTER);
    CHECK_EQ_INT(table[CONTENT_BACKDROP_CLEAR], CONTENT_BACKDROP_CLEAR);
    for (int i = 0; i < CONTENT_BACKDROP_POINTER; i++) CHECK(table[i] < CONTENT_BACKDROP_POINTER);
}

/* a flat colour: the nearest entry of the palette, never the pointer's or the see-through one even when it is the exact colour */
static void test_nearest_in_own_palette(void)
{
    uint8_t pal[768];
    ramp(pal);
    narrow_usable();
    CHECK_EQ_INT(content_backdrop_nearest(pal, 0x404040), 0x40);
    CHECK(content_backdrop_nearest(pal, 0xff0000) < CONTENT_BACKDROP_POINTER);
}

/* the usable set handed to a whole picture's match: all but the two */
static void test_usable(void)
{
    uint8_t usable[256];
    memset(usable, 7, sizeof usable);
    content_backdrop_usable(usable);
    for (int i = 0; i < CONTENT_BACKDROP_POINTER; i++) CHECK_EQ_INT(usable[i], 1);
    CHECK_EQ_INT(usable[CONTENT_BACKDROP_POINTER], 0);
    CHECK_EQ_INT(usable[CONTENT_BACKDROP_CLEAR], 0);
}

/* the card's panel: inside x0..x1, y0..y1 every pixel through the table, its one-pixel border the edge entry, the rest untouched */
static void test_panel(void)
{
    enum { W = 12, H = 9 };
    uint8_t px[W * H], table[256];
    for (int i = 0; i < W * H; i++) px[i] = (uint8_t)(100 + i % 50);
    for (int i = 0; i < 256; i++) table[i] = (uint8_t)(i / 2);
    content_backdrop_panel(px, W, H, table, 2, 3, 8, 7, 9);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const uint8_t was = (uint8_t)(100 + (y * W + x) % 50), now = px[y * W + x];
            const int in = x >= 2 && x <= 8 && y >= 3 && y <= 7, edge = in && (x == 2 || x == 8 || y == 3 || y == 7);
            CHECK_EQ_INT(now, edge ? 9 : in ? was / 2 : was);
        }
    /* clipped to the picture: a panel past its sides changes nothing outside it */
    uint8_t small[4 * 4];
    memset(small, 40, sizeof small);
    content_backdrop_panel(small, 4, 4, table, -3, -3, 9, 9, 9);
    for (int i = 0; i < 16; i++) CHECK_EQ_INT(small[i], 20);
}

int main(void)
{
    test_panel();
    test_darken_in_own_palette();
    test_nearest_in_own_palette();
    test_usable();
    return TEST_REPORT();
}
