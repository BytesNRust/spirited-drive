/* See car_migrate_art.h. */
#include "car_migrate_art.h"

#include "formats/car_mask_codec.h"
#include "formats/png.h"
#include "formats/psd_mask.h"
#include "game/race/dash_mask.h"
#include "studio/fsio.h"

#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* the owner's changes since data/ was made: the PSD wins (owner OK 2026-11-21) */
static int on_purpose(const char *stem, const char *name)
{
    static const char *const AXXESS[] = { "frnt_mask.png", "frnt_lo_mask.png", "side_mask.png", "side_lo_mask.png" };
    if (strcmp(stem, "AXXESS") == 0)
        for (size_t i = 0; i < sizeof AXXESS / sizeof AXXESS[0]; ++i)
            if (strcmp(name, AXXESS[i]) == 0) return 1;
    return strcmp(stem, "ANSX") == 0 && (strcmp(name, "dash_mask.png") == 0 || strcmp(name, "dash_color.png") == 0);
}

/* a PSD with no extra dot: letters, digits and '_' then ".psd" (frnt.psd, frnt_lo.psd, dash.psd); frnt.before-1650.psd is a backup */
static int real_psd(const char *name)
{
    const size_t n = strlen(name);
    if (n < 5 || strcmp(name + n - 4, ".psd") != 0) return 0;
    for (size_t i = 0; i + 4 < n; ++i)
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') return 0;
    return 1;
}

static void fail(MigrateReport *r, const char *stem, const char *what, const char *detail)
{
    if (r->failure_count < (int)(sizeof r->failures / sizeof r->failures[0])) snprintf(r->failures[r->failure_count], sizeof r->failures[0], "%s: %s%s%s", stem, what, detail[0] ? ": " : "", detail);
    ++r->failure_count;
}

static void note(char list[][96], int *count, const char *stem, const char *name, const char *extra)
{
    if (*count < 16) snprintf(list[*count], 96, "%s/%s%s", stem, name, extra);
    ++*count;
}

/* the picture of a PNG as RGBA, or NULL */
static uint8_t *rgba_of(const uint8_t *png, size_t len, uint32_t *w, uint32_t *h)
{
    PngImage img;
    if (png_read(png, len, &img) != PNG_OK) return NULL;
    const size_t count = (size_t)img.width * img.height;
    uint8_t *out = malloc(count * 4);
    for (size_t i = 0; out != NULL && i < count; ++i) {
        if (img.indexed) memcpy(out + 4 * i, img.palette + 3 * img.pixels[i], 3), out[4 * i + 3] = 255;
        else memcpy(out + 4 * i, img.pixels + 4 * i, 4);
    }
    *w = img.width, *h = img.height;
    png_free(img.pixels);
    return out;
}

static int same_picture(const uint8_t *a, size_t an, const uint8_t *b, size_t bn)
{
    uint32_t aw = 0, ah = 0, bw = 0, bh = 0;
    uint8_t *x = rgba_of(a, an, &aw, &ah), *y = rgba_of(b, bn, &bw, &bh);
    const int same = x != NULL && y != NULL && aw == bw && ah == bh && memcmp(x, y, (size_t)aw * ah * 4) == 0;
    free(x), free(y);
    return same;
}

/* the PSD's made PNG `png` against the kit's file `dir`/`name`: see migrate_art */
static void settle(MigrateReport *r, const char *stem, const char *dir, const char *name, uint8_t *png, size_t len)
{
    char path[1024];
    ExchangeError err = { { 0 } };
    snprintf(path, sizeof path, "%s/%s", dir, name);
    size_t have_len = 0;
    uint8_t *have = fsio_exists(path) ? fsio_read_file(path, &have_len, &err) : NULL;
    if (have != NULL && have_len == len && memcmp(have, png, len) == 0) ++r->same;
    else if (on_purpose(stem, name)) {
        if (!fsio_write_file(path, png, len, 1, &err)) fail(r, stem, "cannot write", path);
        else note(r->changed, &r->changed_count, stem, name, have == NULL ? " (a new file: the PSD has a colour layer)" : "");
    } else if (have == NULL) fail(r, stem, name, "the PSD makes it, data/ has none");
    else if (same_picture(have, have_len, png, len)) note(r->picture, &r->picture_count, stem, name, "");
    else fail(r, stem, name, "the PSD's mask differs from data/'s");
    free(have);
}

static void make_car(MigrateReport *r, const char *stem, const char *psd, const char *tex, const char *dir)
{
    uint8_t *grey = NULL, *png = NULL;
    uint32_t w = 0, h = 0;
    size_t len = 0;
    char why[200] = "", name[128];
    if (!psd_car_mask(psd, &grey, &w, &h, why, sizeof why)) {
        fail(r, stem, tex, why);
        return;
    }
    if (!car_mask_write_grey(grey, (int)w, (int)h, &png, &len)) fail(r, stem, tex, "cannot write the PNG");
    else snprintf(name, sizeof name, "%s_mask.png", tex), settle(r, stem, dir, name, png, len);
    free(grey), png_free(png);
}

/* game/race/dash_mask.c's dash_mask_write, which lives with the game: the classes as the indexed PNG the game reads (black, blue, green, white, red, cyan) */
static int write_dash(const uint8_t *mask, uint32_t w, uint32_t h, uint8_t **png, size_t *len)
{
    uint8_t *indices = malloc((size_t)w * h);
    uint8_t palette[768] = { 0 };
    if (indices == NULL) return 0;
    for (size_t i = 0; i < (size_t)w * h; ++i)
        indices[i] = mask[i] == DASH_MASK_DASH ? 1 : mask[i] == DASH_MASK_DIAL ? 2 : mask[i] == DASH_MASK_LIT ? 3 : mask[i] == DASH_MASK_BLADE ? 4 : mask[i] == DASH_MASK_NOSE ? 5 : 0;
    palette[5] = 255, palette[7] = 255, palette[9] = palette[10] = palette[11] = 255, palette[12] = 255, palette[16] = palette[17] = 255;
    const PngResult r = png_write_indexed(indices, w, h, palette, png, len);
    free(indices);
    return r == PNG_OK;
}

static void make_dash(MigrateReport *r, const char *stem, const char *psd, const char *dir)
{
    uint8_t *classes = NULL, *colour = NULL, *png = NULL;
    uint32_t w = 0, h = 0;
    size_t len = 0;
    char why[200] = "";
    if (!psd_dash_mask(psd, &classes, &colour, &w, &h, why, sizeof why)) {
        fail(r, stem, "dash.psd", why);
        return;
    }
    if (!write_dash(classes, w, h, &png, &len)) fail(r, stem, "dash.psd", "cannot write the PNG");
    else settle(r, stem, dir, "dash_mask.png", png, len);
    png_free(png), png = NULL;
    if (colour != NULL) {
        if (png_write_rgba(colour, w, h, &png, &len) != PNG_OK) fail(r, stem, "dash.psd", "cannot write the colour PNG");
        else settle(r, stem, dir, "dash_color.png", png, len);
        png_free(png);
    } else {
        char path[1024];
        snprintf(path, sizeof path, "%s/dash_color.png", dir);
        if (fsio_exists(path)) fail(r, stem, "dash_color.png", "data/ has one, the PSD has no colour layer");
    }
    free(classes), free(colour);
}

void migrate_art(const char *kit, const char *stem, const char *art_root, MigrateReport *r)
{
    char art[1024], src[1024], cars[1024], dash[1024];
    ExchangeError err = { { 0 } };
    if (!fsio_find_ci(art_root, stem, art, sizeof art)) return;   /* no folder: no PSDs (the kit keeps data/'s masks) */
    snprintf(src, sizeof src, "%s/art-source/masks", kit);
    snprintf(cars, sizeof cars, "%s/art-generated/car_masks", kit);
    snprintf(dash, sizeof dash, "%s/art-generated/dash_masks", kit);
    DIR *d = opendir(art);
    char *names[256];
    int n = 0;
    for (struct dirent *e = d != NULL ? readdir(d) : NULL; e != NULL && n < 256; e = readdir(d))
        if (strlen(e->d_name) > 4 && strcasecmp(e->d_name + strlen(e->d_name) - 4, ".psd") == 0) names[n++] = strdup(e->d_name);
    if (d != NULL) closedir(d);
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (strcmp(names[j], names[i]) < 0) { char *t = names[i]; names[i] = names[j]; names[j] = t; }
    for (int i = 0; i < n; ++i) {
        char from[1100], to[1100];
        if (!real_psd(names[i])) {
            if (r->skipped_count < (int)(sizeof r->skipped / sizeof r->skipped[0])) snprintf(r->skipped[r->skipped_count], sizeof r->skipped[0], "%s/%s", stem, names[i]);
            ++r->skipped_count;
            free(names[i]);
            continue;
        }
        snprintf(from, sizeof from, "%s/%s", art, names[i]);
        snprintf(to, sizeof to, "%s/%s", src, names[i]);
        size_t size = 0;
        uint8_t *bytes = fsio_read_file(from, &size, &err);
        if (bytes == NULL || !fsio_write_file(to, bytes, size, 0, &err)) fail(r, stem, names[i], err.message);
        else {
            ++r->psds_copied;
            if (strcmp(names[i], "dash.psd") == 0) make_dash(r, stem, to, dash);
            else {
                names[i][strlen(names[i]) - 4] = '\0';
                make_car(r, stem, to, names[i], cars);
            }
        }
        free(bytes), free(names[i]);
    }
}

void migrate_report_print(const MigrateReport *r, FILE *out)
{
    fprintf(out, "## Masks from the PSDs (the C reader)\n\n- byte for byte the game's data/ file: %d\n- changed on purpose (the PSD is newer than data/; the owner said the PSD wins): %d\n",
            r->same, r->changed_count);
    for (int i = 0; i < r->changed_count && i < 16; ++i) fprintf(out, "  - %s\n", r->changed[i]);
    fprintf(out, "- the same picture as data/'s, in other PNG bytes: %d (every texel is the same; the C writer packs the PNG differently from the python files, and the older dash PNGs have 5 palette entries "
                 "where the writer has 6; the kit keeps data/'s bytes, so nothing changes for the game)\n", r->picture_count);
    for (int i = 0; i < r->picture_count && i < 16; ++i) fprintf(out, "  - %s\n", r->picture[i]);
    fprintf(out, "\n## PSDs copied to art-source/masks\n\n- %d\n\n## PSDs skipped (a backup: a name with an extra dot)\n\n", r->psds_copied);
    for (int i = 0; i < r->skipped_count && i < 96; ++i) fprintf(out, "- %s\n", r->skipped[i]);
    if (r->skipped_count == 0) fprintf(out, "- none\n");
    fprintf(out, "\n## Failures\n\n");
    for (int i = 0; i < r->failure_count && i < 64; ++i) fprintf(out, "- %s\n", r->failures[i]);
    if (r->failure_count == 0) fprintf(out, "- none\n");
}
