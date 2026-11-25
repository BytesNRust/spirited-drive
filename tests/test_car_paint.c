/* The catalogue paints (car_paint.h): the table's shape, and the recolour of a body's palette entries on synthetic palettes. */
#include "game/race/car_paint.h"
#include "game/sys/port_extras.h"
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

#define GOLD_A 5
#define GOLD_B 0
#define GOLD_C 2
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
            CHECK(car_paint_pick(seed, slot, 5, 3, 4, 4, 0) != PAINT_SECOND);    /* no PS1 files: no console paint */
        }
    for (int id = 0; id < 6; id++) CHECK(seen[id] > 0);                       /* every choice turns up */
    CHECK_EQ_INT(car_paint_pick(7, 2, 5, 5, 0, 0, 0), 0);                     /* nothing else to take: the car's own */
    CHECK_EQ_INT(car_paint_pick(7, 2, 5, -1, 0, 0, 0), 0);
    /* a row of 8 (CAR_PAINT_MAX): every id 0..9 turns up and none is past the row; a row of up to 4 draws what it drew before the maximum went up */
    int wide[2 + CAR_PAINT_MAX] = { 0 };
    for (uint32_t seed = 1; seed < 200; seed++) {
        const int id = car_paint_pick(seed, 3, 1, 0, 0, CAR_PAINT_MAX, 1);
        CHECK(id >= 0 && id < 2 + CAR_PAINT_MAX);
        wide[id]++;
    }
    for (int id = 0; id < 2 + CAR_PAINT_MAX; id++) CHECK(wide[id] > 0);
    CHECK_EQ_INT(car_paint_pick(1, 1, 5, 3, 4, 4, 1), GOLD_A);
    CHECK_EQ_INT(car_paint_pick(2, 5, 1, 0, 0, 2, 0), GOLD_B);
    CHECK_EQ_INT(car_paint_pick(33, 7, 6, 2, 3, 3, 1), GOLD_C);
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

/* the black an added car gets (it has no catalogue entry): CAR_PAINT_ADDED_BLACK is the catalogue's own, the one most of the cars that have a black use; a tie goes
 * to the middle of them by brightness. An added stem has no catalogue black of its own */
static void test_the_added_black(void)
{
    static const char *const STEMS[CAR_PAINT_MODELS] = { "TSupra", "LDiabl", "P911", "CZR1", "F512TR", "DVIPER", "ANSX", "MRX7" };
    uint8_t blacks[CAR_PAINT_MODELS][3];
    int count = 0;
    for (int model = 0; model < CAR_PAINT_MODELS; model++) {
        const CarPaint *b = car_paint_black_of(STEMS[model]);
        if (b != NULL) memcpy(blacks[count++], b->rgb, 3);
    }
    CHECK(count >= 2);
    int best = -1, best_votes = 0, tied = 0;   /* the most used value; the votes of a tie are met by the lightness's middle below */
    for (int i = 0; i < count; i++) {
        int votes = 0;
        for (int j = 0; j < count; j++) votes += memcmp(blacks[i], blacks[j], 3) == 0;
        if (votes > best_votes) best = i, best_votes = votes, tied = 0;
        else if (votes == best_votes && memcmp(blacks[i], blacks[best], 3) != 0) tied = 1;
    }
    const uint8_t added[3] = CAR_PAINT_ADDED_BLACK;
    if (!tied) {
        CHECK(memcmp(added, blacks[best], 3) == 0);
    } else {   /* a tie: the value with the middle brightness among all the blacks (sorted by the sum of channels) */
        int order[CAR_PAINT_MODELS];
        for (int i = 0; i < count; i++) {
            int k = i;
            const int sum = blacks[i][0] + blacks[i][1] + blacks[i][2];
            while (k > 0 && blacks[order[k - 1]][0] + blacks[order[k - 1]][1] + blacks[order[k - 1]][2] > sum) order[k] = order[k - 1], k--;
            order[k] = i;
        }
        CHECK(memcmp(added, blacks[order[count / 2]], 3) == 0 || memcmp(added, blacks[order[(count - 1) / 2]], 3) == 0);
    }
    CHECK(added[0] <= 32 && added[1] <= 32 && added[2] <= 32);   /* a black is dark in every channel */
    CHECK(car_paint_black_of("TESTCAR") == NULL);   /* the added car's own stem finds no catalogue entry: the default is taken by the caller */
}

/* The rivals' colours hold across a stage set's segments: segment 1 draws the paint seed, 2 and 3 of the same set keep it. */
static void test_stage_seed(void)
{
    CarPaintSeedKeep k;
    memset(&k, 0, sizeof k);
    CHECK(car_paint_seed(&k, "CY1", 100) == 100);
    CHECK(car_paint_seed(&k, "CY2", 200) == 100);   /* the next segment of the set keeps stage 1's */
    CHECK(car_paint_seed(&k, "CY3", 300) == 100);
    CHECK(car_paint_seed(&k, "CY1", 400) == 400);   /* a new set */
    CHECK(car_paint_seed(&k, "CL2", 500) == 500);   /* another set's segment 2 with no segment 1 before it: its own */
    CHECK(car_paint_seed(&k, "CL3", 600) == 500);
    CHECK(car_paint_seed(&k, "TR1", 700) == 700);   /* a circuit: its own, and it ends the set */
    CHECK(car_paint_seed(&k, "TR1", 800) == 800);
    CHECK(car_paint_seed(&k, "CY2", 900) == 900);
    CHECK(car_paint_seed(&k, "cy1", 10) == 10 && car_paint_seed(&k, "cy2", 11) == 10);   /* the menus' stems are lower case */
    CHECK(car_paint_seed(NULL, "CY2", 5) == 5 && car_paint_seed(&k, NULL, 6) == 6);
}

/* a rival's colours follow the car its file is (race_models.c's value of its slot), not its field entry's: a pack against a CZR1 is DVIPER, CZR1, P911 */
static void race_cars(void)
{
    CHECK(car_paint_race_car(200 + 3) == 3 && car_paint_race_car(100 + 2) == 2 && car_paint_race_car(100 + 5) == 5);
    CHECK(car_paint_race_car(7) == -1 && car_paint_race_car(400 + 2) == -1 && car_paint_race_car(-1) == -1);   /* traffic, police */
}

/* port (replay colours): a replay's paint comes back as it was where this machine can show it, else the car's own; nothing recorded leaves the pick to the rules */
static void replayed(void)
{
    CHECK(car_paint_replayed(0xff, 3, 1) == -1 && car_paint_replayed(-1, 3, 1) == -1);
    CHECK(car_paint_replayed(0, 3, 1) == 0);
    CHECK(car_paint_replayed(1, 3, 1) == 1 && car_paint_replayed(1, 3, 0) == 0);   /* the console version livery needs its files */
    CHECK(car_paint_replayed(2 + car_paint_count(3) - 1, 3, 0) == 2 + car_paint_count(3) - 1);
    /* the ids of a table colour are its position (2 + k), whatever the table's maximum is: every colour a car has replays as itself, one past the row as the car's own (a replay of an older table keeps its colours) */
    for (int model = 0; model < CAR_PAINT_MODELS; model++) {
        for (int k = 0; k < car_paint_count(model); k++) CHECK_EQ_INT(car_paint_replayed(2 + k, model, 0), 2 + k);
        CHECK_EQ_INT(car_paint_replayed(2 + car_paint_count(model), model, 0), 0);
        CHECK_EQ_INT(car_paint_replayed(2 + CAR_PAINT_MAX, model, 0), 0);
    }
    CHECK_EQ_INT(car_paint_count(1), 8);   /* LDiabl: the owner's card of 8 colours (Copy to game repo wrote them); his Giallo Orion is the third */
    CHECK(car_paint(1, 2) != NULL && strcmp(car_paint(1, 2)->name, "Giallo Orion") == 0);
    CHECK_EQ_INT(CAR_PAINT_MAX, 8);   /* the owner's 8 colours: PAINT_CATALOGUE + 7 is the last id, one decimal digit of the Color box's `paint_K` */
    CHECK(car_paint_replayed(2 + car_paint_count(3), 3, 1) == 0);   /* a catalogue paint the table no longer has */
}

int main(void)
{
    replayed();
    race_cars();
    test_stage_seed();
    test_the_catalogue_black();
    test_the_added_black();
    test_bright_paint_keeps_steps();
    test_table();
    test_pick();
    test_recolour(0x24);
    test_recolour(0x22);
    test_extremes();
    test_dark_body();
    return TEST_REPORT();
}
