/* A long path in one line (path_elide.h) */
#include "path_elide.h"

#include <stdio.h>
#include <string.h>

void path_elide(const char *path, int max, char *out, size_t cap)
{
    if (cap == 0) return;
    const size_t len = strlen(path);
    if (max < 0 || len <= (size_t)max) {
        snprintf(out, cap, "%s", path);
        return;
    }
    const int room = max < PATH_ELIDE_LEAST ? PATH_ELIDE_LEAST : max;
    if (len <= (size_t)room) {   /* no shorter with "..." in it */
        snprintf(out, cap, "%s", path);
        return;
    }
    const int keep = room - 3;              /* the characters of the path that stay: the rest is "..." */
    const int head = (keep - 1) / 2, tail = keep - head;   /* the end a little longer: it names the folder */
    snprintf(out, cap, "%.*s...%s", head, path, path + len - (size_t)tail);
}
