/* port (car colours): the slots of the screen palette a car owns take the colours its textures show most (car_colours.h). A synthetic
 * bank (a body of teal tones the track's palette lacks) and, with the CD's CARFAMS, the eight real cars' banks. */
#include "formats/chunk.h"
#include "formats/fsh.h"
#include "game/race/car_colours.h"
#include "game/race/car_shine.h"
#include "game/race/car_load.h"
#include "game/race/car_paint.h"
#include "game/sys/palette.h"
#include "game/sys/shape_bank.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RacePalette pal;
static RaceState race;
static Car car[RACE_CARS];

/* the screen palette as the track leaves it: greys and warm colours (entries 0..255 but the car's block), the block black */
static void screen_setup(int first, int count)
{
    memset(&pal, 0, sizeof pal);
    for (int i = 0; i < 256; i++) {
        const uint8_t v = (uint8_t)(i % 64 * 4);
        const uint8_t rgb[3] = { (uint8_t)(i < 128 ? v : 255 - v / 2), (uint8_t)v, (uint8_t)(i < 128 ? v : v / 4) };
        memcpy(pal.screen_rgbx + 4 * i, rgb, 3);
    }
    for (int i = first; i < first + count; i++) memset(pal.screen_rgbx + 4 * i, 0, 3);
}

static void race_setup(int cars)
{
    memset(&race, 0, sizeof race);
    memset(car, 0, sizeof car);
    race.race_type = 0;
    race.car_count = cars;
    for (int i = 0; i < cars; i++) race.cars[i] = &car[i];
}

/* a bank of one `w`x`h` shape of the colours `index[]` in turn, as the SHPI bank layout has it */
static uint8_t bank_data[4096];
static uint8_t *make_bank(const uint8_t *index, int n, int w, int h)
{
    memset(bank_data, 0, sizeof bank_data);
    bank_data[8] = 1;
    memcpy(bank_data + 0x10, "body", 4);
    bank_data[0x14] = 0x20;
    uint8_t *shape = bank_data + 0x20;
    shape[4] = (uint8_t)w, shape[6] = (uint8_t)h;
    for (int k = 0; k < w * h; k++) shape[0x10 + k] = index[k % n];
    return bank_data;
}

static void test_a_light_tone_gets_a_slot(void)
{
    screen_setup(0x30, 0x17);
    race_setup(1);
    uint8_t palette6[256 * 3] = { 0 };
    const uint8_t teal[3] = { 16, 26, 28 }, dark[3] = { 4, 16, 18 };   /* 6 bits a channel */
    memcpy(palette6 + 3 * 1, teal, 3);
    memcpy(palette6 + 3 * 2, dark, 3);
    const uint8_t texels[3] = { 1, 1, 2 };
    RaceCarArt art[RACE_CARS];
    memset(art, 0, sizeof art);
    art[0].bank = make_bank(texels, 3, 16, 16);
    art[0].palette = palette6;
    CarFitReport report;
    const int changed = car_colours_fit(&pal, &race, art, 0, NULL, &report);
    CHECK(changed > 0);
    CHECK_EQ_INT(report.count, 1);
    CHECK_EQ_INT(report.block[0].first, 0x30);
    CHECK(report.block[0].after < report.block[0].before);
    int seen_teal = 0, seen_dark = 0;   /* each tone has an entry of its own colour in the block now */
    for (int e = 0x30; e < 0x30 + 0x17; e++) {
        const uint8_t *c = pal.screen_rgbx + 4 * e;
        seen_teal |= c[0] >> 2 == teal[0] && c[1] >> 2 == teal[1] && c[2] >> 2 == teal[2];
        seen_dark |= c[0] >> 2 == dark[0] && c[1] >> 2 == dark[1] && c[2] >> 2 == dark[2];
    }
    CHECK(seen_teal && seen_dark);
}

/* a paint tone is wanted darkened too (the PS1 light and the shadow take it to the nearest colour of its hue), a grey is not */
static void test_darker_tones_are_wanted(void)
{
    screen_setup(0x30, 0x17);
    race_setup(1);
    uint8_t palette6[256 * 3] = { 0 };
    const uint8_t teal[3] = { 8, 40, 44 }, grey[3] = { 30, 30, 30 };
    memcpy(palette6 + 3 * 1, teal, 3);
    memcpy(palette6 + 3 * 2, grey, 3);
    RaceCarArt art[RACE_CARS];
    memset(art, 0, sizeof art);
    const uint8_t teal_only[1] = { 1 }, grey_only[1] = { 2 };
    art[0].bank = make_bank(grey_only, 1, 16, 16);
    art[0].palette = palette6;
    CarFitReport report;
    car_colours_fit(&pal, &race, art, 0, NULL, &report);
    CHECK_EQ_INT(report.block[0].colours, 1);   /* the grey: itself */
    art[0].bank = make_bank(teal_only, 1, 16, 16);
    screen_setup(0x30, 0x17);
    car_colours_fit(&pal, &race, art, 0, NULL, &report);
    CHECK_EQ_INT(report.block[0].colours, 5);   /* the teal, two darker tones and two lighter ones (a saturated paint) */
    int dim = 0;
    for (int e = 0x30; e < 0x30 + 0x17; e++) {
        const uint8_t *c = pal.screen_rgbx + 4 * e;
        const int r = c[0] >> 2, g = c[1] >> 2, b = c[2] >> 2;
        dim |= abs(r - 6) <= 2 && abs(g - 31) <= 2 && abs(b - 34) <= 2;   /* 0.78 of the teal */
    }
    CHECK(dim);
}

/* a saturated paint's lift (the envmap's, toward white with less chroma) is wanted too: the track's palette has one pale tone of a hue, and
 * every level from the middle up fell to it, so a flat panel's reflection was a single flat colour. A muted tone has no such lift wanted. */
static void test_a_saturated_paints_lifted_tones_are_wanted(void)
{
    screen_setup(0x30, 0x17);
    race_setup(1);
    uint8_t palette6[256 * 3] = { 0 };
    const uint8_t blue[3] = { 8, 16, 34 }, muted[3] = { 30, 34, 38 };
    memcpy(palette6 + 3 * 1, blue, 3);
    memcpy(palette6 + 3 * 2, muted, 3);
    RaceCarArt art[RACE_CARS];
    memset(art, 0, sizeof art);
    const uint8_t muted_only[1] = { 2 }, blue_only[1] = { 1 };
    art[0].palette = palette6;
    art[0].bank = make_bank(muted_only, 1, 16, 16);
    CarFitReport report;
    car_colours_fit(&pal, &race, art, 0, NULL, &report);
    CHECK_EQ_INT(report.block[0].colours, 1);   /* the muted tone: itself */
    art[0].bank = make_bank(blue_only, 1, 16, 16);
    screen_setup(0x30, 0x17);
    car_colours_fit(&pal, &race, art, 0, NULL, &report);
    CHECK_EQ_INT(report.block[0].colours, 5);
    uint8_t blue8[3], want[3][3];
    float lift[3];
    palette_6_to_8(1, blue, blue8);
    car_envmap_want(blue8, 0.6f, lift);   /* the upper step: a paler blue with less chroma */
    for (int k = 0; k < 3; k++) want[0][k] = (uint8_t)(lift[k] / 4.0f + 0.5f);
    int near = 0;
    for (int e = 0x30; e < 0x30 + 0x17; e++) {
        const uint8_t *c = pal.screen_rgbx + 4 * e;
        near |= abs((c[0] >> 2) - want[0][0]) <= 2 && abs((c[1] >> 2) - want[0][1]) <= 2 && abs((c[2] >> 2) - want[0][2]) <= 2;
    }
    CHECK(near);
}

/* a dark, muted-saturation paint (the default 911's teal: 6-bit (2,10,10), (5,13,15)) has no lighter teal in a track's palette either: its lifted tones
 * are wanted too. A grey with a faint tint is not a paint tone, so it is not */
static void test_a_dark_paints_lifted_tones_are_wanted(void)
{
    screen_setup(0x30, 0x17);
    race_setup(1);
    uint8_t palette6[256 * 3] = { 0 };
    const uint8_t teal[3] = { 2, 10, 10 }, tinted_grey[3] = { 21, 25, 30 };
    memcpy(palette6 + 3 * 1, teal, 3);
    memcpy(palette6 + 3 * 2, tinted_grey, 3);
    RaceCarArt art[RACE_CARS];
    memset(art, 0, sizeof art);
    const uint8_t grey_only[1] = { 2 }, teal_only[1] = { 1 };
    art[0].palette = palette6;
    art[0].bank = make_bank(grey_only, 1, 16, 16);
    CarFitReport report;
    car_colours_fit(&pal, &race, art, 0, NULL, &report);
    CHECK_EQ_INT(report.block[0].colours, 1);   /* the tinted grey: itself */
    art[0].bank = make_bank(teal_only, 1, 16, 16);
    screen_setup(0x30, 0x17);
    car_colours_fit(&pal, &race, art, 0, NULL, &report);
    CHECK_EQ_INT(report.block[0].colours, 3);   /* the teal and its two lifted tones */
    uint8_t teal8[3], want[3];
    float lift[3];
    palette_6_to_8(1, teal, teal8);
    car_envmap_want(teal8, 0.6f, lift);
    for (int k = 0; k < 3; k++) want[k] = (uint8_t)(lift[k] / 4.0f + 0.5f);
    int near = 0;
    for (int e = 0x30; e < 0x30 + 0x17; e++) {
        const uint8_t *c = pal.screen_rgbx + 4 * e;
        near |= abs((c[0] >> 2) - want[0]) <= 2 && abs((c[1] >> 2) - want[1]) <= 2 && abs((c[2] >> 2) - want[2]) <= 2;
    }
    CHECK(near);
}

static void test_a_block_that_does_well_is_left(void)
{
    screen_setup(0x30, 0x17);
    race_setup(1);
    uint8_t palette6[256 * 3] = { 0 };
    const uint8_t grey[3] = { 16, 16, 16 };   /* the screen palette has it, as the screen takes a 6 bit colour, at entry 16 */
    memset(pal.screen_rgbx + 4 * 16, 16 << 2 | 16 >> 4, 3);
    memcpy(palette6 + 3, grey, 3);
    const uint8_t texels[1] = { 1 };
    RaceCarArt art[RACE_CARS];
    memset(art, 0, sizeof art);
    art[0].bank = make_bank(texels, 1, 8, 8);
    art[0].palette = palette6;
    uint8_t before[256 * 4];
    memcpy(before, pal.screen_rgbx, sizeof before);
    CHECK_EQ_INT(car_colours_fit(&pal, &race, art, 0, NULL, NULL), 0);
    CHECK_MEM(before, pal.screen_rgbx, sizeof before);
}

static void test_other_entries_are_not_touched(void)
{
    screen_setup(0x30, 0x17);
    race_setup(1);
    uint8_t palette6[256 * 3] = { 0 };
    memcpy(palette6 + 3, (const uint8_t[3]){ 16, 26, 28 }, 3);
    const uint8_t texels[1] = { 1 };
    RaceCarArt art[RACE_CARS];
    memset(art, 0, sizeof art);
    art[0].bank = make_bank(texels, 1, 8, 8);
    art[0].palette = palette6;
    uint8_t before[256 * 4];
    memcpy(before, pal.screen_rgbx, sizeof before);
    CHECK(car_colours_fit(&pal, &race, art, 0, NULL, NULL) > 0);
    CHECK_MEM(before, pal.screen_rgbx, 4 * 0x30);                       /* below the block */
    CHECK_MEM(before + 4 * 0x47, pal.screen_rgbx + 4 * 0x47, 4 * (256 - 0x47));   /* above it */
}

/* an entry the scenery is mapped to is not rewritten, whatever the car would gain: the track is as the exe maps it */
static void test_kept_entries_stay(void)
{
    screen_setup(0x30, 0x17);
    race_setup(1);
    uint8_t palette6[256 * 3] = { 0 };
    memcpy(palette6 + 3, (const uint8_t[3]){ 16, 26, 28 }, 3);
    memcpy(palette6 + 6, (const uint8_t[3]){ 4, 16, 18 }, 3);
    const uint8_t texels[2] = { 1, 2 };
    RaceCarArt art[RACE_CARS];
    memset(art, 0, sizeof art);
    art[0].bank = make_bank(texels, 2, 8, 8);
    art[0].palette = palette6;
    uint8_t keep[256] = { 0 }, before[256 * 4];
    for (int e = 0x30; e < 0x38; e++) keep[e] = 1;
    memcpy(before, pal.screen_rgbx, sizeof before);
    CHECK(car_colours_fit(&pal, &race, art, 0, keep, NULL) > 0);
    CHECK_MEM(before + 4 * 0x30, pal.screen_rgbx + 4 * 0x30, 4 * 8);   /* the kept ones */
    CHECK(memcmp(before + 4 * 0x38, pal.screen_rgbx + 4 * 0x38, 4 * 15) != 0);   /* the others took the car's colours */
    memcpy(pal.screen_rgbx, before, sizeof before);
    for (int e = 0x30; e < 0x47; e++) keep[e] = 1;
    CHECK_EQ_INT(car_colours_fit(&pal, &race, art, 0, keep, NULL), 0);   /* every one kept: nothing to take */
    CHECK_MEM(before, pal.screen_rgbx, sizeof before);
}

/* the entries a mapped bank shows */
static void test_bank_entries(void)
{
    const uint8_t texels[3] = { 7, 7, 200 };
    uint8_t *bank = make_bank(texels, 3, 4, 3);
    uint8_t used[256] = { 0 };
    race_palette_bank_entries(bank, used);
    int n = 0;
    for (int e = 0; e < 256; e++) n += used[e];
    CHECK_EQ_INT(n, 2);
    CHECK(used[7] && used[200]);
}

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)n);
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = d != NULL ? (size_t)n : 0;
    return d;
}

/* the eight cars' own banks (the high detail one and the low one) against a track's colours: the block's distance falls, the slots used per
 * car are reported. Skipped without the CD's CARFAMS. */
static void test_the_real_cars(const char *root)
{
    static uint8_t palette6[2][CAR_PAINT_MODELS][256 * 3];
    for (int model = 0; model < CAR_PAINT_MODELS; model++) {
        char path[600];
        snprintf(path, sizeof path, "%s/SIMDATA/CARFAMS/%s.CFM", root, car_model_names[model]);
        size_t size = 0;
        uint8_t *cfm = slurp(path, &size);
        if (cfm == NULL) continue;
        RaceCarArt art[RACE_CARS];
        memset(art, 0, sizeof art);
        for (int level = 0; level < 2; level++) {
            Chunk file, shpi;
            Fsh bank;
            if (chunk_open(&file, cfm, size) != CHUNK_OK || chunk_child(&file, 2 * level + 1, &shpi) != CHUNK_OK) continue;
            if (shpi.kind != CHUNK_KIND_BITMAP || fsh_open(&bank, shpi.data, shpi.size) != FSH_OK) continue;
            static const uint8_t PAL[4] = { '!', 'P', 'A', 'L' };
            const uint8_t *p = shape_bank_find((uint8_t *)(uintptr_t)bank.data, PAL);
            if (p == NULL) continue;
            const int shift = p[0] == 0x22 ? 0 : 2;   /* the race's palettes are 6 bits */
            for (int i = 0; i < 256 * 3; i++) palette6[level][model][i] = (uint8_t)(p[16 + i] >> shift);
            if (level == 0) art[0].bank = (uint8_t *)(uintptr_t)bank.data, art[0].palette = palette6[0][model];
            else art[0].bank2 = (uint8_t *)(uintptr_t)bank.data, art[0].palette2 = palette6[1][model];
        }
        if (art[0].bank == NULL) { free(cfm); continue; }
        screen_setup(0x30, 0x17);
        race_setup(1);
        CarFitReport report;
        const int changed = car_colours_fit(&pal, &race, art, 0, NULL, &report);
        fprintf(stderr, "  %-7s %d colours, %2d of 23 slots new, distance %.1f -> %.1f\n", car_model_names[model], report.count ? report.block[0].colours : 0,
                changed, report.count ? report.block[0].before : 0.0, report.count ? report.block[0].after : 0.0);
        CHECK(report.count == 1 && report.block[0].after <= report.block[0].before);
        CHECK(changed <= 23);
        free(cfm);
    }
}

int main(void)
{
    test_a_light_tone_gets_a_slot();
    test_darker_tones_are_wanted();
    test_a_saturated_paints_lifted_tones_are_wanted();
    test_a_dark_paints_lifted_tones_are_wanted();
    test_a_block_that_does_well_is_left();
    test_other_entries_are_not_touched();
    test_kept_entries_stay();
    test_bank_entries();
    const char *root = getenv("TNFS_DATA");
    char fallback[512] = "";
#ifdef TNFS_SOURCE_DIR
    snprintf(fallback, sizeof fallback, "%s/original", TNFS_SOURCE_DIR);
#endif
    if (root == NULL) root = fallback;
    char probe[600];
    snprintf(probe, sizeof probe, "%s/SIMDATA/CARFAMS/ANSX.CFM", root);
    FILE *f = fopen(probe, "rb");
    if (f != NULL) {
        fclose(f);
        test_the_real_cars(root);
    } else {
        fprintf(stderr, "test_car_colours: no CARFAMS under %s, the real cars skipped\n", root);
    }
    return TEST_REPORT();
}
