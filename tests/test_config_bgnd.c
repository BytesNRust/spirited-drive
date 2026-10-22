/* The configuration screens' clean background (config_bgnd.h): the picture the ten screens share, without their
 * text, the same every time. */
#include "game/frontend/config_bgnd.h"
#include "formats/qfs.h"
#include "test.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* fnv-1a 64 of the pixels the CD gave when the recipe was made */
#define CONFIG_BGND_FNV 0xdb6fac214f738167ULL

enum { BLACK, RED, GREEN, BLUE, AUDIO = 1, GAMEMODE = 8 };

static const uint8_t colours[4][3] = { { 0, 0, 0 }, { 200, 0, 0 }, { 0, 200, 0 }, { 0, 0, 200 } };
static uint8_t *screens[CONFIG_BGND_SCREENS];
static uint8_t palette[768];
static uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H];

static void put(int screen, int x, int y, int colour)
{
    memcpy(screens[screen] + 3 * (y * CONFIG_BGND_W + x), colours[colour], 3);
}

static void put_all(int x, int y, int colour)
{
    for (int s = 0; s < CONFIG_BGND_SCREENS; s++) put(s, x, y, colour);
}

static int at(int x, int y)
{
    return pixels[y * CONFIG_BGND_W + x];
}

static void compose(void)
{
    config_bgnd_compose((const uint8_t *const *)screens, palette, pixels);
}

static void test_the_text_few_screens_carry_is_dropped(void)
{
    put_all(400, 300, RED);
    for (int s = 0; s < 3; s++) put(s, 400, 300, GREEN);
    compose();
    CHECK_EQ_INT(at(400, 300), RED);
}

static void test_the_bands_most_screens_carry_text_in_come_from_a_clean_screen(void)
{
    put_all(200, 70, GREEN);
    put(AUDIO, 200, 70, RED);
    put_all(10, 110, GREEN);
    put(GAMEMODE, 10, 110, BLUE);
    compose();
    CHECK_EQ_INT(at(200, 70), RED);
    CHECK_EQ_INT(at(10, 110), BLUE);
}

/* the title's remnant (0..123 x 60..93) takes the texture 130 to its right */
static void test_the_title_is_covered_by_the_texture_beside_it(void)
{
    put_all(10, 70, GREEN);
    put_all(140, 70, BLUE);
    compose();
    CHECK_EQ_INT(at(10, 70), BLUE);
}

static void test_the_colours_are_the_nearest_of_the_palette(void)
{
    const uint8_t near_red[3] = { 190, 20, 10 };
    for (int s = 0; s < CONFIG_BGND_SCREENS; s++) memcpy(screens[s] + 3 * (400 * CONFIG_BGND_W + 500), near_red, 3);
    compose();
    CHECK_EQ_INT(at(500, 400), RED);
    CHECK_EQ_INT(at(600, 450), BLACK);
}

/* the CD's art file, upper-cased as ISO 9660 has it */
static uint8_t *read_cd(const char *name, size_t *size)
{
    char path[512];
    int n = snprintf(path, sizeof path, "%s/original/FRONTEND/ART/", TNFS_SOURCE_DIR);
    for (const char *c = name; *c && n < (int)sizeof path - 1; c++) path[n++] = (char)toupper((unsigned char)*c);
    path[n] = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    static uint8_t raw[1 << 20];
    const size_t len = fread(raw, 1, sizeof raw, f);
    fclose(f);
    uint8_t *out = NULL;
    if (qfs_decompressed_size(raw, len, size) == QFS_OK && (out = malloc(*size)) != NULL &&
        qfs_decompress(raw, len, out, *size) != QFS_OK)
        free(out), out = NULL;
    return out;
}

/* with the CD at hand: the same pixels as when the recipe was made */
static void test_the_cd_gives_the_same_pixels(void)
{
    uint8_t pal[768];
    if (!config_bgnd_load(read_cd, pixels, pal)) {
        fprintf(stderr, "test_config_bgnd: no CD data, skipping the CD check\n");
        return;
    }
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < sizeof pixels; i++) h = (h ^ pixels[i]) * 1099511628211ULL;
    fprintf(stderr, "test_config_bgnd: fnv %016llx\n", (unsigned long long)h);
    CHECK(h == CONFIG_BGND_FNV);
}

int main(void)
{
    for (int i = 0; i < 4; i++) memcpy(palette + 3 * i, colours[i], 3);
    for (int s = 0; s < CONFIG_BGND_SCREENS; s++) {
        screens[s] = calloc(3, CONFIG_BGND_W * CONFIG_BGND_H);
        if (screens[s] == NULL) return 1;
    }
    test_the_text_few_screens_carry_is_dropped();
    test_the_bands_most_screens_carry_text_in_come_from_a_clean_screen();
    test_the_title_is_covered_by_the_texture_beside_it();
    test_the_colours_are_the_nearest_of_the_palette();
    test_the_cd_gives_the_same_pixels();
    for (int s = 0; s < CONFIG_BGND_SCREENS; s++) free(screens[s]);
    return TEST_REPORT();
}
