/* The PS1 look's soft shadow (soft_shadow.h) on a grey ramp: the half-way table, the light's levels, the shrunken quad and what each level
 * leaves on a surface. */
#include "game/gfx/quad.h"
#include "game/viewer/soft_shadow.h"
#include "test.h"

#include <string.h>

enum { W = 40, H = 40, GROUND = 200 };

/* a grey ramp palette (entry i is i,i,i) and a shade table that halves each grey */
static void ramp(uint8_t palette[768], uint8_t full[256])
{
    for (int i = 0; i < 256; i++) palette[3 * i] = palette[3 * i + 1] = palette[3 * i + 2] = (uint8_t)i, full[i] = (uint8_t)(i / 2);
}

static void test_half(void)
{
    uint8_t palette[768], full[256], half[256];
    ramp(palette, full);
    soft_shadow_half(palette, full, half);
    CHECK_EQ_INT(half[200], 150);   /* half way between 200 and 100 */
    CHECK_EQ_INT(half[150], 112);   /* 112.5 */
    CHECK_EQ_INT(half[0], 0);
    for (int i = 1; i < 256; i++) CHECK(half[i] <= i && half[i] >= full[i]);
}

/* port (lamp_shadow): the half-way entry is found in the palette as it is shown. The night palette tints some entries and leaves others (the lit board's cream) as they are, so the
 * art's palette can name a bright entry for a darkened road: under a street lamp the shadow was a flat cream box. Entry 1 is the lit road, 2 its shade; 3 is the untinted cream whose art
 * colour is the half-way one, 4 the tinted entry whose shown colour is. */
static void test_half_shown(void)
{
    uint8_t art[768] = { 0 }, shown[768] = { 0 }, full[256] = { 0 }, half[256];
    const uint8_t a[5][3] = { { 0, 0, 0 }, { 200, 200, 200 }, { 60, 60, 60 }, { 130, 130, 130 }, { 131, 131, 131 } };
    const uint8_t n[5][3] = { { 0, 0, 0 }, { 80, 80, 80 }, { 24, 24, 24 }, { 130, 130, 130 }, { 52, 52, 52 } };
    memcpy(art, a, sizeof a), memcpy(shown, n, sizeof n);
    full[1] = 2;
    soft_shadow_half(art, full, half);
    CHECK_EQ_INT(half[1], 3);   /* the art's palette: the cream, 130 on screen where the road is 80 */
    soft_shadow_half(shown, full, half);
    CHECK_EQ_INT(half[1], 4);   /* the shown one: 52, between 80 and 24 */
}

static void test_level(void)
{
    CHECK_EQ_INT(soft_shadow_level(255), SOFT_SHADOW_BRIGHT);
    CHECK_EQ_INT(soft_shadow_level(192), SOFT_SHADOW_BRIGHT);
    CHECK_EQ_INT(soft_shadow_level(191), SOFT_SHADOW_MID);
    CHECK_EQ_INT(soft_shadow_level(96), SOFT_SHADOW_MID);
    CHECK_EQ_INT(soft_shadow_level(95), SOFT_SHADOW_DARK);
    CHECK_EQ_INT(soft_shadow_level(0), SOFT_SHADOW_DARK);
}

static void test_shrink(void)
{
    const int32_t in[8] = { 0, 0, 100, 0, 100, 100, 0, 100 };
    int32_t out[8];
    soft_shadow_shrink(in, out, 5);
    const int32_t want[8] = { 5, 5, 95, 5, 95, 95, 5, 95 };
    CHECK_MEM(out, want, sizeof want);
    const int32_t thin[8] = { 0, 0, 4, 0, 4, 4, 0, 4 };   /* smaller than the margin: it closes on its centre, not past it */
    soft_shadow_shrink(thin, out, 5);
    for (int i = 0; i < 8; i++) CHECK_EQ_INT(out[i], 2);
}

static void draw(int level, uint8_t surface[W * H])
{
    uint8_t palette[768], full[256], half[256];
    ramp(palette, full);
    soft_shadow_half(palette, full, half);
    memset(surface, GROUND, W * H);
    const QuadTarget t = { surface, W, 0, 0, W, H };
    const int32_t quad[8] = { 10, 10, 30, 10, 30, 30, 10, 30 };
    soft_shadow_draw(&t, full, half, level, quad);
}

static void test_draw(void)
{
    uint8_t s[W * H];
    draw(SOFT_SHADOW_BRIGHT, s);
    CHECK_EQ_INT(s[20 * W + 20], GROUND / 2);   /* the core: the exe's shade */
    CHECK_EQ_INT(s[20 * W + 11], 150);          /* the rim: half way */
    CHECK_EQ_INT(s[5 * W + 5], GROUND);  /* outside: as it was */
    CHECK_EQ_INT(s[20 * W + 35], GROUND);
    int rim = 0, core = 0, odd = 0;   /* the footprint has no gap, and the only texels darkened twice are the strips' diagonal joins at the corners */
    for (int y = 10; y < 30; y++)
        for (int x = 10; x < 30; x++) {
            const int v = s[y * W + x];
            if (v == 150) rim++;
            else if (v == GROUND / 2) core++;
            else odd++;
        }
    CHECK(odd <= 12);
    CHECK(rim > 0 && core > 0);
    draw(SOFT_SHADOW_MID, s);
    CHECK_EQ_INT(s[20 * W + 20], 150);   /* weaker: the whole quad once */
    CHECK_EQ_INT(s[20 * W + 11], 150);
    CHECK_EQ_INT(s[5 * W + 5], GROUND);
    draw(SOFT_SHADOW_DARK, s);
    CHECK_EQ_INT(s[20 * W + 20], 150);   /* weakest: the core only, smaller */
    CHECK_EQ_INT(s[20 * W + 11], GROUND);
    CHECK_EQ_INT(s[5 * W + 5], GROUND);
}

int main(void)
{
    test_half();
    test_half_shown();
    test_level();
    test_shrink();
    test_draw();
    return TEST_REPORT();
}
