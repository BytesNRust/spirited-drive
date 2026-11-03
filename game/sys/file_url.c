/* port: see file_url.h. */
#include "game/sys/file_url.h"

#include <ctype.h>
#include <string.h>

size_t file_url_from_path(const char *path, char *out, size_t cap)
{
    static const char HEX[] = "0123456789ABCDEF";
    if (path == NULL || path[0] != '/' || cap == 0) return 0;
    size_t n = 0;
    const char *prefix = "file://";
    const size_t plen = strlen(prefix);
    if (plen + 1 > cap) return 0;
    memcpy(out, prefix, plen);
    n = plen;
    for (const unsigned char *p = (const unsigned char *)path; *p != '\0'; p++) {
        const int plain = (*p < 128 && isalnum(*p)) || *p == '-' || *p == '.' || *p == '_' || *p == '~' || *p == '/';
        if (n + (plain ? 1 : 3) + 1 > cap) return 0;
        if (plain) out[n++] = (char)*p;
        else out[n++] = '%', out[n++] = HEX[*p >> 4], out[n++] = HEX[*p & 15];
    }
    out[n] = '\0';
    return n;
}
