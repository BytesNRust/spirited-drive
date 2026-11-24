/* The tweak folder and Apply to the game repo (tweak.h). */
#include "tweak.h"

#include "car_table_rewrite.h"
#include "formats/car_paint.h"
#include "mask_changes.h"
#include "original_track.h"
#include "studio/fsio.h"
#include "track_masks.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum { PATH = 4096, LINE = 1024 };

static int say(char *why, size_t cap, const char *fmt, const char *a, const char *b)
{
    if (why != NULL && cap > 0) snprintf(why, cap, fmt, a, b);
    return 0;
}

void tweak_id(const char *object, char *out, size_t cap)
{
    size_t n = 0;
    for (const char *c = object; *c != '\0' && n + 8 < cap; ++c) out[n++] = (char)tolower((unsigned char)*c);
    if (cap > 0) snprintf(out + n, cap - n, "-tweak");
}

static int make_dir(const char *path)
{
    char tmp[PATH];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p != '\0'; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(tmp, 0777) != 0 && errno != EEXIST) return 0;
        *p = '/';
    }
    return mkdir(tmp, 0777) == 0 || errno == EEXIST;
}

static int has_extension(const char *name, const char *ext)
{
    const size_t n = strlen(name), e = strlen(ext);
    return n > e && strcmp(name + n - e, ext) == 0;
}

/* the files of `from` with the extension `ext` copied into `to` (made); 1 also when `from` is not there */
static int copy_files(const char *from, const char *to, const char *ext, char *why, size_t cap)
{
    DIR *d = from != NULL ? opendir(from) : NULL;
    if (d == NULL) return 1;
    if (!make_dir(to)) return closedir(d), say(why, cap, "cannot make %s%s", to, ""), 0;
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
        if (!has_extension(e->d_name, ext)) continue;
        char a[PATH], b[PATH];
        snprintf(a, sizeof a, "%s/%s", from, e->d_name);
        snprintf(b, sizeof b, "%s/%s", to, e->d_name);
        ExchangeError err;
        size_t size = 0;
        uint8_t *data = fsio_read_file(a, &size, &err);
        const int ok = data != NULL && fsio_write_file(b, data, size, 1, &err);
        free(data);
        if (!ok) return closedir(d), say(why, cap, "cannot copy %s into %s", a, to), 0;
    }
    closedir(d);
    return 1;
}

static int write_text(const char *path, const char *text)
{
    ExchangeError err;
    return fsio_write_file(path, (const uint8_t *)text, strlen(text), 1, &err);
}

static int folder_of(const char *dir, const char *sub, char *out, size_t cap)
{
    return snprintf(out, cap, "%s/%s", dir, sub) < (int)cap;
}

static int studio_psds(const char *dir, const char *psd_dir, char *why, size_t cap)
{
    char to[PATH];
    return folder_of(dir, "studio/psd", to, sizeof to) && copy_files(psd_dir, to, ".psd", why, cap);
}

int tweak_make_car(const char *dir, const ExportCar *car, const char *masks_dir, const char *dash_dir, const char *psd_dir, const ModPaint *paints, int paint_count, char *why, size_t cap)
{
    char to[PATH], text[LINE], id[64];
    tweak_id(car->folder, id, sizeof id);
    if (!make_dir(dir)) return say(why, cap, "cannot make %s%s", dir, "");
    snprintf(text, sizeof text, "id=%s\nname=%s tweak\nversion=1\ntype=tweak\n", id, car->label);
    if (!folder_of(dir, "mod.ini", to, sizeof to) || !write_text(to, text)) return say(why, cap, "cannot write %s%s", to, "");
    if (!folder_of(dir, "car_masks", to, sizeof to) || snprintf(to + strlen(to), sizeof to - strlen(to), "/%s", car->folder) >= (int)sizeof to) return 0;
    if (!copy_files(masks_dir, to, ".png", why, cap)) return 0;
    if (car->kind == EXPORT_DRIVABLE && dash_dir != NULL) {
        if (!folder_of(dir, "dash_masks", to, sizeof to) || snprintf(to + strlen(to), sizeof to - strlen(to), "/%s", car->file) >= (int)sizeof to) return 0;
        if (!copy_files(dash_dir, to, ".png", why, cap)) return 0;
    }
    if (!studio_psds(dir, psd_dir, why, cap)) return 0;
    snprintf(text, sizeof text, "kind=car\nfolder=%s\nfile=%s\nmodel=%d\ncolors=%d\n", car->folder, car->file, car->model, car->kind == EXPORT_DRIVABLE ? 1 : 0);
    if (!folder_of(dir, "studio/target.txt", to, sizeof to) || !write_text(to, text)) return say(why, cap, "cannot write %s%s", to, "");
    if (car->kind == EXPORT_DRIVABLE && paint_count > 0) {
        char value[2048], err[160];
        if (!mod_ini_paints_format(paints, paint_count, value, sizeof value, err, sizeof err)) return say(why, cap, "the colors do not format: %s%s", err, "");
        char body[2100];
        snprintf(body, sizeof body, "%s\n", value);
        if (!folder_of(dir, "studio/paints.txt", to, sizeof to) || !write_text(to, body)) return say(why, cap, "cannot write %s%s", to, "");
    }
    return 1;
}

int tweak_make_track(const char *dir, const char *stem, const char *light_file, const char *masks_dir, const char *psd_dir, char *why, size_t cap)
{
    char to[PATH], text[LINE], id[64];
    tweak_id(stem, id, sizeof id);
    if (!make_dir(dir)) return say(why, cap, "cannot make %s%s", dir, "");
    snprintf(text, sizeof text, "id=%s\nname=%s tweak\nversion=1\ntype=tweak\n", id, stem);
    if (!folder_of(dir, "mod.ini", to, sizeof to) || !write_text(to, text)) return say(why, cap, "cannot write %s%s", to, "");
    if (light_file != NULL) {
        ExchangeError err;
        size_t size = 0;
        uint8_t *data = fsio_read_file(light_file, &size, &err);
        char dest[PATH];
        snprintf(dest, sizeof dest, "%s/track_light/%s.txt", dir, stem);
        const int ok = data != NULL && fsio_write_file(dest, data, size, 1, &err);
        free(data);
        if (!ok) return say(why, cap, "cannot copy %s into the tweak%s", light_file, "");
    }
    snprintf(to, sizeof to, "%s/track_masks/%s", dir, stem);
    if (!copy_files(masks_dir, to, ".png", why, cap)) return 0;
    if (!studio_psds(dir, psd_dir, why, cap)) return 0;
    snprintf(text, sizeof text, "kind=track\nstem=%s\n", stem);
    if (!folder_of(dir, "studio/target.txt", to, sizeof to) || !write_text(to, text)) return say(why, cap, "cannot write %s%s", to, "");
    return 1;
}

/* ---- Apply ---- */

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static char *read_text(const char *path)
{
    ExchangeError err;
    size_t size = 0;
    uint8_t *data = fsio_read_file(path, &size, &err);
    if (data == NULL) return NULL;
    char *text = realloc(data, size + 1);
    if (text == NULL) return free(data), NULL;
    text[size] = '\0';
    return text;
}

/* the value of `key=` in a studio/target.txt text, "" when there is none */
static void target_value(const char *text, const char *key, char *out, size_t cap)
{
    const size_t n = strlen(key);
    out[0] = '\0';
    for (const char *line = text; line != NULL && *line != '\0'; line = strchr(line, '\n') != NULL ? strchr(line, '\n') + 1 : NULL) {
        if (strncmp(line, key, n) != 0 || line[n] != '=') continue;
        const char *end = strchr(line + n + 1, '\n');
        snprintf(out, cap, "%.*s", (int)(end != NULL ? (size_t)(end - line - n - 1) : strlen(line + n + 1)), line + n + 1);
        return;
    }
}

static void table_path(const char *repo_data, char *out, size_t cap)
{
    char root[PATH];
    snprintf(root, sizeof root, "%s", repo_data);
    size_t n = strlen(root);
    while (n > 1 && root[n - 1] == '/') root[--n] = '\0';
    char *slash = strrchr(root, '/');
    if (slash != NULL) *slash = '\0';
    snprintf(out, cap, "%s/core/formats/car_paint.c", root);
}

/* the colors of studio/paints.txt into the car's row of the repo's table, as Copy's colors_state and app_car_copy do; 1, or 0 with `why` */
static int apply_colors(const char *dir, const char *repo_data, const char *folder, int model, TweakApplied *out, char *why, size_t cap)
{
    char path[PATH];
    snprintf(path, sizeof path, "%s/studio/paints.txt", dir);
    char *value = read_text(path);
    if (value == NULL) return 1;   /* no colors in this tweak */
    ModPaint paints[MOD_INI_PAINTS_MAX];
    int count = 0;
    char err[160] = "";
    value[strcspn(value, "\n")] = '\0';
    const int parsed = mod_ini_paints_parse(value, paints, &count, err, sizeof err);
    free(value);
    if (!parsed) return say(why, cap, "studio/paints.txt: %s%s", err, "");
    if (count > CAR_TABLE_MAX_PAINTS) return out->colors_skipped = count, 1;   /* the table holds up to CAR_PAINT_MAX: the colors are not compared */
    table_path(repo_data, path, sizeof path);
    ExchangeError xerr;
    size_t len = 0;
    uint8_t *src = fsio_read_file(path, &len, &xerr);
    if (src == NULL) {   /* not a game repo's data/ (a folder to try things in): fine while the colors are the ones the game has */
        int same = count == car_paint_count(model);
        for (int k = 0; same && k < count; k++) same = strcmp(paints[k].name, car_paint(model, k)->name) == 0 && memcmp(paints[k].rgb, car_paint(model, k)->rgb, 3) == 0;
        return same ? 1 : say(why, cap, "the colors are not the game's and %s is not there: the game repo's data/ must sit in the repo (core/formats/car_paint.c is rewritten)", path, "");
    }
    char before[CAR_TABLE_LIST], after[CAR_TABLE_LIST], msg[256] = "";
    int changed = 0;
    char *text = car_table_rewrite((const char *)src, len, folder, paints, count, &changed, before, after, msg, sizeof msg);
    free(src);
    if (text == NULL) return say(why, cap, "%s%s", msg, "");
    if (changed) {
        char scratch[PATH + 8];
        snprintf(scratch, sizeof scratch, "%s.new", path);
        FILE *f = fopen(scratch, "wb");
        const int ok = f != NULL && fputs(text, f) >= 0;
        if (f != NULL && fclose(f) != 0) { remove(scratch); free(text); return say(why, cap, "cannot write %s%s", path, ""); }
        if (!ok || rename(scratch, path) != 0) { remove(scratch); free(text); return say(why, cap, "cannot write %s%s", path, ""); }
        out->colors = 1;
    }
    free(text);
    return 1;
}

static int apply_masks(const char *from, const char *to, int dash, int *written, char *why, size_t cap)
{
    static MaskChanges changes;
    changes.count = changes.changed = changes.added = changes.unchanged = changes.missing = 0;
    if (!(dash ? mask_changes_compare_dash(from, to, &changes, why, cap) : mask_changes_compare(from, to, &changes, why, cap))) return 0;
    return mask_changes_publish(from, to, &changes, written, why, cap);
}

int tweak_apply(const char *dir, const char *repo_data, TweakApplied *out, char *why, size_t cap)
{
    char path[PATH], target[LINE], kind[16], folder[64], file[64], stem[16], model[8], from[PATH], to[PATH];
    memset(out, 0, sizeof *out);
    snprintf(path, sizeof path, "%s/studio/target.txt", dir);
    char *text = read_text(path);
    if (text == NULL) return say(why, cap, "%s is not a tweak made by Studio (no studio/target.txt)", dir, "");
    snprintf(target, sizeof target, "%s", text);
    free(text);
    target_value(target, "kind", kind, sizeof kind);
    if (strcmp(kind, "car") == 0) {
        target_value(target, "folder", folder, sizeof folder), target_value(target, "file", file, sizeof file), target_value(target, "model", model, sizeof model);
        snprintf(from, sizeof from, "%s/car_masks/%s", dir, folder);
        snprintf(to, sizeof to, "%s/car_masks/%s", repo_data, folder);
        if (!apply_colors(dir, repo_data, folder, atoi(model), out, why, cap)) return 0;   /* first: a table that cannot take the colors stops it before any mask is written */
        snprintf(from, sizeof from, "%s/car_masks/%s", dir, folder);
        if (is_dir(from) && !apply_masks(from, to, 0, &out->masks, why, cap)) return 0;
        snprintf(from, sizeof from, "%s/dash_masks/%s", dir, file);
        snprintf(to, sizeof to, "%s/dash_masks/%s", repo_data, file);
        if (is_dir(from) && !apply_masks(from, to, 1, &out->dash, why, cap)) return 0;
        return 1;
    }
    if (strcmp(kind, "track") == 0) {
        target_value(target, "stem", stem, sizeof stem);
        snprintf(from, sizeof from, "%s/track_light/%s.txt", dir, stem);
        struct stat st;
        if (stat(from, &st) == 0 && !original_track_light_publish(from, stem, repo_data, &out->light, why, cap)) return 0;
        snprintf(from, sizeof from, "%s/track_masks/%s", dir, stem);
        if (!is_dir(from)) return 1;
        static OriginalPublish plan;
        memset(&plan, 0, sizeof plan);
        return track_masks_plan_from(from, repo_data, stem, &plan, why, cap) && track_masks_publish_from(from, repo_data, &plan, &out->track_masks, why, cap);
    }
    return say(why, cap, "%s: studio/target.txt says neither a car nor a track%s", dir, "");
}

/* ---- status and check ---- */

static void add_item(TweakStatus *s, const char *file, TweakState state, const char *note)
{
    if (s->count >= TWEAK_FILES) return;
    TweakFile *f = &s->item[s->count++];
    snprintf(f->file, sizeof f->file, "%s", file);
    f->state = state;
    snprintf(f->note, sizeof f->note, "%s", note != NULL ? note : "");
    if (state == TWEAK_SAME) ++s->same;
    else ++s->changed;
}

static void masks_items(TweakStatus *s, const char *prefix, const MaskChanges *c)
{
    for (int i = 0; i < c->count; ++i) {
        const MaskChange *m = &c->item[i];
        char name[TWEAK_NAME], note[TWEAK_NOTE] = "";
        if (m->kind == MC_MISSING) continue;   /* only the game has it: the tweak does not carry it */
        snprintf(name, sizeof name, "%s/%s", prefix, m->file);
        if (m->kind == MC_CHANGED) snprintf(note, sizeof note, "%d texels", m->texels);
        add_item(s, name, m->kind == MC_UNCHANGED ? TWEAK_SAME : m->kind == MC_NEW ? TWEAK_NEW : TWEAK_CHANGED, note);
    }
}

static void plan_items(TweakStatus *s, const OriginalPublish *p)
{
    for (int i = 0; i < p->count; ++i) {
        const OriginalPublishItem *it = &p->item[i];
        const int mask = strncmp(it->file, "track_masks/", 12) == 0;
        char note[TWEAK_NOTE] = "";
        if (it->state == OT_CHANGED && it->lines_added < 0) snprintf(note, sizeof note, "size differs from the game's");
        else if (it->state == OT_CHANGED) snprintf(note, sizeof note, "+%d -%d %s", it->lines_added, it->lines_removed, mask ? "texels" : "lines");
        add_item(s, it->file, it->state == OT_SAME ? TWEAK_SAME : it->state == OT_NEW ? TWEAK_NEW : TWEAK_CHANGED, note);
    }
}

static int colors_same_as_game(const char *dir, int model)
{
    char path[PATH];
    snprintf(path, sizeof path, "%s/studio/paints.txt", dir);
    char *value = read_text(path);
    if (value == NULL) return -1;   /* no colors in the tweak */
    ModPaint paints[MOD_INI_PAINTS_MAX];
    int count = 0;
    char err[160] = "";
    value[strcspn(value, "\n")] = '\0';
    const int parsed = mod_ini_paints_parse(value, paints, &count, err, sizeof err);
    free(value);
    if (!parsed) return 0;
    int same = count == car_paint_count(model);
    for (int k = 0; same && k < count; k++) same = strcmp(paints[k].name, car_paint(model, k)->name) == 0 && memcmp(paints[k].rgb, car_paint(model, k)->rgb, 3) == 0;
    return same;
}

static void summarize(TweakStatus *s)
{
    int files = 0, colors = 0;
    for (int i = 0; i < s->count; ++i) {
        if (s->item[i].state == TWEAK_SAME) continue;
        if (strcmp(s->item[i].file, "colors") == 0) colors = 1;
        else ++files;
    }
    if (files == 0 && !colors) {
        snprintf(s->summary, sizeof s->summary, "same as the game");
        return;
    }
    const char *what = strcmp(s->target, "car") == 0 ? "mask" : "file";
    char head[80] = "";
    if (files > 0) snprintf(head, sizeof head, "changed %d %s%s", files, what, files == 1 ? "" : "s");
    snprintf(s->summary, sizeof s->summary, "%s%s%s", head, files > 0 && colors ? ", " : colors && files == 0 ? "changed " : "", colors ? "colors" : "");
}

static void mark_own(const char *dir, const char *repo_data, TweakStatus *s);

int tweak_status(const char *dir, const char *base_data, TweakStatus *out, char *why, size_t cap)
{
    char path[PATH], target[LINE], folder[64], file[64], stem[16], model[8], from[PATH], to[PATH];
    memset(out, 0, sizeof *out);
    snprintf(path, sizeof path, "%s/studio/target.txt", dir);
    char *text = read_text(path);
    if (text == NULL) return say(why, cap, "%s is not a tweak made by Studio (no studio/target.txt)", dir, "");
    snprintf(target, sizeof target, "%s", text);
    free(text);
    target_value(target, "kind", out->target, sizeof out->target);
    if (base_data == NULL || !is_dir(base_data)) return say(why, cap, "the game's data/ is not there to compare with: %s", base_data != NULL ? base_data : "(none)", "");
    if (strcmp(out->target, "car") == 0) {
        static MaskChanges changes;
        target_value(target, "folder", folder, sizeof folder), target_value(target, "file", file, sizeof file), target_value(target, "model", model, sizeof model);
        snprintf(out->object, sizeof out->object, "%s", folder);
        snprintf(from, sizeof from, "%s/car_masks/%s", dir, folder);
        snprintf(to, sizeof to, "%s/car_masks/%s", base_data, folder);
        if (is_dir(from)) {
            memset(&changes, 0, sizeof changes);
            if (!mask_changes_compare(from, to, &changes, why, cap)) return 0;
            snprintf(from, sizeof from, "car_masks/%s", folder);
            masks_items(out, from, &changes);
        }
        snprintf(from, sizeof from, "%s/dash_masks/%s", dir, file);
        snprintf(to, sizeof to, "%s/dash_masks/%s", base_data, file);
        if (is_dir(from)) {
            memset(&changes, 0, sizeof changes);
            if (!mask_changes_compare_dash(from, to, &changes, why, cap)) return 0;
            snprintf(from, sizeof from, "dash_masks/%s", file);
            masks_items(out, from, &changes);
        }
        const int colors = colors_same_as_game(dir, atoi(model));
        if (colors >= 0) add_item(out, "colors", colors ? TWEAK_SAME : TWEAK_CHANGED, colors ? "" : "go into core/formats/car_paint.c");
    } else if (strcmp(out->target, "track") == 0) {
        static OriginalPublish plan;
        target_value(target, "stem", stem, sizeof stem);
        snprintf(out->object, sizeof out->object, "%s", stem);
        snprintf(from, sizeof from, "%s/track_light/%s.txt", dir, stem);
        struct stat st;
        if (stat(from, &st) == 0) {
            if (!original_track_light_plan(from, stem, base_data, &plan, why, cap)) return 0;
            plan_items(out, &plan);
        }
        snprintf(from, sizeof from, "%s/track_masks/%s", dir, stem);
        if (is_dir(from)) {
            memset(&plan, 0, sizeof plan);
            if (!track_masks_plan_from(from, base_data, stem, &plan, why, cap)) return 0;
            plan_items(out, &plan);
        }
    } else {
        return say(why, cap, "%s: studio/target.txt says neither a car nor a track%s", dir, "");
    }
    mark_own(dir, base_data, out);
    summarize(out);
    return 1;
}

int tweak_check(const char *dir, char *report, size_t cap)
{
    char path[PATH], target[LINE], kind[16], ini_why[160] = "";
    snprintf(path, sizeof path, "%s/mod.ini", dir);
    ExchangeError err;
    size_t size = 0;
    uint8_t *ini = fsio_read_file(path, &size, &err);
    if (ini == NULL) return say(report, cap, "no mod.ini in %s", dir, "");
    ModInfo info;
    const int parsed = mod_ini_parse(ini, size, &info, ini_why, sizeof ini_why);
    free(ini);
    if (!parsed) return say(report, cap, "mod.ini: %s%s", ini_why, "");
    if (info.type != MOD_TYPE_TWEAK) return say(report, cap, "mod.ini is not type=tweak%s%s", "", "");
    snprintf(path, sizeof path, "%s/studio/target.txt", dir);
    char *text = read_text(path);
    if (text == NULL) return say(report, cap, "no studio/target.txt in %s", dir, "");
    snprintf(target, sizeof target, "%s", text);
    free(text);
    target_value(target, "kind", kind, sizeof kind);
    const int car = strcmp(kind, "car") == 0;
    if (!car && strcmp(kind, "track") != 0) return say(report, cap, "studio/target.txt names no car or track%s%s", "", "");
    char object[64];
    target_value(target, car ? "folder" : "stem", object, sizeof object);
    if (object[0] == '\0') return say(report, cap, "studio/target.txt has no %s", car ? "folder" : "stem", "");
    /* every folder under the tweak's data/ paths is the object's: a stray one would be applied to another car or track */
    static const char *const SUBS[] = { "car_masks", "dash_masks", "track_masks" };
    char own_dash[64];
    target_value(target, "file", own_dash, sizeof own_dash);
    for (int i = 0; i < 3; ++i) {
        char sub[PATH];
        snprintf(sub, sizeof sub, "%s/%s", dir, SUBS[i]);
        DIR *d = opendir(sub);
        for (struct dirent *e; d != NULL && (e = readdir(d)) != NULL;) {
            if (e->d_name[0] == '.') continue;
            const char *own = i == 1 ? own_dash : object;
            if (strcmp(e->d_name, own) != 0) {
                snprintf(report, cap, "%s/%s is for another object than the tweak's (%s)", SUBS[i], e->d_name, own);
                closedir(d);
                return 0;
            }
        }
        if (d != NULL) closedir(d);
    }
    snprintf(path, sizeof path, "%s/track_light", dir);
    DIR *d = opendir(path);
    char own_light[80];
    snprintf(own_light, sizeof own_light, "%s.txt", object);
    for (struct dirent *e; d != NULL && (e = readdir(d)) != NULL;) {
        if (e->d_name[0] != '.' && strcmp(e->d_name, own_light) != 0) {
            snprintf(report, cap, "track_light/%s is for another track than the tweak's (%s)", e->d_name, object);
            closedir(d);
            return 0;
        }
    }
    if (d != NULL) closedir(d);
    snprintf(report, cap, "ok");
    return 1;
}

/* ---- the game's fingerprints and Apply that skips the repo's own changes ---- */

#include <ftw.h>
#include <unistd.h>

enum { REL_MAX = 400 };

typedef struct {
    char rel[REL_MAX][TWEAK_NAME];
    int count;
} Rels;

static void rels_add(Rels *r, const char *rel)
{
    if (r->count < REL_MAX) snprintf(r->rel[r->count++], TWEAK_NAME, "%s", rel);
}

static void rels_files(Rels *r, const char *dir, const char *sub, const char *ext)   /* sub/<file>.ext */
{
    char path[PATH];
    snprintf(path, sizeof path, "%s/%s", dir, sub);
    DIR *d = opendir(path);
    for (struct dirent *e; d != NULL && (e = readdir(d)) != NULL;) {
        char rel[TWEAK_NAME];
        if (!has_extension(e->d_name, ext) || snprintf(rel, sizeof rel, "%s/%s", sub, e->d_name) >= (int)sizeof rel) continue;
        rels_add(r, rel);
    }
    if (d != NULL) closedir(d);
}

/* every file of the tweak that Apply reads: the objects' folders below car_masks, dash_masks, track_masks, and track_light's files */
static void rels_of(const char *dir, Rels *r)
{
    static const char *const SUBS[] = { "car_masks", "dash_masks", "track_masks" };
    memset(r, 0, sizeof *r);
    for (int i = 0; i < 3; ++i) {
        char path[PATH];
        snprintf(path, sizeof path, "%s/%s", dir, SUBS[i]);
        DIR *d = opendir(path);
        for (struct dirent *e; d != NULL && (e = readdir(d)) != NULL;) {
            char sub[TWEAK_NAME];
            if (e->d_name[0] == '.' || snprintf(sub, sizeof sub, "%s/%s", SUBS[i], e->d_name) >= (int)sizeof sub) continue;
            rels_files(r, dir, sub, ".png");
        }
        if (d != NULL) closedir(d);
    }
    rels_files(r, dir, "track_light", ".txt");
}

static int fingerprint(const char *path, unsigned long long *out)   /* FNV-1a 64 of the file; 0 when it is not there */
{
    ExchangeError err;
    size_t size = 0;
    uint8_t *data = fsio_read_file(path, &size, &err);
    if (data == NULL) return 0;
    unsigned long long h = 1469598103934665603ULL;
    for (size_t i = 0; i < size; ++i) h = (h ^ data[i]) * 1099511628211ULL;
    free(data);
    *out = h;
    return 1;
}

int tweak_base_write(const char *dir, const char *base_data, char *why, size_t cap)
{
    char path[PATH], body[REL_MAX * (TWEAK_NAME + 24)];
    struct stat st;
    snprintf(path, sizeof path, "%s/studio/base.txt", dir);
    if (stat(path, &st) == 0) return 1;
    static Rels rels;
    rels_of(dir, &rels);
    size_t n = 0;
    body[0] = '\0';
    for (int i = 0; i < rels.count && n + TWEAK_NAME + 24 < sizeof body; ++i) {
        char file[PATH];
        unsigned long long h;
        snprintf(file, sizeof file, "%s/%s", base_data, rels.rel[i]);
        if (fingerprint(file, &h)) n += (size_t)snprintf(body + n, sizeof body - n, "%s\t%016llx\n", rels.rel[i], h);
        else n += (size_t)snprintf(body + n, sizeof body - n, "%s\t-\n", rels.rel[i]);
    }
    if (!write_text(path, body)) return say(why, cap, "cannot write %s%s", path, "");
    return 1;
}

/* the fingerprint studio/base.txt has for `rel`: 1 with it in `h`, 2 when the game had no such file, 0 when the file says nothing about it */
static int base_of(const char *text, const char *rel, unsigned long long *h)
{
    const size_t n = strlen(rel);
    for (const char *line = text; line != NULL && *line != '\0'; line = strchr(line, '\n') != NULL ? strchr(line, '\n') + 1 : NULL) {
        if (strncmp(line, rel, n) != 0 || line[n] != '\t') continue;
        if (line[n + 1] == '-') return 2;
        *h = strtoull(line + n + 1, NULL, 16);
        return 1;
    }
    return 0;
}

/* marks the items of `s` whose repo copy is neither the base's nor... (the item is changed or new against the repo, and the repo's file is not the one the tweak was made from) */
static void mark_own(const char *dir, const char *repo_data, TweakStatus *s)
{
    char path[PATH];
    snprintf(path, sizeof path, "%s/studio/base.txt", dir);
    char *base = read_text(path);
    if (base == NULL) return;
    for (int i = 0; i < s->count; ++i) {
        TweakFile *f = &s->item[i];
        unsigned long long was, now;
        char repo[PATH];
        if (f->state == TWEAK_SAME || strcmp(f->file, "colors") == 0) continue;
        snprintf(repo, sizeof repo, "%s/%s", repo_data, f->file);
        const int known = base_of(base, f->file, &was);
        if (known == 0 || !fingerprint(repo, &now)) continue;   /* nothing known, or the repo has no such file: nothing of its own */
        if (known == 2 || now != was) f->own = 1, ++s->own;
    }
    free(base);
}

static int remove_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st, (void)flag, (void)ftw;
    return remove(path);
}

static int copy_one(const char *from, const char *to)
{
    ExchangeError err;
    size_t size = 0;
    uint8_t *data = fsio_read_file(from, &size, &err);
    const int ok = data != NULL && fsio_write_file(to, data, size, 1, &err);
    free(data);
    return ok;
}

static int apply_inner(const char *dir, const char *repo_data, TweakApplied *out, char *why, size_t cap)
{
    static TweakStatus status;
    if (!tweak_status(dir, repo_data, &status, why, cap)) return 0;
    if (status.own == 0) {
        if (!tweak_apply(dir, repo_data, out, why, cap)) return 0;
        char base[PATH];
        snprintf(base, sizeof base, "%s/studio/base.txt", dir);
        remove(base);   /* the repo is the tweak's now: the next base is the repo as it is then */
        return 1;
    }
    char tmp[PATH];
    const char *root = getenv("TMPDIR");
    snprintf(tmp, sizeof tmp, "%s/tweak-skip-XXXXXX", root != NULL && root[0] != '\0' ? root : "/tmp");
    if (mkdtemp(tmp) == NULL) return say(why, cap, "cannot make a folder to apply from in %s%s", tmp, "");
    int ok = 1;
    static const char *const KEEP[] = { "studio/target.txt", "studio/paints.txt" };
    for (int i = 0; i < 2 && ok; ++i) {
        char from[PATH], to[PATH];
        struct stat st;
        snprintf(from, sizeof from, "%s/%s", dir, KEEP[i]);
        snprintf(to, sizeof to, "%s/%s", tmp, KEEP[i]);
        if (stat(from, &st) == 0) ok = copy_one(from, to);
    }
    for (int i = 0; i < status.count && ok; ++i) {
        char from[PATH], to[PATH];
        if (status.item[i].own || strcmp(status.item[i].file, "colors") == 0) continue;
        snprintf(from, sizeof from, "%s/%s", dir, status.item[i].file);
        snprintf(to, sizeof to, "%s/%s", tmp, status.item[i].file);
        ok = copy_one(from, to);
    }
    if (!ok) say(why, cap, "cannot copy the tweak's files to apply them%s%s", "", "");
    else ok = tweak_apply(tmp, repo_data, out, why, cap);
    nftw(tmp, remove_entry, 16, FTW_DEPTH | FTW_PHYS);
    if (ok) out->skipped = status.own;
    return ok;
}

/* ---- all or nothing ---- */

typedef struct {
    char path[PATH];
    uint8_t *data;   /* the file as the repo had it; NULL when it was not there */
    size_t size;
} Backup;

int tweak_apply_skipping(const char *dir, const char *repo_data, TweakApplied *out, char *why, size_t cap)
{
    static TweakStatus status;
    if (!tweak_status(dir, repo_data, &status, why, cap)) return 0;
    /* what Apply may write, kept as it is now: a failure anywhere puts every one of them back */
    Backup *kept = calloc((size_t)status.count + 1, sizeof *kept);
    if (kept == NULL) return say(why, cap, "out of memory%s%s", "", "");
    int n = 0;
    for (int i = 0; i < status.count; ++i) {
        if (status.item[i].state == TWEAK_SAME || status.item[i].own) continue;
        if (strcmp(status.item[i].file, "colors") == 0) table_path(repo_data, kept[n].path, sizeof kept[n].path);
        else snprintf(kept[n].path, sizeof kept[n].path, "%s/%s", repo_data, status.item[i].file);
        ExchangeError err;
        kept[n].data = fsio_read_file(kept[n].path, &kept[n].size, &err);
        ++n;
    }
    const int ok = apply_inner(dir, repo_data, out, why, cap);
    if (!ok) {
        int bad = 0;
        for (int i = 0; i < n; ++i) {
            ExchangeError err;
            size_t now_size = 0;
            uint8_t *now = fsio_read_file(kept[i].path, &now_size, &err);
            const int untouched = kept[i].data != NULL ? now != NULL && now_size == kept[i].size && memcmp(now, kept[i].data, now_size) == 0 : now == NULL;
            free(now);
            if (untouched) continue;   /* the write that failed never changed it */
            if (kept[i].data != NULL) bad += !fsio_write_file(kept[i].path, kept[i].data, kept[i].size, 1, &err);
            else remove(kept[i].path);
        }
        if (why != NULL && cap > 0) {
            const size_t used = strlen(why);
            snprintf(why + used, cap - used, bad == 0 ? " (nothing was written: the repo is as it was)" : " (%d files could not be put back: check the repo)", bad);
        }
    }
    for (int i = 0; i < n; ++i) free(kept[i].data);
    free(kept);
    return ok;
}
