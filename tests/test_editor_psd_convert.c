/* Convert PSD without python (editor/psd_convert.h): a folder of small PSDs becomes the car's mask PNGs and the dash's, the backups are not PSDs of the car,
 * a PSD that cannot be read is named in words, and the PSDs are never touched. */
#include "psd_convert.h"

#include "formats/car_mask_codec.h"
#include "formats/png.h"
#include "psd_fixture.h"
#include "test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { W = 4 };

static char root[512];

static void path_of(char *out, size_t cap, const char *rel)
{
    snprintf(out, cap, "%s/%s", root, rel);
}

static void dir_of(const char *rel)
{
    char p[600];
    path_of(p, sizeof p, rel);
    CHECK(mkdir(p, 0755) == 0);
}

static void put(const char *rel, const FxBuf *b)
{
    char p[600];
    path_of(p, sizeof p, rel);
    FILE *f = fopen(p, "wb");
    CHECK(f != NULL && fwrite(b->p, 1, b->n, f) == b->n);
    if (f != NULL) fclose(f);
}

static int exists(const char *rel)
{
    char p[600];
    path_of(p, sizeof p, rel);
    return access(p, F_OK) == 0;
}

/* the grey of PNG `rel`'s texel x, or (dash) its palette colour as 0xRRGGBB; -1 when it cannot be read */
static int texel(const char *rel, int x, int dash)
{
    char p[600];
    path_of(p, sizeof p, rel);
    FILE *f = fopen(p, "rb");
    if (f == NULL) return -1;
    uint8_t *bytes = malloc(1 << 16);
    const size_t n = fread(bytes, 1, 1 << 16, f);
    fclose(f);
    PngImage img;
    const PngResult r = png_read(bytes, n, &img);
    free(bytes);
    if (r != PNG_OK) return -1;
    int v;
    if (img.indexed) {
        const uint8_t *c = img.palette + img.pixels[x] * 3;
        v = dash ? (c[0] << 16) | (c[1] << 8) | c[2] : c[0];
    }
    else v = img.pixels[x * 4];
    png_free(img.pixels);
    return v;
}

static const uint8_t SIDE[W][4] = { { 255, 255, 255, 0 }, { 20, 40, 220, 255 }, { 20, 220, 40, 255 }, { 255, 0, 255, 255 } };   /* paint, shine, body, black */

static FxBuf side_psd(void)
{
    FxLayer l[1] = { { "mask", NULL, 0, 0, 0, W, 1, &SIDE[0][0], 0, 1, 0 } };
    return fx_psd(W, 1, l, 1, 8, 3, 1);
}

static FxBuf dash_psd(void)
{
    static const uint8_t M[W][4] = { { 0, 0, 255, 255 }, { 0, 255, 0, 255 }, { 255, 255, 255, 255 }, { 0, 0, 0, 255 } };
    static const uint8_t C[W][4] = { { 0 }, { 0 }, { 10, 200, 30, 255 }, { 0 } };
    FxLayer l[2] = { { "mask", NULL, 0, 0, 0, W, 1, &M[0][0], 0, 1, 0 }, { "color", NULL, 0, 0, 0, W, 1, &C[0][0], 0, 1, 0 } };
    return fx_psd(W, 1, l, 2, 8, 3, 1);
}

static void test_a_folder_of_psds(void)
{
    dir_of("psds");
    FxBuf side = side_psd(), dash = dash_psd();
    put("psds/side.psd", &side);
    put("psds/side_lo.psd", &side);
    put("psds/side.before-1.psd", &side);   /* a backup: not a PSD of the car */
    put("psds/dash.psd", &dash);
    dir_of("masks");
    dir_of("dashes");
    char masks[600], dashes[600], why[300] = "", psds[600];
    path_of(masks, sizeof masks, "masks");
    path_of(dashes, sizeof dashes, "dashes");
    path_of(psds, sizeof psds, "psds");
    PsdConverted done;
    CHECK(psd_convert(psds, "P911", masks, dashes, NULL, &done, why, sizeof why));
    CHECK_EQ_INT(done.masks, 2);
    CHECK_EQ_INT(done.dash_files, 2);
    CHECK(exists("masks/P911/side_mask.png") && exists("masks/P911/side_lo_mask.png"));
    CHECK(!exists("masks/P911/side.before-1_mask.png"));
    CHECK_EQ_INT(texel("masks/P911/side_mask.png", 0, 0), CAR_MASK_PAINT);
    CHECK_EQ_INT(texel("masks/P911/side_mask.png", 1, 0), CAR_MASK_SHINE);
    CHECK_EQ_INT(texel("masks/P911/side_mask.png", 2, 0), CAR_MASK_BODY);
    CHECK_EQ_INT(texel("masks/P911/side_lo_mask.png", 3, 0), CAR_MASK_BLACK);
    CHECK(exists("dashes/P911/dash_mask.png") && exists("dashes/P911/dash_color.png"));
    CHECK_EQ_INT(texel("dashes/P911/dash_mask.png", 0, 1), 0x0000FF);   /* the dash class blue */
    CHECK_EQ_INT(texel("dashes/P911/dash_mask.png", 1, 1), 0x00FF00);   /* green */
    CHECK_EQ_INT(texel("dashes/P911/dash_mask.png", 2, 1), 0xFFFFFF);   /* white */
    free(side.p);
    free(dash.p);
}

static void test_a_psd_that_cannot_be_read(void)
{
    dir_of("bad");
    FxBuf side = side_psd();
    put("bad/side.psd", &side);
    const uint8_t junk[8] = { 'n', 'o', 't', ' ', 'a', ' ', 'P', 'S' };
    FxBuf j = { (uint8_t *)junk, sizeof junk, sizeof junk };
    put("bad/broken.psd", &j);
    dir_of("m2");
    dir_of("d2");
    char masks[600], dashes[600], why[300] = "", psds[600];
    path_of(masks, sizeof masks, "m2");
    path_of(dashes, sizeof dashes, "d2");
    path_of(psds, sizeof psds, "bad");
    PsdConverted done;
    CHECK(!psd_convert(psds, "P911", masks, dashes, NULL, &done, why, sizeof why));
    CHECK(strstr(why, "broken.psd") != NULL);   /* the file is named */
    free(side.p);
}

static void test_a_folder_with_no_dash(void)
{
    dir_of("nodash");
    FxBuf side = side_psd();
    put("nodash/side.psd", &side);
    dir_of("m3");
    dir_of("d3");
    char masks[600], dashes[600], why[300] = "", psds[600];
    path_of(masks, sizeof masks, "m3");
    path_of(dashes, sizeof dashes, "d3");
    path_of(psds, sizeof psds, "nodash");
    PsdConverted done;
    CHECK(psd_convert(psds, "P911", masks, dashes, NULL, &done, why, sizeof why));
    CHECK_EQ_INT(done.masks, 1);
    CHECK_EQ_INT(done.dash_files, 0);
    CHECK(!exists("d3/P911/dash_mask.png"));
    free(side.p);
}

static void test_a_stopped_conversion(void)
{
    char masks[600], dashes[600], why[300] = "", psds[600];
    path_of(masks, sizeof masks, "m3");
    path_of(dashes, sizeof dashes, "d3");
    path_of(psds, sizeof psds, "psds");
    PsdConverted done;
    const int stop = 1;
    CHECK(!psd_convert(psds, "Q", masks, dashes, &stop, &done, why, sizeof why));
    CHECK(strstr(why, "stopped") != NULL && done.masks == 0);
}

static void remove_tree(void)
{
    char cmd[700];
    snprintf(cmd, sizeof cmd, "find '%s' -depth -delete", root);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", root);
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/tnfs-psd-convert-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(root) == NULL) return 1;
    test_a_folder_of_psds();
    test_a_psd_that_cannot_be_read();
    test_a_folder_with_no_dash();
    test_a_stopped_conversion();
    remove_tree();
    return TEST_REPORT();
}
