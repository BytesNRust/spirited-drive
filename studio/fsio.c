#include "studio/fsio.h"

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
#else
#define make_dir(path) mkdir((path), 0777)
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
