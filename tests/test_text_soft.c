/* The port's soft text (text_soft.h): the shadow blurred and lighter, the letters' strokes lifted to full colour. */
#include "game/sys/text_soft.h"
#include "test.h"

static void test_the_shadow_spreads_and_softens(void)
{
    uint8_t in[5 * 5] = { 0 }, out[5 * 5];
    in[2 * 5 + 2] = 15;
    text_soft_shadow(in, out, 5, 5);
    /* one solid pixel spread by the 1-4-1 kernel at 90%: the centre keeps 16/36, a side 4/36, a corner 1/36 */
    CHECK_EQ_INT(out[2 * 5 + 2], 6);
    CHECK_EQ_INT(out[2 * 5 + 1], 2);
    CHECK_EQ_INT(out[1 * 5 + 1], 0);
    CHECK_EQ_INT(out[0], 0);
    uint8_t full[3 * 3], soft[3 * 3];
    for (int i = 0; i < 9; i++) full[i] = 15;
    text_soft_shadow(full, soft, 3, 3);
    CHECK_EQ_INT(soft[4], 14);   /* inside a solid area: 90% of 15 */
    CHECK_EQ_INT(soft[0], 9);    /* a corner sees 25 of the kernel's 36: 9.4 */
}

static void test_the_letters_are_lifted(void)
{
    CHECK_EQ_INT(text_soft_lift(0), 0);
    CHECK_EQ_INT(text_soft_lift(4), 6);
    CHECK_EQ_INT(text_soft_lift(10), 15);
    CHECK_EQ_INT(text_soft_lift(15), 15);
}

int main(void)
{
    test_the_shadow_spreads_and_softens();
    test_the_letters_are_lifted();
    return TEST_REPORT();
}
