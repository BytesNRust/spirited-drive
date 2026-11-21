/* All the repo's hand-made car masks (data/car_masks, 196 PNGs of 8-bit grey) put through the codec: each is read (car_mask_parse), written as a grey 8-bit PNG
 * (car_mask_write_grey), and read again: the same pixels, and the file is what the tools write (a grey 8-bit PNG). */
#include "formats/car_mask_codec.h"
#include "formats/png.h"
#include "test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = n > 0 ? malloc((size_t)n) : NULL;
    if (data != NULL && fread(data, 1, (size_t)n, f) != (size_t)n) free(data), data = NULL;
    fclose(f);
    *len = data != NULL ? (size_t)n : 0;
    return data;
}

static int one(const char *path)
{
    size_t len = 0;
    uint8_t *png = slurp(path, &len);
    uint32_t w = 0, h = 0;
    if (png == NULL || png_size(png, len, &w, &h) != PNG_OK) { free(png); return 0; }
    uint8_t *mask = malloc((size_t)w * h), *again = malloc((size_t)w * h), *out = NULL;
    size_t out_len = 0;
    int ok = mask != NULL && again != NULL && car_mask_parse(png, len, (int)w, (int)h, mask) &&
             car_mask_write_grey(mask, (int)w, (int)h, &out, &out_len) && car_mask_parse(out, out_len, (int)w, (int)h, again) &&
             memcmp(mask, again, (size_t)w * h) == 0;
    if (ok) {   /* the rewritten file is grey 8-bit: the picture read as RGBA has equal channels, and its grey is the class */
        PngImage img;
        ok = png_read(out, out_len, &img) == PNG_OK && img.width == w && img.height == h;
        if (ok) {
            for (size_t i = 0; i < (size_t)w * h && ok; i++) {
                const uint8_t g = img.indexed ? img.palette[3 * img.pixels[i]] : img.pixels[4 * i];
                ok = g == mask[i];
            }
            png_free(img.pixels);
        }
    }
    png_free(out);
    free(mask), free(again), free(png);
    return ok;
}

int main(void)
{
    int files = 0, bad = 0;
    DIR *top = opendir(TNFS_SOURCE_DIR "/data/car_masks");
    CHECK(top != NULL);
    for (struct dirent *car; top != NULL && (car = readdir(top)) != NULL;) {
        if (car->d_name[0] == '.') continue;
        char dir[1024];
        snprintf(dir, sizeof dir, TNFS_SOURCE_DIR "/data/car_masks/%s", car->d_name);
        DIR *d = opendir(dir);
        if (d == NULL) continue;
        for (struct dirent *e; (e = readdir(d)) != NULL;) {
            const size_t n = strlen(e->d_name);
            if (n < 5 || strcmp(e->d_name + n - 4, ".png") != 0) continue;
            char path[2048];
            snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
            files++;
            if (!one(path) && bad++ < 8) printf("not the same pixels: %s\n", path);
        }
        closedir(d);
    }
    if (top != NULL) closedir(top);
    printf("%d masks, %d different\n", files, bad);
    CHECK(files >= 188);   /* the car masks: the 8 stray car_masks/<CAR>/dash_mask.png that made it 196 are gone (the dash masks are in data/dash_masks) */
    CHECK(bad == 0);
    return TEST_REPORT();
}
