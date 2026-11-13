/* port: see dash_mask.h. */
#include "game/race/dash_mask.h"
#include "game/platform/platform.h"
#include "game/sys/data_path.h"
#include "game/sys/port_log.h"

#include "formats/png.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_SIDE = 4096, WHITE_FROM = 192, YELLOW_BLUE_MAX = 64 };

int dash_mask_class(const uint8_t c[3])
{
    const int top = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]);
    const int low = c[0] < c[1] ? (c[0] < c[2] ? c[0] : c[2]) : (c[1] < c[2] ? c[1] : c[2]);
    if (top < 64) return DASH_MASK_AUTO;                                       /* black */
    if (low >= WHITE_FROM) return DASH_MASK_LIT;                               /* white */
    if (c[0] >= WHITE_FROM && c[1] >= WHITE_FROM && c[2] <= YELLOW_BLUE_MAX) return DASH_MASK_LIT;   /* yellow: reads on the dash where white does not */
    if (c[0] >= WHITE_FROM && c[1] <= YELLOW_BLUE_MAX && c[2] <= YELLOW_BLUE_MAX) return DASH_MASK_BLADE;   /* red: a wiper blade */
    if (c[1] >= WHITE_FROM && c[2] >= WHITE_FROM && c[0] <= YELLOW_BLUE_MAX) return DASH_MASK_NOSE;          /* cyan: the nose through the windscreen */
    if (c[2] > c[0] && c[2] > c[1]) return DASH_MASK_DASH;                     /* blue */
    if (c[1] > c[0] && c[1] > c[2]) return DASH_MASK_DIAL;                     /* green */
    return DASH_MASK_AUTO;                                                     /* any other colour: not marked */
}

int dash_mask_parse(const uint8_t *png, size_t len, int width, int height, uint8_t *out)
{
    if (png == NULL || out == NULL || width <= 0 || height <= 0 || width > MAX_SIDE || height > MAX_SIDE) return 0;
    PngImage img;
    if (png_read(png, len, &img) != PNG_OK) return 0;
    for (int y = 0; y < height; y++) {
        const size_t sy = (size_t)y * img.height / (size_t)height;
        for (int x = 0; x < width; x++) {
            const size_t sx = (size_t)x * img.width / (size_t)width, at = sy * img.width + sx;
            const uint8_t *c = img.indexed ? img.palette + 3 * img.pixels[at] : img.pixels + 4 * at;
            const int transparent = !img.indexed && img.pixels[4 * at + 3] < 128;   /* an unpainted pixel of a layer export */
            out[(size_t)y * (size_t)width + (size_t)x] = transparent ? DASH_MASK_AUTO : (uint8_t)dash_mask_class(c);
        }
    }
    png_free(img.pixels);
    return 1;
}

int dash_mask_write(const uint8_t *mask, int width, int height, uint8_t **out, size_t *len)
{
    if (mask == NULL || width <= 0 || height <= 0 || width > MAX_SIDE || height > MAX_SIDE) return 0;
    const size_t count = (size_t)width * (size_t)height;
    uint8_t *indices = malloc(count);
    if (indices == NULL) return 0;
    for (size_t i = 0; i < count; i++)
        indices[i] = mask[i] == DASH_MASK_DASH ? 1 : mask[i] == DASH_MASK_DIAL ? 2 : mask[i] == DASH_MASK_LIT ? 3 : mask[i] == DASH_MASK_BLADE ? 4 : mask[i] == DASH_MASK_NOSE ? 5 : 0;
    uint8_t palette[768] = { 0 };
    palette[5] = 255;                                  /* 1 blue */
    palette[7] = 255;                                  /* 2 green */
    palette[9] = palette[10] = palette[11] = 255;      /* 3 white */
    palette[12] = 255;                                 /* 4 red */
    palette[16] = palette[17] = 255;                   /* 5 cyan */
    const PngResult r = png_write_indexed(indices, (uint32_t)width, (uint32_t)height, palette, out, len);
    free(indices);
    return r == PNG_OK;
}

uint8_t *dash_asset_read(const char *stem, const char *name, size_t *size)
{
    const char *env = getenv("TNFS_DASH_MASKS");
    char file_name[256], found[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    snprintf(file_name, sizeof file_name, "dash_masks/%s/%s", stem, name);
    const int dev = env != NULL && env[0] != '\0';   /* a dev override first: TNFS_DASH_MASKS=<dir of <CAR>/dash_mask.png> */
    (void)data_path_candidates(file_name, found);
    for (int r = 0; r < 1 + DATA_PATH_CANDIDATES; r++) {
        char path[DATA_PATH_MAX];
        if (r == 0) {
            if (!dev) continue;
            snprintf(path, sizeof path, "%s/%s/%s", env, stem, name);
        } else {
            if (found[r - 1][0] == '\0') continue;
            snprintf(path, sizeof path, "%s", found[r - 1]);
        }
        if (r > 0 && !data_path_exists(path)) continue;   /* optional: a data folder without the file is passed over, no "cannot read" line (i191); the dev override is read as named */
        uint8_t *file = platform_read_file(path, size);
        if (file != NULL) {
            if (r <= 1) port_log("dash %s: %s\n", name, path);   /* a dev override's or a mod's: what a bug report needs */
            else port_log_detail("dash %s: %s\n", name, path);   /* the port's own data: a detail */
            return file;
        }
    }
    return NULL;
}

static uint8_t *load_stem(const char *stem)
{
    size_t size = 0;
    uint8_t *file = dash_asset_read(stem, "dash_mask.png", &size);
    if (file == NULL) return NULL;
    uint8_t *mask = malloc((size_t)DASH_MASK_W * DASH_MASK_H);
    const int ok = mask != NULL && dash_mask_parse(file, size, DASH_MASK_W, DASH_MASK_H, mask);
    platform_free_file(file);
    if (ok) return mask;
    free(mask);
    return NULL;
}

uint8_t *dash_mask_load(const char *stem)
{
    if (stem == NULL || stem[0] == '\0') return NULL;
    uint8_t *mask = load_stem(stem);
    if (mask == NULL) {
        char upper[64];
        size_t i = 0;
        for (; stem[i] != '\0' && i < sizeof upper - 1; i++) upper[i] = (char)toupper((unsigned char)stem[i]);
        upper[i] = '\0';
        if (strcmp(upper, stem) != 0) mask = load_stem(upper);
    }
    if (mask == NULL) port_log_detail("dash mask: none for %s\n", stem);
    return mask;
}
