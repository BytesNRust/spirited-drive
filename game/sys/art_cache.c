/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see art_cache.h. Plain stdio, so the tests and the tools link it without the platform layer; main.c gives it the folder. */
#include "game/sys/art_cache.h"

#include "game/sys/port_log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#endif

enum { HEADER = 24, FORMAT = 1, PATH_MAX_LEN = 1024 };
static const char MAGIC[4] = { 'S', 'D', 'A', 'C' };
static char folder[PATH_MAX_LEN];

static uint64_t fnv64(uint64_t h, const void *data, size_t size)
{
    const uint8_t *p = data;
    for (size_t i = 0; i < size; i++) h = (h ^ p[i]) * 0x100000001b3ULL;
    return h;
}

static uint32_t fnv32(const uint8_t *p, size_t size)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < size; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

static void put32(uint8_t *at, uint32_t v) { for (int i = 0; i < 4; i++) at[i] = (uint8_t)(v >> (8 * i)); }
static void put64(uint8_t *at, uint64_t v) { for (int i = 0; i < 8; i++) at[i] = (uint8_t)(v >> (8 * i)); }
static uint32_t get32(const uint8_t *at) { uint32_t v = 0; for (int i = 3; i >= 0; i--) v = v << 8 | at[i]; return v; }
static uint64_t get64(const uint8_t *at) { uint64_t v = 0; for (int i = 7; i >= 0; i--) v = v << 8 | at[i]; return v; }

void art_cache_set_dir(const char *dir)
{
    snprintf(folder, sizeof folder, "%s", dir != NULL ? dir : "");
}

void art_cache_key_init(ArtCacheKey *key, const char *generator, uint32_t version)
{
    uint8_t v[4];
    put32(v, version);
    key->h = fnv64(0xcbf29ce484222325ULL, generator, strlen(generator) + 1);
    key->h = fnv64(key->h, v, sizeof v);
}

void art_cache_key_add(ArtCacheKey *key, const void *data, size_t size)
{
    uint8_t n[8];
    put64(n, data != NULL ? (uint64_t)size : UINT64_MAX);
    key->h = fnv64(key->h, n, sizeof n);
    if (data != NULL) key->h = fnv64(key->h, data, size);
}

static int path_of(const char *name, char *out, size_t size)
{
    if (folder[0] == '\0' || strpbrk(name, "/\\") != NULL) return 0;
    return snprintf(out, size, "%s%s", folder, name) < (int)size;
}

int art_cache_read(const char *name, const ArtCacheKey *key, void *out, size_t size)
{
    char path[PATH_MAX_LEN + 64];
    if (!path_of(name, path, sizeof path)) return 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;   /* not made yet */
    uint8_t head[HEADER];
    const char *why = NULL;
    if (fread(head, 1, HEADER, f) != HEADER || memcmp(head, MAGIC, 4) != 0 || get32(head + 4) != FORMAT) why = "not a cache file of this version";
    else if (get64(head + 8) != key->h) why = "made from other files or by another generator";
    else if (get32(head + 16) != size) why = "of another size";
    else if (fread(out, 1, size, f) != size || fgetc(f) != EOF) why = "cut short or too long";
    else if (fnv32(out, size) != get32(head + 20)) why = "damaged";
    fclose(f);
    if (why != NULL) port_log("art cache: %s is %s, made again\n", name, why);
    return why == NULL;
}

static int make_folder(void)
{
    char dir[PATH_MAX_LEN];
    snprintf(dir, sizeof dir, "%s", folder);
    const size_t n = strlen(dir);
    if (n > 1 && (dir[n - 1] == '/' || dir[n - 1] == '\\')) dir[n - 1] = '\0';
#ifdef _WIN32
    const int made = _mkdir(dir);
#else
    const int made = mkdir(dir, 0755);
#endif
    return made == 0 || errno == EEXIST;
}

int art_cache_write(const char *name, const ArtCacheKey *key, const void *data, size_t size)
{
    char path[PATH_MAX_LEN + 64], temp[PATH_MAX_LEN + 96];
    if (!path_of(name, path, sizeof path) || size > UINT32_MAX) return 0;
    if (!make_folder()) {
        port_log("art cache: cannot make %s (%s), %s stays in memory\n", folder, strerror(errno), name);
        return 0;
    }
    static unsigned serial;
    snprintf(temp, sizeof temp, "%s.%lx.%u.tmp", path, (unsigned long)time(NULL), ++serial);   /* this write's own: two games do not mix their bytes */
    uint8_t head[HEADER];
    memcpy(head, MAGIC, 4);
    put32(head + 4, FORMAT), put64(head + 8, key->h), put32(head + 16, (uint32_t)size), put32(head + 20, fnv32(data, size));
    FILE *f = fopen(temp, "wb");
    int ok = f != NULL && fwrite(head, 1, HEADER, f) == HEADER && fwrite(data, 1, size, f) == size;
    if (f != NULL && fclose(f) != 0) ok = 0;
#ifdef _WIN32
    if (ok) remove(path);   /* rename does not replace there */
#endif
    if (ok) ok = rename(temp, path) == 0;
    if (!ok) {
        port_log("art cache: cannot write %s, it stays in memory\n", path);
        remove(temp);
    }
    return ok;
}
