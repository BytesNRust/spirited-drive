/* port (dep59 step 4): edition_list (edition_file.h), apart from the rest so that reading the edition in use needs no folder listing. */
#include "game/sys/edition_file.h"

#include "game/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { REL_MAX = EDITION_ID_MAX + 32 };

static int by_name(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

int edition_list(char ids[][EDITION_ID_MAX + 1], int cap)
{
    char dir[1024];
    if (!platform_user_dir(dir, sizeof dir)) return 0;
    if (strlen(dir) + sizeof "editions" >= sizeof dir) return 0;
    strcat(dir, "editions");
    int count = 0, n = 0;
    char **names = platform_list_files(dir, "*.ini", &count);
    for (int i = 0; i < count && n < cap; i++) {
        const size_t len = strlen(names[i]);
        if (len <= 4 || len - 4 > EDITION_ID_MAX || strcmp(names[i] + len - 4, ".ini") != 0) continue;   /* (the pattern ignores case) */
        char id[EDITION_ID_MAX + 1], rel[REL_MAX];
        memcpy(id, names[i], len - 4), id[len - 4] = '\0';
        snprintf(rel, sizeof rel, "editions/%s", names[i]);
        if (!edition_id_ok(id) || platform_user_is_dir(rel)) continue;
        snprintf(ids[n++], EDITION_ID_MAX + 1, "%s", id);
    }
    platform_free_names(names);
    qsort(ids, (size_t)n, sizeof ids[0], by_name);
    return n;
}
