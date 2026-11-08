/* The reflective masks of a track's textures (track_mask.h): white (red 128 or more, opaque) is reflective and black or transparent is not, a mask of another size is scaled
 * by nearest neighbour, an indexed PNG reads as an RGBA one, what is not a PNG is refused, the lookup finds <root>/<TRACK>/slot_<NN>.png and scenery_<NN>.png (a dev override,
 * a mod's, the program's data), and track_masks_set hands each texture its own mask by its address (none for the ones without a file). No pixel of the game is used. */
#include "formats/png.h"
#include "game/race/track_mask.h"
#include "game/sys/data_path.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

uint8_t *platform_read_file(const char *path, size_t *size)
{
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    long n = 0;
    uint8_t *d = NULL;
    if (fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    if (n > 0 && fseek(f, 0, SEEK_SET) == 0) d = malloc((size_t)n);
    if (d != NULL && fread(d, 1, (size_t)n, f) == (size_t)n) *size = (size_t)n;
    else free(d), d = NULL;
    fclose(f);
    return d;
}
void platform_free_file(uint8_t *data) { free(data); }

#define ROOT "track_mask_test_masks"   /* the directory the test makes in the working directory */

static void make_dirs(const char *path)   /* mkdir -p, every step ignoring "exists" */
{
    char part[256];
    for (size_t i = 1; path[i] != '\0'; i++) {
        if (path[i] != '/') continue;
        snprintf(part, sizeof part, "%.*s", (int)i, path);
        mkdir(part, 0755);
    }
    mkdir(path, 0755);
}

/* an RGBA mask of `width` x `height` from `pixels` (4 bytes each) written to <dir>/<track>/<file> */
static void write_rgba(const char *dir, const char *track, const char *file, const uint8_t *pixels, int width, int height)
{
    char path[256];
    snprintf(path, sizeof path, "%s/%s", dir, track);
    make_dirs(path);
    snprintf(path, sizeof path, "%s/%s/%s", dir, track, file);
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_rgba(pixels, (uint32_t)width, (uint32_t)height, &png, &len), PNG_OK);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(fwrite(png, 1, len, f) == len);
        fclose(f);
    }
    png_free(png);
}

/* white is reflective; black, transparent white, and a dark red are not; red 128 is */
static void test_colours(void)
{
    const uint8_t rgba[6 * 4] = { 255, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 0, 127, 255, 255, 255, 128, 0, 0, 255, 200, 40, 40, 128 };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_rgba(rgba, 6, 1, &png, &len), PNG_OK);
    uint8_t out[6];
    CHECK(track_mask_parse(png, len, 6, 1, out));
    static const uint8_t WANT[6] = { 1, 0, 0, 0, 1, 1 };   /* black, transparent white and red 127 are not; half transparent still counts */
    CHECK_MEM(out, WANT, sizeof WANT);
    png_free(png);
}

/* an indexed export (a palette) reads the same: its palette colour decides */
static void test_indexed(void)
{
    uint8_t palette[768] = { 0 };
    palette[3] = palette[4] = palette[5] = 255;
    const uint8_t indices[4] = { 1, 0, 0, 1 };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_indexed(indices, 4, 1, palette, &png, &len), PNG_OK);
    uint8_t out[4];
    CHECK(track_mask_parse(png, len, 4, 1, out));
    CHECK_MEM(out, indices, sizeof indices);
    png_free(png);
}

/* a mask of another size is scaled by nearest neighbour */
static void test_scaled(void)
{
    const uint8_t rgba[2 * 2 * 4] = { 255, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255 };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_rgba(rgba, 2, 2, &png, &len), PNG_OK);
    uint8_t big[4 * 4];
    CHECK(track_mask_parse(png, len, 4, 4, big));
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) CHECK_EQ_INT(big[y * 4 + x], (y / 2 + x / 2) % 2 == 0);
    uint8_t one[1];
    CHECK(track_mask_parse(png, len, 1, 1, one));
    CHECK_EQ_INT(one[0], 1);   /* the top left texel */
    png_free(png);
}

static void test_refused(void)
{
    uint8_t out[4];
    CHECK(!track_mask_parse((const uint8_t *)"not a png", 9, 2, 2, out));
    CHECK(!track_mask_parse(NULL, 0, 2, 2, out));
    const uint8_t rgba[4 * 4] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_rgba(rgba, 2, 2, &png, &len), PNG_OK);
    CHECK(!track_mask_parse(png, len, 0, 2, out));
    CHECK(!track_mask_parse(png, len / 2, 2, 2, out));   /* cut short */
    png_free(png);
    CHECK(track_mask_load("TNOSUCH", "slot_00", 4, 4) == NULL);   /* no file */
    CHECK(track_mask_load(NULL, "slot_00", 4, 4) == NULL);
}

/* the lookup order: TNFS_TRACK_MASKS (a dev override) first, then the mods folder's track_masks/, then the port's data/ */
static void test_mod_before_data(void)
{
    const uint8_t white[4 * 4] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
    const uint8_t black[4 * 4] = { 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255 };
    make_dirs(ROOT "/base/data/track_masks/TORD"), make_dirs(ROOT "/mod/track_masks/TORD"), make_dirs(ROOT "/env/TORD");
    write_rgba(ROOT "/base/data/track_masks", "TORD", "slot_03.png", white, 2, 2);   /* the port's own: reflective */
    data_path_set_base(ROOT "/base");
    unsetenv("TNFS_TRACK_MASKS");
    uint8_t *m = track_mask_load("TORD", "slot_03", 2, 2);
    CHECK(m != NULL && m[0] == 1);
    free(m);
    write_rgba(ROOT "/mod/track_masks", "TORD", "slot_03.png", black, 2, 2);   /* a mod's: nothing */
    data_path_set_mod(ROOT "/mod");
    m = track_mask_load("TORD", "slot_03", 2, 2);
    CHECK(m != NULL && m[0] == 0);   /* the mod's, not the port's */
    free(m);
    write_rgba(ROOT "/env", "TORD", "slot_03.png", white, 2, 2);
    setenv("TNFS_TRACK_MASKS", ROOT "/env", 1);
    m = track_mask_load("TORD", "slot_03", 2, 2);
    CHECK(m != NULL && m[0] == 1);   /* the dev override before both */
    free(m);
    unsetenv("TNFS_TRACK_MASKS");
    data_path_set_mod(NULL);
    data_path_set_base(NULL);
}

static TrackTexture texture(int width, int height, const uint8_t *pixels)
{
    TrackTexture t;
    memset(&t, 0, sizeof t);
    t.present = 1;
    t.bitmap.width = (uint16_t)width, t.bitmap.height = (uint16_t)height, t.bitmap.payload = pixels;
    return t;
}

/* track_masks_set: each texture is found by its address; a slot's variant 0 and a scenery board; the others have none; the next call drops the first track's */
static void test_registry(void)
{
    static TrackArt art;
    memset(&art, 0, sizeof art);
    uint8_t pixels[2 * 2];
    art.slots[3][0] = texture(2, 2, pixels);
    art.slots[7][0] = texture(2, 2, pixels);
    art.slots[3][1] = texture(1, 1, pixels);   /* a variant: not asked for */
    art.scenery[5] = texture(4, 2, pixels);
    const uint8_t white[4 * 4] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
    const uint8_t half[4 * 4] = { 255, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255, 255 };
    write_rgba(ROOT "/env", "TREG", "slot_03.png", half, 2, 2);
    write_rgba(ROOT "/env", "TREG", "scenery_05.png", white, 2, 2);   /* 2x2 for a 4x2 board: scaled */
    write_rgba(ROOT "/env", "TREG", "slot_09.png", white, 2, 2);      /* a slot with no bitmap: no mask */
    setenv("TNFS_TRACK_MASKS", ROOT "/env", 1);
    CHECK_EQ_INT(track_masks_set("TREG", &art), 2);
    const uint8_t *slot = track_mask_for(&art.slots[3][0]);
    CHECK(slot != NULL);
    if (slot != NULL) {
        static const uint8_t WANT[4] = { 1, 0, 0, 1 };
        CHECK_MEM(slot, WANT, sizeof WANT);
    }
    const uint8_t *board = track_mask_for(&art.scenery[5]);
    CHECK(board != NULL);
    if (board != NULL)
        for (int i = 0; i < 8; i++) CHECK_EQ_INT(board[i], 1);
    CHECK(track_mask_for(&art.slots[7][0]) == NULL);   /* a texture with no file */
    CHECK(track_mask_for(&art.slots[3][1]) == NULL);
    CHECK(track_mask_for(NULL) == NULL);
    CHECK_EQ_INT(track_masks_set("TNONE", &art), 0);   /* another track: the first one's masks are gone */
    CHECK(track_mask_for(&art.slots[3][0]) == NULL);
    CHECK_EQ_INT(track_masks_set("TREG", &art), 2);
    CHECK_EQ_INT(track_masks_set(NULL, NULL), 0);
    CHECK(track_mask_for(&art.scenery[5]) == NULL);
    unsetenv("TNFS_TRACK_MASKS");
}

/* a rally's road is gravel, dirt or snow: the slots' masks (the cat's eyes) are not kept, the scenery boards' (signs, posts) are */
static void test_a_rally_keeps_no_road_masks(void)
{
    static TrackArt art;
    memset(&art, 0, sizeof art);
    uint8_t pixels[2 * 2];
    art.slots[3][0] = texture(2, 2, pixels);
    art.scenery[5] = texture(4, 2, pixels);
    const uint8_t white[4 * 4] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
    write_rgba(ROOT "/env", "TRALLY", "slot_03.png", white, 2, 2);
    write_rgba(ROOT "/env", "TRALLY", "scenery_05.png", white, 2, 2);
    setenv("TNFS_TRACK_MASKS", ROOT "/env", 1);
    CHECK_EQ_INT(track_masks_set_race("TRALLY", &art, 0), 2);   /* the circuit: both */
    CHECK(track_mask_for(&art.slots[3][0]) != NULL);
    CHECK_EQ_INT(track_masks_set_race("TRALLY", &art, 1), 1);   /* the rally: the board only */
    CHECK(track_mask_for(&art.slots[3][0]) == NULL);
    CHECK(track_mask_for(&art.scenery[5]) != NULL);
    CHECK_EQ_INT(track_masks_set("TRALLY", &art), 2);           /* the old call is the circuit's */
    unsetenv("TNFS_TRACK_MASKS");
}

int main(void)
{
    test_colours();
    test_indexed();
    test_scaled();
    test_refused();
    test_mod_before_data();
    test_registry();
    test_a_rally_keeps_no_road_masks();
    return TEST_REPORT();
}
