/* The dash colours the owner paints (dash_color.h): a painted RGBA reads into slots (transparent and black have none, near colours share a slot, a far one gets its own),
 * the slots hold at DASH_COLOUR_SLOTS, a size other than 640 x 480 is scaled by nearest neighbour, the lookup finds <root>/<CAR>/dash_color.png (as given or in
 * upper case), and what is not a PNG, or is absent, gives nothing (the game then lights with gauge_light 3's orange). */
#include "formats/png.h"
#include "game/race/dash_color.h"
#include "game/race/dash_mask.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <sys/stat.h>
#include <unistd.h>

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

enum { W = 4, H = 2 };

static void put(uint8_t *rgba, int i, int r, int g, int b, int a)
{
    rgba[4 * i] = (uint8_t)r, rgba[4 * i + 1] = (uint8_t)g, rgba[4 * i + 2] = (uint8_t)b, rgba[4 * i + 3] = (uint8_t)a;
}

static void test_slots(void)
{
    uint8_t rgba[W * H * 4];
    put(rgba, 0, 0, 0, 0, 0);          /* transparent: none */
    put(rgba, 1, 255, 0, 0, 255);      /* red: slot 1 */
    put(rgba, 2, 250, 10, 5, 255);     /* near red: the same slot */
    put(rgba, 3, 0, 255, 0, 255);      /* green: slot 2 */
    put(rgba, 4, 10, 10, 10, 255);     /* black-ish: none */
    put(rgba, 5, 0, 255, 0, 100);      /* barely opaque: none */
    put(rgba, 6, 40, 120, 255, 255);   /* blue: slot 3 */
    put(rgba, 7, 255, 0, 0, 255);      /* red again: slot 1 */
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(png_write_rgba(rgba, W, H, &png, &len) == PNG_OK);
    DashColour c;
    CHECK(dash_color_parse(png, len, W, H, &c));
    const uint8_t want[W * H] = { 0, 1, 1, 2, 0, 0, 3, 1 };
    CHECK(c.slot != NULL && memcmp(c.slot, want, sizeof want) == 0);
    CHECK_EQ_INT(c.count, 3);
    CHECK(c.rgb[0][0] == 255 && c.rgb[0][1] == 0 && c.rgb[1][1] == 255 && c.rgb[2][2] == 255);
    dash_color_free(&c);
    CHECK(c.slot == NULL && c.count == 0);
    DashColour half;   /* scaled down by nearest neighbour: the pixel at (x * 4 / 2, 0) */
    CHECK(dash_color_parse(png, len, 2, 1, &half));
    CHECK(half.slot != NULL && half.slot[0] == 0 && half.slot[1] == 1);   /* pixels 0 and 2: transparent, near red */
    dash_color_free(&half);
    CHECK(!dash_color_parse(png, 7, W, H, &c));   /* cut short: refused, nothing kept */
    CHECK(c.slot == NULL && c.count == 0);
    const uint8_t junk[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    CHECK(!dash_color_parse(junk, sizeof junk, W, H, &c));
    CHECK(!dash_color_parse(NULL, 0, W, H, &c));
    png_free(png);
}

static void test_slot_limit(void)
{
    enum { N = DASH_COLOUR_SLOTS + 4 };   /* more far colours than slots: the extra ones take the nearest slot */
    uint8_t rgba[N * 4];
    for (int i = 0; i < N; i++) put(rgba, i, i * 16, 255 - i * 16, ((i * 37) & 255) | 64, 255);
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(png_write_rgba(rgba, N, 1, &png, &len) == PNG_OK);
    DashColour c;
    CHECK(dash_color_parse(png, len, N, 1, &c));
    CHECK(c.count <= DASH_COLOUR_SLOTS);
    for (int i = 0; c.slot != NULL && i < N; i++) CHECK(c.slot[i] >= 1 && c.slot[i] <= c.count);
    dash_color_free(&c);
    png_free(png);
}

/* a scratch folder of this run's own (two ctests at once shared a fixed /tmp one), removed at the end */
static char root[64];
static const char *under(const char *tail)
{
    static char paths[4][256];
    static int next;
    char *p = paths[next++ & 3];
    snprintf(p, 256, "%s%s", root, tail);
    return p;
}
static int remove_entry(const char *path, const struct stat *st, int type, struct FTW *ftw)
{
    (void)st, (void)type, (void)ftw;
    return remove(path);
}
static void scratch_make(const char *template_path)
{
    snprintf(root, sizeof root, "%s", template_path);
    if (mkdtemp(root) == NULL) {
        perror("mkdtemp");
        exit(1);
    }
}
static void scratch_remove(void) { nftw(root, remove_entry, 8, FTW_DEPTH | FTW_PHYS); }

static void test_lookup(void)
{
    static uint8_t full[DASH_MASK_W * DASH_MASK_H * 4];
    put(full, 5, 255, 128, 0, 255);
    put(full, DASH_MASK_W + 7, 0, 160, 255, 255);
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(png_write_rgba(full, DASH_MASK_W, DASH_MASK_H, &png, &len) == PNG_OK);
    mkdir(root, 0755);
    mkdir(under("/TDASH"), 0755);
    FILE *f = fopen(under("/TDASH/dash_color.png"), "wb");
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(fwrite(png, 1, len, f) == len);
        fclose(f);
    }
    png_free(png);
    setenv("TNFS_DASH_MASKS", root, 1);
    DashColour c;
    CHECK(dash_color_load("TDASH", &c));
    CHECK(c.slot != NULL && c.slot[5] == 1 && c.slot[DASH_MASK_W + 7] == 2 && c.slot[0] == 0 && c.count == 2);
    dash_color_free(&c);
    CHECK(dash_color_load("TDash", &c));                       /* the dash's stem in mixed case: found in upper case */
    dash_color_free(&c);
    CHECK(!dash_color_load("TNOSUCH", &c));                    /* no file: no colours, the orange fallback */
    CHECK(c.slot == NULL && c.count == 0);
    CHECK(!dash_color_load(NULL, &c));
}

int main(void)
{
    scratch_make("/tmp/tnfs_test_dash_colors.XXXXXX");
    test_slots();
    test_slot_limit();
    test_lookup();
    scratch_remove();
    return TEST_REPORT();
}
