#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include "workspace.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int workspace_path(const char *dir, int kind, const char *name, char *out, size_t cap)
{
    const char *sub = kind == WORKSPACE_KIND_CAR ? "cars" : "tracks";
    const int n = name != NULL && name[0] != '\0' ? snprintf(out, cap, "%s/%s/%s", dir, sub, name) : snprintf(out, cap, "%s/%s", dir, sub);
    return n > 0 && (size_t)n < cap;
}

int workspace_prepare(const char *dir, char *why, size_t why_cap)
{
    if (dir == NULL || dir[0] == '\0' || !is_dir(dir)) return snprintf(why, why_cap, "the workspace must be a folder that exists"), 0;
    for (int kind = 0; kind < 2; ++kind) {
        char sub[4096];
        if (!workspace_path(dir, kind, NULL, sub, sizeof sub)) return snprintf(why, why_cap, "the workspace's path is too long"), 0;
        if (mkdir(sub, 0777) != 0 && !(errno == EEXIST && is_dir(sub))) return snprintf(why, why_cap, "cannot make %s", sub), 0;
    }
    return 1;
}

static int by_name(const void *a, const void *b)
{
    const int c = strcasecmp(a, b);
    return c != 0 ? c : strcmp(a, b);
}

static int list_kind(const char *dir, int kind, WorkspacePresent present, char names[][WORKSPACE_NAME])
{
    char sub[4096];
    int count = 0;
    DIR *d = workspace_path(dir, kind, NULL, sub, sizeof sub) ? opendir(sub) : NULL;
    for (struct dirent *e = d != NULL ? readdir(d) : NULL; e != NULL; e = readdir(d)) {
        char path[4096];
        if (e->d_name[0] == '.' || strlen(e->d_name) >= WORKSPACE_NAME || count == WORKSPACE_LIST) continue;
        if (!workspace_path(dir, kind, e->d_name, path, sizeof path) || !is_dir(path) || !present(path, kind)) continue;
        snprintf(names[count++], WORKSPACE_NAME, "%s", e->d_name);
    }
    if (d != NULL) closedir(d);
    qsort(names, (size_t)count, WORKSPACE_NAME, by_name);
    return count;
}

void workspace_scan(const char *dir, WorkspacePresent present, WorkspaceList *out)
{
    memset(out, 0, sizeof *out);
    if (dir == NULL || dir[0] == '\0') return;
    out->cars = list_kind(dir, WORKSPACE_KIND_CAR, present, out->car);
    out->tracks = list_kind(dir, WORKSPACE_KIND_TRACK, present, out->track);
}

void workspace_start_dir(const char *dir, int kind, const char *open_dir, char *out, size_t cap)
{
    out[0] = '\0';
    if (cap == 0) return;
    if (dir != NULL && dir[0] != '\0') {
        if (!workspace_path(dir, kind, NULL, out, cap)) out[0] = '\0';
        return;
    }
    if (open_dir == NULL) return;
    snprintf(out, cap, "%s", open_dir);
    char *slash = strrchr(out, '/');
    if (slash != NULL && slash != out) *slash = '\0';
    else out[0] = '\0';
}

void workspace_load(const char *file, char *dir, size_t cap, int *put_aside)
{
    dir[0] = '\0';
    *put_aside = 0;
    FILE *f = file != NULL && file[0] != '\0' ? fopen(file, "r") : NULL;
    if (f == NULL) return;
    char line[4096 + 16];
    while (fgets(line, sizeof line, f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "dir\t", 4) == 0 && strlen(line + 4) < cap) snprintf(dir, cap, "%s", line + 4);
        else if (strcmp(line, "put aside\t1") == 0) *put_aside = 1;
    }
    fclose(f);
}

static int write_settings(const char *file, const char *dir, int put_aside, const char *repo)
{
    if (file == NULL || file[0] == '\0') return 0;
    char tmp[4096 + 8];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", file) >= (int)sizeof tmp) return 0;
    FILE *f = fopen(tmp, "w");
    if (f == NULL) return 0;
    int ok = 1;
    if (dir != NULL && dir[0] != '\0') ok &= fprintf(f, "dir\t%s\n", dir) > 0;
    if (put_aside) ok &= fprintf(f, "put aside\t1\n") > 0;
    if (repo != NULL && repo[0] != '\0') ok &= fprintf(f, "repo data\t%s\n", repo) > 0;
    ok &= fclose(f) == 0;
    if (ok) ok = rename(tmp, file) == 0;
    if (!ok) remove(tmp);
    return ok;
}

void workspace_load_repo(const char *file, char *repo, size_t cap)
{
    repo[0] = '\0';
    FILE *f = file != NULL && file[0] != '\0' ? fopen(file, "r") : NULL;
    if (f == NULL) return;
    char line[4096 + 16];
    while (fgets(line, sizeof line, f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "repo data\t", 10) == 0 && strlen(line + 10) < cap) snprintf(repo, cap, "%s", line + 10);
    }
    fclose(f);
}

int workspace_save(const char *file, const char *dir, int put_aside)
{
    char repo[4096];
    workspace_load_repo(file, repo, sizeof repo);   // the repo's path is not this call's to change
    return write_settings(file, dir, put_aside, repo);
}

int workspace_save_repo(const char *file, const char *repo)
{
    char dir[4096];
    int put_aside = 0;
    workspace_load(file, dir, sizeof dir, &put_aside);
    return write_settings(file, dir, put_aside, repo);
}

int workspace_id_from_name(const char *name, char *id, size_t cap, size_t max)
{
    size_t n = 0;
    int pending = 0;   /* a separator is due before the next letter */
    for (const char *p = name; p != NULL && *p != '\0' && n < max && n + 1 < cap; ++p) {
        const char ch = (*p >= 'A' && *p <= 'Z') ? (char)(*p - 'A' + 'a') : *p;
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
            if (pending && n > 0 && n + 1 < max && n + 2 < cap) id[n++] = '-';
            pending = 0;
            id[n++] = ch;
        } else pending = 1;
    }
    id[n] = '\0';
    return n > 0;
}
