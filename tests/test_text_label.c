/* The menus' labels (text_label.h): a coverage mask blended over what is under it, not cut to one bit. A grey palette (entry i is grey i)
 * makes every blend readable: the letters white on a mid-grey picture. */
#include "game/sys/palette.h"
#include "game/sys/text_label.h"
#include "test.h"

#include <string.h>

enum { W = 12, H = 8, GREY = 100, WHITE = 255, BLACK = 0 };

static void grey_palette(uint8_t pal[768], uint8_t usable[256])
{
    for (int i = 0; i < 256; i++) pal[3 * i] = pal[3 * i + 1] = pal[3 * i + 2] = (uint8_t)i, usable[i] = 1;
    palette_usable_set(usable);
}

/* a 4 x 4 block of letter, its left column half covered and its bottom row a quarter */
static void block(uint8_t mask[W * H])
{
    memset(mask, 0, W * H);
    for (int y = 1; y <= 4; y++)
        for (int x = 1; x <= 4; x++) mask[y * W + x] = x == 1 ? 8 : y == 4 ? 4 : 15;
}

static void test_the_edge_is_a_mix_not_a_bit(void)
{
    uint8_t pal[768], usable[256], mask[W * H], to[W * H];
    grey_palette(pal, usable);
    block(mask);
    memset(to, GREY, sizeof to);
    const TextLabelLook look = { WHITE, 0, -1, 0 };
    text_label_blit_mask(to, W, H, pal, mask, W, H, 0, 0, &look);
    CHECK_EQ_INT(to[2 * W + 3], WHITE);                 /* full coverage: the letter's colour */
    CHECK(to[2 * W + 1] > GREY && to[2 * W + 1] < WHITE);   /* half covered: between the picture and the letter */
    CHECK(to[4 * W + 3] > GREY && to[4 * W + 3] < to[2 * W + 3]);
    CHECK(to[4 * W + 3] < to[2 * W + 1] + 1 || to[4 * W + 3] > GREY);   /* a quarter covered is lighter than half */
    CHECK_EQ_INT(to[6 * W + 6], GREY);                  /* nothing covered: the picture */
}

static void test_a_shadow_goes_down_and_right_and_under_the_letters(void)
{
    uint8_t pal[768], usable[256], mask[W * H], to[W * H];
    grey_palette(pal, usable);
    block(mask);
    memset(to, GREY, sizeof to);
    const TextLabelLook look = { WHITE, 0, BLACK, 2 };
    text_label_blit_mask(to, W, H, pal, mask, W, H, 0, 0, &look);
    CHECK_EQ_INT(to[2 * W + 3], WHITE);   /* the letter is over its own shadow */
    CHECK(to[3 * W + 6] < GREY);          /* two pixels right of the block's last full column: darkened */
    CHECK(to[5 * W + 4] < GREY);          /* under it */
    CHECK_EQ_INT(to[1 * W + 6], GREY);    /* the row above the shadow: untouched */
}

static void test_relief_gives_the_items_grain_and_edge(void)
{
    uint8_t pal[768], usable[256], mask[W * H], to[W * H];
    grey_palette(pal, usable);
    block(mask);
    memset(to, GREY, sizeof to);
    const TextLabelLook look = { 200, 1, -1, 0 };
    text_label_blit_mask(to, W, H, pal, mask, W, H, 0, 0, &look);
    CHECK_EQ_INT(to[3 * W + 3], 200 * 14 / 16);   /* the edge facing down-right (the pixel down-right is not letter): 14/16 of the fill */
    int grain = 0, plain = 0;
    for (int y = 1; y <= 2; y++)   /* rows whose down-right neighbour is still solid */
        for (int x = 2; x <= 3; x++) {
            grain += to[y * W + x] == 200 * 15 / 16;
            plain += to[y * W + x] == 200;
        }
    CHECK(grain + plain == 4);   /* solid pixels away from the edge are the fill or its grain */
}

int main(void)
{
    test_the_edge_is_a_mix_not_a_bit();
    test_a_shadow_goes_down_and_right_and_under_the_letters();
    test_relief_gives_the_items_grain_and_edge();
    return TEST_REPORT();
}
