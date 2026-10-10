#include "formats/modpath.h"

#include <ctype.h>
#include <string.h>

static const char *skip_dot_slash(const char *p)
{
    while (p[0] == '.' && p[1] == '/') p += 2;
    return p;
}

static size_t trimmed_length(const char *p)
{
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/') --n;
    return n;
}

int modpath_relative(const char *data_root, const char *path, char *rel, size_t cap)
{
    if (data_root == NULL || path == NULL || rel == NULL || cap == 0) return 0;
    data_root = skip_dot_slash(data_root);
    path = skip_dot_slash(path);
    const size_t root_len = trimmed_length(data_root);
    if (strncmp(path, data_root, root_len) != 0) return 0;
    const char *rest = path + root_len;
    if (*rest != '/' && *rest != '\0') return 0;          /* "originals/x" is not under "original" */
    while (*rest == '/') ++rest;
    if (*rest == '\0') { rel[0] = '\0'; return 1; }

    if (strchr(rest, '\\') != NULL) return 0;
    size_t out = 0;
    const char *p = rest;
    while (*p) {
        const char *end = strchr(p, '/');
        const size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len == 0) { p = end + 1; continue; }            /* a doubled slash */
        if (len == 1 && p[0] == '.') { p = end ? end + 1 : p + len; continue; }
        if (len == 2 && p[0] == '.' && p[1] == '.') return 0;
        if (out + len + 2 > cap) return 0;
        if (out) rel[out++] = '/';
        memcpy(rel + out, p, len);
        out += len;
        p = end ? end + 1 : p + len;
    }
    rel[out] = '\0';
    return 1;
}

int modpath_join(const char *mods, const char *rel, char *out, size_t cap)
{
    const size_t m = trimmed_length(mods), r = strlen(rel);
    if (m + 1 + r + 1 > cap) return 0;
    memcpy(out, mods, m);
    out[m] = '/';
    memcpy(out + m + 1, rel, r + 1);
    return 1;
}

int modpath_upper(const char *rel, char *out, size_t cap)
{
    const size_t n = strlen(rel);
    if (n + 1 > cap) return 0;
    for (size_t i = 0; i <= n; ++i) out[i] = (char)toupper((unsigned char)rel[i]);
    return 1;
}

int modpath_inside(const char *real_root, const char *real_path)
{
    const size_t n = trimmed_length(real_root);
    if (strncmp(real_path, real_root, n) != 0) return 0;
    return real_path[n] == '/' || real_path[n] == '\0' || n == 1;
}
