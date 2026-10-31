/* The catalogue paints (car_paint.h): the table's shape, and the recolour of a body's palette entries on synthetic palettes. */
#include "game/race/car_paint.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static void test_table(void)
{
    for (int model = 0; model < CAR_PAINT_MODELS; model++) {
        const int n = car_paint_count(model);
        CHECK(n >= 1 && n <= CAR_PAINT_MAX);
        for (int k = 0; k < n; k++) {
            const CarPaint *p = car_paint(model, k);
            CHECK(p != NULL && p->name != NULL && p->name[0] != '\0');
            for (int j = k + 1; j < n; j++) CHECK(memcmp(p->rgb, car_paint(model, j)->rgb, 3) != 0);   /* all different */
            CHECK(memcmp(p->rgb, car_paint_default(model), 3) != 0);   /* and none of them the car's own colour */
        }
        CHECK(car_paint(model, n) == NULL);
    }
    CHECK_EQ_INT(car_paint_count(-1), 0);
    CHECK_EQ_INT(car_paint_count(CAR_PAINT_MODELS), 0);
    CHECK(car_paint(-1, 0) == NULL);
}

/* a body of four red tones (entries 10..13, the middle two heavy) among a glass entry 1, a tyre entry 2 and trim entry 3 */
static void body_palette(uint8_t palette[768], int type)
{
    memset(palette, 0, 768);
    static const uint8_t RED[4][3] = { { 80, 8, 8 }, { 120, 12, 12 }, { 160, 16, 16 }, { 220, 40, 40 } };
    for (int i = 0; i < 4; i++)
        for (int c = 0; c < 3; c++) palette[3 * (10 + i) + c] = type == 0x22 ? RED[i][c] >> 2 : RED[i][c];
    const uint8_t glass[3] = { 16, 16, 16 }, tyre[3] = { 30, 30, 30 }, trim[3] = { 200, 200, 120 };
    for (int c = 0; c < 3; c++) {
        palette[3 + c] = type == 0x22 ? glass[c] >> 2 : glass[c];
        palette[6 + c] = type == 0x22 ? tyre[c] >> 2 : tyre[c];
        palette[9 + c] = type == 0x22 ? trim[c] >> 2 : trim[c];
    }
}

static int channel(const uint8_t *palette, int type, int entry, int c)
{
    return type == 0x22 ? palette[3 * entry + c] << 2 : palette[3 * entry + c];
}

static int luma(const uint8_t *palette, int type, int entry)
{
    return (30 * channel(palette, type, entry, 0) + 59 * channel(palette, type, entry, 1) + 11 * channel(palette, type, entry, 2)) / 100;
}

static void test_recolour(int type)
{
    uint8_t palette[768], before[768];
    body_palette(palette, type);
    memcpy(before, palette, sizeof palette);
    const uint8_t body[4] = { 10, 11, 12, 13 };
    uint32_t weight[256] = { 0 };
    weight[10] = 100, weight[11] = 400, weight[12] = 300, weight[13] = 50;
    const uint8_t blue[3] = { 40, 80, 200 };
    car_paint_recolour(palette, type, body, 4, weight, blue);
    for (int e = 0; e < 256; e++)
        if (e < 10 || e > 13) CHECK(memcmp(palette + 3 * e, before + 3 * e, 3) == 0);   /* nothing but the body moves */
    CHECK(luma(palette, type, 10) < luma(palette, type, 11));   /* the shading's order survives */
    CHECK(luma(palette, type, 11) < luma(palette, type, 12));
    CHECK(luma(palette, type, 12) < luma(palette, type, 13));
    CHECK(channel(palette, type, 11, 2) > channel(palette, type, 11, 0) + 40);   /* blue now */
    /* the main tone (weighted median: entry 11) is the target itself */
    CHECK(abs(channel(palette, type, 11, 0) - blue[0]) <= 4);
    CHECK(abs(channel(palette, type, 11, 2) - blue[2]) <= 4);
}

/* a near-black body (a grey ramp of 24..64, the Diablo's) in red: the lightness steps stay in order and gentle, not blown into flat
 * bright patches over dark ones */
static void test_dark_body(void)
{
    uint8_t palette[768] = { 0 };
    uint8_t body[6];
    uint32_t weight[256] = { 0 };
    for (int i = 0; i < 6; i++) {
        body[i] = (uint8_t)(10 + i);
        const int v = 24 + 8 * i;
        palette[3 * (10 + i)] = palette[3 * (10 + i) + 1] = palette[3 * (10 + i) + 2] = (uint8_t)v;
        weight[10 + i] = 1000;
    }
    const uint8_t red[3] = { 179, 18, 30 };
    car_paint_recolour(palette, 0x24, body, 6, weight, red);
    for (int i = 1; i < 6; i++) CHECK(luma(palette, 0x24, 10 + i) > luma(palette, 0x24, 9 + i));
    CHECK(luma(palette, 0x24, 15) * 10 <= luma(palette, 0x24, 10) * 16);   /* brightest within 1.6x the darkest */
    CHECK(luma(palette, 0x24, 15) * 10 >= luma(palette, 0x24, 10) * 12);   /* and still shaded */
}

static void test_extremes(void)
{
    uint8_t palette[768];
    const uint8_t body[4] = { 10, 11, 12, 13 };
    uint32_t weight[256] = { 0 };
    weight[10] = 100, weight[11] = 400, weight[12] = 300, weight[13] = 50;
    const uint8_t white[3] = { 242, 242, 238 }, black[3] = { 14, 14, 16 };
    body_palette(palette, 0x24);
    car_paint_recolour(palette, 0x24, body, 4, weight, white);
    CHECK(luma(palette, 0x24, 11) > 200);                              /* a white car is white */
    CHECK(luma(palette, 0x24, 10) < luma(palette, 0x24, 13) - 8);      /* and still shaded: its shadows are darker */
    body_palette(palette, 0x24);
    car_paint_recolour(palette, 0x24, body, 4, weight, black);
    CHECK(luma(palette, 0x24, 11) < 40);
    CHECK(luma(palette, 0x24, 13) > luma(palette, 0x24, 10));          /* its highlights still show */
    car_paint_recolour(palette, 0x99, body, 4, weight, black);         /* a palette of another type is left alone */
    car_paint_recolour(NULL, 0x24, body, 4, weight, black);
    body_palette(palette, 0x24);
    car_paint_recolour(palette, 0x24, body, 0, weight, black);         /* no body: nothing */
}

/* a rival's paint: from the race's seed and its slot, never the player's when it drives the same car */
static void test_pick(void)
{
    int seen[16] = { 0 };
    for (uint32_t seed = 1; seed < 60; seed++)
        for (int slot = 1; slot < 8; slot++) {
            const int same = car_paint_pick(seed, slot, 3, 3, 4, 4, 1);   /* the player's Corvette in paint 4: another for the rival */
            CHECK(same >= 0 && same < 2 + 4 && same != 4);
            const int other = car_paint_pick(seed, slot, 5, 3, 4, 4, 1);  /* another car: any of the choices */
            CHECK(other >= 0 && other < 2 + 4);
            seen[other]++;
            CHECK_EQ_INT(car_paint_pick(seed, slot, 5, 3, 4, 4, 1), other);   /* the same race, the same colours */
            CHECK(car_paint_pick(seed, slot, 5, 3, 4, 4, 0) != PAINT_PS1);    /* no PS1 files: no PS1 paint */
        }
    for (int id = 0; id < 6; id++) CHECK(seen[id] > 0);                       /* every choice turns up */
    CHECK_EQ_INT(car_paint_pick(7, 2, 5, 5, 0, 0, 0), 0);                     /* nothing else to take: the car's own */
    CHECK_EQ_INT(car_paint_pick(7, 2, 5, -1, 0, 0, 0), 0);
}

/* a paint near the top of a channel leaves the body's lighter tones room: they do not all clip to one colour (a yellow 242, 197, 0 gave its
 * nine lighter tones six levels), and the main tone keeps most of the paint; a paint with room is as it was */
static void test_bright_paint_keeps_steps(void)
{
    const uint8_t ramp[6][3] = { { 16, 32, 32 }, { 16, 64, 72 }, { 32, 72, 80 }, { 48, 80, 88 }, { 48, 88, 96 }, { 80, 112, 120 } };
    const uint32_t weight[6] = { 900, 2040, 1778, 1056, 1224, 300 };
    const uint8_t yellow[3] = { 242, 197, 0 }, blue[3] = { 30, 70, 160 };
    CarPaintPlan plan;
    car_paint_plan(&plan, ramp, weight, 6, yellow);
    uint8_t out[6][3];
    for (int i = 0; i < 6; i++) car_paint_shade(&plan, ramp[i], out[i]);
    CHECK(plan.rgb[0] >= 242 * 8 / 10 && plan.rgb[0] < 242);   /* the main tone gave up some, not all */
    for (int i = 1; i < 6; i++) CHECK(out[i][0] > out[i - 1][0] || out[i][1] > out[i - 1][1]);   /* each tone its own colour, no clip */
    CHECK(out[5][1] - out[1][1] >= 20);   /* and a step the eye sees between the body's tones */
    car_paint_plan(&plan, ramp, weight, 6, blue);
    CHECK_EQ_INT(plan.rgb[2], 160);   /* a paint with room is as the table has it */
}

/* the black a car's catalogue has (the one the player gets by choosing black), by the race model's stem in any case; none for a car without one */
static void test_the_catalogue_black(void)
{
    const CarPaint *nsx = car_paint_black_of("ANSX"), *nero = car_paint_black_of("f512tr"), *p911 = car_paint_black_of("P911");
    CHECK(nsx != NULL && nero != NULL && p911 != NULL);
    if (nsx != NULL) CHECK(nsx->rgb[0] == 14 && nsx->rgb[1] == 14 && nsx->rgb[2] == 16);
    if (nero != NULL) CHECK(nero->rgb[0] == 10);
    if (p911 != NULL) CHECK(strcmp(p911->name, "Black") == 0);
    CHECK(car_paint_black_of("TSupra") == NULL);   /* white, silver and blue only */
    CHECK(car_paint_black_of("NOPE") == NULL && car_paint_black_of(NULL) == NULL);
    static const char *const STEMS[CAR_PAINT_MODELS] = { "TSupra", "LDiabl", "P911", "CZR1", "F512TR", "DVIPER", "ANSX", "MRX7" };
    for (int model = 0; model < CAR_PAINT_MODELS; model++) {
        const CarPaint *b = car_paint_black_of(STEMS[model]);
        if (b != NULL) CHECK(b->rgb[0] <= 32 && b->rgb[1] <= 32 && b->rgb[2] <= 32);   /* a black is dark in every channel */
    }
}

int main(void)
{
    test_the_catalogue_black();
    test_bright_paint_keeps_steps();
    test_table();
    test_pick();
    test_recolour(0x24);
    test_recolour(0x22);
    test_extremes();
    test_dark_body();
    return TEST_REPORT();
}
