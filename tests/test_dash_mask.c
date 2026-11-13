/* The hand-made dash masks (dash_mask.h): the colours class as they are painted, a draft written as a PNG reads back as it was, a mask of another size is
 * scaled by nearest neighbour, the lookup finds <root>/<CAR>/dash_mask.png (as given or in upper case), and what is not a PNG is refused. */
#include "formats/png.h"
#include "game/race/dash_mask.h"
#include "game/sys/data_path.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <sys/stat.h>
#include <unistd.h>

enum { W = 6, H = 2 };

static int read_misses;   /* reads of a file that is not there */

uint8_t *platform_read_file(const char *path, size_t *size)
{
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        read_misses++;   /* the game logs each as "cannot read" (i191) */
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

static void test_colours(void)
{
    const uint8_t black[3] = { 0, 0, 0 }, dim[3] = { 30, 40, 50 }, blue[3] = { 0, 0, 255 }, paleblue[3] = { 90, 120, 230 }, green[3] = { 0, 255, 0 };
    const uint8_t white[3] = { 255, 255, 255 }, grey[3] = { 200, 200, 200 }, red[3] = { 255, 0, 0 }, yellow[3] = { 255, 255, 0 }, midgrey[3] = { 128, 128, 128 };
    const uint8_t paleyellow[3] = { 200, 195, 60 }, orange[3] = { 255, 160, 0 }, olive[3] = { 150, 150, 0 }, greenish[3] = { 120, 255, 0 };
    CHECK_EQ_INT(dash_mask_class(black), DASH_MASK_AUTO);
    CHECK_EQ_INT(dash_mask_class(dim), DASH_MASK_AUTO);       /* a dark colour is not a mark */
    CHECK_EQ_INT(dash_mask_class(blue), DASH_MASK_DASH);
    CHECK_EQ_INT(dash_mask_class(paleblue), DASH_MASK_DASH);
    CHECK_EQ_INT(dash_mask_class(green), DASH_MASK_DIAL);
    CHECK_EQ_INT(dash_mask_class(white), DASH_MASK_LIT);
    CHECK_EQ_INT(dash_mask_class(grey), DASH_MASK_LIT);
    CHECK_EQ_INT(dash_mask_class(red), DASH_MASK_BLADE);      /* red: a wiper blade the dash art draws (hidden in rain and snow) */
    const uint8_t cyan[3] = { 0, 255, 255 }, palecyan[3] = { 40, 220, 200 }, darkcyan[3] = { 0, 150, 150 };
    CHECK_EQ_INT(dash_mask_class(cyan), DASH_MASK_NOSE);      /* cyan: the nose through the windscreen (dep49; it read as AUTO before) */
    CHECK_EQ_INT(dash_mask_class(palecyan), DASH_MASK_NOSE);
    CHECK_EQ_INT(dash_mask_class(darkcyan), DASH_MASK_AUTO);  /* a dark cyan is not a mark */
    CHECK_EQ_INT(dash_mask_class(orange), DASH_MASK_AUTO);    /* any other colour: not marked */
    CHECK_EQ_INT(dash_mask_class(yellow), DASH_MASK_LIT);     /* yellow ffff00 is LIT too: it contrasts on the dash */
    CHECK_EQ_INT(dash_mask_class(paleyellow), DASH_MASK_LIT);
    CHECK_EQ_INT(dash_mask_class(orange), DASH_MASK_AUTO);    /* a yellow with blue to spare, or too little green, is not */
    CHECK_EQ_INT(dash_mask_class(olive), DASH_MASK_AUTO);
    CHECK_EQ_INT(dash_mask_class(greenish), DASH_MASK_DIAL);  /* a green with some red stays a dial */
    CHECK_EQ_INT(dash_mask_class(midgrey), DASH_MASK_AUTO);
}

static void test_round_trip_and_scale(void)
{
    uint8_t mask[W * H], back[W * H];
    static const uint8_t CLASSES[6] = { DASH_MASK_AUTO, DASH_MASK_DASH, DASH_MASK_DIAL, DASH_MASK_LIT, DASH_MASK_BLADE, DASH_MASK_NOSE };
    for (int i = 0; i < W * H; i++) mask[i] = CLASSES[(i * 5 + i / W) % 6];
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(dash_mask_write(mask, W, H, &png, &len));
    CHECK(png != NULL && dash_mask_parse(png, len, W, H, back));
    CHECK(memcmp(mask, back, sizeof mask) == 0);
    uint8_t half[3 * 1];   /* scaled down by nearest neighbour: the pixel at (x * 6 / 3, 0) */
    CHECK(dash_mask_parse(png, len, 3, 1, half));
    for (int x = 0; x < 3; x++) CHECK_EQ_INT(half[x], mask[x * W / 3]);
    CHECK(!dash_mask_parse(png, 7, W, H, back));              /* cut short: refused */
    const uint8_t junk[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    CHECK(!dash_mask_parse(junk, sizeof junk, W, H, back));
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
    static uint8_t full[DASH_MASK_W * DASH_MASK_H];
    memset(full, DASH_MASK_AUTO, sizeof full);
    full[5] = DASH_MASK_LIT, full[DASH_MASK_W + 7] = DASH_MASK_DIAL;
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(dash_mask_write(full, DASH_MASK_W, DASH_MASK_H, &png, &len));
    mkdir(root, 0755);
    mkdir(under("/TDASH"), 0755);
    FILE *f = fopen(under("/TDASH/dash_mask.png"), "wb");
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(fwrite(png, 1, len, f) == len);
        fclose(f);
    }
    png_free(png);
    setenv("TNFS_DASH_MASKS", root, 1);
    uint8_t *m = dash_mask_load("TDASH");
    CHECK(m != NULL);
    if (m != NULL) CHECK(m[5] == DASH_MASK_LIT && m[DASH_MASK_W + 7] == DASH_MASK_DIAL && m[0] == DASH_MASK_AUTO);
    free(m);
    m = dash_mask_load("TDash");                              /* the dash's stem in mixed case: found in upper case */
    CHECK(m != NULL);
    free(m);
    unsetenv("TNFS_DASH_MASKS");   /* the data folders only: the dev override is read as named */
    read_misses = 0;
    CHECK(dash_mask_load("TNOSUCH") == NULL);
    CHECK_EQ_INT(read_misses, 0);   /* i191: the folders without the file are not read (no "cannot read" line each) */
    CHECK(dash_mask_load(NULL) == NULL);
}

static void put(const char *dir, const char *text)   /* <dir>/dash_masks/TORDER/note.txt (or <dir>/TORDER/note.txt for the env override's root) holding `text` */
{
    char path[256], parent[256];
    for (size_t i = 1; dir[i] != '\0'; i++)
        if (dir[i] == '/') snprintf(parent, sizeof parent, "%.*s", (int)i, dir), mkdir(parent, 0755);
    mkdir(dir, 0755);
    snprintf(path, sizeof path, "%s/note.txt", dir);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) fputs(text, f), fclose(f);
}

static int note_is(const char *text)
{
    size_t size = 0;
    uint8_t *file = dash_asset_read("TORDER", "note.txt", &size);
    const int same = file != NULL && size == strlen(text) && memcmp(file, text, size) == 0;
    free(file);
    return same;
}

/* the lookup order: TNFS_DASH_MASKS (a dev override) first, then the mods folder's dash_masks/, then the port's data/ */
static void test_mod_before_data(void)
{
    put(under("/order/base/data/dash_masks/TORDER"), "base");
    data_path_set_base(under("/order/base"));
    unsetenv("TNFS_DASH_MASKS");
    CHECK(note_is("base"));   /* no mod: the port's */
    data_path_set_mod(under("/order/empty_mod"));   /* i191: a mods folder without dash_masks/ is passed over without a read */
    read_misses = 0;
    CHECK(note_is("base"));
    CHECK_EQ_INT(read_misses, 0);
    put(under("/order/mod/dash_masks/TORDER"), "mod");
    data_path_set_mod(under("/order/mod"));
    CHECK(note_is("mod"));    /* the mod's first */
    put(under("/order/env/TORDER"), "env");
    setenv("TNFS_DASH_MASKS", under("/order/env"), 1);
    CHECK(note_is("env"));    /* the env override before both */
    unsetenv("TNFS_DASH_MASKS");
    data_path_set_mod(NULL), data_path_set_base(NULL);
}

int main(void)
{
    scratch_make("/tmp/tnfs_test_dash_masks.XXXXXX");
    test_colours();
    test_round_trip_and_scale();
    test_lookup();
    test_mod_before_data();
    scratch_remove();
    return TEST_REPORT();
}
