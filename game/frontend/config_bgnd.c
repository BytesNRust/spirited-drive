/* port: the configuration screens' clean background (config_bgnd.h). */
#include "game/frontend/config_bgnd.h"

#include "formats/fsh.h"
#include "game/sys/art_cache.h"

#include <stdio.h>

#include <stdlib.h>
#include <string.h>

enum { AUDIO = 1, GAMEMODE = CONFIG_BGND_GAME_MODE, SIZE = CONFIG_BGND_W * CONFIG_BGND_H };

static const char *const SCREEN_NAMES[CONFIG_BGND_SCREENS] = {
    "option/options.qfs", "option/audio.qfs",    "option/graphics.qfs", "option/cntrlset.qfs",
    "option/loadsave.qfs", "option/modconf.qfs", "option/modemset.qfs", "option/netset.qfs",
    "control/gamemode.qfs", "control/gamemod2.qfs",
};

/* bands where most screens carry text, and the screen that has none there: x0, y0, x1, y1 (exclusive), screen */
static const int BANDS[][5] = {
    { 110, 58, 640, 95, AUDIO },     /* the titles' row right of the logo */
    { 0, 100, 290, 135, GAMEMODE },  /* the first label row */
};

/* DONE and CANCEL, from Game Mode which has neither */
static const int CORNERS[][5] = {
    { 0, 430, 140, 475, GAMEMODE },
    { 500, 430, 640, 475, GAMEMODE },
};

/* the title's remnant no screen is free of, covered by the texture PATCH_SHIFT to its right with feathered edges */
enum { PATCH_X0 = 0, PATCH_Y0 = 60, PATCH_X1 = 124, PATCH_Y1 = 94, PATCH_SHIFT = 130, FEATHER_X = 14, FEATHER_Y = 5 };

static int l1(const uint8_t *a, const uint8_t *b)
{
    return abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2]);
}

/* the sample closest to all the others (the first of equals) */
static const uint8_t *medoid(const uint8_t *const rgb[CONFIG_BGND_SCREENS], size_t at)
{
    const uint8_t *best = NULL;
    int best_d = 0;
    for (int i = 0; i < CONFIG_BGND_SCREENS; i++) {
        int d = 0;
        for (int j = 0; j < CONFIG_BGND_SCREENS; j++) d += l1(rgb[i] + at, rgb[j] + at);
        if (best == NULL || d < best_d) best = rgb[i] + at, best_d = d;
    }
    return best;
}

static int feather(int x, int y)
{
    const int w = PATCH_X1 - PATCH_X0, h = PATCH_Y1 - PATCH_Y0;
    int a = 255;
    if (x > w - FEATHER_X) a = 255 * (w - x) / FEATHER_X;
    if (y < FEATHER_Y - 1 && 255 * (y + 1) / FEATHER_Y < a) a = 255 * (y + 1) / FEATHER_Y;
    if (y > h - FEATHER_Y && 255 * (h - y) / FEATHER_Y < a) a = 255 * (h - y) / FEATHER_Y;
    return a;
}

static void cover_title(uint8_t *rgb)
{
    for (int y = PATCH_Y0; y < PATCH_Y1; y++)
        for (int x = PATCH_X0; x < PATCH_X1; x++) {
            const int a = feather(x - PATCH_X0, y - PATCH_Y0);
            uint8_t *d = rgb + 3 * (y * CONFIG_BGND_W + x);
            const uint8_t *s = rgb + 3 * (y * CONFIG_BGND_W + x + PATCH_SHIFT);
            for (int c = 0; c < 3; c++) d[c] = (uint8_t)((d[c] * (255 - a) + s[c] * a + 127) / 255);
        }
}

/* port: where every screen had a label the medoid can keep a pixel or a dash of the letters, light on the darker
 * texture. In the labels' column (they all start at the left edge; the "&" art to the right has thin light lines a
 * second pass would wear down), between the title row and DONE/CANCEL, a light pixel (luma SPECK_LIGHT and up)
 * brighter than the median of its 3x3 neighbours by more than SPECK_RISE is a speck: the art there never stands out
 * so much (at most 51 on the CD, the remnants 57 and up), so the texture is left as it is; the floor keeps dark
 * colours out, the dimmest remnant is 107. A speck takes the median colour of the pixels around it (5x5) that are not
 * specks, so a dash is not filled from itself; a second pass takes what the first left of one. On the CD 14 pixels
 * change, all of them remnants. */
enum {
    SPECK_X0 = 2, SPECK_X1 = 96, SPECK_Y0 = 95, SPECK_Y1 = 430,
    SPECK_LIGHT = 100, SPECK_RISE = 56, SPECK_PASSES = 2, SPECK_FILL = 2
};

static int luma(const uint8_t *c)
{
    return (299 * c[0] + 587 * c[1] + 114 * c[2]) / 1000;
}

/* adds c to n[0..k) kept in luma order (after its equals); returns the new count */
static int sorted_add(const uint8_t *n[], int k, const uint8_t *c)
{
    int i = k;
    for (; i > 0 && luma(n[i - 1]) > luma(c); i--) n[i] = n[i - 1];
    n[i] = c;
    return k + 1;
}

static int is_speck(const uint8_t *rgb, int x, int y)
{
    const uint8_t *n[8];
    int k = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx != 0 || dy != 0) k = sorted_add(n, k, rgb + 3 * ((y + dy) * CONFIG_BGND_W + x + dx));
    const int l = luma(rgb + 3 * (y * CONFIG_BGND_W + x));
    return l >= SPECK_LIGHT && l - luma(n[3]) > SPECK_RISE;
}

/* the lower median of the pixels around (x, y) that are not specks; NULL when all are */
static const uint8_t *fill(const uint8_t *rgb, const uint8_t *speck, int x, int y)
{
    const uint8_t *n[(2 * SPECK_FILL + 1) * (2 * SPECK_FILL + 1)];
    int k = 0;
    for (int dy = -SPECK_FILL; dy <= SPECK_FILL; dy++)
        for (int dx = -SPECK_FILL; dx <= SPECK_FILL; dx++) {
            const size_t at = (size_t)(y + dy) * CONFIG_BGND_W + x + dx;
            if (!speck[at]) k = sorted_add(n, k, rgb + 3 * at);
        }
    return k > 0 ? n[(k - 1) / 2] : NULL;
}

static void despeckle(uint8_t *rgb, uint8_t *speck)
{
    for (int pass = 0; pass < SPECK_PASSES; pass++) {
        memset(speck, 0, SIZE);
        for (int y = SPECK_Y0; y < SPECK_Y1; y++)
            for (int x = SPECK_X0; x < SPECK_X1; x++) speck[y * CONFIG_BGND_W + x] = (uint8_t)is_speck(rgb, x, y);
        /* the fills read only pixels that are not specks, which no fill changes: the order does not matter */
        for (int y = SPECK_Y0; y < SPECK_Y1; y++)
            for (int x = SPECK_X0; x < SPECK_X1; x++) {
                const uint8_t *m = speck[y * CONFIG_BGND_W + x] ? fill(rgb, speck, x, y) : NULL;
                if (m != NULL) memcpy(rgb + 3 * (y * CONFIG_BGND_W + x), m, 3);
            }
    }
}

static uint8_t nearest(const uint8_t palette[768], const uint8_t *c)
{
    int best = 0, best_d = -1;
    for (int i = 0; i < 256; i++) {
        int d = 0;
        for (int k = 0; k < 3; k++) d += (palette[3 * i + k] - c[k]) * (palette[3 * i + k] - c[k]);
        if (best_d < 0 || d < best_d) best = i, best_d = d;
    }
    return (uint8_t)best;
}

static void take(uint8_t *clean, const uint8_t *const rgb[CONFIG_BGND_SCREENS], const int band[5])
{
    for (int y = band[1]; y < band[3]; y++) {
        const size_t at = 3 * ((size_t)y * CONFIG_BGND_W + band[0]);
        memcpy(clean + at, rgb[band[4]] + at, 3 * (size_t)(band[2] - band[0]));
    }
}

void config_bgnd_compose(const uint8_t *const rgb[CONFIG_BGND_SCREENS], const uint8_t palette[768], int done_cancel,
                         uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H])
{
    uint8_t *clean = malloc(3 * SIZE), *speck = malloc(SIZE);
    if (clean == NULL || speck == NULL) {
        free(clean), free(speck);
        memset(pixels, 0, SIZE);
        return;
    }
    for (size_t i = 0; i < SIZE; i++) memcpy(clean + 3 * i, medoid(rgb, 3 * i), 3);
    for (size_t b = 0; b < sizeof BANDS / sizeof BANDS[0]; b++) take(clean, rgb, BANDS[b]);
    for (size_t b = 0; !done_cancel && b < sizeof CORNERS / sizeof CORNERS[0]; b++) take(clean, rgb, CORNERS[b]);
    cover_title(clean);
    despeckle(clean, speck);
    free(speck);
    /* the colours repeat a lot: remember the last match */
    uint8_t last[3] = { 0 };
    uint8_t last_index = nearest(palette, last);
    for (size_t i = 0; i < SIZE; i++) {
        if (memcmp(clean + 3 * i, last, 3) != 0) memcpy(last, clean + 3 * i, 3), last_index = nearest(palette, last);
        pixels[i] = last_index;
    }
    free(clean);
}

/* a screen's "bgnd" as RGB through its own palette; 0 when it is not there or not 640x480 */
static int screen_rgb(ConfigBgndRead read, const char *name, uint8_t *rgb, uint8_t palette[768])
{
    static const char BGND[4] = { 'b', 'g', 'n', 'd' }, PAL[4] = { '!', 'p', 'a', 'l' };
    size_t size;
    uint8_t *data = read(name, &size);
    if (data == NULL) return 0;
    Fsh f;
    FshEntry bg, pal;
    uint32_t j, k;
    const int ok = fsh_open(&f, data, size) == FSH_OK && fsh_find(&f, BGND, &j) == FSH_OK &&
                   fsh_entry(&f, j, &bg) == FSH_OK && bg.type == FSH_TYPE_BITMAP && bg.width == CONFIG_BGND_W &&
                   bg.height == CONFIG_BGND_H && bg.payload_size >= SIZE && fsh_find(&f, PAL, &k) == FSH_OK &&
                   fsh_entry(&f, k, &pal) == FSH_OK && fsh_palette_rgb(&pal, palette) == FSH_OK;
    if (ok)
        for (size_t i = 0; i < SIZE; i++) memcpy(rgb + 3 * i, palette + 3 * bg.payload[i], 3);
    free(data);
    return ok;
}

void config_bgnd_key(ConfigBgndRead read, ArtCacheKey *key)
{
    for (int s = 0; s < CONFIG_BGND_SCREENS; s++) {
        size_t size = 0;
        uint8_t *data = read(SCREEN_NAMES[s], &size);
        art_cache_key_add(key, data, size);
        free(data);
    }
}

int config_bgnd_load(ConfigBgndRead read, int palette_screen, int done_cancel, uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H], uint8_t palette[768])
{
    if (palette_screen < 0 || palette_screen >= CONFIG_BGND_SCREENS) return 0;
    /* made once from the CD, kept in the player's cache (art_cache.h) */
    ArtCacheKey key;
    char name[48];
    uint8_t *blob = malloc(SIZE + 768);
    art_cache_key_init(&key, "config_bgnd", CONFIG_BGND_ART_VERSION);
    const uint8_t how[2] = { (uint8_t)palette_screen, (uint8_t)(done_cancel != 0) };
    art_cache_key_add(&key, how, sizeof how);
    config_bgnd_key(read, &key);
    snprintf(name, sizeof name, "config_bgnd_%d_%d.bin", palette_screen, done_cancel != 0);
    if (blob != NULL && art_cache_read(name, &key, blob, SIZE + 768)) {
        memcpy(pixels, blob, SIZE), memcpy(palette, blob + SIZE, 768);
        free(blob);
        return 1;
    }
    uint8_t *rgb[CONFIG_BGND_SCREENS] = { NULL };
    uint8_t own[768];
    int ok = 1;
    for (int s = 0; s < CONFIG_BGND_SCREENS && ok; s++)
        ok = (rgb[s] = malloc(3 * SIZE)) != NULL && screen_rgb(read, SCREEN_NAMES[s], rgb[s], s == palette_screen ? palette : own);
    if (ok) config_bgnd_compose((const uint8_t *const *)rgb, palette, done_cancel, pixels);
    for (int s = 0; s < CONFIG_BGND_SCREENS; s++) free(rgb[s]);
    if (ok && blob != NULL) {
        memcpy(blob, pixels, SIZE), memcpy(blob + SIZE, palette, 768);
        (void)art_cache_write(name, &key, blob, SIZE + 768);
    }
    free(blob);
    return ok;
}

size_t config_bgnd_shape(const uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H], int x, int y, int w, int h, uint8_t *out)
{
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > CONFIG_BGND_W || y + h > CONFIG_BGND_H) return 0;
    const size_t size = CONFIG_BGND_SHAPE_HEADER + (size_t)w * h;
    const int16_t fields[6] = { (int16_t)w, (int16_t)h, 0, 0, (int16_t)x, (int16_t)y };
    memset(out, 0, CONFIG_BGND_SHAPE_HEADER);
    out[0] = FSH_TYPE_BITMAP;
    out[1] = (uint8_t)size, out[2] = (uint8_t)(size >> 8), out[3] = (uint8_t)(size >> 16);
    for (int i = 0; i < 6; i++) out[4 + 2 * i] = (uint8_t)fields[i], out[5 + 2 * i] = (uint8_t)((uint16_t)fields[i] >> 8);
    for (int row = 0; row < h; row++)
        memcpy(out + CONFIG_BGND_SHAPE_HEADER + (size_t)row * w, pixels + (size_t)(y + row) * CONFIG_BGND_W + x, (size_t)w);
    return size;
}
