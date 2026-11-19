#include "session.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void recent_load(Recent *r, const char *file)
{
    memset(r, 0, sizeof *r);
    FILE *f = file != NULL && file[0] != '\0' ? fopen(file, "r") : NULL;
    if (f == NULL) return;
    char line[SESSION_PATH + 2];
    while (r->count < RECENT_MAX && fgets(line, sizeof line, f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || strlen(line) >= SESSION_PATH) continue;
        int again = 0;
        for (int i = 0; i < r->count; ++i) again |= strcmp(r->dir[i], line) == 0;
        if (!again) snprintf(r->dir[r->count++], SESSION_PATH, "%s", line);
    }
    fclose(f);
}

void recent_push(Recent *r, const char *dir)
{
    if (dir == NULL || dir[0] == '\0' || strlen(dir) >= SESSION_PATH) return;
    int at = r->count;   /* where it was, or past the end */
    for (int i = 0; i < r->count; ++i)
        if (strcmp(r->dir[i], dir) == 0) at = i;
    if (at == r->count) {
        if (r->count < RECENT_MAX) ++r->count;
        at = r->count - 1;   /* the oldest makes room */
    }
    for (int i = at; i > 0; --i) memcpy(r->dir[i], r->dir[i - 1], SESSION_PATH);
    snprintf(r->dir[0], SESSION_PATH, "%s", dir);
}

int recent_save(const Recent *r, const char *file)
{
    if (file == NULL || file[0] == '\0') return 0;
    char tmp[SESSION_PATH + 8];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", file) >= (int)sizeof tmp) return 0;
    FILE *f = fopen(tmp, "w");
    if (f == NULL) return 0;
    int ok = 1;
    for (int i = 0; i < r->count; ++i) ok &= fprintf(f, "%s\n", r->dir[i]) > 0;
    ok &= fclose(f) == 0;
    if (ok) ok = rename(tmp, file) == 0;
    if (!ok) remove(tmp);
    return ok;
}

int recent_is_project(const char *dir)
{
    char ini[SESSION_PATH + 16];
    struct stat st;
    if (snprintf(ini, sizeof ini, "%s/project.ini", dir) >= (int)sizeof ini) return 0;
    return stat(ini, &st) == 0 && S_ISREG(st.st_mode);
}

/* FNV-1a over the bytes of the values */
static uint64_t mix(uint64_t h, const void *bytes, size_t n)
{
    const unsigned char *p = bytes;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

uint64_t session_fingerprint(const Trace *trace)
{
    uint64_t h = 14695981039346656037ull;
    h = mix(h, &trace->count, sizeof trace->count);
    h = mix(h, &trace->width, sizeof trace->width);
    for (int i = 0; i < trace->count; ++i) {
        h = mix(h, &trace->points[i].x, sizeof trace->points[i].x);
        h = mix(h, &trace->points[i].z, sizeof trace->points[i].z);
    }
    return h;
}
