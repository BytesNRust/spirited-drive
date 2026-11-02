/* port: see data_check.h. */
#include "game/sys/data_check.h"

#include "game/sys/sha256.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

/* `dir`/`rel` with each part found without regard to case; 0 when it is not there or does not fit */
static int find_path(const char *dir, const char *rel, char *out, size_t out_size)
{
    char cur[1024];
    if (snprintf(cur, sizeof cur, "%s", dir) >= (int)sizeof cur) return 0;
    const char *part = rel;
    while (*part != '\0') {
        const char *end = strchr(part, '/');
        const size_t len = end != NULL ? (size_t)(end - part) : strlen(part);
        DIR *d = opendir(cur);
        if (d == NULL) return 0;
        int found = 0;
        for (struct dirent *e = readdir(d); e != NULL; e = readdir(d)) {
            if (strlen(e->d_name) == len && strncasecmp(e->d_name, part, len) == 0) {
                char next[1024];
                if (snprintf(next, sizeof next, "%s/%s", cur, e->d_name) < (int)sizeof next) {
                    memcpy(cur, next, strlen(next) + 1);
                    found = 1;
                }
                break;
            }
        }
        closedir(d);
        if (!found) return 0;
        part += len + (end != NULL ? 1 : 0);
    }
    return snprintf(out, out_size, "%s", cur) < (int)out_size;
}

static int hash_file(const char *path, char hex[65], long *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    Sha256 s;
    sha256_init(&s);
    unsigned char buf[65536];
    long total = 0;
    for (size_t n = fread(buf, 1, sizeof buf, f); n > 0; n = fread(buf, 1, sizeof buf, f)) sha256_update(&s, buf, n), total += (long)n;
    const int ok = !ferror(f);
    fclose(f);
    sha256_hex(&s, hex);
    *size = total;
    return ok;
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* the folder under `dir` that holds the first file, `dir` itself when it does; "" when none */
static void find_root(const char *dir, const char *first, char root[1024])
{
    char probe[1024];
    root[0] = '\0';
    if (find_path(dir, first, probe, sizeof probe)) {
        snprintf(root, 1024, "%s", dir);
        return;
    }
    DIR *d = opendir(dir);
    if (d == NULL) return;
    for (struct dirent *e = readdir(d); e != NULL && root[0] == '\0'; e = readdir(d)) {
        char sub[1024];
        if (e->d_name[0] == '.' || snprintf(sub, sizeof sub, "%s/%s", dir, e->d_name) >= (int)sizeof sub || !is_dir(sub)) continue;
        if (find_path(sub, first, probe, sizeof probe)) snprintf(root, 1024, "%s", sub);
    }
    closedir(d);
}

int data_check_against(const char *dir, const DataFile *files, int count, DataCheck *out)
{
    DataCheck r = { count, 0, 0, 0, { 0 } };
    if (dir != NULL && count > 0) find_root(dir, files[0].path, r.root);
    for (int i = 0; i < count; i++) {
        char path[1024], hex[65];
        long size = 0;
        if (r.root[0] == '\0' || !find_path(r.root, files[i].path, path, sizeof path) || !hash_file(path, hex, &size)) r.missing++;
        else if (size != files[i].size || strcmp(hex, files[i].sha256) != 0) r.different++;
        else r.matched++;
    }
    if (out != NULL) *out = r;
    return r.matched == count && count > 0;
}

int data_check(const char *dir, DataCheck *out) { return data_check_against(dir, DATA_MANIFEST, DATA_MANIFEST_COUNT, out); }

int data_looks_installed(const char *dir)
{
    char path[1024];
    struct stat st;
    return dir != NULL && DATA_MANIFEST_COUNT > 0 && find_path(dir, DATA_MANIFEST[0].path, path, sizeof path) && stat(path, &st) == 0 &&
           (long)st.st_size == DATA_MANIFEST[0].size;
}
