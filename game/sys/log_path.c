/* port: see log_path.h. */
#include "game/sys/log_path.h"

#include <stdio.h>
#include <string.h>

static int is_separator(char c) { return c == '/' || c == '\\'; }

/* the length of `folder` without trailing separators, when `path` starts with it as whole names; else 0 */
static size_t prefix_of(const char *path, const char *folder)
{
    if (folder == NULL) return 0;
    size_t n = strlen(folder);
    while (n > 0 && is_separator(folder[n - 1])) n--;
    if (n == 0 || strncmp(path, folder, n) != 0) return 0;
    return path[n] == '\0' || is_separator(path[n]) ? n : 0;
}

void log_path_for_report(const char *path, const char *home, const char *user_dir, char *out, size_t cap)
{
    if (cap == 0) return;
    if (path == NULL) path = "";
    size_t n = prefix_of(path, user_dir);
    if (n != 0) {
        snprintf(out, cap, "<user>%s", path + n);
        return;
    }
    n = prefix_of(path, home);
    if (n != 0) snprintf(out, cap, "~%s", path + n);
    else snprintf(out, cap, "%s", path);
}
