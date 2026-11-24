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
        if (is_dir(from) && !apply_masks(from, to, 0, &out->masks, why, cap)) return 0;
        snprintf(from, sizeof from, "%s/dash_masks/%s", dir, file);
        snprintf(to, sizeof to, "%s/dash_masks/%s", repo_data, file);
        if (is_dir(from) && !apply_masks(from, to, 1, &out->dash, why, cap)) return 0;
        return apply_colors(dir, repo_data, folder, atoi(model), out, why, cap);
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
