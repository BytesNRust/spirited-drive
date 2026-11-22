/* The manifest of generated masks: see psd_manifest.h. */
#include "psd_manifest.h"

#include "game/sys/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { PATH = 4096 };

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

GenState manifest_state(const Manifest *m, const char *dir, const char *name)
{
    char path[PATH], sha[MANIFEST_SHA];
    if (!file_in(path, sizeof path, dir, name) || !manifest_file_sha(path, sha)) return GEN_MISSING;
    const char *written = manifest_sha(m, name);
    return written == NULL ? GEN_UNKNOWN : strcmp(written, sha) == 0 ? GEN_OK : GEN_EDITED;
}
