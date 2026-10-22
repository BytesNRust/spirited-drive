/* The port's mirror + rally art (fam_mirror.h): the rally's bitmaps the mirror's FAM holds flipped (the signs) are
 * flipped in place; the rest stay the rally's. */
#include "game/race/fam_mirror.h"
#include "test.h"

#include <string.h>

enum { ROOT = 12, SHPI_SIZE = 16 + 2 * 8 + 16 + 6 + 16 + 2, FILE_SIZE = ROOT + SHPI_SIZE };

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24);
}

/* root wwww [12] -> SHPI holding "0000" 3x2 and "0001" 2x1 */
static void build(uint8_t buf[FILE_SIZE], const uint8_t sign[6], const uint8_t other[2])
{
    memset(buf, 0, FILE_SIZE);
    memcpy(buf, "wwww", 4);
    put_u32(buf + 4, 1);
    put_u32(buf + 8, ROOT);
    uint8_t *s = buf + ROOT;
    memcpy(s, "SHPI", 4);
    put_u32(s + 4, SHPI_SIZE);
    put_u32(s + 8, 2);
    memcpy(s + 12, "GIMX", 4);
    memcpy(s + 16, "0000", 4);
    put_u32(s + 20, 32);
    memcpy(s + 24, "0001", 4);
    put_u32(s + 28, 32 + 16 + 6);
    s[32] = 0x7b, s[32 + 4] = 3, s[32 + 6] = 2;
    memcpy(s + 48, sign, 6);
    s[54] = 0x7b, s[54 + 4] = 2, s[54 + 6] = 1;
    memcpy(s + 70, other, 2);
}

static void test_the_flipped_sign_is_flipped_and_the_rest_kept(void)
{
    uint8_t rally[FILE_SIZE], mirror[FILE_SIZE];
    const uint8_t sign[6] = { 1, 2, 3, 4, 5, 6 }, flipped[6] = { 3, 2, 1, 6, 5, 4 };
    const uint8_t rally_other[2] = { 7, 8 }, mirror_other[2] = { 9, 9 };
    build(rally, sign, rally_other);
    build(mirror, flipped, mirror_other);
    CHECK_EQ_INT(fam_mirror_signs(rally, sizeof rally, mirror, sizeof mirror), 1);
    CHECK_MEM(rally + ROOT + 48, flipped, 6);
    CHECK_MEM(rally + ROOT + 70, rally_other, 2);
}

static void test_the_same_art_changes_nothing(void)
{
    uint8_t rally[FILE_SIZE], mirror[FILE_SIZE], before[FILE_SIZE];
    const uint8_t sign[6] = { 1, 2, 3, 4, 5, 6 }, other[2] = { 7, 8 };
    build(rally, sign, other);
    build(mirror, sign, other);
    memcpy(before, rally, sizeof before);
    CHECK_EQ_INT(fam_mirror_signs(rally, sizeof rally, mirror, sizeof mirror), 0);
    CHECK_MEM(rally, before, sizeof before);
}

static void test_a_damaged_mirror_changes_nothing(void)
{
    uint8_t rally[FILE_SIZE], mirror[8] = { 'w', 'w', 'w', 'w', 9, 0, 0, 0 }, before[FILE_SIZE];
    const uint8_t sign[6] = { 1, 2, 3, 4, 5, 6 }, other[2] = { 7, 8 };
    build(rally, sign, other);
    memcpy(before, rally, sizeof before);
    CHECK_EQ_INT(fam_mirror_signs(rally, sizeof rally, mirror, sizeof mirror), 0);
    CHECK_MEM(rally, before, sizeof before);
}

int main(void)
{
    test_the_flipped_sign_is_flipped_and_the_rest_kept();
    test_the_same_art_changes_nothing();
    test_a_damaged_mirror_changes_nothing();
    return TEST_REPORT();
}
