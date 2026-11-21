#include "car_photos.h"

#include "formats/car_piece.h"
#include "formats/png.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NAME = MOD_INI_PHOTO_NAME_MAX + 1, STEM_MAX = MOD_INI_PHOTO_NAME_MAX - 4, LIST_MAX = MOD_INI_PHOTOS_MAX * (NAME + 1) };

static void say(char *why, size_t cap, const char *fmt, const char *a, unsigned w, unsigned h)
{
    if (why != NULL && cap > 0) snprintf(why, cap, fmt, a, w, h);
}

int car_photos_split(const char *value, char (*names)[NAME], int cap)
{
    int n = 0;
    for (const char *p = value; n < cap && *p != '\0';) {
        const char *comma = strchr(p, ',');
        const char *end = comma != NULL ? comma : p + strlen(p), *s = p;
        while (s < end && isspace((unsigned char)*s)) s++;
        while (end > s && isspace((unsigned char)end[-1])) end--;
        const size_t len = (size_t)(end - s) < NAME - 1 ? (size_t)(end - s) : NAME - 1;
        if (len > 0) {
            memcpy(names[n], s, len);
            names[n][len] = '\0';
            n++;
        }
        if (comma == NULL) break;
        p = comma + 1;
    }
    return n;
}

void car_photos_join(char (*names)[NAME], int n, char *out, size_t cap)
{
    size_t used = 0;
    if (cap == 0) return;
    out[0] = '\0';
    for (int i = 0; i < n; i++) {
        const size_t len = strlen(names[i]);
        if (used + len + (i > 0 ? 1 : 0) + 1 > cap) return;
        if (i > 0) out[used++] = ',';
        memcpy(out + used, names[i], len);
        used += len;
        out[used] = '\0';
    }
}

/* the source's file name without folder and extension, lower case, runs of other characters one dash, none at either end; "photo" if nothing is left */
static void stem_of(const char *source, char *stem)
{
    const char *base = source, *slash;
    while ((slash = strpbrk(base, "/\\")) != NULL) base = slash + 1;
    const char *dot = strrchr(base, '.');
    const size_t n = dot != NULL ? (size_t)(dot - base) : strlen(base);
    size_t used = 0;
    for (size_t i = 0; i < n && used < STEM_MAX; i++) {
        unsigned char c = (unsigned char)tolower((unsigned char)base[i]);
        const int keep = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
        if (!keep) c = '-';
        if (c == '-' && (used == 0 || stem[used - 1] == '-')) continue;
        stem[used++] = (char)c;
    }
    while (used > 0 && stem[used - 1] == '-') used--;
    if (used == 0) {
        memcpy(stem, "photo", 5);
        used = 5;
    }
    stem[used] = '\0';
}

static int is_taken(const char *name, char (*taken)[NAME], int n)
{
    for (int i = 0; i < n; i++)
        if (strcmp(taken[i], name) == 0) return 1;
    return 0;
}

int car_photos_safe_name(const char *source, char (*taken)[NAME], int n, char *out, size_t cap)
{
    char stem[STEM_MAX + 1], name[NAME + 8];
    stem_of(source, stem);
    for (int k = 1; k < 1000; k++) {
        char suffix[8] = "";
        if (k > 1) snprintf(suffix, sizeof suffix, "-%d", k);
        const size_t room = STEM_MAX - strlen(suffix), keep = strlen(stem) < room ? strlen(stem) : room;
        snprintf(name, sizeof name, "%.*s%s.png", (int)keep, stem, suffix);
        if (mod_ini_photo_name_ok(name) && !is_taken(name, taken, n)) {
            if (strlen(name) + 1 > cap) return 0;
            memcpy(out, name, strlen(name) + 1);
            return 1;
        }
    }
    return 0;
}

static uint8_t *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *data = NULL;
    long size = -1;
    if (fseek(f, 0, SEEK_END) == 0) size = ftell(f);
    if (size > 0 && size <= 64L * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0 && (data = malloc((size_t)size)) != NULL) {
        if (fread(data, 1, (size_t)size, f) != (size_t)size) {
            free(data);
            data = NULL;
        } else {
            *len = (size_t)size;
        }
    }
    fclose(f);
    return data;
}

int car_photos_import(const char *source, const char *piece, const char *name, char *why, size_t why_cap)
{
    size_t len = 0;
    uint8_t *data = read_file(source, &len);
    uint32_t w = 0, h = 0;
    if (data == NULL) {
        say(why, why_cap, "%s cannot be read", source, 0, 0);
        return 0;
    }
    if (png_size(data, len, &w, &h) != PNG_OK) {
        free(data);
        say(why, why_cap, "%s is not a PNG picture", source, 0, 0);
        return 0;
    }
    if (w < CAR_PIECE_PHOTO_MIN_W || h < CAR_PIECE_PHOTO_MIN_H || w > CAR_PIECE_PHOTO_MAX_W || h > CAR_PIECE_PHOTO_MAX_H) {
        free(data);
        say(why, why_cap, "%s is %ux%u: a showcase photo is 100x60 to 1280x960", source, w, h);
        return 0;
    }
    char final_path[4096], tmp_path[4100];
    const int a = snprintf(final_path, sizeof final_path, "%s/%s", piece, name);
    const int b = snprintf(tmp_path, sizeof tmp_path, "%s/%s.tmp", piece, name);
    FILE *f = (a > 0 && (size_t)a < sizeof final_path && b > 0 && (size_t)b < sizeof tmp_path) ? fopen(tmp_path, "wb") : NULL;
    const int wrote = f != NULL && fwrite(data, 1, len, f) == len;
    const int closed = f != NULL && fclose(f) == 0;
    free(data);
    if (!wrote || !closed || rename(tmp_path, final_path) != 0) {
        if (f != NULL) remove(tmp_path);
        say(why, why_cap, "%s could not be copied into the car's folder", source, 0, 0);
        return 0;
    }
    return 1;
}

static int path_of(char *path, size_t cap, const char *piece, const char *name, const char *suffix)
{
    const int n = snprintf(path, cap, "%s/%s%s", piece, name, suffix);
    return n > 0 && (size_t)n < cap;
}

static int exists_file(const char *piece, const char *name, const char *suffix)
{
    char path[4200];
    FILE *f = path_of(path, sizeof path, piece, name, suffix) ? fopen(path, "rb") : NULL;
    if (f == NULL) return 0;
    fclose(f);
    return 1;
}

int car_photos_commit(const char *piece, const char *listed)
{
    char names[MOD_INI_PHOTOS_MAX][NAME];
    const int n = car_photos_split(listed, names, MOD_INI_PHOTOS_MAX);
    int done = 0;
    for (int i = 0; i < n; i++) {
        char from[4200], to[4200];
        if (exists_file(piece, names[i], "") || !exists_file(piece, names[i], ".new")) continue;
        if (path_of(from, sizeof from, piece, names[i], ".new") && path_of(to, sizeof to, piece, names[i], "") && rename(from, to) == 0) done++;
    }
    return done;
}

int car_photos_sweep(const char *piece, char (*made)[NAME], int *made_n, const char *listed)
{
    char names[MOD_INI_PHOTOS_MAX][NAME];
    const int n = car_photos_split(listed, names, MOD_INI_PHOTOS_MAX);
    int removed = 0, kept = 0;
    for (int i = 0; i < *made_n; i++) {
        int keep = is_taken(made[i], names, n);
        if (!keep) {
            char path[4200];
            if (path_of(path, sizeof path, piece, made[i], "")) remove(path);
            if (path_of(path, sizeof path, piece, made[i], ".new")) remove(path);
            keep = exists_file(piece, made[i], "") || exists_file(piece, made[i], ".new");   /* could not be removed: still ours */
            if (!keep) removed++;
        }
        if (keep) {
            if (kept != i) memcpy(made[kept], made[i], NAME);
            kept++;
        }
    }
    *made_n = kept;
    return removed;
}

int car_photos_size(const char *piece, const char *name, uint32_t *w, uint32_t *h)
{
    char path[4200];
    uint8_t head[64];
    const int n = snprintf(path, sizeof path, "%s/%s", piece, name);
    FILE *f = (n > 0 && (size_t)n < sizeof path) ? fopen(path, "rb") : NULL;
    if (f == NULL && n > 0 && (size_t)n < sizeof path - 4) {
        strcat(path, ".new");
        f = fopen(path, "rb");
    }
    if (f == NULL) return 0;
    const size_t got = fread(head, 1, sizeof head, f);
    fclose(f);
    return png_size(head, got, w, h) == PNG_OK;
}
