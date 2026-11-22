#include "psd_convert.h"

#include "formats/car_mask_codec.h"
#include "formats/dash_png.h"
#include "formats/png.h"
#include "formats/psd_mask.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

enum { PATH = 4096 };

static int join(char *out, size_t cap, const char *a, const char *b)
{
    const int n = snprintf(out, cap, "%s/%s", a, b);
    return n > 0 && (size_t)n < cap;
}

static int make_dir(const char *path)
{
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

static int write_file(const char *path, const uint8_t *bytes, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;
    const int ok = fwrite(bytes, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}

/* `psd_name` without ".psd", of a PSD that is the car's (not a backup: a second dot) */
static int stem_of(const char *psd_name, char *stem, size_t cap)
{
    const size_t len = strlen(psd_name);
    if (len < 5 || strcasecmp(psd_name + len - 4, ".psd") != 0 || memchr(psd_name, '.', len - 4) != NULL || len - 4 >= cap) return 0;
    memcpy(stem, psd_name, len - 4);
    stem[len - 4] = '\0';
    return 1;
}

static int body(const char *psd, const char *name, const char *png_name, const char *dir, char *why, size_t why_cap)
{
    uint8_t *grey = NULL, *png = NULL;
    uint32_t w = 0, h = 0;
    size_t len = 0;
    char reason[200], out[PATH];
    if (!psd_car_mask(psd, &grey, &w, &h, reason, sizeof reason)) return snprintf(why, why_cap, "%s: %s", name, reason), 0;
    int ok = car_mask_write_grey(grey, (int)w, (int)h, &png, &len);
    free(grey);
    if (!ok) return snprintf(why, why_cap, "%s: the mask could not be made into a PNG", name), 0;
    ok = join(out, sizeof out, dir, png_name) && write_file(out, png, len);
    png_free(png);
    return ok ? 1 : (snprintf(why, why_cap, "%s: the PNG could not be written", name), 0);
}

static int dash(const char *psd, const char *name, const char *dir, int *files, char *why, size_t why_cap)
{
    uint8_t *classes = NULL, *colour = NULL, *png = NULL;
    uint32_t w = 0, h = 0;
    size_t len = 0;
    char reason[200], out[PATH];
    if (!psd_dash_mask(psd, &classes, &colour, &w, &h, reason, sizeof reason)) return snprintf(why, why_cap, "%s: %s", name, reason), 0;
    int ok = dash_png_write(classes, (int)w, (int)h, &png, &len) && join(out, sizeof out, dir, "dash_mask.png") && write_file(out, png, len);
    png_free(png);
    png = NULL;
    free(classes);
    if (ok) ++*files;
    if (ok && colour != NULL) {
        ok = png_write_rgba(colour, w, h, &png, &len) == PNG_OK && join(out, sizeof out, dir, "dash_color.png") && write_file(out, png, len);
        png_free(png);
        if (ok) ++*files;
    }
    free(colour);
    return ok ? 1 : (snprintf(why, why_cap, "%s: the PNG could not be written", name), 0);
}

int psd_convert(const char *psd_dir, const char *folder, const char *masks_out, const char *dash_out, const int *cancel, PsdConverted *done, char *why, size_t why_cap)
{
    char masks[PATH], dashes[PATH];
    *done = (PsdConverted){ 0 };
    if (!join(masks, sizeof masks, masks_out, folder) || !join(dashes, sizeof dashes, dash_out, folder) || !make_dir(masks_out) || !make_dir(dash_out) || !make_dir(masks) || !make_dir(dashes))
        return snprintf(why, why_cap, "no scratch folder for the conversion"), 0;
    DIR *d = opendir(psd_dir);
    if (d == NULL) return snprintf(why, why_cap, "the PSD folder cannot be read"), 0;
    int ok = 1;
    for (const struct dirent *e = readdir(d); e != NULL && ok; e = readdir(d)) {
        char stem[PATH], psd[PATH], png_name[PATH + 16];
        if (cancel != NULL && __atomic_load_n(cancel, __ATOMIC_RELAXED) != 0) {
            snprintf(why, why_cap, "it was stopped");
            ok = 0;
            break;
        }
        if (!stem_of(e->d_name, stem, sizeof stem) || !join(psd, sizeof psd, psd_dir, e->d_name)) continue;
        if (strcasecmp(stem, "dash") == 0) {
            ok = dash(psd, e->d_name, dashes, &done->dash_files, why, why_cap);
            continue;
        }
        snprintf(png_name, sizeof png_name, "%s_mask.png", stem);
        ok = body(psd, e->d_name, png_name, masks, why, why_cap);
        if (ok) ++done->masks;
    }
    closedir(d);
    return ok;
}
