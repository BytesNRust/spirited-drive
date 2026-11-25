/* port: see file_url.h. */
#include "game/sys/file_url.h"

#include <ctype.h>
#include <string.h>

/* a Windows path: a drive letter and a colon then a separator ("C:\\Player\\x", "D:/y"), or a network share ("\\\\server\\share\\x") */
static int is_drive_path(const char *p) { return isalpha((unsigned char)p[0]) && p[1] == ':' && (p[2] == '\\' || p[2] == '/'); }
static int is_share_path(const char *p) { return p[0] == '\\' && p[1] == '\\' && p[2] != '\0' && p[2] != '\\'; }

size_t file_url_from_path(const char *path, char *out, size_t cap)
{
    static const char HEX[] = "0123456789ABCDEF";
    if (path == NULL || cap == 0) return 0;
    const int drive = is_drive_path(path), share = is_share_path(path);
    if (path[0] != '/' && !drive && !share) return 0;
    const char *const prefix = drive ? "file:///" : (share ? "file:" : "file://");   /* a share: file://server/share/... */
    const size_t plen = strlen(prefix);
    if (plen + 1 > cap) return 0;
    memcpy(out, prefix, plen);
    size_t n = plen;
    const int windows = drive || share;   /* a backslash is a separator there, and an ordinary (escaped) byte of a name elsewhere */
    size_t at = 0;
    for (const unsigned char *p = (const unsigned char *)path; *p != '\0'; p++, at++) {
        const int separator = *p == '/' || (windows && *p == '\\');
        const int plain = separator || (*p < 128 && isalnum(*p)) || *p == '-' || *p == '.' || *p == '_' || *p == '~' || (drive && at == 1 && *p == ':');
        if (n + (plain ? 1 : 3) + 1 > cap) return 0;
        if (separator) out[n++] = '/';
        else if (plain) out[n++] = (char)*p;
        else out[n++] = '%', out[n++] = HEX[*p >> 4], out[n++] = HEX[*p & 15];
    }
    out[n] = '\0';
    return n;
}
