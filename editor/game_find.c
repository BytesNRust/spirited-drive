#include "game_find.h"

#include <stdio.h>
#include <string.h>

/* `out` gets `path` when it is a file; `tried` is told in any case. 1 when found */
static int look(const GameFinder *f, const char *path, char *out, size_t cap, char *tried, size_t tried_cap)
{
    if (tried != NULL && tried_cap > 0) {
        const size_t n = strlen(tried);
        snprintf(tried + n, tried_cap - n, "%s%s", n > 0 ? ", " : "", path);
    }
    if (!f->is_file(path, f->ctx)) return 0;
    snprintf(out, cap, "%s", path);
    return 1;
}

int game_find(const GameFinder *f, char *out, size_t cap, char *tried, size_t tried_cap)
{
    char path[GAME_FIND_PATH];
    if (tried != NULL && tried_cap > 0) tried[0] = '\0';
    if (f->remembered != NULL && f->remembered[0] != '\0' && look(f, f->remembered, out, cap, tried, tried_cap)) return 1;
    if (f->base != NULL && strstr(f->base, "/AppTranslocation/") == NULL) {
        if (snprintf(path, sizeof path, "%s../../../%s", f->base, GAME_FIND_APP_EXE) < (int)sizeof path && look(f, path, out, cap, tried, tried_cap)) return 1;
    }
    if (snprintf(path, sizeof path, "/Applications/%s", GAME_FIND_APP_EXE) < (int)sizeof path && look(f, path, out, cap, tried, tried_cap)) return 1;
    if (f->home != NULL && snprintf(path, sizeof path, "%sApplications/%s", f->home, GAME_FIND_APP_EXE) < (int)sizeof path &&
        look(f, path, out, cap, tried, tried_cap)) return 1;
    char app[GAME_FIND_PATH];
    if (f->by_bundle_id != NULL && f->by_bundle_id(app, sizeof app, f->ctx) && game_find_program_of(app, path, sizeof path) &&
        look(f, path, out, cap, tried, tried_cap)) return 1;
    if (look(f, "./tnfs", out, cap, tried, tried_cap)) return 1;
    snprintf(out, cap, "./tnfs");
    return 0;
}

int game_find_is_app_program(const char *path)
{
    static const char TAIL[] = ".app/Contents/MacOS/tnfs";
    const size_t n = path != NULL ? strlen(path) : 0;
    return n >= sizeof TAIL - 1 && strcmp(path + n - (sizeof TAIL - 1), TAIL) == 0;
}

int game_find_program_of(const char *picked, char *out, size_t cap)
{
    if (picked == NULL) return 0;
    size_t n = strlen(picked);
    if (n == 0 || n >= GAME_FIND_PATH) return 0;
    if (game_find_is_app_program(picked)) return snprintf(out, cap, "%s", picked) < (int)cap;
    while (n > 1 && picked[n - 1] == '/') n--;
    if (n < 5 || strncmp(picked + n - 4, ".app", 4) != 0) return 0;
    return snprintf(out, cap, "%.*s/Contents/MacOS/tnfs", (int)n, picked) < (int)cap;
}
