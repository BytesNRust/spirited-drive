#include "studio/fsio.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#define make_dir(path) _mkdir(path)
#define remove_dir(path) _rmdir(path)
#else
#include <unistd.h>
#define make_dir(path) mkdir((path), 0777)
#define remove_dir(path) rmdir(path)
#endif

enum { MAX_PATH_BYTES = 1024, MAX_DEPTH = 16, MAX_FILE_BYTES = 1 << 30 };

uint8_t *fsio_read_file(const char *path, size_t *size, ExchangeError *err)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) { exchange_fail(err, "%s: cannot open: %s", path, strerror(errno)); return NULL; }
    uint8_t *data = NULL;
    long length = -1;
    if (fseek(f, 0, SEEK_END) == 0) length = ftell(f);
    if (length < 0 || length > MAX_FILE_BYTES || fseek(f, 0, SEEK_SET) != 0) {
        exchange_fail(err, "%s: cannot read its size", path);
    } else if ((data = malloc((size_t)length + 1)) == NULL) {
        exchange_fail(err, "%s: out of memory", path);
    } else if (fread(data, 1, (size_t)length, f) != (size_t)length) {
        exchange_fail(err, "%s: read error at offset 0x%lx", path, (unsigned long)ftell(f));
        free(data);
        data = NULL;
    }
    fclose(f);
    if (data != NULL) *size = (size_t)length;
    return data;
}

static int exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

/* mkdir -p for the directory part of `path`. */
static int make_parents(const char *path)
{
    char buf[MAX_PATH_BYTES];
    if (strlen(path) >= sizeof buf) return 0;
    strcpy(buf, path);
    for (char *p = buf + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        if (make_dir(buf) != 0 && errno != EEXIST) return 0;
        *p = '/';
    }
    return 1;
}

int fsio_write_file(const char *path, const uint8_t *data, size_t size, int force, ExchangeError *err)
{
    if (!force && exists(path)) { exchange_fail(err, "%s: exists (use --force to overwrite)", path); return 0; }
    if (!make_parents(path)) { exchange_fail(err, "%s: cannot create its directory: %s", path, strerror(errno)); return 0; }
    FILE *f = fopen(path, "wb");
    if (f == NULL) { exchange_fail(err, "%s: cannot create: %s", path, strerror(errno)); return 0; }
    const int ok = (size == 0 || fwrite(data, 1, size, f) == size) && fclose(f) == 0;
    if (!ok) exchange_fail(err, "%s: write error", path);
    return ok;
}

static int join(char *out, size_t cap, const char *dir, const char *name)
{
    const size_t n = strlen(dir);
    return (size_t)snprintf(out, cap, "%s%s%s", dir, n && dir[n - 1] != '/' ? "/" : "", name) < cap;
}

int fsio_write_tree(const char *dir, const FileSet *set, int force, ExchangeError *err)
{
    char path[MAX_PATH_BYTES];
    for (size_t i = 0; i < set->count; ++i) {
        if (!join(path, sizeof path, dir, set->file[i].name)) { exchange_fail(err, "%s/%s: path too long", dir, set->file[i].name); return 0; }
        if (!force && exists(path)) { exchange_fail(err, "%s: exists (use --force to overwrite); nothing was written", path); return 0; }
    }
    for (size_t i = 0; i < set->count; ++i) {
        join(path, sizeof path, dir, set->file[i].name);
        if (!fsio_write_file(path, set->file[i].data, set->file[i].size, 1, err)) return 0;
    }
    return 1;
}

static int read_dir(const char *root, const char *relative, int depth, FileSet *set, ExchangeError *err)
{
    char dir[MAX_PATH_BYTES];
    if (depth > MAX_DEPTH) { exchange_fail(err, "%s: directories nested deeper than %d", root, MAX_DEPTH); return 0; }
    if (!join(dir, sizeof dir, root, relative)) { exchange_fail(err, "%s: path too long", root); return 0; }
    DIR *d = opendir(dir);
    if (d == NULL) { exchange_fail(err, "%s: cannot open: %s", dir, strerror(errno)); return 0; }
    int ok = 1;
    struct dirent *e;
    while (ok && (e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char rel[MAX_PATH_BYTES], full[MAX_PATH_BYTES];
        if (relative[0]) snprintf(rel, sizeof rel, "%s/%s", relative, e->d_name);
        else snprintf(rel, sizeof rel, "%s", e->d_name);
        if (!join(full, sizeof full, root, rel)) { exchange_fail(err, "%s: path too long", dir); ok = 0; break; }
        struct stat st;
        if (lstat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            ok = read_dir(root, rel, depth + 1, set, err);
        } else if (S_ISREG(st.st_mode)) {
            size_t size = 0;
            uint8_t *data = fsio_read_file(full, &size, err);
            if (data == NULL) { ok = 0; break; }
            ok = fileset_add_owned(set, rel, data, size);
            if (!ok) { free(data); exchange_fail(err, "%s: unusable name or out of memory", full); }
        }
    }
    closedir(d);
    return ok;
}

int fsio_read_tree(const char *dir, FileSet *set, ExchangeError *err)
{
    return read_dir(dir, "", 0, set, err);
}

int fsio_exists(const char *path)
{
    return exists(path);
}

int fsio_remove(const char *path)
{
    return remove(path) == 0 || errno == ENOENT;
}

int fsio_remove_tree(const char *path)
{
    struct stat st;
    if (lstat(path, &st) != 0) return errno == ENOENT;
    if (!S_ISDIR(st.st_mode)) return remove(path) == 0;
    DIR *d = opendir(path);
    if (d == NULL) return 0;
    int ok = 1;
    struct dirent *e;
    while (ok && (e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char child[MAX_PATH_BYTES];
        if (!join(child, sizeof child, path, e->d_name)) { ok = 0; break; }
        ok = fsio_remove_tree(child);
    }
    closedir(d);
    return ok && remove_dir(path) == 0;
}

void fsio_prune_empty(const char *dir, const char *stop)
{
    char path[MAX_PATH_BYTES];
    snprintf(path, sizeof path, "%s", dir);
    while (strlen(path) > strlen(stop) && strcmp(path, stop) != 0 && remove_dir(path) == 0) {
        char *slash = strrchr(path, '/');
        if (slash == NULL) break;
        *slash = '\0';
    }
}

static int ieq_name(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

int fsio_find_ci(const char *root, const char *rel, char *out, size_t cap)
{
    char dir[MAX_PATH_BYTES];
    if (strlen(root) >= sizeof dir) return 0;
    strcpy(dir, root);
    const char *p = rel;
    while (*p) {
        const char *end = strchr(p, '/');
        const size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len == 0 || len >= 256) return 0;
        char part[256];
        memcpy(part, p, len);
        part[len] = '\0';
        DIR *d = opendir(dir);
        if (d == NULL) return 0;
        int found = 0;
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (!ieq_name(e->d_name, part)) continue;
            char next[MAX_PATH_BYTES];
            if (!join(next, sizeof next, dir, e->d_name)) break;
            strcpy(dir, next);
            found = 1;
            break;
        }
        closedir(d);
        if (!found) return 0;
        p = end ? end + 1 : p + len;
    }
    if (strlen(dir) >= cap) return 0;
    strcpy(out, dir);
    return 1;
}

int fsio_write_atomic(const char *path, const uint8_t *data, size_t size, ExchangeError *err)
{
    char temp[MAX_PATH_BYTES];
    if ((size_t)snprintf(temp, sizeof temp, "%s.tmp", path) >= sizeof temp) { exchange_fail(err, "%s: path too long", path); return 0; }
    if (!fsio_write_file(temp, data, size, 1, err)) return 0;
    if (rename(temp, path) != 0) {
        exchange_fail(err, "%s: cannot replace: %s", path, strerror(errno));
        remove(temp);
        return 0;
    }
    return 1;
}

uint64_t fsio_hash(const uint8_t *data, size_t size)
{
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < size; ++i) h = (h ^ data[i]) * 1099511628211ULL;
    return h;
}
