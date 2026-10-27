#include "game/race/car_mask.h"

#include "formats/png.h"
#include "game/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_SIDE = 4096, PATH = 512, MATTE_FROM = 192 };

/* the class of a mask colour */
static int class_of(const uint8_t c[3])
{
    if (c[0] == 0 && c[1] == 0 && c[2] == 0) return CAR_MASK_PAINT;
    if (c[2] > c[0] && c[2] > c[1]) return CAR_MASK_SHINE;
    const int top = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]);
    return top >= MATTE_FROM ? CAR_MASK_MATTE : CAR_MASK_SHINE;
}

int car_mask_parse(const uint8_t *png, size_t len, int width, int height, uint8_t *out)
{
    if (png == NULL || out == NULL || width <= 0 || height <= 0 || width > MAX_SIDE || height > MAX_SIDE) return 0;
    PngImage img;
    if (png_read(png, len, &img) != PNG_OK) return 0;
    for (int y = 0; y < height; y++) {
        const size_t sy = (size_t)y * img.height / (size_t)height;
        for (int x = 0; x < width; x++) {
            const size_t sx = (size_t)x * img.width / (size_t)width, at = sy * img.width + sx;
            const uint8_t *c = img.indexed ? img.palette + 3 * img.pixels[at] : img.pixels + 4 * at;
            out[(size_t)y * (size_t)width + (size_t)x] = (uint8_t)class_of(c);
        }
    }
    png_free(img.pixels);
    return 1;
}

int car_mask_write(const uint8_t *mask, int width, int height, uint8_t **out, size_t *len)
{
    if (mask == NULL || width <= 0 || height <= 0 || width > MAX_SIDE || height > MAX_SIDE) return 0;
    const size_t count = (size_t)width * (size_t)height;
    uint8_t *indices = malloc(count);
    if (indices == NULL) return 0;
    for (size_t i = 0; i < count; i++) indices[i] = mask[i] == 0 ? 0 : mask[i] == CAR_MASK_SHINE ? 1 : 2;
    uint8_t palette[768] = { 0 };
    palette[3] = palette[4] = palette[5] = CAR_MASK_SHINE;
    palette[6] = palette[7] = palette[8] = CAR_MASK_MATTE;
    const PngResult r = png_write_indexed(indices, (uint32_t)width, (uint32_t)height, palette, out, len);
    free(indices);
    return r == PNG_OK;
}

uint8_t *car_mask_load(const char *stem, const char name[4], int level, int width, int height)
{
    if (stem == NULL || width <= 0 || height <= 0) return NULL;
    const char *const suffix = level != 0 ? "_lo" : "";   /* the low detail bank's files are <tex>_lo_mask.png */
    const char *env = getenv("TNFS_CAR_MASKS");
    const char *roots[3] = { env, "data/car_masks",
#ifdef TNFS_SOURCE_DIR
                             TNFS_SOURCE_DIR "/data/car_masks"
#else
                             NULL
#endif
    };
    for (int r = 0; r < 3; r++) {
        if (roots[r] == NULL || roots[r][0] == '\0') continue;
        char path[PATH];
        snprintf(path, sizeof path, "%s/%s/%.4s%s_mask.png", roots[r], stem, name, suffix);
        size_t size = 0;
        uint8_t *file = platform_read_file(path, &size);
        if (file == NULL) continue;
        uint8_t *mask = malloc((size_t)width * (size_t)height);
        const int ok = mask != NULL && car_mask_parse(file, size, width, height, mask);
        platform_free_file(file);
        if (ok) return mask;
        free(mask);
    }
    return NULL;
}
