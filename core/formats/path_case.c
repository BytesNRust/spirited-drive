/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "formats/path_case.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

enum { FOUND_MAX = 2048 };

/* the name in `dir` that is `part` (`len` bytes): `part` itself when it is there, else the first that differs in case only; 0 for none */
static int name_in(const char *dir, const char *part, size_t len, char *name, size_t cap)
{
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    int found = 0, exact = 0;
    for (const struct dirent *e = readdir(d); e != NULL && !exact; e = readdir(d)) {
        if (strlen(e->d_name) != len || len + 1 > cap || strncasecmp(e->d_name, part, len) != 0) continue;
        exact = memcmp(e->d_name, part, len) == 0;
        if (exact || !found) memcpy(name, e->d_name, len + 1);
        found = 1;
    }
    closedir(d);
    return found;
}

int path_find_case(const char *dir, const char *rel, char *out, size_t cap)
{
    char cur[FOUND_MAX];
    if (snprintf(cur, sizeof cur, "%s", dir) >= (int)sizeof cur) return 0;
    size_t at = strlen(cur);
    for (const char *part = rel; *part != '\0';) {
        const size_t len = strcspn(part, "/");
        char name[512];
        if (!name_in(cur, part, len, name, sizeof name) || at + 1 + len + 1 > sizeof cur) return 0;
        cur[at++] = '/';
        memcpy(cur + at, name, len + 1);
        at += len;
        part += len + (part[len] == '/');
    }
    return snprintf(out, cap, "%s", cur) < (int)cap;
}

int path_spelled(const char *path, char *out, size_t cap)
{
    struct stat st;
    if (path == NULL || path[0] == '\0') return 0;
    if (stat(path, &st) == 0) return snprintf(out, cap, "%s", path) < (int)cap;
    char there[FOUND_MAX];
    if (snprintf(there, sizeof there, "%s", path) >= (int)sizeof there) return 0;
    for (char *slash = strrchr(there, '/'); slash != NULL && slash != there; slash = strrchr(there, '/')) {
        *slash = '\0';
        if (stat(there, &st) == 0) return path_find_case(there, path + (slash - there) + 1, out, cap);
    }
    return path[0] != '/' && path_find_case(".", path, out, cap);   /* a relative path none of whose folders is spelt as written */
}
