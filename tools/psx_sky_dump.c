/* psx_sky_dump <psx_dir> <track> [A|B|C ...]: what the PS1 sky reader (game/race/psx_sky.h) takes from
 * <psx_dir>/Z<TRACK><letter>.INF and .PSH: the gradient and ring fields, the SHPP entry count, the hrz0..hrz7 and rhrz sizes, a
 * checksum of the decoded RGB panorama and the made bitmap. Read only; prints, writes nothing. */
#include "formats/psx_bigf.h"
#include "game/race/psx_sky.h"
#include "game/race/psx_sky_bitmap.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = n > 0 ? malloc((size_t)n) : NULL;
    if (data != NULL && fread(data, 1, (size_t)n, f) != (size_t)n) free(data), data = NULL;
    fclose(f);
    *size = data != NULL ? (size_t)n : 0;
    return data;
}

static void dump_inf(const char *path)
{
    size_t size;
    uint8_t *file = slurp(path, &size);
    PsxSkyInf inf;
    if (file == NULL) {
        printf("  %s: not found\n", path);
        return;
    }
    if (!psx_sky_inf_parse(file, size, &inf)) {
        printf("  %s: does not read (%zu bytes)\n", path, size);
    } else {
        printf("  %s: %d of 14 values\n", path, inf.fields);
        printf("    geometry:");
        for (int i = 0; i < PSX_SKY_GEOMETRY; i++) printf(" %g", inf.geometry[i]);
        printf("\n    top %d %d %d  bottom %d %d %d\n", inf.top[0], inf.top[1], inf.top[2], inf.bottom[0], inf.bottom[1], inf.bottom[2]);
        printf("    dither %d  blend %d  ring y %d  ring height %d\n", inf.dither, inf.blend, inf.ring_y, inf.ring_height);
    }
    free(file);
}

static void dump_psh(const char *path, const PsxSkyInf *inf)
{
    size_t size;
    uint8_t *file = slurp(path, &size);
    if (file == NULL) {
        printf("  %s: not found\n", path);
        return;
    }
    printf("  %s: %zu bytes, %u entries\n", path, size, psx_shpp_count(file, size));
    static const char *names[] = { "hrz0", "hrz1", "hrz2", "hrz3", "hrz4", "hrz5", "hrz6", "hrz7", "rhrz" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        PsxImage8 img;
        if (psx_shpp_image8(file, size, names[i], &img)) printf("    %s %dx%d 8bpp\n", names[i], img.width, img.height);
        else printf("    %s: not an 8bpp image\n", names[i]);
    }
    PsxSky sky;
    if (psx_sky_panorama(file, size, &sky)) {
        printf("    panorama %dx%d, rgb checksum %08x\n", sky.width, sky.height,
               psx_sky_checksum(sky.rgb, (size_t)sky.width * (size_t)sky.height * 3));
        const char *ppm = getenv("PSX_SKY_PPM");   /* PSX_SKY_PPM=<dir>: the panorama as <dir>/<track><letter>.ppm */
        if (ppm != NULL) {
            char out[512];
            snprintf(out, sizeof out, "%s/%s", ppm, strrchr(path, '/') != NULL ? strrchr(path, '/') + 1 : path);
            memcpy(out + strlen(out) - 3, "ppm", 3);
            FILE *f = fopen(out, "wb");
            if (f != NULL) {
                fprintf(f, "P6\n%d %d\n255\n", sky.width, sky.height);
                fwrite(sky.rgb, 3, (size_t)sky.width * (size_t)sky.height, f);
                fclose(f);
            }
        }
        PsxSkyBitmap bitmap;
        if (psx_sky_bitmap_build(&sky, inf, &bitmap)) {
            printf("    bitmap %dx%d, horizon row %d, index checksum %08x\n", bitmap.width, bitmap.height, bitmap.horizon_row,
                   psx_sky_checksum(bitmap.pixels, (size_t)bitmap.width * (size_t)bitmap.height));
            psx_sky_bitmap_free(&bitmap);
        }
        psx_sky_free(&sky);
    } else {
        printf("    no panorama (hrz0..hrz7 are not all 8bpp images of one size)\n");
    }
    free(file);
}

int main(int argc, char **argv)
{
    if (argc < 3) return fprintf(stderr, "usage: psx_sky_dump <psx_dir> <track> [A|B|C ...]\n"), 2;
    char upper[16] = "";
    for (size_t i = 0; argv[2][i] != '\0' && i < sizeof upper - 1; i++) upper[i] = (char)toupper((unsigned char)argv[2][i]);
    const char *letters[4] = { "A", "B", "C", NULL };
    if (argc > 3) {
        for (int i = 3; i < argc && i < 6; i++) letters[i - 3] = argv[i];
        for (int i = argc - 3; i < 4; i++) letters[i] = NULL;
    }
    for (int i = 0; letters[i] != NULL; i++) {
        char path[512];
        printf("Z%s%s\n", upper, letters[i]);
        snprintf(path, sizeof path, "%s/Z%s%s.INF", argv[1], upper, letters[i]);
        dump_inf(path);
        PsxSkyInf inf;
        size_t size;
        uint8_t *file = slurp(path, &size);
        if (file == NULL || !psx_sky_inf_parse(file, size, &inf)) memset(&inf, 0, sizeof inf);
        free(file);
        snprintf(path, sizeof path, "%s/Z%s%s.PSH", argv[1], upper, letters[i]);
        dump_psh(path, &inf);
    }
    return 0;
}
