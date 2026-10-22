/* The port's large text (text_soft.h): a dark edge down-right, a rim up-left, a hard shadow. */
#include "game/sys/text_soft.h"
#include "test.h"

/* a 3x3 letter at (2..4, 2..4) of an 8x8 mask */
static void square(uint8_t m[64], int level)
{
    for (int i = 0; i < 64; i++) m[i] = 0;
    for (int y = 2; y < 5; y++)
        for (int x = 2; x < 5; x++) m[y * 8 + x] = (uint8_t)level;
}

static void test_the_parts_of_a_letter(void)
{
    uint8_t m[64];
    square(m, 15);
    CHECK_EQ_INT(text_soft_edge(m, 8, 8, 2, 2), 0);
    CHECK_EQ_INT(text_soft_edge(m, 8, 8, 4, 3), 1);   /* right side */
    CHECK_EQ_INT(text_soft_edge(m, 8, 8, 3, 4), 1);   /* bottom */
    CHECK_EQ_INT(text_soft_back(m, 8, 8, 1, 1), TEXT_SOFT_RIM);
    CHECK_EQ_INT(text_soft_back(m, 8, 8, 1, 3), TEXT_SOFT_RIM);     /* left side */
    CHECK_EQ_INT(text_soft_back(m, 8, 8, 5, 5), TEXT_SOFT_SHADOW);
    CHECK_EQ_INT(text_soft_back(m, 8, 8, 6, 6), TEXT_SOFT_SHADOW);
    CHECK_EQ_INT(text_soft_back(m, 8, 8, 7, 7), TEXT_SOFT_NONE);   /* past the shadow's 2 px */
    CHECK_EQ_INT(text_soft_back(m, 8, 8, 0, 0), TEXT_SOFT_NONE);
}

/* a pixel less than half covered is not letter: no rim is cast from it */
static void test_half_covered_is_letter(void)
{
    uint8_t m[64];
    square(m, TEXT_SOFT_SOLID_FROM - 1);
    CHECK_EQ_INT(text_soft_back(m, 8, 8, 1, 1), TEXT_SOFT_NONE);
    square(m, TEXT_SOFT_SOLID_FROM);
    CHECK_EQ_INT(text_soft_back(m, 8, 8, 1, 1), TEXT_SOFT_RIM);
}

/* the shadow as dark as the letters cover the pixels it is pushed from */
static void test_the_shadow_follows_the_coverage(void)
{
    uint8_t m[64];
    square(m, 15);
    CHECK_EQ_INT(text_soft_shadow(m, 8, 8, 6, 6), TEXT_SOFT_SHADOW_LEVEL);
    CHECK_EQ_INT(text_soft_shadow(m, 8, 8, 7, 7), 0);
    square(m, 5);
    CHECK_EQ_INT(text_soft_shadow(m, 8, 8, 6, 6), (5 * TEXT_SOFT_SHADOW_LEVEL + 7) / 15);
}

/* the credits' header: its shadow 1 px down-right, and the underline measured from the letters' last row */
static void test_the_header(void)
{
    uint8_t m[64];
    square(m, 15);
    CHECK_EQ_INT(text_soft_header_shadow(m, 8, 8, 5, 5), 15);
    CHECK_EQ_INT(text_soft_header_shadow(m, 8, 8, 6, 6), 0);
    CHECK_EQ_INT(text_soft_header_shadow(m, 8, 8, 0, 0), 0);
    CHECK_EQ_INT(text_soft_last_row(m, 8, 8), 4);
    square(m, 0);
    CHECK_EQ_INT(text_soft_last_row(m, 8, 8), -1);
}

/* the grain: about a quarter of the pixels, the same each time */
static void test_the_grain(void)
{
    int count = 0;
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) count += text_soft_grain(x, y);
    CHECK(count > 64 * 64 / 6 && count < 64 * 64 / 3);
    CHECK_EQ_INT(text_soft_grain(17, 5), text_soft_grain(17, 5));
}

int main(void)
{
    test_the_parts_of_a_letter();
    test_half_covered_is_letter();
    test_the_shadow_follows_the_coverage();
    test_the_header();
    test_the_grain();
    return TEST_REPORT();
}
