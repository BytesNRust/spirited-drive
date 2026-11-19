#include "car_project.h"

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

/* the folder holding mod.ini for `dir`: itself, else its coche/. 0 when neither has one. */
static int find_piece(const char *dir, char *piece, size_t cap)
{
    char ini[CAR_PROJECT_PATH + 16];
    if (join(ini, sizeof ini, dir, "mod.ini") && is_file(ini)) return snprintf(piece, cap, "%s", dir) < (int)cap;
    char coche[CAR_PROJECT_PATH + 16];
    if (!join(coche, sizeof coche, dir, "coche") || !join(ini, sizeof ini, coche, "mod.ini")) return 0;
    return is_file(ini) && snprintf(piece, cap, "%s", coche) < (int)cap;
}

int car_project_present(const char *dir)
{
    char piece[CAR_PROJECT_PATH];
    return dir != NULL && dir[0] != '\0' && find_piece(dir, piece, sizeof piece);
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
    if (!find_piece(given, out->piece, sizeof out->piece))
        return refuse(why, why_cap,
                      "No car found in %s: it has no mod.ini and no coche/mod.ini. Choose the car's kit folder (the one that holds coche/ and modelo/) "
                      "or its coche/ folder.",
                      name);
    char card[MOD_INI_MAX_SIZE + 1], path[CAR_PROJECT_PATH + 32], reason[200];
    size_t card_len = 0;
    const int piece_is_given = strcmp(out->piece, given) == 0;
    const char *where = piece_is_given ? "mod.ini" : "coche/mod.ini";
    if (!join(path, sizeof path, out->piece, "mod.ini") || !read_card(path, card, MOD_INI_MAX_SIZE, &card_len))
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
    char suffix[32];
    snprintf(suffix, sizeof suffix, "SIMDATA/CARFAMS/%s.CFM", out->info.stem);
    if (!join(out->cfm, sizeof out->cfm, out->piece, suffix) || !is_file(out->cfm)) {
        memset(out, 0, sizeof *out);
        return refuse(why, why_cap, "%s in %s is a car, but its car file is missing: %s%s", where, name, piece_is_given ? "" : "coche/", suffix);
    }
    /* the kit: the folder above coche/ when it holds modelo/; a folder given with its own mod.ini and a modelo/ above it is the same kit */
    snprintf(out->root, sizeof out->root, "%s", out->piece);
    char up[CAR_PROJECT_PATH], above[CAR_PROJECT_PATH + 16];
    snprintf(up, sizeof up, "%s", out->piece);
    char *slash = strrchr(up, '/');
    if (slash != NULL && slash != up) {
        *slash = '\0';
        if (join(above, sizeof above, up, "modelo") && is_dir(above)) snprintf(out->root, sizeof out->root, "%s", up);
    }
    base_name(out->root, out->kit, sizeof out->kit);
    char dirpath[CAR_PROJECT_PATH + 64];
    snprintf(suffix, sizeof suffix, "car_masks/%s", out->info.stem);
    out->has_masks_dir = join(dirpath, sizeof dirpath, out->piece, suffix) && is_dir(dirpath);
    if (out->has_masks_dir) out->mask_count = list_png(dirpath, out->mask);
    out->has_modelo = join(dirpath, sizeof dirpath, out->root, "modelo") && is_dir(dirpath);
    if (out->has_modelo) {
        if (join(dirpath, sizeof dirpath, out->root, "modelo/c1")) out->texture_count = list_png(dirpath, out->texture);
        char far_names[CAR_PROJECT_LIST][CAR_PROJECT_NAME];
        if (join(dirpath, sizeof dirpath, out->root, "modelo/c3")) out->far_count = list_png(dirpath, far_names);
    }
    if (join(dirpath, sizeof dirpath, out->root, "mascaras") && is_dir(dirpath)) out->kit_mask_count = list_png(dirpath, out->kit_mask);
    return 1;
}
