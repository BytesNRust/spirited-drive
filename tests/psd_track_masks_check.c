/* usage: psd_track_masks_check <psd root (<TRACK>/<name>.psd, or a workspace's tracks/ with <TRACK>/art-source/masks/<name>.psd)> <data/track_masks>
 * Reads every track PSD with the C reader (psd_track_mask) and compares its marks with the PNG tools/track_mask_from_psd.py made, as the game reads it (red 128 or more and opaque
 * reflective; red and green 128 or more, blue under 128, opaque, a lamp), and the PNG's colour type (grey without lamps, RGBA with). Lists every difference; exit 0 only when there is
 * none. Read only. */
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

static void check(const char *psd, const char *png, const char *label)
{
    uint8_t *marks = NULL;
    uint32_t w = 0, h = 0;
    int lamp = 0;
    char why[160] = "";
    ++g_psds;
    size_t len;
    uint8_t *bytes = slurp(png, &len);
    PngImage img = { 0 };
    if (bytes == NULL || png_read(bytes, len, &img) != PNG_OK) { ++g_diffs; printf("DIFF %s: no readable PNG (%s)\n", label, png), free(bytes); return; }
    const int rgba_file = len > 25 && bytes[25] == 6;   /* IHDR colour type: 0 grey, 6 RGBA */
    if (!psd_track_mask(psd, &marks, &lamp, &w, &h, why, sizeof why)) { ++g_diffs; printf("DIFF %s: the reader refused: %s\n", label, why); }
    else if (w != img.width || h != img.height) { ++g_diffs; printf("DIFF %s: size %ux%u, the PNG %ux%u\n", label, w, h, img.width, img.height); }
    else {
        size_t bad = 0, first = 0, lamps = 0;
        for (size_t i = 0; i < (size_t)w * h; ++i) {
            int want = 0;
            const uint8_t *px;
            uint8_t rgba[4];
            if (img.indexed) { const uint8_t *p = img.palette + img.pixels[i] * 3; rgba[0] = p[0], rgba[1] = p[1], rgba[2] = p[2], rgba[3] = 255; px = rgba; }
            else px = img.pixels + i * 4;
            if (px[3] != 0 && px[0] >= 128) want = (px[1] >= 128 && px[2] < 128) ? TRACK_MASK_LAMP : TRACK_MASK_REFLECTIVE;
            lamps += marks[i] == TRACK_MASK_LAMP;
            if (marks[i] != want && bad++ == 0) first = i;
        }
        if (bad != 0) { ++g_diffs; printf("DIFF %s: %zu texels differ, first at x=%zu y=%zu (C %d, PNG class %d)\n", label, bad, first % w, first / w, marks[first], marks[first] == 0 ? 1 : 0); }
        if (lamp != (rgba_file ? 1 : 0)) { ++g_diffs; printf("DIFF %s: the PSD has %s lamps (%zu texels), the PNG is %s\n", label, lamp ? "" : "no", lamps, rgba_file ? "RGBA" : "grey"); }
    }
    png_free(img.pixels);
    free(bytes), free(marks);
}

int main(int argc, char **argv)
{
    if (argc != 3) return fprintf(stderr, "usage: psd_track_masks_check <psd root> <data/track_masks>\n"), 2;
    size_t tracks;
    char **track = list(argv[1], &tracks);
    for (size_t t = 0; t < tracks; ++t) {
        char dir[1024];
        snprintf(dir, sizeof dir, "%s/%s/art-source/masks", argv[1], track[t]);   /* a workspace's <TRACK>/art-source/masks, else the art folder's <TRACK> */
        if (opendir(dir) == NULL) snprintf(dir, sizeof dir, "%s/%s", argv[1], track[t]);
        size_t n;
        char **files = list(dir, &n);
        for (size_t i = 0; i < n; ++i) {
            const size_t len = strlen(files[i]);
            if (len > 4 && strcmp(files[i] + len - 4, ".psd") == 0) {
                char psd[1536], png[1536], label[512];
                snprintf(psd, sizeof psd, "%s/%s", dir, files[i]);
                snprintf(png, sizeof png, "%s/%s/%.*s.png", argv[2], track[t], (int)(len - 4), files[i]);
                snprintf(label, sizeof label, "%s/%s", track[t], files[i]);
                check(psd, png, label);
            }
            free(files[i]);
        }
        free(files);
        free(track[t]);
    }
    free(track);
    printf("%d PSDs, %d differences\n", g_psds, g_diffs);
    return g_diffs != 0;
}
