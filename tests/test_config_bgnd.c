/* The configuration screens' clean background (config_bgnd.h): the picture the ten screens share, without their
 * text, the same every time. */
#include "game/frontend/config_bgnd.h"
#include "formats/qfs.h"
#include "test.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* fnv-1a 64 of the pixels the CD gave when the recipe was made; since the light specks left of the labels go
 * (14 pixels: the dash at 11..18 x 280..281, 11 x 319..320, 31 x 320, 79 x 160), no longer 0xdb6fac214f738167 */
#define CONFIG_BGND_FNV 0x181910a92ad88bb0ULL

enum { BLACK, RED, GREEN, BLUE, WHITE, AUDIO = 1, GAMEMODE = 8 };

static const uint8_t colours[5][3] = { { 0, 0, 0 }, { 200, 0, 0 }, { 0, 200, 0 }, { 0, 0, 200 }, { 220, 220, 220 } };
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

static void compose_with(int done_cancel)
{
    config_bgnd_compose((const uint8_t *const *)screens, palette, done_cancel, pixels);
}

static void compose(void)
{
    compose_with(1);
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

/* DONE and CANCEL most screens carry, or Game Mode's plain corners */
static void test_done_and_cancel_kept_or_left_out(void)
{
    put_all(30, 450, GREEN);
    put_all(580, 450, GREEN);
    put(GAMEMODE, 30, 450, BLUE);
    put(GAMEMODE, 580, 450, BLUE);
    compose_with(1);
    CHECK_EQ_INT(at(30, 450), GREEN);
    compose_with(0);
    CHECK_EQ_INT(at(30, 450), BLUE);
    CHECK_EQ_INT(at(580, 450), BLUE);
}

/* in the labels' column a light pixel every screen kept (the medoid's remnant of their labels) goes, on a flat patch
 * or a texture; a line two pixels thick and an edge stay, so do the texture and the art right of the column */
static void test_light_specks_go_lines_and_edges_stay(void)
{
    put_all(40, 200, WHITE);
    for (int y = 230; y < 240; y++)
        for (int x = 30; x < 40; x++) put_all(x, y, (x + y) % 2 ? BLUE : BLACK);
    put_all(34, 234, WHITE);
    for (int x = 30; x < 36; x++) put_all(x, 260, WHITE), put_all(x, 261, WHITE);
    for (int y = 280; y < 290; y++)
        for (int x = 60; x < 70; x++) put_all(x, y, WHITE);
    put_all(300, 200, WHITE);
    compose();
    CHECK_EQ_INT(at(40, 200), BLACK);
    CHECK_EQ_INT(at(34, 234), BLACK);
    CHECK_EQ_INT(at(35, 234), BLUE);
    CHECK_EQ_INT(at(32, 260), WHITE);
    CHECK_EQ_INT(at(33, 261), WHITE);
    CHECK_EQ_INT(at(65, 280), WHITE);
    CHECK_EQ_INT(at(60, 285), WHITE);
    CHECK_EQ_INT(at(300, 200), WHITE);
}

/* a piece cut as a shape placed where it was */
static void test_a_piece_is_a_shape_at_its_place(void)
{
    static uint8_t px[CONFIG_BGND_W * CONFIG_BGND_H];
    px[207 * CONFIG_BGND_W + 189] = 5, px[208 * CONFIG_BGND_W + 190] = 6;
    uint8_t shape[CONFIG_BGND_SHAPE_HEADER + 2 * 2];
    CHECK_EQ_INT(config_bgnd_shape(px, 189, 207, 2, 2, shape), sizeof shape);
    CHECK_EQ_INT(shape[0], 0x7b);
    CHECK_EQ_INT(shape[1] | shape[2] << 8 | shape[3] << 16, sizeof shape);
    CHECK_EQ_INT(shape[4] | shape[5] << 8, 2);
    CHECK_EQ_INT(shape[6] | shape[7] << 8, 2);
    CHECK_EQ_INT(shape[12] | shape[13] << 8, 189);
    CHECK_EQ_INT(shape[14] | shape[15] << 8, 207);
    const uint8_t piece[4] = { 5, 0, 0, 6 };
    CHECK_MEM(shape + CONFIG_BGND_SHAPE_HEADER, piece, 4);
    CHECK_EQ_INT(config_bgnd_shape(px, 639, 0, 2, 1, shape), 0);
    CHECK_EQ_INT(config_bgnd_shape(px, -1, 0, 1, 1, shape), 0);
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
    if (!config_bgnd_load(read_cd, CONFIG_BGND_OPTIONS, 1, pixels, pal)) {
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
    for (int i = 0; i < 5; i++) memcpy(palette + 3 * i, colours[i], 3);
    for (int s = 0; s < CONFIG_BGND_SCREENS; s++) {
        screens[s] = calloc(3, CONFIG_BGND_W * CONFIG_BGND_H);
        if (screens[s] == NULL) return 1;
    }
    test_the_text_few_screens_carry_is_dropped();
    test_the_bands_most_screens_carry_text_in_come_from_a_clean_screen();
    test_the_title_is_covered_by_the_texture_beside_it();
    test_the_colours_are_the_nearest_of_the_palette();
    test_done_and_cancel_kept_or_left_out();
    test_light_specks_go_lines_and_edges_stay();
    test_a_piece_is_a_shape_at_its_place();
    test_the_cd_gives_the_same_pixels();
    CHECK(!config_bgnd_load(read_cd, CONFIG_BGND_SCREENS, 1, pixels, palette));
    for (int s = 0; s < CONFIG_BGND_SCREENS; s++) free(screens[s]);
    return TEST_REPORT();
}
