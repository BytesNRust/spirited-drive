#include "session.h"

#include "car_project.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
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
        const int car = strncmp(line, "car\t", 4) == 0;
        const char *dir = line + (car ? 4 : 0);
        if (dir[0] == '\0' || strlen(dir) >= SESSION_PATH) continue;
        int again = 0;
        for (int i = 0; i < r->count; ++i) again |= strcmp(r->dir[i], dir) == 0;
        if (again) continue;
        r->kind[r->count] = car ? RECENT_CAR : RECENT_TRACK;
        snprintf(r->dir[r->count++], SESSION_PATH, "%s", dir);
    }
    fclose(f);
}

void recent_push(Recent *r, const char *dir, int kind)
{
    if (dir == NULL || dir[0] == '\0' || strlen(dir) >= SESSION_PATH) return;
    int at = r->count;   /* where it was, or past the end */
    for (int i = 0; i < r->count; ++i)
        if (strcmp(r->dir[i], dir) == 0) at = i;
    if (at == r->count) {
        if (r->count < RECENT_MAX) ++r->count;
        at = r->count - 1;   /* the oldest makes room */
    }
    for (int i = at; i > 0; --i) {
        memcpy(r->dir[i], r->dir[i - 1], SESSION_PATH);
        r->kind[i] = r->kind[i - 1];
    }
    snprintf(r->dir[0], SESSION_PATH, "%s", dir);
    r->kind[0] = kind == RECENT_CAR ? RECENT_CAR : RECENT_TRACK;
}

void recent_remove(Recent *r, int index)
{
    if (index < 0 || index >= r->count) return;
    for (int i = index; i + 1 < r->count; ++i) {
        memcpy(r->dir[i], r->dir[i + 1], SESSION_PATH);
        r->kind[i] = r->kind[i + 1];
    }
    --r->count;
}

void recent_clear(Recent *r, int kind)
{
    for (int i = r->count - 1; i >= 0; --i)
        if (r->kind[i] == kind) recent_remove(r, i);
}

int recent_inside(const char *dir, const char *folder)
{
    if (folder == NULL || folder[0] == '\0') return 1;
    char a[PATH_MAX], b[PATH_MAX];
    if (realpath(dir, a) == NULL) snprintf(a, sizeof a, "%s", dir);   /* a folder that is gone is compared as it was written */
    if (realpath(folder, b) == NULL) snprintf(b, sizeof b, "%s", folder);
    size_t n = strlen(b);
    while (n > 1 && b[n - 1] == '/') b[--n] = '\0';
    if (strncmp(a, b, n) != 0) return 0;
    return a[n] == '\0' || a[n] == '/' || n == 1;   /* "/" holds everything */
}

int recent_save(const Recent *r, const char *file)
{
    if (file == NULL || file[0] == '\0') return 0;
    char tmp[SESSION_PATH + 8];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", file) >= (int)sizeof tmp) return 0;
    FILE *f = fopen(tmp, "w");
    if (f == NULL) return 0;
    int ok = 1;
    for (int i = 0; i < r->count; ++i) ok &= fprintf(f, "%s%s\n", r->kind[i] == RECENT_CAR ? "car\t" : "", r->dir[i]) > 0;
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
    return stat(ini, &st) == 0 && S_ISREG(st.st_mode) && !car_project_is_masks(dir);   /* a masks project has a project.ini too: it is a car's */
}

int recent_present(const char *dir, int kind)
{
    return kind == RECENT_CAR ? car_project_present(dir) : recent_is_project(dir);
}
