#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
/* track_masks.h: the masks of an original track's folder. */
#include "track_masks.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "formats/png.h"
#include "formats/psd_mask.h"

#ifdef __APPLE__
#define MTIME(st) ((double)(st).st_mtimespec.tv_sec + (double)(st).st_mtimespec.tv_nsec * 1e-9)
#else
#define MTIME(st) ((double)(st).st_mtim.tv_sec + (double)(st).st_mtim.tv_nsec * 1e-9)
#endif

typedef struct { uint8_t *marks; uint32_t w, h; } Marks;

static void say(char *why, size_t cap, const char *fmt, const char *a)
{
    if (why != NULL && cap > 0) snprintf(why, cap, fmt, a);
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int mtime_of(const char *path, double *out)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    *out = MTIME(st);
    return 1;
}

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    rewind(f);
    uint8_t *b = n > 0 ? malloc((size_t)n) : NULL;
    if (b != NULL && fread(b, 1, (size_t)n, f) != (size_t)n) free(b), b = NULL;
    fclose(f);
    *len = b != NULL ? (size_t)n : 0;
    return b;
}

/* a mask PNG as marks, the game's reading (track_mask.h): red 128 or more and not transparent is reflective; red and green 128 or more with blue under 128 a lamp */
static int png_marks(const char *path, Marks *out)
{
    size_t len;
    uint8_t *bytes = slurp(path, &len);
    if (bytes == NULL) return 0;
    PngImage img;
    const PngResult r = png_read(bytes, len, &img);
    free(bytes);
    if (r != PNG_OK) return 0;
    const size_t n = (size_t)img.width * img.height;
    out->marks = malloc(n > 0 ? n : 1);
    if (out->marks == NULL) return png_free(img.pixels), 0;
    for (size_t i = 0; i < n; ++i) {
        uint8_t px[4];
        if (img.indexed) memcpy(px, img.palette + img.pixels[i] * 3, 3), px[3] = 255;
        else memcpy(px, img.pixels + i * 4, 4);
        out->marks[i] = px[3] != 0 && px[0] >= 128 ? (px[1] >= 128 && px[2] < 128 ? TRACK_MASK_LAMP : TRACK_MASK_REFLECTIVE) : TRACK_MASK_NONE;
    }
    out->w = img.width, out->h = img.height;
    png_free(img.pixels);
    return 1;
}

static void count_marks(const Marks *m, int *reflective, int *lamps)
{
    *reflective = *lamps = 0;
    for (size_t i = 0; i < (size_t)m->w * m->h; ++i) *reflective += m->marks[i] == TRACK_MASK_REFLECTIVE, *lamps += m->marks[i] == TRACK_MASK_LAMP;
}

static int same(const Marks *a, const Marks *b)
{
    return a->w == b->w && a->h == b->h && memcmp(a->marks, b->marks, (size_t)a->w * a->h) == 0;
}

static int is_psd(const char *name)
{
    const size_t n = strlen(name);
    if (n < 5 || strcmp(name + n - 4, ".psd") != 0) return 0;
    for (size_t i = 0; i + 4 < n; ++i) if (name[i] == '.') return 0;   /* a backup or a hidden file */
    return 1;
}

static int is_png(const char *name)
{
    const size_t n = strlen(name);
    return n >= 5 && name[0] != '.' && strcmp(name + n - 4, ".png") == 0;
}

static int rank(const char *name) { return strncmp(name, "slot_", 5) == 0 ? 0 : 1; }

static int by_name(const void *a, const void *b)
{
    const TrackMaskRow *x = a, *y = b;
    return rank(x->name) != rank(y->name) ? rank(x->name) - rank(y->name) : strcmp(x->name, y->name);
}

static TrackMaskRow *row_for(TrackMasks *m, const char *name)
{
    for (int i = 0; i < m->count; ++i) if (strcmp(m->row[i].name, name) == 0) return &m->row[i];
    if (m->count >= TM_MAX || strlen(name) >= TRACK_TEX_NAME) return NULL;
    TrackMaskRow *r = &m->row[m->count++];
    memset(r, 0, sizeof *r);
    snprintf(r->name, sizeof r->name, "%s", name);
    return r;
}

static void collect(const char *dir, int psd, TrackMasks *m)
{
    DIR *d = opendir(dir);
    for (struct dirent *e = d != NULL ? readdir(d) : NULL; e != NULL; e = readdir(d)) {
        if (psd ? !is_psd(e->d_name) : !is_png(e->d_name)) continue;
        char name[TRACK_TEX_NAME + 8];
        snprintf(name, sizeof name, "%.*s", (int)strlen(e->d_name) - 4, e->d_name);
        TrackMaskRow *r = row_for(m, name);
        if (r != NULL && psd) r->has_psd = 1;
        else if (r != NULL) r->has_png = 1;
    }
    if (d != NULL) closedir(d);
}

static void paths(const char *dir, const char *name, char *psd, char *png, size_t cap)
{
    snprintf(psd, cap, "%s/art-source/masks/%s.psd", dir, name);
    snprintf(png, cap, "%s/art-generated/track_masks/%s.png", dir, name);
}

static void judge(const char *dir, TrackMaskRow *r)
{
    char psd[1100], png[1100], why[TM_NOTE] = "";
    paths(dir, r->name, psd, png, sizeof psd);
    Marks from_psd = { NULL, 0, 0 }, from_png = { NULL, 0, 0 };
    const int png_ok = r->has_png && png_marks(png, &from_png);
    if (!r->has_psd) {
        r->state = TM_PNG_SOURCE;
        if (png_ok) count_marks(&from_png, &r->reflective, &r->lamps), r->width = r->width != 0 ? r->width : (int)from_png.w, r->height = r->height != 0 ? r->height : (int)from_png.h;
        else snprintf(r->note, sizeof r->note, "the PNG does not read");
        free(from_png.marks);
        return;
    }
    int lamp = 0;
    if (!psd_track_mask(psd, &from_psd.marks, &lamp, &from_psd.w, &from_psd.h, why, sizeof why)) {
        r->state = TM_BAD;
        snprintf(r->note, sizeof r->note, "%s", why);
        free(from_png.marks);
        return;
    }
    if (r->width == 0) r->width = (int)from_psd.w, r->height = (int)from_psd.h;
    count_marks(&from_psd, &r->reflective, &r->lamps);
    if (psd_track_marks_count(from_psd.marks, (size_t)from_psd.w * from_psd.h) == 0) r->state = TM_EMPTY;   /* not a source: a PNG next to it stays */
    else if (!r->has_png) r->state = TM_NEW;
    else if (png_ok && same(&from_psd, &from_png)) r->state = TM_OK;
    else {
        double tp = 0, tg = 0;
        mtime_of(psd, &tp), mtime_of(png, &tg);
        r->state = tp > tg ? TM_STALE : TM_EDITED;
        if (r->state == TM_EDITED && png_ok) count_marks(&from_png, &r->reflective, &r->lamps);   // an edited PNG is what the game uses now: its texels are the ones shown
    }
    free(from_psd.marks), free(from_png.marks);
}

int track_masks_scan(const char *dir, const TrackTexInfo *tex, int tex_count, TrackMasks *out, char *why, size_t why_cap)
{
    char src[1100], gen[1100];
    memset(out, 0, sizeof *out);
    snprintf(src, sizeof src, "%s/art-source/masks", dir);
    snprintf(gen, sizeof gen, "%s/art-generated/track_masks", dir);
    if (!is_dir(src) && !is_dir(gen) && (tex == NULL || tex_count <= 0)) return say(why, why_cap, "%s has no art-source/masks or art-generated/track_masks", dir), 0;
    collect(src, 1, out);
    collect(gen, 0, out);
    qsort(out->row, (size_t)out->count, sizeof out->row[0], by_name);
    for (int i = 0; i < out->count; ++i) {
        TrackMaskRow *r = &out->row[i];
        for (int t = 0; tex != NULL && t < tex_count; ++t)
            if (strcmp(tex[t].name, r->name) == 0) r->width = tex[t].width, r->height = tex[t].height;
        judge(dir, r);
        out->to_make += r->state == TM_NEW || r->state == TM_STALE;
        out->edited += r->state == TM_EDITED;
    }
    for (int t = 0; tex != NULL && t < tex_count; ++t) {   /* the textures that have no mask: a row each, so the table lists the whole track */
        int have = 0;
        for (int i = 0; i < out->count && !have; ++i) have = strcmp(out->row[i].name, tex[t].name) == 0;
        TrackMaskRow *r = have ? NULL : row_for(out, tex[t].name);
        if (r == NULL) continue;
        r->state = TM_NONE, r->width = tex[t].width, r->height = tex[t].height;
    }
    qsort(out->row, (size_t)out->count, sizeof out->row[0], by_name);
    return 1;
}

/* the PNG of one PSD's marks, written beside its name and renamed whole; grey without lamps, RGBA with them */
static int write_png(const char *dir, const char *name, char *why, size_t cap)
{
    char psd[1100], png[1100], part[1120], text[TM_NOTE] = "";
    paths(dir, name, psd, png, sizeof psd);
    Marks m = { NULL, 0, 0 };
    int lamp = 0;
    if (!psd_track_mask(psd, &m.marks, &lamp, &m.w, &m.h, text, sizeof text)) return say(why, cap, "%s", text), 0;
    if (psd_track_marks_count(m.marks, (size_t)m.w * m.h) == 0) return free(m.marks), say(why, cap, "%s: nothing is painted in the PSD's mask layer", name), 0;
    const size_t n = (size_t)m.w * m.h;
    uint8_t *px = malloc(n * (lamp ? 4 : 1)), *out = NULL;
    size_t len = 0;
    if (px == NULL) return free(m.marks), say(why, cap, "%s: out of memory", name), 0;
    for (size_t i = 0; i < n; ++i) {
        const int v = m.marks[i];
        if (lamp) {
            const uint8_t c[4] = { 255, 255, (uint8_t)(v == TRACK_MASK_REFLECTIVE ? 255 : 0), (uint8_t)(v != TRACK_MASK_NONE ? 255 : 0) };
            memcpy(px + i * 4, c, 4);
        } else {
            px[i] = v != TRACK_MASK_NONE ? 255 : 0;
        }
    }
    const PngResult r = lamp ? png_write_rgba(px, m.w, m.h, &out, &len) : png_write_grey(px, m.w, m.h, &out, &len);
    free(px), free(m.marks);
    if (r != PNG_OK) return say(why, cap, "%s: cannot encode the PNG", name), 0;
    char folder[1100];
    snprintf(folder, sizeof folder, "%s/art-generated", dir);
    mkdir(folder, 0777);
    snprintf(folder, sizeof folder, "%s/art-generated/track_masks", dir);
    mkdir(folder, 0777);
    snprintf(part, sizeof part, "%s.part", png);
    FILE *f = fopen(part, "wb");
    const int ok = f != NULL && fwrite(out, 1, len, f) == len;
    if (f != NULL) fclose(f);
    png_free(out);
    if (!ok || rename(part, png) != 0) return remove(part), say(why, cap, "cannot write %s", png), 0;
    return 1;
}

double track_masks_newest_psd(const char *dir)
{
    char src[1100];
    double newest = 0;
    snprintf(src, sizeof src, "%s/art-source/masks", dir);
    DIR *d = opendir(src);
    for (struct dirent *e = d != NULL ? readdir(d) : NULL; e != NULL; e = readdir(d)) {
        double t = 0;
        char path[1500];
        if (!is_psd(e->d_name)) continue;
        snprintf(path, sizeof path, "%s/%s", src, e->d_name);
        if (mtime_of(path, &t) && t > newest) newest = t;
    }
    if (d != NULL) closedir(d);
    return newest;
}

int track_masks_generate(const char *dir, const TrackMasks *rows, int *written, char *why, size_t why_cap)
{
    *written = 0;
    for (int i = 0; i < rows->count; ++i) {
        if (rows->row[i].state != TM_NEW && rows->row[i].state != TM_STALE) continue;
        if (!write_png(dir, rows->row[i].name, why, why_cap)) return 0;
        ++*written;
    }
    return 1;
}

int track_masks_regenerate(const char *dir, const char *name, char *why, size_t why_cap)
{
    char psd[1100], png[1100];
    paths(dir, name, psd, png, sizeof psd);
    struct stat st;
    if (stat(psd, &st) != 0) return say(why, why_cap, "%s has no PSD to make it from", name), 0;
    return write_png(dir, name, why, why_cap);
}

int track_masks_load(const char *dir, const char *name, unsigned char **marks, unsigned *w, unsigned *h)
{
    char psd[1100], png[1100];
    paths(dir, name, psd, png, sizeof psd);
    Marks m = { NULL, 0, 0 };
    if (!png_marks(png, &m)) return 0;
    *marks = m.marks, *w = m.w, *h = m.h;
    return 1;
}

static int is_mask_item(const OriginalPublishItem *it) { return strncmp(it->file, "track_masks/", 12) == 0; }

int track_masks_publish_plan(const char *dir, const char *data_dir, const char *stem, OriginalPublish *plan, char *why, size_t why_cap)
{
    char gen[1100];
    snprintf(gen, sizeof gen, "%s/art-generated/track_masks", dir);
    return track_masks_plan_from(gen, data_dir, stem, plan, why, why_cap);
}

int track_masks_plan_from(const char *gen, const char *data_dir, const char *stem, OriginalPublish *plan, char *why, size_t why_cap)
{
    DIR *d = opendir(gen);
    if (d == NULL) return 1;   /* no masks: nothing to publish */
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
        if (!is_png(e->d_name)) continue;
        if (plan->count >= OT_PUBLISH_MAX) return closedir(d), say(why, why_cap, "%s: too many masks to publish", stem), 0;
        OriginalPublishItem *it = &plan->item[plan->count];
        memset(it, 0, sizeof *it);
        snprintf(it->file, sizeof it->file, "track_masks/%s/%s", stem, e->d_name);
        char mine[1200], theirs[1200];
        snprintf(mine, sizeof mine, "%s/%s", gen, e->d_name);
        snprintf(theirs, sizeof theirs, "%s/%s", data_dir, it->file);
        Marks a = { NULL, 0, 0 }, b = { NULL, 0, 0 };
        if (!png_marks(mine, &a)) { closedir(d); return say(why, why_cap, "%s does not read as a PNG", mine), 0; }
        if (!png_marks(theirs, &b)) it->state = OT_NEW, ++plan->added;
        else if (same(&a, &b)) it->state = OT_SAME, ++plan->same;
        else {
            it->state = OT_CHANGED, ++plan->changed;
            if (a.w != b.w || a.h != b.h) {
                it->lines_added = it->lines_removed = -1;   // another size: texels cannot be matched, the text says so
            } else {
                for (size_t i = 0; i < (size_t)a.w * a.h; ++i) {
                    if (a.marks[i] == b.marks[i]) continue;   // a texel that changes class (reflective to lamp) counts as the old mark lost and the new one gained
                    it->lines_added += a.marks[i] != TRACK_MASK_NONE;
                    it->lines_removed += b.marks[i] != TRACK_MASK_NONE;
                }
            }
        }
        free(a.marks), free(b.marks);
        ++plan->count;
    }
    closedir(d);
    return 1;
}

int track_masks_publish(const char *dir, const char *data_dir, const OriginalPublish *plan, int *written, char *why, size_t why_cap)
{
    char gen[1100];
    snprintf(gen, sizeof gen, "%s/art-generated/track_masks", dir);
    return track_masks_publish_from(gen, data_dir, plan, written, why, why_cap);
}

int track_masks_publish_from(const char *gen, const char *data_dir, const OriginalPublish *plan, int *written, char *why, size_t why_cap)
{
    for (int i = 0; i < plan->count; ++i) {
        const OriginalPublishItem *it = &plan->item[i];
        if (!is_mask_item(it) || it->state == OT_SAME) continue;
        char mine[1200], theirs[1200], folder[1200];
        snprintf(mine, sizeof mine, "%s/%s", gen, strrchr(it->file, '/') + 1);
        snprintf(theirs, sizeof theirs, "%s/%s", data_dir, it->file);
        snprintf(folder, sizeof folder, "%s/track_masks", data_dir);
        mkdir(folder, 0777);
        snprintf(folder, sizeof folder, "%s/%.*s", data_dir, (int)(strrchr(it->file, '/') - it->file), it->file);
        mkdir(folder, 0777);
        size_t len;
        uint8_t *b = slurp(mine, &len);
        FILE *f = b != NULL ? fopen(theirs, "wb") : NULL;
        const int ok = f != NULL && fwrite(b, 1, len, f) == len;
        if (f != NULL) fclose(f);
        free(b);
        if (!ok) return say(why, why_cap, "cannot write %s into the game's data", it->file), 0;
        ++*written;
    }
    return 1;
}

const char *track_mask_state_text(TrackMaskState s)
{
    switch (s) {
    case TM_OK: return "ok";
    case TM_NEW: return "new";
    case TM_STALE: return "stale";
    case TM_EDITED: return "edited by hand";
    case TM_PNG_SOURCE: return "PNG is the source";
    case TM_EMPTY: return "no mask painted in the PSD";
    case TM_BAD: return "unreadable PSD";
    case TM_NONE: return "no mask";
    }
    return "";
}
