/* A test helper (macOS): a dylib put in front of the editor (DYLD_INSERT_LIBRARIES) that counts the file calls of the MAIN thread, which is Studio's UI thread: open, fopen,
 * opendir, and stat / lstat / access. At exit it writes "io: open N dir N stat N" to the file named by TNFS_IO_COUNT_OUT. The rule it serves: the UI thread opens no file and reads no
 * directory per frame at rest (the workspace may be on a network or synced folder, where each is slow); tests/editor_ui_io.sh runs a short and a long idle run and compares. */
#include <dirent.h>
#include <execinfo.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static volatile long n_open, n_dir, n_stat;

#define INTERPOSE(replacement, replacee) \
    __attribute__((used)) static struct { const void *a, *b; } interpose_##replacee __attribute__((section("__DATA,__interpose"))) = { (const void *)(unsigned long)&replacement, (const void *)(unsigned long)&replacee }

static int trace_on = -1;

/* TNFS_IO_TRACE=N: the call stack of every main-thread call after the first N calls (to find who does file work at rest) */
static void count(volatile long *n, const char *what, const char *path)
{
    ++*n;
    if (trace_on < 0) trace_on = getenv("TNFS_IO_TRACE") != NULL ? atoi(getenv("TNFS_IO_TRACE")) : 0;
    if (trace_on > 0 && n_open + n_dir + n_stat > trace_on) {
        void *frames[12];
        fprintf(stderr, "io-trace: %s %s\n", what, path != NULL ? path : "");
        backtrace_symbols_fd(frames, backtrace(frames, 12), 2);
    }
}

static int main_thread(void)
{
    return pthread_main_np() == 1;
}

static int my_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    if (main_thread()) count(&n_open, "open", path);
    return open(path, flags, mode);
}

static FILE *my_fopen(const char *path, const char *mode)
{
    if (main_thread()) count(&n_open, "fopen", path);
    return fopen(path, mode);
}

static DIR *my_opendir(const char *path)
{
    if (main_thread()) count(&n_dir, "opendir", path);
    return opendir(path);
}

static int my_stat(const char *path, struct stat *st)
{
    if (main_thread()) count(&n_stat, "stat", path);
    return stat(path, st);
}

static int my_lstat(const char *path, struct stat *st)
{
    if (main_thread()) count(&n_stat, "lstat", path);
    return lstat(path, st);
}

static int my_access(const char *path, int mode)
{
    if (main_thread()) count(&n_stat, "access", path);
    return access(path, mode);
}

INTERPOSE(my_open, open);
INTERPOSE(my_fopen, fopen);
INTERPOSE(my_opendir, opendir);
INTERPOSE(my_stat, stat);
INTERPOSE(my_lstat, lstat);
INTERPOSE(my_access, access);

__attribute__((destructor)) static void report(void)
{
    const char *out = getenv("TNFS_IO_COUNT_OUT");
    FILE *f = out != NULL ? fopen(out, "w") : NULL;
    if (f == NULL) return;
    fprintf(f, "io: open %ld dir %ld stat %ld\n", n_open, n_dir, n_stat);
    fclose(f);
}
