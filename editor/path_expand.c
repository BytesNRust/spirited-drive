/* A path as a person types it (path_expand.h). */
#include "path_expand.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { EXPANDED_SLOTS = 16, EXPANDED_CAP = 4096 };

int path_expand(const char *in, const char *home, char *out, size_t cap)
{
    if (cap == 0) return 0;
    out[0] = '\0';
    if (in == NULL) return 0;
    size_t skip = 0;
    if (in[0] == '~' && (in[1] == '\0' || in[1] == '/')) skip = 1;
    else if (strncmp(in, "$HOME", 5) == 0 && (in[5] == '\0' || in[5] == '/')) skip = 5;
    else if (strncmp(in, "${HOME}", 7) == 0 && (in[7] == '\0' || in[7] == '/')) skip = 7;
    if (skip == 0 || home == NULL || home[0] == '\0') return snprintf(out, cap, "%s", in), 0;
    size_t n = strlen(home);
    while (n > 1 && home[n - 1] == '/') --n;   /* "/home/x/" + "/y" is one slash */
    snprintf(out, cap, "%.*s%s", (int)n, home, in + skip);
    return 1;
}

const char *path_expand_arg(const char *in)
{
    static char slots[EXPANDED_SLOTS][EXPANDED_CAP];
    static int next;
    if (in == NULL) return NULL;
    char *const out = slots[next];
    if (!path_expand(in, getenv("HOME"), out, EXPANDED_CAP)) return in;
    next = (next + 1) % EXPANDED_SLOTS;
    return out;
}
