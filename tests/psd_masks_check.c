/* usage: psd_masks_check <psd root (<CAR>/<tex>.psd, <CAR>/dash.psd)> <data dir (car_masks, dash_masks)>
 * Reads every PSD with the C reader and compares the classes with the PNGs the Python scripts made (data/car_masks/<CAR>/<tex>_mask.png,
 * data/dash_masks/<CAR upper>/dash_mask.png and dash_color.png), pixel by pixel. Lists every difference; exit 0 only when there is none. Read only. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats/png.h"
#include "formats/psd_mask.h"

static int g_psds, g_diffs;

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    rewind(f);
    uint8_t *b = malloc((size_t)n + 1);
    *len = b != NULL && fread(b, 1, (size_t)n, f) == (size_t)n ? (size_t)n : 0;
    fclose(f);
    if (*len != (size_t)n) free(b), b = NULL;
    return b;
}

static int dash_class_of(const uint8_t rgb[3])   /* the palette colours of dash_mask.png */
{
    const int r = rgb[0] > 127, g = rgb[1] > 127, b = rgb[2] > 127;
    if (r && g && b) return 255;
    if (r && !g && !b) return 192;
    if (!r && g && b) return 160;
    if (!r && g && !b) return 128;
    if (!r && !g && b) return 64;
    return 0;
}

/* the PNG's classes (car: the grey; dash: the palette's class), or NULL */
static uint8_t *png_classes(const char *path, int dash, uint32_t *w, uint32_t *h)
{
    size_t len;
    uint8_t *bytes = slurp(path, &len);
    if (bytes == NULL) return NULL;
    PngImage img;
    const PngResult r = png_read(bytes, len, &img);
    free(bytes);
    if (r != PNG_OK) return NULL;
    uint8_t *out = malloc((size_t)img.width * img.height);
    for (size_t i = 0; out != NULL && i < (size_t)img.width * img.height; ++i)
        out[i] = img.indexed ? (uint8_t)(dash ? dash_class_of(img.palette + img.pixels[i] * 3) : img.palette[img.pixels[i] * 3])
                             : (uint8_t)(dash ? dash_class_of(img.pixels + i * 4) : img.pixels[i * 4]);   /* a 2 or 4-bit palette PNG comes back as RGBA */
    *w = img.width, *h = img.height;
    png_free(img.pixels);
    return out;
}

static void compare(const char *label, const uint8_t *got, uint32_t gw, uint32_t gh, const uint8_t *want, uint32_t ww, uint32_t wh, size_t unit)
{
    if (gw != ww || gh != wh) { ++g_diffs; printf("DIFF %s: size %ux%u, the PNG %ux%u\n", label, gw, gh, ww, wh); return; }
    size_t bad = 0, first = 0;
    if (getenv("PSD_CHECK_DETAIL") != NULL) {
        static size_t pair[256][256];
        unsigned x0 = gw, y0 = gh, x1 = 0, y1 = 0;
        memset(pair, 0, sizeof pair);
        for (size_t i = 0; i < (size_t)gw * gh && unit == 1; ++i)
            if (got[i] != want[i]) {
                ++pair[got[i]][want[i]];
                const unsigned x = (unsigned)(i % gw), y = (unsigned)(i / gw);
                if (x < x0) x0 = x;
                if (y < y0) y0 = y;
                if (x > x1) x1 = x;
                if (y > y1) y1 = y;
            }
        for (int a = 0; a < 256; ++a)
            for (int b = 0; b < 256; ++b)
                if (pair[a][b] != 0) printf("  %s: C %d, PNG %d: %zu texels\n", label, a, b, pair[a][b]);
        printf("  %s: differences within x %u..%u, y %u..%u\n", label, x0, x1, y0, y1);
    }
    for (size_t i = 0; i < (size_t)gw * gh * unit; ++i)
        if (got[i] != want[i]) { if (bad++ == 0) first = i / unit; }
    if (bad != 0) { ++g_diffs; printf("DIFF %s: %zu values differ, first at x=%zu y=%zu (C %d, PNG %d)\n", label, bad, first % gw, first / gw, got[first * unit], want[first * unit]); }
}

static void check_car(const char *psd, const char *png, const char *label)
{
    uint8_t *grey = NULL;
    uint32_t w = 0, h = 0, pw = 0, ph = 0;
    char why[160] = "";
    ++g_psds;
    uint8_t *want = png_classes(png, 0, &pw, &ph);
    if (want == NULL) { ++g_diffs; printf("DIFF %s: no PNG to compare with (%s)\n", label, png); return; }
    if (!psd_car_mask(psd, &grey, &w, &h, why, sizeof why)) { ++g_diffs; printf("DIFF %s: the reader refused: %s\n", label, why); }
    else compare(label, grey, w, h, want, pw, ph, 1);
    free(grey), free(want);
}

static void check_dash(const char *psd, const char *folder, const char *label)
{
    char path[1024];
    uint8_t *cls = NULL, *rgba = NULL;
    uint32_t w = 0, h = 0, pw = 0, ph = 0;
    char why[160] = "";
    ++g_psds;
    snprintf(path, sizeof path, "%s/dash_mask.png", folder);
    uint8_t *want = png_classes(path, 1, &pw, &ph);
    if (want == NULL) { ++g_diffs; printf("DIFF %s: no PNG to compare with (%s)\n", label, path); return; }
    if (!psd_dash_mask(psd, &cls, &rgba, &w, &h, why, sizeof why)) { ++g_diffs; printf("DIFF %s: the reader refused: %s\n", label, why); free(want); return; }
    compare(label, cls, w, h, want, pw, ph, 1);
    free(want);
    snprintf(path, sizeof path, "%s/dash_color.png", folder);
    size_t len;
    uint8_t *png = slurp(path, &len);
    PngImage img = { 0 };
    if ((png != NULL) != (rgba != NULL)) { ++g_diffs; printf("DIFF %s: the colour layer is %s, the PNG %s\n", label, rgba != NULL ? "there" : "not there", png != NULL ? "there" : "not there"); }
    else if (png != NULL && png_read(png, len, &img) == PNG_OK && !img.indexed) compare(label, rgba, w, h, img.pixels, img.width, img.height, 4);
    png_free(img.pixels);
    free(png), free(cls), free(rgba);
}

static int cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static char **list(const char *dir, size_t *n)
{
    DIR *d = opendir(dir);
    char **names = NULL;
    *n = 0;
    for (struct dirent *e = d != NULL ? readdir(d) : NULL; e != NULL; e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        names = realloc(names, (*n + 1) * sizeof *names);
        names[(*n)++] = strdup(e->d_name);
    }
    if (d != NULL) closedir(d);
    if (*n > 1) qsort(names, *n, sizeof *names, cmp);
    return names;
}

int main(int argc, char **argv)
{
    if (argc != 3) return fprintf(stderr, "usage: psd_masks_check <psd root> <data dir>\n"), 2;
    size_t cars;
    char **car = list(argv[1], &cars);
    for (size_t c = 0; c < cars; ++c) {
        char dir[1024], upper[256];
        snprintf(dir, sizeof dir, "%s/%s", argv[1], car[c]);
        size_t n;
        char **files = list(dir, &n);
        for (size_t i = 0; i < n; ++i) {
            const size_t len = strlen(files[i]);
            if (len < 5 || strcmp(files[i] + len - 4, ".psd") != 0) continue;
            if (strncmp(files[i], "dash.", 5) == 0 && strcmp(files[i], "dash.psd") != 0) continue;   /* a dash's backup (dash.before-wipers.psd): the script reads dash.psd only */
            char psd[1536], png[1536], label[512];
            snprintf(psd, sizeof psd, "%s/%s", dir, files[i]);
            snprintf(label, sizeof label, "%s/%s", car[c], files[i]);
            if (strcmp(files[i], "dash.psd") == 0) {
                for (size_t k = 0; k <= strlen(car[c]); ++k) upper[k] = (char)(car[c][k] >= 'a' && car[c][k] <= 'z' ? car[c][k] - 32 : car[c][k]);
                snprintf(png, sizeof png, "%s/dash_masks/%s", argv[2], upper);
                check_dash(psd, png, label);
            } else {
                snprintf(png, sizeof png, "%s/car_masks/%s/%.*s_mask.png", argv[2], car[c], (int)(len - 4), files[i]);
                check_car(psd, png, label);
            }
            free(files[i]);
        }
        free(files);
        free(car[c]);
    }
    free(car);
    printf("%d PSDs, %d differences\n", g_psds, g_diffs);
    return g_diffs != 0;
}
