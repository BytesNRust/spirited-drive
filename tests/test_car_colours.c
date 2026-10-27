/* port (car colours): the slots of the screen palette a car owns take the colours its textures show most (car_colours.h). A synthetic
 * bank (a body of teal tones the track's palette lacks) and, with the CD's CARFAMS, the eight real cars' banks. */
#include "formats/chunk.h"
#include "formats/fsh.h"
#include "game/race/car_colours.h"
#include "game/race/car_load.h"
#include "game/race/car_paint.h"
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
    const int changed = car_colours_fit(&pal, &race, art, 0, &report);
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
    CHECK_EQ_INT(car_colours_fit(&pal, &race, art, 0, NULL), 0);
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
    CHECK(car_colours_fit(&pal, &race, art, 0, NULL) > 0);
    CHECK_MEM(before, pal.screen_rgbx, 4 * 0x30);                       /* below the block */
    CHECK_MEM(before + 4 * 0x47, pal.screen_rgbx + 4 * 0x47, 4 * (256 - 0x47));   /* above it */
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
        const int changed = car_colours_fit(&pal, &race, art, 0, &report);
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
    test_a_block_that_does_well_is_left();
    test_other_entries_are_not_touched();
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
