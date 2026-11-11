/* The reflective masks of a track's textures (track_mask.h): white (red 128 or more, opaque) is reflective and black or transparent is not, a mask of another size is scaled
 * by nearest neighbour, an indexed PNG reads as an RGBA one, what is not a PNG is refused, the lookup finds <root>/<TRACK>/slot_<NN>.png and scenery_<NN>.png (a dev override,
 * a mod's, the program's data), and track_masks_set hands each texture its own mask by its address (none for the ones without a file). No pixel of the game is used. */
#include "formats/png.h"
#include "game/race/track_mask.h"
#include "game/sys/data_path.h"
#include "game/sys/port_log.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int read_misses;   /* reads of a file that is not there: the game logs each as "cannot read" (i191) */

uint8_t *platform_read_file(const char *path, size_t *size)
{
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        read_misses++;
        return NULL;
    }
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

/* i191: the candidates without the file are not read (a clean install has no masks: no "cannot read" line for each); a file that is there and is not a mask is logged */
static void test_missing_masks_are_quiet(void)
{
    const uint8_t white[4 * 4] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
    make_dirs(ROOT "/quiet/data/track_masks/TQUI"), make_dirs(ROOT "/quiet_mod");
    write_rgba(ROOT "/quiet/data/track_masks", "TQUI", "slot_04.png", white, 2, 2);
    data_path_set_base(ROOT "/quiet");
    data_path_set_mod(ROOT "/quiet_mod");   /* a mods folder without track_masks/ */
    unsetenv("TNFS_TRACK_MASKS");
    read_misses = 0;
    uint8_t *m = track_mask_load("TQUI", "slot_04", 2, 2);
    CHECK(m != NULL && m[0] == 1);
    free(m);
    CHECK(track_mask_load("TQUI", "slot_05", 2, 2) == NULL);   /* in no folder */
    CHECK_EQ_INT(read_misses, 0);
    FILE *f = fopen(ROOT "/quiet/data/track_masks/TQUI/slot_06.png", "wb");   /* there, but not a PNG */
    CHECK(f != NULL);
    if (f != NULL) fputs("not a png", f), fclose(f);
    port_log_watch("cannot read");
    CHECK(track_mask_load("TQUI", "slot_06", 2, 2) == NULL);
    CHECK(port_log_watch_hit());
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

/* yellow (red and green 128 or more, blue below 128) is a lamp and not reflective; red and white stay reflective; a pale yellow with blue 128 is white-ish: reflective */
static void test_yellow_is_not_reflective(void)
{
    const uint8_t rgba[5 * 4] = { 255, 255, 0, 255, 255, 0, 0, 255, 255, 255, 255, 255, 255, 255, 127, 255, 255, 255, 128, 255 };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_rgba(rgba, 5, 1, &png, &len), PNG_OK);
    uint8_t out[5];
    CHECK(track_mask_parse(png, len, 5, 1, out));
    static const uint8_t WANT[5] = { 0, 1, 1, 0, 1 };   /* yellow no, red yes, white yes, yellow with blue 127 no, blue 128 yes */
    CHECK_MEM(out, WANT, sizeof WANT);
    png_free(png);
}

/* the lamps the yellow of a road slot's mask names: the slot, the bounding box of the yellow texels, the palette entries under them (the most used first); rally or not; the table is the fallback */
static void test_yellow_marks_make_lamps(void)
{
    static TrackArt art;
    memset(&art, 0, sizeof art);
    uint8_t pixels[4 * 4] = { 7, 7, 7, 7,   7, 9, 9, 7,   7, 9, 4, 7,   7, 7, 7, 255 };   /* texel (1,1) .. (2,2) are the lamp's: 9, 9, 9, 4 */
    art.slots[3][0] = texture(4, 4, pixels);
    art.slots[8][0] = texture(4, 4, pixels);
    uint8_t rgba[4 * 4 * 4];
    memset(rgba, 0, sizeof rgba);
    for (int y = 1; y <= 2; y++)
        for (int x = 1; x <= 2; x++) memcpy(rgba + 4 * (y * 4 + x), (const uint8_t[]){ 255, 255, 0, 255 }, 4);
    write_rgba(ROOT "/env", "TYEL", "slot_03.png", rgba, 4, 4);
    uint8_t red[4 * 4 * 4];
    memset(red, 0, sizeof red);
    memcpy(red, (const uint8_t[]){ 255, 0, 0, 255 }, 4);
    write_rgba(ROOT "/env", "TYEL", "slot_08.png", red, 4, 4);   /* a mask of red only: reflective, no lamp */
    setenv("TNFS_TRACK_MASKS", ROOT "/env", 1);
    CHECK_EQ_INT(track_masks_set_race("TYEL", &art, 0), 2);   /* both masks are kept (the yellow one has no reflective texel, as the reflect pass ignores) */
    const TunnelLampTexture *lamps = NULL;
    CHECK_EQ_INT(track_lamp_marks("tyel", &lamps), 1);   /* any case */
    if (lamps != NULL) {
        CHECK_EQ_INT(lamps[0].slot, 3);
        CHECK_EQ_INT(lamps[0].rect[0], 1);
        CHECK_EQ_INT(lamps[0].rect[1], 1);
        CHECK_EQ_INT(lamps[0].rect[2], 2);
        CHECK_EQ_INT(lamps[0].rect[3], 2);
        CHECK_EQ_INT(lamps[0].count, 2);
        CHECK_EQ_INT(lamps[0].idx[0], 9);   /* three texels */
        CHECK_EQ_INT(lamps[0].idx[1], 4);   /* one */
        uint8_t exempt[256];
        tunnel_lamp_texture_exempt(&lamps[0], pixels, 4, 4, exempt);
        CHECK(exempt[9] && exempt[4] && !exempt[7] && !exempt[255]);   /* the colours under the yellow stay bright, the wall's 7 does not (rect or no rect) */
    }
    CHECK_EQ_INT(track_lamp_marks("TOTHER", NULL), 0);   /* another track has none */
    CHECK_EQ_INT(track_lamps_for("TYEL", &lamps), 1);
    CHECK_EQ_INT(track_masks_set_race("TYEL", &art, 1), 0);   /* a rally: no reflective masks kept, the lamp is still there */
    CHECK_EQ_INT(track_lamp_marks("TYEL", NULL), 1);
    /* a track with no marks has the table's lamps: CY1's, with no art */
    CHECK_EQ_INT(track_masks_set_race("CY1", &art, 0), 0);
    CHECK_EQ_INT(track_lamps_for("CY1", &lamps), 1);
    CHECK(lamps != NULL && lamps[0].slot == 38);
    CHECK_EQ_INT(track_lamps_for("TR1", NULL), 0);
    CHECK_EQ_INT(track_masks_set_race(NULL, NULL, 0), 0);
    CHECK_EQ_INT(track_lamp_marks("TYEL", NULL), 0);   /* dropped with the track */
    unsetenv("TNFS_TRACK_MASKS");
}

/* the race's palette remaps the track's texels in place after the marks were read (the exe's mapping to the screen palette): track_lamp_marks_refresh reads the colours under the yellow again */
static void test_marks_follow_the_remapped_texels(void)
{
    static TrackArt art;
    memset(&art, 0, sizeof art);
    uint8_t pixels[4 * 4] = { 7, 7, 7, 7,   7, 9, 9, 7,   7, 9, 4, 7,   7, 7, 7, 255 };
    art.slots[3][0] = texture(4, 4, pixels);
    uint8_t rgba[4 * 4 * 4];
    memset(rgba, 0, sizeof rgba);
    for (int y = 1; y <= 2; y++)
        for (int x = 1; x <= 2; x++) memcpy(rgba + 4 * (y * 4 + x), (const uint8_t[]){ 255, 255, 0, 255 }, 4);
    write_rgba(ROOT "/env", "TREM", "slot_03.png", rgba, 4, 4);
    setenv("TNFS_TRACK_MASKS", ROOT "/env", 1);
    CHECK_EQ_INT(track_masks_set_race("TREM", &art, 0), 1);
    static const uint8_t REMAP[3][2] = { { 7, 170 }, { 9, 158 }, { 4, 231 } };   /* the race palette's entries for the track's */
    for (int i = 0; i < 16; i++)
        for (int k = 0; k < 3; k++)
            if (pixels[i] == REMAP[k][0]) { pixels[i] = REMAP[k][1]; break; }
    track_lamp_marks_refresh();
    const TunnelLampTexture *lamps = NULL;
    CHECK_EQ_INT(track_lamp_marks("TREM", &lamps), 1);
    if (lamps != NULL) {
        CHECK_EQ_INT(lamps[0].count, 2);
        CHECK_EQ_INT(lamps[0].idx[0], 158);
        CHECK_EQ_INT(lamps[0].idx[1], 231);
        CHECK(lamps[0].set != NULL && lamps[0].set[158] && lamps[0].set[231] && !lamps[0].set[9] && !lamps[0].set[4] && !lamps[0].set[170]);
        CHECK_EQ_INT(lamps[0].rect[0], 1);
        CHECK_EQ_INT(lamps[0].rect[3], 2);
    }
    track_lamp_marks_refresh();   /* again: the same (it reads, it does not remap) */
    CHECK_EQ_INT(track_lamp_marks("TREM", &lamps), 1);
    if (lamps != NULL) CHECK_EQ_INT(lamps[0].idx[0], 158);
    CHECK_EQ_INT(track_masks_set(NULL, NULL), 0);
    track_lamp_marks_refresh();   /* none kept: nothing to do */
    CHECK_EQ_INT(track_lamp_marks("TREM", NULL), 0);
}

int main(void)
{
    test_colours();
    test_indexed();
    test_scaled();
    test_refused();
    test_mod_before_data();
    test_missing_masks_are_quiet();
    test_registry();
    test_a_rally_keeps_no_road_masks();
    test_yellow_is_not_reflective();
    test_yellow_marks_make_lamps();
    test_marks_follow_the_remapped_texels();
    return TEST_REPORT();
}
