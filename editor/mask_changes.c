#include "mask_changes.h"

#include "formats/car_mask_codec.h"
#include "formats/png.h"

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PATH = 4096 };

static uint8_t *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *buf = NULL;
    size_t n = 0, cap = 0;
    for (;;) {
        if (n == cap) {
            cap = cap ? cap * 2 : 1 << 16;
            uint8_t *bigger = realloc(buf, cap);
            if (bigger == NULL) { free(buf); fclose(f); return NULL; }
            buf = bigger;
        }
        const size_t got = fread(buf + n, 1, cap - n, f);
        n += got;
        if (got == 0) break;
    }
    fclose(f);
    *len = n;
    return buf;
}

static int is_dash_name(const char *name)
{
    return strcmp(name, "dash_mask.png") == 0 || strcmp(name, "dash_color.png") == 0;
}

static int is_mask_name(const char *name)
{
    const size_t n = strlen(name);
    if (n <= 9 || n >= MC_NAME || strcmp(name + n - 9, "_mask.png") != 0) return 0;
    for (size_t i = 0; i < n - 9; i++)
        if (name[i] == '.') return 0;   /* a backup or a draft such as frnt.before-1650_mask.png: the export leaves those out, and so does the comparison */
    return 1;
}

static int by_name(const void *a, const void *b)
{
    return strcmp(((const MaskChange *)a)->file, ((const MaskChange *)b)->file);
}

/* the mask files of a folder, added to `out` (those already there are kept); 0 when the folder cannot be read */
static int list_folder(const char *dir, MaskChanges *out, int which)
{
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (out->dash ? !is_dash_name(de->d_name) : !is_mask_name(de->d_name)) continue;
        int at = -1;
        for (int i = 0; i < out->count; i++)
            if (strcmp(out->item[i].file, de->d_name) == 0) at = i;
        if (at < 0 && out->count < MC_MAX_FILES) {
            at = out->count++;
            memset(&out->item[at], 0, sizeof out->item[at]);
            snprintf(out->item[at].file, MC_NAME, "%s", de->d_name);
            out->item[at].kind = which == 0 ? MC_NEW : MC_MISSING;   /* a file only one side has stays so */
        } else if (at >= 0) {
            out->item[at].kind = MC_UNCHANGED;   /* both have it: compared next */
        }
    }
    closedir(d);
    return 1;
}

/* the mask of a file as classes at its own size; NULL when it is not a mask picture */
static uint8_t *classes_of(const uint8_t *png, size_t len, int *w, int *h)
{
    uint32_t pw = 0, ph = 0;
    if (png_size(png, len, &pw, &ph) != PNG_OK || pw == 0 || ph == 0 || (size_t)pw * ph > PNG_MAX_PIXELS) return NULL;
    uint8_t *m = malloc((size_t)pw * ph);
    if (m != NULL && !car_mask_parse(png, len, (int)pw, (int)ph, m)) {
        free(m);
        return NULL;
    }
    *w = (int)pw;
    *h = (int)ph;
    return m;
}

/* texel I of a PNG as RGBA (an indexed one through its palette, opaque) */
static void rgba_at(const PngImage *p, size_t i, uint8_t out[4])
{
    if (p->indexed) {
        memcpy(out, p->palette + 3 * p->pixels[i], 3);
        out[3] = 255;
    } else {
        memcpy(out, p->pixels + 4 * i, 4);
    }
}

/* the texels whose picture differs between two PNGs of one size (the colour a texel shows, not its index or the kind of PNG: another palette length is no change); -1 when the size differs */
static int picture_diff(const PngImage *a, const PngImage *b)
{
    if (a->width != b->width || a->height != b->height) return -1;
    int texels = 0;
    for (size_t i = 0; i < (size_t)a->width * a->height; i++) {
        uint8_t pa[4], pb[4];
        rgba_at(a, i, pa), rgba_at(b, i, pb);
        texels += memcmp(pa, pb, 4) != 0;
    }
    return texels;
}

static void compare_picture(const uint8_t *pa, size_t la, const uint8_t *pb, size_t lb, MaskChange *c)
{
    PngImage ia = { 0 }, ib = { 0 };
    const int ok = pa != NULL && pb != NULL && png_read(pa, la, &ia) == PNG_OK && png_read(pb, lb, &ib) == PNG_OK;
    const int texels = ok ? picture_diff(&ia, &ib) : -1;
    if (texels < 0) {
        c->kind = MC_CHANGED;
        c->texels = ok ? (int)(ia.width * ia.height) : 0;
        if (!ok) snprintf(c->note, sizeof c->note, "not a readable PNG");
        else snprintf(c->note, sizeof c->note, "%ux%u -> %ux%u", ib.width, ib.height, ia.width, ia.height);   /* the game's size to the project's */
    } else if (texels > 0) {
        c->kind = MC_CHANGED;
        c->texels = texels;
    }
    if (ia.pixels != NULL) png_free(ia.pixels);
    if (ib.pixels != NULL) png_free(ib.pixels);
}

static void compare_file(const char *project, const char *game, MaskChange *c, int dash)
{
    char a[PATH], b[PATH];
    snprintf(a, sizeof a, "%s/%s", project, c->file);
    snprintf(b, sizeof b, "%s/%s", game, c->file);
    size_t la = 0, lb = 0;
    uint8_t *pa = read_file(a, &la), *pb = read_file(b, &lb);
    c->kind = MC_UNCHANGED;
    if (pa != NULL && pb != NULL && la == lb && memcmp(pa, pb, la) == 0) { free(pa); free(pb); return; }   /* the same bytes: the export's own copy */
    if (dash) {
        compare_picture(pa, la, pb, lb, c);
        free(pa), free(pb);
        return;
    }
    int wa = 0, ha = 0, wb = 0, hb = 0;
    uint8_t *ma = pa != NULL ? classes_of(pa, la, &wa, &ha) : NULL, *mb = pb != NULL ? classes_of(pb, lb, &wb, &hb) : NULL;
    if (ma == NULL || mb == NULL) {
        c->kind = MC_CHANGED;   /* one of them is not a mask picture: not the same file */
        snprintf(c->note, sizeof c->note, "%s is not a mask picture", ma == NULL ? "the project's file" : "the game's file");
    } else if (wa != wb || ha != hb) {
        c->kind = MC_CHANGED;
        c->texels = wa * ha;
        snprintf(c->note, sizeof c->note, "%dx%d -> %dx%d", wb, hb, wa, ha);   /* the game's size to the project's */
    } else {
        for (size_t i = 0; i < (size_t)wa * ha; i++)
            if (ma[i] != mb[i]) {
                c->texels++;
                c->lost[mask_edit_class_index(mb[i])]++;
                c->gained[mask_edit_class_index(ma[i])]++;
            }
        if (c->texels > 0) c->kind = MC_CHANGED;
    }
    free(pa), free(pb), free(ma), free(mb);
}

static int compare_folders(const char *project, const char *game, MaskChanges *out, int dash, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    out->dash = dash;
    if (!list_folder(project, out, 0)) {
        snprintf(why, why_cap, "Cannot read the project's masks folder %s", project);
        return 0;
    }
    if (!list_folder(game, out, 1)) {
        memset(out, 0, sizeof *out);
        snprintf(why, why_cap, "The game's masks folder %s is not there: there is nothing to compare with", game);
        return 0;
    }
    qsort(out->item, (size_t)out->count, sizeof out->item[0], by_name);
    for (int i = 0; i < out->count; i++) {
        MaskChange *c = &out->item[i];
        if (c->kind == MC_UNCHANGED) compare_file(project, game, c, out->dash);
        switch (c->kind) {
        case MC_UNCHANGED: out->unchanged++; break;
        case MC_CHANGED: out->changed++; break;
        case MC_NEW: out->added++; break;
        default: out->missing++; break;
        }
    }
    return 1;
}

int mask_changes_compare(const char *project, const char *game, MaskChanges *out, char *why, size_t why_cap)
{
    return compare_folders(project, game, out, 0, why, why_cap);
}

int mask_changes_compare_dash(const char *project, const char *game, MaskChanges *out, char *why, size_t why_cap)
{
    return compare_folders(project, game, out, 1, why, why_cap);
}

int mask_changes_publish(const char *project, const char *game, const MaskChanges *c, int *written, char *why, size_t why_cap)
{
    *written = 0;
    for (int i = 0; i < c->count; i++) {
        const MaskChange *m = &c->item[i];
        if (m->kind != MC_CHANGED && m->kind != MC_NEW) continue;
        char from[PATH], to[PATH];
        snprintf(from, sizeof from, "%s/%s", project, m->file);
        snprintf(to, sizeof to, "%s/%s", game, m->file);
        size_t len = 0;
        uint8_t *bytes = read_file(from, &len);
        FILE *f = bytes != NULL ? fopen(to, "wb") : NULL;
        const int ok = f != NULL && fwrite(bytes, 1, len, f) == len;
        if (f != NULL && fclose(f) != 0) { free(bytes); snprintf(why, why_cap, "cannot write %s", to); return 0; }
        free(bytes);
        if (!ok) { snprintf(why, why_cap, "cannot copy %s into %s", m->file, game); return 0; }
        ++*written;
    }
    return 1;
}

static void append(char *out, size_t cap, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

static void append(char *out, size_t cap, const char *fmt, ...)
{
    const size_t n = strlen(out);
    if (n + 1 >= cap) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(out + n, cap - n, fmt, args);
    va_end(args);
}

void mask_changes_text(const MaskChanges *c, const char *folder, char *out, size_t cap)
{
    if (cap == 0) return;
    out[0] = '\0';
    const char *kind = c->dash ? "dash_masks" : "car_masks";
    append(out, cap, "%s/%s: %d changed, %d new, %d missing, %d unchanged (compared with the game's data/%s/%s)\n", kind, folder, c->changed, c->added, c->missing, c->unchanged, kind, folder);
    for (int i = 0; i < c->count; i++) {
        const MaskChange *m = &c->item[i];
        if (m->kind == MC_CHANGED) {
            append(out, cap, "changed: %s %d texels", m->file, m->texels);
            if (m->note[0] != '\0') append(out, cap, " (%s)", m->note);
            const char *sep = ": ";
            for (int k = 0; k < ME_CLASSES; k++)
                if (m->gained[k] > 0) append(out, cap, "%s%s +%d", sep, mask_edit_class(k)->tag, m->gained[k]), sep = ", ";
            for (int k = 0; k < ME_CLASSES; k++)
                if (m->lost[k] > 0) append(out, cap, "%s%s -%d", sep, mask_edit_class(k)->tag, m->lost[k]), sep = ", ";
            append(out, cap, "\n");
        } else if (m->kind == MC_NEW) {
            append(out, cap, "new: %s\n", m->file);
        } else if (m->kind == MC_MISSING) {
            append(out, cap, "missing: %s\n", m->file);
        }
    }
}
