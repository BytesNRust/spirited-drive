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
    CHECK_EQ_INT(fam_mirror_signs(rally, sizeof rally, rally, sizeof rally, mirror, sizeof mirror), 1);
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
    CHECK_EQ_INT(fam_mirror_signs(rally, sizeof rally, rally, sizeof rally, mirror, sizeof mirror), 0);
    CHECK_MEM(rally, before, sizeof before);
}

static void test_a_damaged_mirror_changes_nothing(void)
{
    uint8_t rally[FILE_SIZE], mirror[8] = { 'w', 'w', 'w', 'w', 9, 0, 0, 0 }, before[FILE_SIZE];
    const uint8_t sign[6] = { 1, 2, 3, 4, 5, 6 }, other[2] = { 7, 8 };
    build(rally, sign, other);
    memcpy(before, rally, sizeof before);
    CHECK_EQ_INT(fam_mirror_signs(rally, sizeof rally, rally, sizeof rally, mirror, sizeof mirror), 0);
    CHECK_MEM(rally, before, sizeof before);
}

/* evening + mirror: the evening's art flipped where the mirror's is the day's flipped */
static void test_the_reference_decides_the_flip(void)
{
    uint8_t timed[FILE_SIZE], day[FILE_SIZE], mirror[FILE_SIZE];
    const uint8_t dark[6] = { 11, 12, 13, 14, 15, 16 }, sign[6] = { 1, 2, 3, 4, 5, 6 }, flipped[6] = { 3, 2, 1, 6, 5, 4 };
    const uint8_t other[2] = { 7, 8 };
    const uint8_t dark_flipped[6] = { 13, 12, 11, 16, 15, 14 };
    build(timed, dark, other);
    build(day, sign, other);
    build(mirror, flipped, other);
    CHECK_EQ_INT(fam_mirror_signs(timed, sizeof timed, day, sizeof day, mirror, sizeof mirror), 1);
    CHECK_MEM(timed + ROOT + 48, dark_flipped, 6);
}

/* root wwww [terrain wwww [SHPI "!pal" + "0000" 2x1]] */
enum { T_SHPI = 12 + 12, T_PAL = 16 + 2 * 8, T_BMP = T_PAL + 16 + 768, T_SIZE = T_SHPI + T_BMP + 16 + 2 };

static void build_track(uint8_t buf[T_SIZE], const uint8_t pal[768], const uint8_t px[2])
{
    memset(buf, 0, T_SIZE);
    memcpy(buf, "wwww", 4), put_u32(buf + 4, 1), put_u32(buf + 8, 12);
    memcpy(buf + 12, "wwww", 4), put_u32(buf + 16, 1), put_u32(buf + 20, 12);
    uint8_t *s = buf + T_SHPI;
    memcpy(s, "SHPI", 4), put_u32(s + 4, T_SIZE - T_SHPI), put_u32(s + 8, 2), memcpy(s + 12, "LN32", 4);
    memcpy(s + 16, "!pal", 4), put_u32(s + 20, T_PAL);
    memcpy(s + 24, "0000", 4), put_u32(s + 28, T_BMP);
    s[T_PAL] = 0x24;
    memcpy(s + T_PAL + 16, pal, 768);
    s[T_BMP] = 0x7b, s[T_BMP + 4] = 2, s[T_BMP + 6] = 1;
    memcpy(s + T_BMP + 16, px, 2);
}

/* evening + rally: the rally's own bitmap goes into the evening art, its colours darkened as the day's go */
static void test_the_rally_bitmap_takes_the_evening_colours(void)
{
    static uint8_t day_pal[768], dark_pal[768];
    memset(day_pal, 0, sizeof day_pal), memset(dark_pal, 0, sizeof dark_pal);
    for (int i = 0; i < 4; i++)
        for (int k = 0; k < 3; k++) day_pal[3 * i + k] = (uint8_t)(i * 80), dark_pal[3 * i + k] = (uint8_t)(i * 40);
    uint8_t day[T_SIZE], timed[T_SIZE], rally[T_SIZE];
    const uint8_t day_px[2] = { 0, 2 }, timed_px[2] = { 0, 2 }, rally_px[2] = { 2, 0xff };
    build_track(day, day_pal, day_px);
    build_track(timed, dark_pal, timed_px);
    build_track(rally, day_pal, rally_px);
    CHECK_EQ_INT(fam_time_rally(timed, sizeof timed, day, sizeof day, rally, sizeof rally), 1);
    /* colour 2 is 160 by day and 80 at evening: the rally's 2 (160) goes to 80, the evening's colour 2 */
    CHECK_EQ_INT(timed[T_SHPI + T_BMP + 16], 2);
    CHECK_EQ_INT(timed[T_SHPI + T_BMP + 17], 0xff);
}

int main(void)
{
    test_the_flipped_sign_is_flipped_and_the_rest_kept();
    test_the_same_art_changes_nothing();
    test_a_damaged_mirror_changes_nothing();
    test_the_reference_decides_the_flip();
    test_the_rally_bitmap_takes_the_evening_colours();
    return TEST_REPORT();
}
