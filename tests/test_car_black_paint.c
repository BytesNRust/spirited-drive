/* The fixed black (class 224) and the paint do not compete for palette entries: on the F512TR (its high bank's body textures already use every free entry when
 * painted white) a few texels marked black in a mask leave every texel the mask does not mark black exactly the colour it has with no black, and the marked
 * texels come out dark. Both the Studio's preview and the game's Test drive run car_livery_black then car_livery_paint (this pass). Needs the CD's
 * CARFAMS ($TNFS_DATA or original/ of the source tree) and the game's masks; skipped when they are not there. */
#include "formats/car_livery_pass.h"
#include "formats/car_mask_codec.h"
#include "raster/car_scene.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TEXTURES = 4, MAX_PIXELS = 40000 };

static const char *const NAMES[TEXTURES] = { "rsid", "rear", "frnt", "J001" };
static int g_black;        /* how many texels of J001 the mask marks black (0: none) */
static char g_masks[512];

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

static uint8_t *mask_fn(void *user, const char name[4], int level, int w, int h)
{
    (void)user;
    if (level != 0) return NULL;
    char path[640];
    snprintf(path, sizeof path, "%s/%.4s_mask.png", g_masks, name);
    size_t n;
    uint8_t *png = slurp(path, &n);
    if (png == NULL) return NULL;
    uint8_t *m = malloc((size_t)w * (size_t)h);
    if (m == NULL || !car_mask_parse(png, n, w, h, m)) free(m), m = NULL;
    free(png);
    if (m != NULL && g_black > 0 && memcmp(name, "J001", 4) == 0)
        for (int k = 0; k < g_black && 5 + k < h && 20 + k < w; k++) m[(size_t)(5 + k) * (size_t)w + (size_t)(20 + k)] = CAR_MASK_BLACK;
    return m;
}

/* the white-painted car's texel colours of the four textures, as RGB; the J001 texels the mask marks black are listed apart */
typedef struct {
    uint8_t rgb[TEXTURES][MAX_PIXELS][3];
    int width[TEXTURES], count[TEXTURES];
} Shots;

static int run(const uint8_t *orig, size_t size, int black, Shots *out)
{
    g_black = black;
    uint8_t *cfm = malloc(size);
    if (cfm == NULL) return 0;
    memcpy(cfm, orig, size);
    const uint8_t nero[3] = { 10, 10, 10 }, white[3] = { 244, 244, 240 };
    const CarLiveryPass pass = { "F512TR", 0, 0, mask_fn, NULL };
    car_livery_black(cfm, size, &pass, nero);
    car_livery_paint(cfm, size, &pass, white);
    static CarScene s;
    int ok = car_scene_open(&s, cfm, size);
    for (int t = 0; ok && t < TEXTURES; t++) {
        uint32_t idx;
        FshEntry e;
        ok = fsh_find(&s.bank, NAMES[t], &idx) == FSH_OK && fsh_entry(&s.bank, idx, &e) == FSH_OK && e.width * e.height <= MAX_PIXELS;
        if (!ok) break;
        out->width[t] = e.width, out->count[t] = e.width * e.height;
        for (int i = 0; i < out->count[t]; i++) memcpy(out->rgb[t][i], s.palette + 3 * e.payload[i], 3);
    }
    if (ok) car_scene_close(&s);
    free(cfm);
    return ok;
}

static int luma(const uint8_t c[3]) { return (c[0] * 3 + c[1] * 6 + c[2]) / 10; }

int main(void)
{
    const char *root = getenv("TNFS_DATA");
    char path[600], fallback[512];
    snprintf(fallback, sizeof fallback, "%s/original", TNFS_SOURCE_DIR);
    if (root == NULL) root = fallback;
    snprintf(path, sizeof path, "%s/SIMDATA/CARFAMS/F512TR.CFM", root);
    snprintf(g_masks, sizeof g_masks, "%s/data/car_masks/F512TR", TNFS_SOURCE_DIR);
    size_t size;
    uint8_t *orig = slurp(path, &size);
    if (orig == NULL) {
        fprintf(stderr, "no %s: skipped\n", path);
        return 0;
    }
    static Shots base, one, many;
    CHECK(run(orig, size, 0, &base));
    CHECK(run(orig, size, 1, &one));
    CHECK(run(orig, size, 40, &many));
    const int variants = 2;
    Shots *const with[2] = { &one, &many };
    const int marked[2] = { 1, 40 };
    for (int v = 0; v < variants; v++) {
        int changed = 0, dark = 0, texels = 0;
        for (int t = 0; t < TEXTURES; t++)
            for (int i = 0; i < base.count[t]; i++) {
                const int w = base.width[t];
                const int x = i % w, y = i / w, k = x - 20;
                const int is_black = t == TEXTURES - 1 && y == 5 + k && k >= 0 && k < marked[v];
                if (is_black) {
                    texels++;
                    dark += luma(with[v]->rgb[t][i]) < 60;
                } else if (memcmp(base.rgb[t][i], with[v]->rgb[t][i], 3) != 0) {
                    changed++;
                }
            }
        CHECK_EQ_INT(changed, 0);   /* no other texel changes colour */
        CHECK_EQ_INT(dark, texels);   /* the marked ones are dark */
        CHECK(texels == marked[v]);
    }
    free(orig);
    return TEST_REPORT();
}
