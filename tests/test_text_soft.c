/* The port's large text (text_soft.h): hard letters, a dark edge down-right, a rim up-left, a hard shadow. */
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
    CHECK_EQ_INT(text_soft_part(m, 8, 8, 2, 2), TEXT_SOFT_FILL);
    CHECK_EQ_INT(text_soft_part(m, 8, 8, 4, 3), TEXT_SOFT_EDGE);    /* right side */
    CHECK_EQ_INT(text_soft_part(m, 8, 8, 3, 4), TEXT_SOFT_EDGE);    /* bottom */
    CHECK_EQ_INT(text_soft_part(m, 8, 8, 1, 1), TEXT_SOFT_RIM);
    CHECK_EQ_INT(text_soft_part(m, 8, 8, 1, 3), TEXT_SOFT_RIM);     /* left side */
    CHECK_EQ_INT(text_soft_part(m, 8, 8, 5, 5), TEXT_SOFT_SHADOW);
    CHECK_EQ_INT(text_soft_part(m, 8, 8, 6, 6), TEXT_SOFT_SHADOW);
    CHECK_EQ_INT(text_soft_part(m, 8, 8, 7, 7), TEXT_SOFT_NONE);   /* past the shadow's 2 px */
    CHECK_EQ_INT(text_soft_part(m, 8, 8, 0, 0), TEXT_SOFT_NONE);
}

/* hard letters: a faint level is outside, from TEXT_SOFT_SOLID_FROM on inside */
static void test_the_letters_are_hard(void)
{
    uint8_t m[64];
    square(m, TEXT_SOFT_SOLID_FROM - 1);
    CHECK_EQ_INT(text_soft_part(m, 8, 8, 3, 3), TEXT_SOFT_NONE);
    square(m, TEXT_SOFT_SOLID_FROM);
    CHECK_EQ_INT(text_soft_part(m, 8, 8, 3, 3), TEXT_SOFT_FILL);
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
    test_the_letters_are_hard();
    test_the_grain();
    return TEST_REPORT();
}
