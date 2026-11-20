#include "car_export.h"

#include "formats/car_piece.h"
#include "formats/mod_ini.h"
#include "game/race/car_paint.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define MADE_FROM "Made from the game's files: for your own use, do not share."

/* the folders are the game's data/car_masks/ ones; the car files are the game's, by stem (the traffic and police ones have no catalogue entry) */
static const ExportCar CARS[EXPORT_CARS] = {
    { EXPORT_DRIVABLE, "TSupra", "TSUPRA", "Toyota Supra Turbo", 0 },
    { EXPORT_DRIVABLE, "LDiabl", "LDIABL", "Lamborghini Diablo VT", 1 },
    { EXPORT_DRIVABLE, "P911", "P911", "Porsche 911 Carrera", 2 },
    { EXPORT_DRIVABLE, "CZR1", "CZR1", "Chevrolet Corvette ZR-1", 3 },
    { EXPORT_DRIVABLE, "F512TR", "F512TR", "Ferrari 512 TR", 4 },
    { EXPORT_DRIVABLE, "DVIPER", "DVIPER", "Dodge Viper RT/10", 5 },
    { EXPORT_DRIVABLE, "ANSX", "ANSX", "Acura NSX", 6 },
    { EXPORT_DRIVABLE, "MRX7", "MRX7", "Mazda RX-7", 7 },
    { EXPORT_TRAFFIC, "Wagon", "WAGON", "Wagon", -1 },
    { EXPORT_TRAFFIC, "CRX", "CRX", "CRX", -1 },
    { EXPORT_TRAFFIC, "Vandura", "VANDURA", "Vandura", -1 },
    { EXPORT_TRAFFIC, "BMW", "BMW", "BMW", -1 },
    { EXPORT_TRAFFIC, "SunBird", "SUNBIRD", "Sunbird", -1 },
    { EXPORT_TRAFFIC, "JETTA", "JETTA", "Jetta", -1 },
    { EXPORT_TRAFFIC, "RODEO", "RODEO", "Rodeo", -1 },
    { EXPORT_TRAFFIC, "axxess", "AXXESS", "Axxess", -1 },
    { EXPORT_TRAFFIC, "probe", "PROBE", "Probe", -1 },
    { EXPORT_TRAFFIC, "Jeep", "JEEP", "Jeep", -1 },
    { EXPORT_TRAFFIC, "Lemans", "LEMANS", "Le Mans", -1 },
    { EXPORT_TRAFFIC, "Pickup", "PICKUP", "Pickup", -1 },
    { EXPORT_POLICE, "COPMUST", "COPMUST", "Police Mustang", -1 },
};

const ExportCar *car_export_car(int index)
{
    return index >= 0 && index < EXPORT_CARS ? &CARS[index] : NULL;
}

static int fail(char *why, size_t cap, const char *format, ...) __attribute__((format(printf, 3, 4)));
static int fail(char *why, size_t cap, const char *format, ...)
{
    if (why != NULL && cap > 0) {
        va_list args;
        va_start(args, format);
        vsnprintf(why, cap, format, args);
        va_end(args);
    }
    return 0;
}

static int join(char *out, size_t cap, const char *a, const char *b)
{
    const int n = snprintf(out, cap, "%s/%s", a, b);
    return n >= 0 && (size_t)n < cap;
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* the entry of `dir` named `name` in any case: its path in `out` (the name as the disk has it); 0 when there is none */
static int find_ci(const char *dir, const char *name, char *out, size_t cap)
{
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    int found = 0;
    for (struct dirent *e; !found && (e = readdir(d)) != NULL;)
        if (strcasecmp(e->d_name, name) == 0) found = join(out, cap, dir, e->d_name);
    closedir(d);
    return found;
}

/* `rel` ("SIMDATA/CARFAMS/X.CFM") below `root`, each part in any case */
static int find_path_ci(const char *root, const char *rel, char *out, size_t cap)
{
    char cur[EXPORT_PATH], part[PATH_MAX];
    if (snprintf(cur, sizeof cur, "%s", root) >= (int)sizeof cur) return 0;
    while (*rel != '\0') {
        const char *slash = strchr(rel, '/');
        const size_t n = slash != NULL ? (size_t)(slash - rel) : strlen(rel);
        if (n == 0 || n >= sizeof part) return 0;
        memcpy(part, rel, n);
        part[n] = '\0';
        char next[EXPORT_PATH];
        if (!find_ci(cur, part, next, sizeof next)) return 0;
        memcpy(cur, next, strlen(next) + 1);
        rel += n + (slash != NULL ? 1 : 0);
    }
    if (strlen(cur) >= cap) return 0;
    memcpy(out, cur, strlen(cur) + 1);
    return 1;
}

static uint8_t *read_file(const char *path, size_t *size, char *why, size_t why_cap)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return fail(why, why_cap, "cannot read %s: %s", path, strerror(errno)), NULL;
    uint8_t *data = NULL;
    size_t len = 0, cap = 0;
    for (;;) {
        if (len == cap) {
            cap = cap != 0 ? cap * 2 : 65536;
            uint8_t *bigger = realloc(data, cap);
            if (bigger == NULL) {
                free(data);
                fclose(f);
                return fail(why, why_cap, "out of memory"), NULL;
            }
            data = bigger;
        }
        const size_t got = fread(data + len, 1, cap - len, f);
        len += got;
        if (got == 0) break;
    }
    const int bad = ferror(f);
    fclose(f);
    if (bad) {
        free(data);
        return fail(why, why_cap, "cannot read %s", path), NULL;
    }
    *size = len;
    return data;
}

static int write_file(const char *path, const uint8_t *data, size_t size, char *why, size_t why_cap)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return fail(why, why_cap, "cannot write %s: %s", path, strerror(errno));
    const int wrote = size == 0 || fwrite(data, 1, size, f) == size;
    const int closed = fclose(f) == 0;   /* closed whether it wrote or not */
    return wrote && closed ? 1 : fail(why, why_cap, "cannot write %s: %s", path, strerror(errno));
}

static int make_dir(const char *path, char *why, size_t why_cap)
{
    if (mkdir(path, 0777) == 0 || (errno == EEXIST && is_dir(path))) return 1;
    return fail(why, why_cap, "cannot make the folder %s: %s", path, strerror(errno));
}

/* `path` and every folder above it */
static int make_dirs(const char *path, char *why, size_t why_cap)
{
    char tmp[EXPORT_PATH];
    if (snprintf(tmp, sizeof tmp, "%s", path) >= (int)sizeof tmp) return fail(why, why_cap, "the path is too long: %s", path);
    for (char *p = tmp + 1; *p != '\0'; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        if (!is_dir(tmp) && !make_dir(tmp, why, why_cap)) return 0;
        *p = '/';
    }
    return make_dir(tmp, why, why_cap);
}

/* removes what this module made in `path` (files and folders at most MAX_DEPTH deep: a project is three levels) */
static void remove_made(const char *path, int depth)
{
    DIR *d = opendir(path);
    if (d != NULL) {
        for (struct dirent *e; (e = readdir(d)) != NULL;) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            char child[EXPORT_PATH];
            if (!join(child, sizeof child, path, e->d_name)) continue;
            struct stat st;
            if (lstat(child, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) {
                if (depth < 6) remove_made(child, depth + 1);
            } else {
                unlink(child);
            }
        }
        closedir(d);
    }
    rmdir(path);
}

/* the leaf of a path */
static const char *leaf(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static int is_mask_name(const char *name)
{
    const size_t n = strlen(name), tail = sizeof "_mask.png" - 1;
    if (n <= tail || strcasecmp(name + n - tail, "_mask.png") != 0 || name[0] == '.') return 0;
    for (size_t i = 0; i < n - tail; ++i)
        if (name[i] == '.') return 0;   /* "frnt.before-1603_mask.png" is an old copy: the game does not read it */
    return 1;
}

static int by_name(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* The names of the masks in `dir` (is_mask_name), sorted, malloc'd one by one; the count, or -1 when out of memory */
static int list_masks(const char *dir, char ***names)
{
    *names = NULL;
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    int count = 0, cap = 0;
    char **list = NULL;
    for (struct dirent *e; (e = readdir(d)) != NULL;) {
        if (!is_mask_name(e->d_name)) continue;
        if (count == cap) {
            cap = cap != 0 ? cap * 2 : 16;
            char **bigger = realloc(list, (size_t)cap * sizeof *list);
            if (bigger == NULL) { count = -1; break; }
            list = bigger;
        }
        list[count] = strdup(e->d_name);
        if (list[count] == NULL) { count = -1; break; }
        count++;
    }
    closedir(d);
    if (count < 0) {
        free(list);   /* the names already taken are lost with it: an out-of-memory run is ending anyway */
        return -1;
    }
    if (count > 1) qsort(list, (size_t)count, sizeof *list, by_name);
    *names = list;
    return count;
}

static void free_names(char **names, int count)
{
    for (int i = 0; i < count; ++i) free(names[i]);
    free(names);
}

/* the width and height a PNG file's header gives; 0 when it is not one */
static int png_size(const uint8_t *data, size_t size, long *w, long *h)
{
    static const uint8_t SIGNATURE[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
    if (size < 24 || memcmp(data, SIGNATURE, 8) != 0 || memcmp(data + 12, "IHDR", 4) != 0) return 0;
    *w = (long)((uint32_t)data[16] << 24 | (uint32_t)data[17] << 16 | (uint32_t)data[18] << 8 | data[19]);
    *h = (long)((uint32_t)data[20] << 24 | (uint32_t)data[21] << 16 | (uint32_t)data[22] << 8 | data[23]);
    return *w > 0 && *h > 0;
}

void car_export_source(const char *game, const char *user, const char *pref, const char *cd_override, const char *data_override, ExportSource *out)
{
    memset(out, 0, sizeof *out);
    char cand[3][EXPORT_PATH];
    int count = 0;
    if (data_override != NULL && data_override[0] != '\0') {
        snprintf(out->data, sizeof out->data, "%s", data_override);
    } else {
        char dir[EXPORT_PATH];
        snprintf(dir, sizeof dir, "%s", game != NULL ? game : "");
        char *slash = strrchr(dir, '/');
        if (slash != NULL) *slash = '\0';
        else dir[0] = '\0';
        const char *tail = "/MacOS";
        const size_t n = strlen(dir), t = strlen(tail);
        if (n > t && strcmp(dir + n - t, tail) == 0) snprintf(cand[count++], sizeof cand[0], "%.*s/Resources/data", (int)(n - t), dir);   /* the packaged app */
        if (dir[0] != '\0') snprintf(cand[count++], sizeof cand[0], "%s/data", dir);                                                    /* a build tree's program */
        snprintf(cand[count++], sizeof cand[0], "data");                                                                                 /* run from the source tree */
        snprintf(out->data, sizeof out->data, "%s", cand[0]);
        for (int i = 0; i < count; ++i) {
            char probe[EXPORT_PATH];
            if (join(probe, sizeof probe, cand[i], "car_masks") && is_dir(probe)) { snprintf(out->data, sizeof out->data, "%s", cand[i]); break; }
        }
    }
    if (cd_override != NULL && cd_override[0] != '\0') {
        snprintf(out->cd, sizeof out->cd, "%s", cd_override);
        return;
    }
    count = 0;
    const char *folder = user != NULL && user[0] != '\0' ? user : pref;
    if (folder != NULL && folder[0] != '\0') {
        const size_t n = strlen(folder);
        snprintf(cand[count++], sizeof cand[0], "%s%sgamedata", folder, folder[n - 1] == '/' ? "" : "/");
    }
    snprintf(cand[count++], sizeof cand[0], "original");   /* a development tree's copy of the game's files */
    snprintf(out->cd, sizeof out->cd, "%s", cand[0]);
    for (int i = 0; i < count; ++i) {
        char probe[EXPORT_PATH];
        if (find_path_ci(cand[i], "SIMDATA/CARFAMS", probe, sizeof probe)) { snprintf(out->cd, sizeof out->cd, "%s", cand[i]); break; }
    }
}

/* the folder of the car's masks in the game's data, as the disk spells it (`name`), and its path */
static int masks_folder(const ExportSource *src, const char *kind, const char *folder, char *path, size_t cap)
{
    char base[EXPORT_PATH];
    return join(base, sizeof base, src->data, kind) && find_ci(base, folder, path, cap);
}

int car_export_available(const ExportSource *src, int index, char *why, size_t why_cap)
{
    const ExportCar *car = car_export_car(index);
    if (car == NULL) return fail(why, why_cap, "no such car");
    char path[EXPORT_PATH], rel[64];
    snprintf(rel, sizeof rel, "SIMDATA/CARFAMS/%s.CFM", car->file);
    if (!find_path_ci(src->cd, rel, path, sizeof path))
        return fail(why, why_cap, "The game's files have no %s (looked in %s).", rel, src->cd[0] != '\0' ? src->cd : "nowhere");
    char **names;
    char dir[EXPORT_PATH];
    if (!masks_folder(src, "car_masks", car->folder, dir, sizeof dir))
        return fail(why, why_cap, "The game has no masks for it (no car_masks/%s in %s): there is nothing to adjust.", car->folder, src->data);
    const int count = list_masks(dir, &names);
    if (count < 0) return fail(why, why_cap, "out of memory");
    free_names(names, count);
    if (count == 0) return fail(why, why_cap, "The game has no masks for it (car_masks/%s holds none): there is nothing to adjust.", car->folder);
    return 1;
}

void car_export_folder_name(int index, char *out, size_t cap)
{
    const ExportCar *car = car_export_car(index);
    if (cap == 0) return;
    if (car == NULL) { out[0] = '\0'; return; }
    char id[48];
    size_t n = 0;
    for (const char *p = car->folder; *p != '\0' && n + 1 < sizeof id; ++p) id[n++] = (char)tolower((unsigned char)*p);
    id[n] = '\0';
    snprintf(out, cap, "%s-%s", id, car->kind == EXPORT_DRIVABLE ? "original" : "masks");
}

void car_export_repo_path(const char *folder, char *out, size_t cap)
{
    snprintf(out, cap, "data/car_masks/%s/", folder);
}

int car_export_file_url(const char *path, char *url, size_t cap)
{
    static const char HEX[] = "0123456789ABCDEF";
    size_t n = 0;
    const char *prefix = "file://";
    if (cap <= strlen(prefix)) return 0;
    memcpy(url, prefix, strlen(prefix));
    n = strlen(prefix);
    for (const unsigned char *p = (const unsigned char *)path; *p != '\0'; ++p) {
        const int plain = isalnum(*p) || *p == '/' || *p == '-' || *p == '_' || *p == '.' || *p == '~';
        if (n + (plain ? 1 : 3) + 1 > cap) return 0;
        if (plain) {
            url[n++] = (char)*p;
        } else {
            url[n++] = '%';
            url[n++] = HEX[*p >> 4];
            url[n++] = HEX[*p & 15];
        }
    }
    url[n] = '\0';
    return 1;
}

/* the masks of `from` copied under `to` (the same names, the same bytes); `sizes`, when not NULL, gets " name:WxH" for each. 0 with `why` */
static int copy_masks(const char *from, const char *to, char *sizes, size_t sizes_cap, char *why, size_t why_cap)
{
    char **names;
    const int count = list_masks(from, &names);
    if (count < 0) return fail(why, why_cap, "out of memory");
    int ok = make_dirs(to, why, why_cap);
    for (int i = 0; ok && i < count; ++i) {
        char src[EXPORT_PATH], dst[EXPORT_PATH];
        size_t size = 0;
        ok = join(src, sizeof src, from, names[i]) && join(dst, sizeof dst, to, names[i]);
        if (!ok) { fail(why, why_cap, "the path is too long: %s/%s", from, names[i]); break; }
        uint8_t *data = read_file(src, &size, why, why_cap);
        ok = data != NULL;
        long w = 0, h = 0;
        if (ok && !png_size(data, size, &w, &h)) ok = fail(why, why_cap, "%s is not a PNG picture", src);
        if (ok && sizes != NULL) {
            const size_t used = strlen(sizes);
            const int n = snprintf(sizes + used, sizes_cap - used, "%s%s:%ldx%ld", used == 0 ? "" : " ", names[i], w, h);
            if (n < 0 || (size_t)n >= sizes_cap - used) ok = fail(why, why_cap, "too many masks for project.ini");
        }
        ok = ok && write_file(dst, data, size, why, why_cap);
        free(data);
    }
    free_names(names, count);
    return ok;
}

/* the game's file `rel` copied to `dst` (a path in the project, made by the caller's folder); `leaf_out`, when not NULL, gets the name the game's disc has */
static int copy_game_file(const ExportSource *src, const char *rel, const char *dst, char *leaf_out, size_t leaf_cap, char *why, size_t why_cap)
{
    char path[EXPORT_PATH];
    if (!find_path_ci(src->cd, rel, path, sizeof path)) return fail(why, why_cap, "The game's files have no %s (looked in %s).", rel, src->cd);
    if (leaf_out != NULL) snprintf(leaf_out, leaf_cap, "%s", leaf(path));
    size_t size = 0;
    uint8_t *data = read_file(path, &size, why, why_cap);
    if (data == NULL) return 0;
    const int ok = write_file(dst, data, size, why, why_cap);
    free(data);
    return ok;
}

/* the card of a drivable car: what `car new` writes for the car, with the catalogue's colours and redistributable=0, checked by the game's own parser */
static int write_card(const ExportCar *car, const char *dir, const char *abs, const char *traction, char *why, size_t why_cap)
{
    char id[48], name[96], paints[512] = "", texts[MOD_INI_MAX_SIZE], card[MOD_INI_MAX_SIZE];
    car_export_folder_name((int)(car - CARS), id, sizeof id);
    snprintf(name, sizeof name, "%s (original)", car->label);
    ModPaint list[MOD_INI_PAINTS_MAX];
    int count = 0;
    for (int k = 0; k < car_paint_count(car->model) && count < MOD_INI_PAINTS_MAX; ++k) {
        const CarPaint *p = car_paint(car->model, k);
        snprintf(list[count].name, sizeof list[count].name, "%s", p->name);
        memcpy(list[count].rgb, p->rgb, 3);
        count++;
    }
    if (count > 0) {
        char value[480], reason[200];
        if (!mod_ini_paints_format(list, count, value, sizeof value, reason, sizeof reason)) return fail(why, why_cap, "the catalogue colours: %s", reason);
        snprintf(paints, sizeof paints, "paints=%s\n", value);
    }
    if (!mod_ini_text_keys(NULL, MADE_FROM, texts, sizeof texts)) return fail(why, why_cap, "the card does not fit mod.ini's %d bytes", MOD_INI_MAX_SIZE);
    const char *word = car_piece_word_of_model(car->model);
    const int n = snprintf(card, sizeof card,
                           "id=%s\nname=%s\nversion=0.1\ntype=car\nstem=%s\nshort_name=%s\nsound_from=%s\ndash_from=%s\nabs=%s\ntraction=%s\nredistributable=0\n%s%s", id, name,
                           car->file, car->file, word, word, abs, traction, paints, texts);
    ModInfo info;
    char reason[160];
    if (n < 0 || (size_t)n >= sizeof card) return fail(why, why_cap, "the card does not fit mod.ini's %d bytes", MOD_INI_MAX_SIZE);
    if (!mod_ini_parse((const uint8_t *)card, (size_t)n, &info, reason, sizeof reason)) return fail(why, why_cap, "mod.ini: %s", reason);
    char path[EXPORT_PATH];
    return join(path, sizeof path, dir, "mod.ini") && write_file(path, (const uint8_t *)card, (size_t)n, why, why_cap);
}

/* a drivable car: its CFM, PDN and PBS under its own stem, the masks, the dash masks, the card */
static int build_drivable(const ExportSource *src, const ExportCar *car, const char *dir, char *why, size_t why_cap)
{
    char from[CAR_PIECE_FILES][CAR_PIECE_PATH], to[CAR_PIECE_FILES][CAR_PIECE_PATH], path[EXPORT_PATH], sub[EXPORT_PATH];
    car_piece_paths(car->file, from);
    car_piece_paths(car->file, to);
    if (!join(path, sizeof path, dir, "SIMDATA/CARFAMS") || !make_dirs(path, why, why_cap)) return 0;
    if (!join(path, sizeof path, dir, "SIMDATA/CARSPECS") || !make_dirs(path, why, why_cap)) return 0;
    for (int i = 0; i < CAR_PIECE_FILES; ++i)
        if (!join(path, sizeof path, dir, to[i]) || !copy_game_file(src, from[i], path, NULL, 0, why, why_cap)) return 0;
    char pbs_path[EXPORT_PATH];
    size_t pbs_size = 0;
    uint8_t *pbs = NULL;
    if (join(pbs_path, sizeof pbs_path, dir, to[2])) pbs = read_file(pbs_path, &pbs_size, why, why_cap);
    if (pbs == NULL) return 0;
    const char *abs = car_piece_assist("", pbs, pbs_size, CAR_PIECE_PBS_ABS) ? "yes" : "no";
    const char *traction = car_piece_assist("", pbs, pbs_size, CAR_PIECE_PBS_TRACTION) ? "yes" : "no";
    free(pbs);
    char found[EXPORT_PATH];
    if (!masks_folder(src, "car_masks", car->folder, found, sizeof found)) return fail(why, why_cap, "The game has no masks for it (car_masks/%s).", car->folder);
    if (!join(sub, sizeof sub, dir, "car_masks") || !make_dirs(sub, why, why_cap)) return 0;
    if (!join(path, sizeof path, sub, leaf(found)) || !copy_masks(found, path, NULL, 0, why, why_cap)) return 0;
    if (masks_folder(src, "dash_masks", car->file, found, sizeof found)) {   /* the dash masks go the way `car new` puts them: by the car's stem */
        if (!join(sub, sizeof sub, dir, "dash_masks") || !make_dirs(sub, why, why_cap)) return 0;
        if (!join(path, sizeof path, sub, car->file) || !copy_masks(found, path, NULL, 0, why, why_cap)) return 0;
    }
    return write_card(car, dir, abs, traction, why, why_cap);
}

/* a masks project: project.ini, the car's CFM kept to look at, the masks */
static int build_masks(const ExportSource *src, const ExportCar *car, const char *dir, char *why, size_t why_cap)
{
    char rel[64], path[EXPORT_PATH], sub[EXPORT_PATH], found[EXPORT_PATH], cfm[64];
    if (!join(sub, sizeof sub, dir, "original") || !make_dirs(sub, why, why_cap)) return 0;
    snprintf(rel, sizeof rel, "SIMDATA/CARFAMS/%s.CFM", car->file);
    if (!find_path_ci(src->cd, rel, found, sizeof found)) return fail(why, why_cap, "The game's files have no %s (looked in %s).", rel, src->cd);
    snprintf(cfm, sizeof cfm, "%s", leaf(found));
    if (!join(path, sizeof path, sub, cfm) || !copy_game_file(src, rel, path, NULL, 0, why, why_cap)) return 0;
    if (!masks_folder(src, "car_masks", car->folder, found, sizeof found)) return fail(why, why_cap, "The game has no masks for it (car_masks/%s).", car->folder);
    char sizes[1024] = "", folder[64];
    snprintf(folder, sizeof folder, "%s", leaf(found));
    if (!join(sub, sizeof sub, dir, "car_masks") || !make_dirs(sub, why, why_cap)) return 0;
    if (!join(path, sizeof path, sub, folder) || !copy_masks(found, path, sizes, sizeof sizes, why, why_cap)) return 0;
    char id[48], ini[2048];
    car_export_folder_name((int)(car - CARS), id, sizeof id);
    const int n = snprintf(ini, sizeof ini,
                           "# %s\nid=%s\nname=%s masks\nkind=masks\nmasks=%s\ncfm=original/%s\nexport_sizes=%s\nredistributable=0\n", MADE_FROM, id, car->label, folder, cfm, sizes);
    if (n < 0 || (size_t)n >= sizeof ini) return fail(why, why_cap, "project.ini does not fit");
    return join(path, sizeof path, dir, "project.ini") && write_file(path, (const uint8_t *)ini, (size_t)n, why, why_cap);
}

int car_export(const ExportSource *src, int index, const char *dir, char *why, size_t why_cap)
{
    const ExportCar *car = car_export_car(index);
    if (car == NULL) return fail(why, why_cap, "no such car");
    if (dir == NULL || dir[0] == '\0') return fail(why, why_cap, "no folder was given");
    if (!car_export_available(src, index, why, why_cap)) return 0;
    char tmp[EXPORT_PATH];
    if (snprintf(tmp, sizeof tmp, "%s.exporting", dir) >= (int)sizeof tmp) return fail(why, why_cap, "the path is too long: %s", dir);
    struct stat st;
    if (lstat(dir, &st) == 0) return fail(why, why_cap, "%s is already there: choose another folder (nothing was written).", dir);
    if (lstat(tmp, &st) == 0) return fail(why, why_cap, "%s is there from an export that did not finish: remove it first (nothing was written).", tmp);
    if (!make_dirs(tmp, why, why_cap)) {
        remove_made(tmp, 0);
        return 0;
    }
    const int ok = car->kind == EXPORT_DRIVABLE ? build_drivable(src, car, tmp, why, why_cap) : build_masks(src, car, tmp, why, why_cap);
    if (ok && rename(tmp, dir) != 0) {
        fail(why, why_cap, "cannot make %s: %s", dir, strerror(errno));
        remove_made(tmp, 0);
        return 0;
    }
    if (!ok) remove_made(tmp, 0);
    return ok;
}
