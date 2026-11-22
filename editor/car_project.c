#include "car_project.h"

#include "formats/car_kit_paths.h"

#include <dirent.h>
#include <stdarg.h>
#include <strings.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_file(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int join(char *out, size_t cap, const char *dir, const char *rest)
{
    const int n = snprintf(out, cap, "%s/%s", dir, rest);
    return n > 0 && (size_t)n < cap;
}

static int refuse(char *why, size_t cap, const char *format, ...) __attribute__((format(printf, 3, 4)));
static int refuse(char *why, size_t cap, const char *format, ...)
{
    if (why != NULL && cap > 0) {
        va_list args;
        va_start(args, format);
        vsnprintf(why, cap, format, args);
        va_end(args);
    }
    return 0;
}

/* the last component of `path`, without trailing slashes */
static void base_name(const char *path, char *out, size_t cap)
{
    size_t n = strlen(path);
    while (n > 1 && path[n - 1] == '/') --n;
    size_t start = n;
    while (start > 0 && path[start - 1] != '/') --start;
    const size_t len = n - start < cap - 1 ? n - start : cap - 1;
    memcpy(out, path + start, len);
    out[len] = '\0';
}

static int by_name(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

/* the names of the files in `dir` that end in ".png" (in any case), sorted: the count of all of them, the first CAR_PROJECT_LIST kept in `names` */
static int list_png(const char *dir, char names[CAR_PROJECT_LIST][CAR_PROJECT_NAME])
{
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    static char all[512][CAR_PROJECT_NAME];   /* more than a kit holds; past it the count still goes on */
    int count = 0, kept = 0;
    for (struct dirent *e; (e = readdir(d)) != NULL;) {
        const size_t n = strlen(e->d_name);
        if (n < 5 || n >= CAR_PROJECT_NAME || strcasecmp(e->d_name + n - 4, ".png") != 0 || e->d_name[0] == '.') continue;
        count++;
        if (kept < (int)(sizeof all / sizeof all[0])) snprintf(all[kept++], CAR_PROJECT_NAME, "%s", e->d_name);
    }
    closedir(d);
    qsort(all, (size_t)kept, CAR_PROJECT_NAME, by_name);
    for (int i = 0; i < kept && i < CAR_PROJECT_LIST; i++) memcpy(names[i], all[i], CAR_PROJECT_NAME);
    return count;
}

/* 1 when `dir` holds a mod.ini */
static int has_card(const char *dir)
{
    char ini[CAR_PROJECT_PATH + 16];
    return join(ini, sizeof ini, dir, "mod.ini") && is_file(ini);
}

/* 1 when `dir` holds an old-layout coche/mod.ini */
static int has_old_card(const char *dir)
{
    char ini[CAR_PROJECT_PATH + 16];
    return join(ini, sizeof ini, dir, "coche/mod.ini") && is_file(ini);
}

/* 1 when `dir` is the coche/ of an old kit: a folder named coche with a modelo/ beside it */
static int is_old_kit_card_folder(const char *dir)
{
    char up[CAR_PROJECT_PATH], name[CAR_PROJECT_NAME], modelo[CAR_PROJECT_PATH + 16];
    base_name(dir, name, sizeof name);
    snprintf(up, sizeof up, "%s", dir);
    char *slash = strrchr(up, '/');
    if (strcmp(name, "coche") != 0 || slash == NULL || slash == up) return 0;
    *slash = '\0';
    return join(modelo, sizeof modelo, up, "modelo") && is_dir(modelo);
}

/* 1 when `dir` is a kit's folder: a mod.ini and a model/ */
static int is_kit_folder(const char *dir)
{
    char model[CAR_PROJECT_PATH + 16];
    return has_card(dir) && join(model, sizeof model, dir, car_kit_path(CAR_KIT_MODEL)) && is_dir(model);
}

/* the value of `key` in an ini text of `len` bytes (the first line "key=value", the value trimmed): 1 with it in `out` */
static int ini_get(const char *text, size_t len, const char *key, char *out, size_t cap)
{
    const size_t klen = strlen(key);
    for (const char *p = text, *end = text + len; p < end;) {
        const char *nl = memchr(p, '\n', (size_t)(end - p)), *line_end = nl != NULL ? nl : end;
        if ((size_t)(line_end - p) > klen && memcmp(p, key, klen) == 0 && p[klen] == '=') {
            const char *v = p + klen + 1, *ve = line_end;
            while (ve > v && (ve[-1] == '\r' || ve[-1] == ' ' || ve[-1] == '\t')) --ve;
            while (v < ve && (*v == ' ' || *v == '\t')) ++v;
            if ((size_t)(ve - v) >= cap) return 0;
            memcpy(out, v, (size_t)(ve - v));
            out[ve - v] = '\0';
            return 1;
        }
        p = nl != NULL ? nl + 1 : end;
    }
    return 0;
}

/* dir/project.ini's bytes into `buf` (NUL terminated); 0 when there is none or it is over `cap` - 1 bytes */
static int read_project_ini(const char *dir, char *buf, size_t cap, size_t *len)
{
    char path[CAR_PROJECT_PATH + 16];
    if (!join(path, sizeof path, dir, "project.ini")) return 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    *len = fread(buf, 1, cap - 1, f);
    const int over = fgetc(f) != EOF;
    fclose(f);
    buf[*len] = '\0';
    return !over;
}

int car_project_is_masks(const char *dir)
{
    char text[4096], kind[16];
    size_t len = 0;
    return dir != NULL && dir[0] != '\0' && read_project_ini(dir, text, sizeof text, &len) && ini_get(text, len, "kind", kind, sizeof kind) && strcmp(kind, "masks") == 0;
}

/* the folder of `parent` named `name` in any case, as the disk spells it; 0 when there is none */
static int find_folder(const char *parent, const char *name, char *out, size_t cap)
{
    DIR *d = opendir(parent);
    if (d == NULL) return 0;
    int found = 0;
    for (struct dirent *e; !found && (e = readdir(d)) != NULL;) {
        char path[CAR_PROJECT_PATH + 64];
        if (strcasecmp(e->d_name, name) != 0 || !join(path, sizeof path, parent, e->d_name) || !is_dir(path)) continue;
        found = snprintf(out, cap, "%s", e->d_name) < (int)cap;
    }
    closedir(d);
    return found;
}

/* a masks project: its project.ini says which folder of car_masks/ holds the masks and which file of original/ is the car */
static int detect_masks(const char *given, const char *name, CarProject *out, char *why, size_t why_cap)
{
    char text[4096], value[CAR_PROJECT_PATH], folder[CAR_PROJECT_NAME];
    size_t len = 0;
    if (!read_project_ini(given, text, sizeof text, &len)) return refuse(why, why_cap, "project.ini in %s cannot be read (it is over %zu bytes or unreadable)", name, sizeof text - 1);
    if (!ini_get(text, len, "masks", folder, sizeof folder) || folder[0] == '\0' || strchr(folder, '/') != NULL || folder[0] == '.')
        return refuse(why, why_cap, "project.ini in %s says kind=masks but has no masks= folder name (the folder of car_masks/ that holds the masks)", name);
    out->masks_only = 1;
    snprintf(out->piece, sizeof out->piece, "%s", given);
    snprintf(out->root, sizeof out->root, "%s", given);
    snprintf(out->kit, sizeof out->kit, "%s", name);
    snprintf(out->folder, sizeof out->folder, "%s", folder);
    if (ini_get(text, len, "id", value, sizeof value)) snprintf(out->info.id, sizeof out->info.id, "%s", value);
    if (ini_get(text, len, "name", value, sizeof value)) snprintf(out->info.name, sizeof out->info.name, "%s", value);
    if (out->info.id[0] == '\0') snprintf(out->info.id, sizeof out->info.id, "%s", name);
    if (out->info.name[0] == '\0') snprintf(out->info.name, sizeof out->info.name, "%s", out->info.id);
    const int named = ini_get(text, len, "cfm", value, sizeof value);
    if (!named || strstr(value, "..") != NULL || !join(out->cfm, sizeof out->cfm, given, value) || !is_file(out->cfm)) {   /* the car file kept to look at */
        memset(out, 0, sizeof *out);
        return refuse(why, why_cap, "%s is a masks project, but its car file is missing: %s", name, named ? value : "project.ini has no cfm= line");
    }
    char dirpath[CAR_PROJECT_PATH + 64], suffix[CAR_PROJECT_NAME + 16];
    snprintf(suffix, sizeof suffix, "car_masks/%s", folder);
    out->has_masks_dir = join(dirpath, sizeof dirpath, given, suffix) && is_dir(dirpath);
    if (out->has_masks_dir) out->mask_count = list_png(dirpath, out->mask);
    return 1;
}

int car_project_present(const char *dir)
{
    return dir != NULL && dir[0] != '\0' && (has_card(dir) || has_old_card(dir) || car_project_is_masks(dir));
}

/* mod.ini's bytes: 0 when unreadable or over the card's cap */
static int read_card(const char *path, char *buf, size_t cap, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    *len = fread(buf, 1, cap, f);
    const int over = fgetc(f) != EOF;
    fclose(f);
    return !over;
}

int car_project_detect(const char *dir, CarProject *out, char *why, size_t why_cap)
{
    memset(out, 0, sizeof *out);
    if (dir == NULL || dir[0] == '\0' || !is_dir(dir)) return refuse(why, why_cap, "%s is not a folder", dir != NULL && dir[0] != '\0' ? dir : "nothing");
    char given[CAR_PROJECT_PATH], name[CAR_PROJECT_NAME];
    if (realpath(dir, given) == NULL) snprintf(given, sizeof given, "%s", dir);
    base_name(given, name, sizeof name);
    if (car_project_is_masks(given)) return detect_masks(given, name, out, why, why_cap);
    if (!has_card(given)) {
        if (has_old_card(given))
            return refuse(why, why_cap, "%s is not a kit in the current layout: it has coche/mod.ini, the old coche/ + modelo/ layout, which Studio no longer opens.", name);
        return refuse(why, why_cap, "No car found in %s: it has no mod.ini. Choose the car's kit folder (the one with mod.ini, model/ and car_masks/).", name);
    }
    const int kit = is_kit_folder(given);   /* else a piece folder as Export original makes it (a masks-less `car new` piece too): mod.ini and SIMDATA at its top */
    if (!kit && is_old_kit_card_folder(given))
        return refuse(why, why_cap, "%s is not a kit in the current layout: it is the coche/ of an old coche/ + modelo/ kit, which Studio no longer opens.", name);
    snprintf(out->root, sizeof out->root, "%s", given);
    snprintf(out->kit, sizeof out->kit, "%s", name);
    out->legacy = !kit;
    if (kit && !join(out->piece, sizeof out->piece, given, car_kit_path(CAR_KIT_BUILD_PIECE))) return refuse(why, why_cap, "%s is too deep: its path is too long", name);   /* the folder the game installs */
    if (!kit) snprintf(out->piece, sizeof out->piece, "%s", given);
    char card[MOD_INI_MAX_SIZE + 1], path[CAR_PROJECT_PATH + 32], reason[200];
    size_t card_len = 0;
    const char *where = "mod.ini";
    if (!join(path, sizeof path, out->root, "mod.ini") || !read_card(path, card, MOD_INI_MAX_SIZE, &card_len))
        return refuse(why, why_cap, "%s in %s cannot be read (it is over %d bytes or unreadable)", where, name, MOD_INI_MAX_SIZE);
    if (!mod_ini_parse((const uint8_t *)card, card_len, &out->info, reason, sizeof reason)) {
        memset(out, 0, sizeof *out);
        return refuse(why, why_cap, "%s in %s is not a valid card: %s", where, name, reason);
    }
    if (out->info.type != MOD_TYPE_CAR) {
        const char *type = out->info.type == MOD_TYPE_TRACK ? "type=track" : "no type= line (a tweak)";
        memset(out, 0, sizeof *out);
        return refuse(why, why_cap, "%s in %s is not a car: it has %s (a car's mod.ini says type=car)", where, name, type);
    }
    if (out->info.stem[0] == '\0') {
        memset(out, 0, sizeof *out);
        return refuse(why, why_cap, "%s in %s has no stem= (the root of the car's file names, such as stem=C84)", where, name);
    }
    char suffix[CAR_PROJECT_NAME + 64], dirpath[CAR_PROJECT_PATH + 64];
    snprintf(suffix, sizeof suffix, "SIMDATA/CARFAMS/%s.CFM", out->info.stem);
    if (!join(out->cfm, sizeof out->cfm, out->piece, suffix)) {
        memset(out, 0, sizeof *out);
        return refuse(why, why_cap, "%s is too deep: its car file's path is too long", name);
    }
    out->built = is_file(out->cfm);
    if (!kit && !out->built) {   /* a piece with no car file; a kit has none until it is built */
        memset(out, 0, sizeof *out);
        return refuse(why, why_cap, "%s in %s is a car, but its car file is missing: %s", where, name, suffix);
    }
    snprintf(out->folder, sizeof out->folder, "%s", out->info.stem);   /* a piece's folder may be spelt as the game's data does (TSupra for stem TSUPRA) */
    if (!kit && join(dirpath, sizeof dirpath, out->piece, "car_masks")) find_folder(dirpath, out->info.stem, out->folder, sizeof out->folder);
    car_project_masks_dir(out, dirpath, sizeof dirpath);
    out->has_masks_dir = is_dir(dirpath);
    if (out->has_masks_dir) out->mask_count = list_png(dirpath, out->mask);
    out->has_model = kit;
    if (kit && join(dirpath, sizeof dirpath, out->root, car_kit_path(CAR_KIT_NEAR_BANK))) out->texture_count = list_png(dirpath, out->texture);
    char far_names[CAR_PROJECT_LIST][CAR_PROJECT_NAME];
    if (kit && join(dirpath, sizeof dirpath, out->root, car_kit_path(CAR_KIT_FAR_BANK))) out->far_count = list_png(dirpath, far_names);
    return 1;
}

void car_project_masks_rel(const CarProject *c, char *out, size_t cap)
{
    if (c->masks_only || c->legacy) snprintf(out, cap, "car_masks/%s", c->folder);
    else snprintf(out, cap, "%s", car_kit_path(CAR_KIT_GEN_CAR_MASKS));
}

void car_project_card_dir(const CarProject *c, char *out, size_t cap)
{
    snprintf(out, cap, "%s", c->legacy || c->masks_only ? c->piece : c->root);
}

void car_project_photos_dir(const CarProject *c, char *out, size_t cap)
{
    if (c->legacy || c->masks_only) snprintf(out, cap, "%s", c->piece);
    else snprintf(out, cap, "%s/%s", c->root, car_kit_path(CAR_KIT_PHOTOS));
}

void car_project_masks_dir(const CarProject *c, char *out, size_t cap)
{
    if (c->masks_only || c->legacy) snprintf(out, cap, "%s/car_masks/%s", c->piece, c->folder);
    else snprintf(out, cap, "%s/%s", c->root, car_kit_path(CAR_KIT_GEN_CAR_MASKS));
}

void car_project_dash_dir(const CarProject *c, char *out, size_t cap)
{
    if (c->masks_only || c->legacy) snprintf(out, cap, "%s/dash_masks/%s", c->piece, c->folder);
    else snprintf(out, cap, "%s/%s", c->root, car_kit_path(CAR_KIT_GEN_DASH_MASKS));
}

void car_project_manifest(const CarProject *c, char *out, size_t cap)
{
    if (c->masks_only || c->legacy) snprintf(out, cap, "%s", "");
    else snprintf(out, cap, "%s/%s", c->root, car_kit_path(CAR_KIT_GEN_MANIFEST));
}

void car_project_edit_dir(const CarProject *c, char *out, size_t cap)
{
    if (c->masks_only || c->legacy) car_project_masks_dir(c, out, cap);   /* the PNGs are what is edited */
    else snprintf(out, cap, "%s/%s", c->root, car_kit_path(CAR_KIT_SRC_MASKS));   /* a kit's masks are made from its PSDs: those are edited, the PNGs are Studio's */
}
