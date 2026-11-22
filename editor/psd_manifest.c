/* The manifest of generated masks: see psd_manifest.h. */
#include "psd_manifest.h"

#include "game/sys/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { PATH = 4096, SHA_CACHE = 128 };

static unsigned opens;   /* files opened: manifest_opens */

unsigned manifest_opens(void)
{
    return opens;
}

static long long mtime_of(const struct stat *st)
{
#ifdef __APPLE__
    return (long long)st->st_mtimespec.tv_sec * 1000000000LL + st->st_mtimespec.tv_nsec;
#else
    return (long long)st->st_mtim.tv_sec * 1000000000LL + st->st_mtim.tv_nsec;
#endif
}

static int is_hex64(const char *s)
{
    if (strlen(s) != MANIFEST_SHA - 1) return 0;
    for (; *s != '\0'; ++s)
        if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f'))) return 0;
    return 1;
}

int manifest_load(const char *path, Manifest *m)
{
    memset(m, 0, sizeof *m);
    opens++;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return access(path, F_OK) != 0;   /* not there: empty; there and unreadable: no */
    char line[MANIFEST_NAME + MANIFEST_SHA + 8];
    while (fgets(line, sizeof line, f) != NULL) {
        char *tab = strchr(line, '\t');
        line[strcspn(line, "\r\n")] = '\0';
        if (tab == NULL || tab == line) continue;
        *tab = '\0';
        if (is_hex64(tab + 1)) manifest_set(m, line, tab + 1);
    }
    fclose(f);
    return 1;
}

const char *manifest_sha(const Manifest *m, const char *name)
{
    for (int i = 0; i < m->count; ++i)
        if (strcmp(m->entry[i].name, name) == 0) return m->entry[i].sha;
    return NULL;
}

int manifest_set(Manifest *m, const char *name, const char *sha)
{
    if (strlen(name) >= MANIFEST_NAME || strlen(sha) >= MANIFEST_SHA || name[0] == '\0' || strpbrk(name, "\t\r\n") != NULL) return 0;
    for (int i = 0; i < m->count; ++i)
        if (strcmp(m->entry[i].name, name) == 0) return snprintf(m->entry[i].sha, MANIFEST_SHA, "%s", sha), 1;
    if (m->count >= MANIFEST_MAX) return 0;
    snprintf(m->entry[m->count].name, MANIFEST_NAME, "%s", name);
    snprintf(m->entry[m->count].sha, MANIFEST_SHA, "%s", sha);
    m->count++;
    return 1;
}

int manifest_forget(Manifest *m, const char *name)
{
    for (int i = 0; i < m->count; ++i) {
        if (strcmp(m->entry[i].name, name) != 0) continue;
        memmove(&m->entry[i], &m->entry[i + 1], (size_t)(m->count - i - 1) * sizeof m->entry[0]);
        m->count--;
        return 1;
    }
    return 0;
}

static int by_name(const void *a, const void *b)
{
    return strcmp(((const char *)a), ((const char *)b));   /* the name is the entry's first member */
}

int manifest_save(const char *path, const Manifest *m)
{
    Manifest sorted = *m;
    qsort(sorted.entry, (size_t)sorted.count, sizeof sorted.entry[0], by_name);
    char tmp[PATH];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return 0;
    FILE *f = fopen(tmp, "wb");
    if (f == NULL) return 0;
    int ok = 1;
    for (int i = 0; i < sorted.count; ++i) ok &= fprintf(f, "%s\t%s\n", sorted.entry[i].name, sorted.entry[i].sha) > 0;
    ok &= fclose(f) == 0;
    if (!ok || rename(tmp, path) != 0) return unlink(tmp), 0;
    return 1;
}

int manifest_file_sha(const char *file, char sha[MANIFEST_SHA])
{
    opens++;
    FILE *f = fopen(file, "rb");
    if (f == NULL) return 0;
    Sha256 s;
    sha256_init(&s);
    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) sha256_update(&s, buf, n);
    const int ok = ferror(f) == 0;
    fclose(f);
    if (ok) sha256_hex(&s, sha);
    return ok;
}

static int file_in(char *out, size_t cap, const char *dir, const char *name)
{
    return snprintf(out, cap, "%s/%s", dir, name) < (int)cap;
}

int manifest_record(Manifest *m, const char *dir, const char *name)
{
    char path[PATH], sha[MANIFEST_SHA];
    return file_in(path, sizeof path, dir, name) && manifest_file_sha(path, sha) && manifest_set(m, name, sha);
}

/* the hash of `path`, from the last look when its time and size are the same */
static int sha_cached(const char *path, char sha[MANIFEST_SHA])
{
    static struct { char path[PATH]; long long mtime; long long size; char sha[MANIFEST_SHA]; int used; } cache[SHA_CACHE];
    static int next;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return 0;
    for (int i = 0; i < SHA_CACHE; ++i)
        if (cache[i].used && cache[i].mtime == mtime_of(&st) && cache[i].size == st.st_size && strcmp(cache[i].path, path) == 0) return memcpy(sha, cache[i].sha, MANIFEST_SHA), 1;
    if (!manifest_file_sha(path, sha)) return 0;
    int slot = -1;
    for (int i = 0; i < SHA_CACHE && slot < 0; ++i)
        if (cache[i].used && strcmp(cache[i].path, path) == 0) slot = i;
    if (slot < 0) slot = next, next = (next + 1) % SHA_CACHE;
    if (strlen(path) < sizeof cache[slot].path) {
        snprintf(cache[slot].path, sizeof cache[slot].path, "%s", path);
        cache[slot].mtime = mtime_of(&st), cache[slot].size = st.st_size, cache[slot].used = 1;
        memcpy(cache[slot].sha, sha, MANIFEST_SHA);
    }
    return 1;
}

int manifest_load_cached(const char *path, Manifest *m)
{
    static struct { char path[PATH]; long long mtime; long long size; Manifest m; int used; } last;
    struct stat st;
    if (stat(path, &st) != 0) return last.used = 0, manifest_load(path, m);   /* not there (or unreadable): the plain answer */
    if (last.used && last.mtime == mtime_of(&st) && last.size == st.st_size && strcmp(last.path, path) == 0) return *m = last.m, 1;
    if (!manifest_load(path, m)) return last.used = 0, 0;
    if (strlen(path) < sizeof last.path) snprintf(last.path, sizeof last.path, "%s", path), last.mtime = mtime_of(&st), last.size = st.st_size, last.m = *m, last.used = 1;
    return 1;
}

GenState manifest_state(const Manifest *m, const char *dir, const char *name)
{
    char path[PATH], sha[MANIFEST_SHA];
    if (!file_in(path, sizeof path, dir, name) || !sha_cached(path, sha)) return GEN_MISSING;
    const char *written = manifest_sha(m, name);
    return written == NULL ? GEN_UNKNOWN : strcmp(written, sha) == 0 ? GEN_OK : GEN_EDITED;
}
