/* The drafts of the hand-made car masks: for each of the eight cars' body textures (high detail bank), what today's repaint rules leave
 * as it is, as the mask's PNG (car_mask.h: black paint, grey 128 shine, white matte) and the texture itself in the car's own colours as an RGBA PNG.
 * usage: car_mask_draft <data dir with SIMDATA/CARFAMS> <output dir>
 * writes <output>/<CAR>/<tex>.keep.png and <tex>.rgb.png. The RGB files hold the game's pixels: keep them out of the repository. */
#include "formats/png.h"
#include "game/race/car_mask.h"
#include "game/race/car_paint.h"
#include "game/race/car_repaint.h"
#include "game/race/car_load.h"
#include "game/viewer/drive_livery.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* the drafts come from the rules alone: no mask is read */
uint8_t *platform_read_file(const char *path, size_t *size)
{
    (void)path;
    *size = 0;
    return NULL;
}
void platform_free_file(uint8_t *data) { free(data); }

typedef struct {
    char dir[600];
    int written;
} Out;

static int write_file(const char *path, const uint8_t *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    const int ok = fwrite(data, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}

static void draft(void *user, int level, const char name[4], int width, int height, const uint8_t *pixels, const uint8_t *keep,
                  const uint8_t rgb[768])
{
    Out *o = user;
    if (level != 0) return;
    const size_t count = (size_t)width * (size_t)height;
    uint8_t *rgba = malloc(count * 4);
    if (rgba == NULL) return;
    for (size_t i = 0; i < count; i++) {
        memcpy(rgba + 4 * i, rgb + 3 * pixels[i], 3);
        rgba[4 * i + 3] = 255;
    }
    uint8_t *classes = malloc(count), *png = NULL, *mask = NULL;
    for (size_t i = 0; classes != NULL && i < count; i++)
        classes[i] = keep[i] == REPAINT_DRAFT_PAINT ? CAR_MASK_PAINT : keep[i] == REPAINT_DRAFT_SHINE ? CAR_MASK_SHINE : CAR_MASK_MATTE;
    size_t len = 0, mask_len = 0;
    char path[700];
    if (png_write_rgba(rgba, (uint32_t)width, (uint32_t)height, &png, &len) == PNG_OK) {
        snprintf(path, sizeof path, "%s/%.4s.rgb.png", o->dir, name);
        o->written += write_file(path, png, len);
    }
    if (classes != NULL && car_mask_write(classes, width, height, &mask, &mask_len)) {
        snprintf(path, sizeof path, "%s/%.4s.keep.png", o->dir, name);
        o->written += write_file(path, mask, mask_len);
    }
    png_free(png);
    png_free(mask);
    free(classes);
    free(rgba);
}

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = d != NULL ? (size_t)n : 0;
    return d;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <data dir> <output dir>\n", argv[0]);
        return 2;
    }
    int failed = 0;
    for (int model = 0; model < CAR_PAINT_MODELS; model++) {
        const char *stem = car_model_names[model];
        char path[700];
        snprintf(path, sizeof path, "%s/SIMDATA/CARFAMS/%s.CFM", argv[1], stem);
        size_t size = 0;
        uint8_t *cfm = slurp(path, &size);
        if (cfm == NULL) {
            fprintf(stderr, "no %s\n", path);
            failed = 1;
            continue;
        }
        Out o = { { 0 }, 0 };
        snprintf(o.dir, sizeof o.dir, "%s/%s", argv[2], stem);
        mkdir(argv[2], 0755);
        mkdir(o.dir, 0755);
        const int paint = drive_livery_draft(stem, cfm, size, draft, &o);
        printf("%s: %d texels to paint, %d files in %s\n", stem, paint, o.written, o.dir);
        free(cfm);
    }
    return failed;
}
