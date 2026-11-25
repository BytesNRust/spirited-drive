/* Windows (MinGW, -std=c99): the few POSIX calls the sources use, as a header the build puts in front of every C file on
 * Windows (CMakeLists.txt, -include). The calls keep their POSIX spelling in the code; nothing here is used on macOS or Linux. */
#ifndef TNFS_WIN_POSIX_H
#define TNFS_WIN_POSIX_H
#ifdef _WIN32

#include <direct.h>
#include <io.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>

#define mkdir(path, mode) _mkdir(path)        /* the mode has no meaning here */
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

static inline char *realpath(const char *path, char *resolved) { return _fullpath(resolved, path, resolved != NULL ? PATH_MAX : 0); }

#endif
#endif
