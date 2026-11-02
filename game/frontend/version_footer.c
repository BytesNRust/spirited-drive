/* port: see version_footer.h. */
#include "game/frontend/version_footer.h"

#include <stdio.h>

int version_footer_line(char *out, size_t cap, int line, const char *version, UpdateState state, const char *latest)
{
    if (out == NULL || cap == 0) return 0;
    out[0] = 0;
    if (line == 0) {
        if (version == NULL || version[0] == 0) return 0;
        snprintf(out, cap, "v%s", version);
        return 1;
    }
    if (line == 1 && state == UPDATE_NEWER && latest != NULL && latest[0] != 0) {
        snprintf(out, cap, "Update available: v%s", latest);
        return 1;
    }
    return 0;
}
