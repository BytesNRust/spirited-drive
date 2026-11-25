/* Windows (MinGW, -std=c99): the few POSIX calls the sources use, as a header the build puts in front of every C file on
 * Windows (CMakeLists.txt, -include). The calls keep their POSIX spelling in the code; nothing here is used on macOS or Linux. */
#ifndef TNFS_WIN_POSIX_H
#define TNFS_WIN_POSIX_H
#ifdef _WIN32

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* <io.h> and <sys/stat.h> (which includes it) are left out: <io.h> declares read, write, dup and tell, which the sources use as names of their own.
 * The macros below only need `stat` where they are used. */
int _mkdir(const char *path);
#define mkdir(path, ...) _mkdir(path)         /* the mode has no meaning here; variadic so that <direct.h>'s own one-argument declaration still reads */
#define lstat(path, st) stat(path, st)        /* no links to tell from their targets */
#ifndef S_ISLNK
#define S_ISLNK(m) 0
#endif

static inline int setenv(const char *name, const char *value, int overwrite)
{
    if (!overwrite && getenv(name) != NULL) return 0;
    return _putenv_s(name, value);
}

static inline int unsetenv(const char *name) { return _putenv_s(name, ""); }   /* an empty value removes the variable */

/* the folder a template ending in XXXXXX names, made; the template filled in */
static inline char *mkdtemp(char *tmpl)
{
    static const char CH[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    const size_t n = strlen(tmpl);
    if (n < 6 || strcmp(tmpl + n - 6, "XXXXXX") != 0) return errno = EINVAL, NULL;
    static unsigned seed;
    if (seed == 0) seed = (unsigned)time(NULL) ^ 0x9e3779b9u;
    for (int tries = 0; tries < 200; tries++) {
        for (size_t k = n - 6; k < n; k++) seed = seed * 1103515245u + 12345u, tmpl[k] = CH[(seed >> 16) % (sizeof CH - 1)];
        if (_mkdir(tmpl) == 0) return tmpl;
        if (errno != EEXIST) return NULL;
    }
    return NULL;
}

static inline int symlink(const char *target, const char *link) { (void)target; (void)link; errno = ENOSYS; return -1; }   /* no links */
static inline int geteuid(void) { return 1000; }   /* never root */

static inline char *realpath(const char *path, char *resolved) { return _fullpath(resolved, path, resolved != NULL ? PATH_MAX : 0); }

static inline char *strndup(const char *s, size_t n)
{
    size_t len = 0;
    while (len < n && s[len] != '\0') len++;
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) memcpy(copy, s, len), copy[len] = '\0';
    return copy;
}

static inline void *memmem(const void *hay, size_t hn, const void *needle, size_t nn)
{
    if (nn == 0) return (void *)hay;
    for (size_t i = 0; hn >= nn && i + nn <= hn; i++)
        if (memcmp((const char *)hay + i, needle, nn) == 0) return (char *)hay + i;
    return NULL;
}

#endif
#endif
