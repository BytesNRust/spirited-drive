/* port: the configuration screens' clean background (config_bgnd.h). */
#include "game/frontend/config_bgnd.h"

#include "formats/fsh.h"

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
    uint8_t *clean = malloc(3 * SIZE);
    if (clean == NULL) {
        memset(pixels, 0, SIZE);
        return;
    }
    for (size_t i = 0; i < SIZE; i++) memcpy(clean + 3 * i, medoid(rgb, 3 * i), 3);
    for (size_t b = 0; b < sizeof BANDS / sizeof BANDS[0]; b++) take(clean, rgb, BANDS[b]);
    for (size_t b = 0; !done_cancel && b < sizeof CORNERS / sizeof CORNERS[0]; b++) take(clean, rgb, CORNERS[b]);
    cover_title(clean);
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

int config_bgnd_load(ConfigBgndRead read, int palette_screen, int done_cancel, uint8_t pixels[CONFIG_BGND_W * CONFIG_BGND_H], uint8_t palette[768])
{
    uint8_t *rgb[CONFIG_BGND_SCREENS] = { NULL };
    uint8_t own[768];
    int ok = palette_screen >= 0 && palette_screen < CONFIG_BGND_SCREENS;
    for (int s = 0; s < CONFIG_BGND_SCREENS && ok; s++)
        ok = (rgb[s] = malloc(3 * SIZE)) != NULL && screen_rgb(read, SCREEN_NAMES[s], rgb[s], s == palette_screen ? palette : own);
    if (ok) config_bgnd_compose((const uint8_t *const *)rgb, palette, done_cancel, pixels);
    for (int s = 0; s < CONFIG_BGND_SCREENS; s++) free(rgb[s]);
    return ok;
}
