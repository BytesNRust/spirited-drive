/* port: see data_check.h. */
#include "game/sys/data_check.h"

#include "game/sys/sha256.h"
#include "iso/iso9660.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
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

int data_find_file(const char *dir, const char *rel, char *out, size_t out_size)
{
    return find_path(dir, rel, out, out_size);
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
    DataCheck r = { count, 0, 0, 0, { 0 }, 0 };
    if (dir != NULL && count > 0) find_root(dir, files[0].path, r.root);
    r.found = r.root[0] != '\0';
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

/* port (ISO): the same check inside an image. The folder that holds the files is the image's root or one folder under it. */
static int iso_has(IsoImage *iso, const char *root, const char *rel, IsoEntry *e)
{
    char path[1100];
    if (snprintf(path, sizeof path, "%s%s%s", root, root[0] != '\0' ? "/" : "", rel) >= (int)sizeof path) return 0;
    return iso_stat(iso, path, e) && !e->is_dir;
}

static int iso_find_root(IsoImage *iso, const char *first, char root[1024])
{
    IsoEntry e;
    root[0] = '\0';
    if (iso_has(iso, "", first, &e)) return 1;
    const int n = iso_list(iso, "", NULL, 0);
    IsoEntry *top = n > 0 ? malloc((size_t)n * sizeof *top) : NULL;
    int found = 0;
    if (top != NULL && iso_list(iso, "", top, n) == n) {
        for (int i = 0; i < n && !found; i++)
            if (top[i].is_dir && iso_has(iso, top[i].name, first, &e)) snprintf(root, 1024, "%s", top[i].name), found = 1;
    }
    free(top);
    return found;
}

static int iso_hash(IsoImage *iso, const IsoEntry *e, char hex[65])
{
    Sha256 s;
    sha256_init(&s);
    static unsigned char buf[65536];
    int64_t off = 0;
    while (off < (int64_t)e->size) {
        const int64_t n = iso_read(iso, e, off, buf, sizeof buf);
        if (n <= 0) return 0;
        sha256_update(&s, buf, (size_t)n);
        off += n;
    }
    sha256_hex(&s, hex);
    return 1;
}

int data_check_iso_against(const char *iso_path, const DataFile *files, int count, DataCheck *out)
{
    DataCheck r = { count, 0, 0, 0, { 0 }, 0 };
    IsoImage *iso = iso_path != NULL ? iso_open(iso_path, NULL, 0) : NULL;
    if (iso != NULL && count > 0) r.found = iso_find_root(iso, files[0].path, r.root);
    for (int i = 0; i < count; i++) {
        IsoEntry e;
        char hex[65];
        if (iso == NULL || !r.found || !iso_has(iso, r.root, files[i].path, &e) || !iso_hash(iso, &e, hex)) r.missing++;
        else if ((long)e.size != files[i].size || strcmp(hex, files[i].sha256) != 0) r.different++;
        else r.matched++;
    }
    iso_close(iso);
    if (out != NULL) *out = r;
    return r.matched == count && count > 0;
}

int data_check_iso(const char *iso_path, DataCheck *out) { return data_check_iso_against(iso_path, DATA_MANIFEST, DATA_MANIFEST_COUNT, out); }

int data_is_iso_path(const char *path)
{
    const size_t n = path != NULL ? strlen(path) : 0;
    return n > 4 && strcasecmp(path + n - 4, ".iso") == 0;
}

int data_iso_readable(const char *iso_path, char *err, size_t err_cap)
{
    IsoImage *iso = iso_open(iso_path, err, err_cap);
    iso_close(iso);
    return iso != NULL;
}

/* the file that says the player chose this copy knowing some of its files differ ("Use it anyway") */
#define ACCEPTED_FILE ".accepted"

int data_mark_accepted(const char *dir, const DataCheck *c)
{
    char path[1100];
    if (dir == NULL || snprintf(path, sizeof path, "%s/" ACCEPTED_FILE, dir) >= (int)sizeof path) return 0;
    FILE *f = fopen(path, "w");
    if (f == NULL) return 0;
    fprintf(f, "accepted: %d of %d key files as in the manifest, %d missing, %d different\n", c != NULL ? c->matched : 0, c != NULL ? c->total : 0,
            c != NULL ? c->missing : 0, c != NULL ? c->different : 0);
    return fclose(f) == 0;
}

int data_looks_installed(const char *dir)
{
    char path[1100];
    struct stat st;
    if (dir == NULL || DATA_MANIFEST_COUNT <= 0) return 0;
    if (!find_path(dir, DATA_MANIFEST[0].path, path, sizeof path) || stat(path, &st) != 0) return 0;
    if ((long)st.st_size == DATA_MANIFEST[0].size) return 1;
    snprintf(path, sizeof path, "%s/" ACCEPTED_FILE, dir);   /* a different first file, but the player accepted this copy */
    return stat(path, &st) == 0;
}
