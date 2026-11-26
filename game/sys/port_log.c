/* port: see port_log.h. */
#include "game/sys/port_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static int verbose = -1;   /* -1: not read yet */
static FILE *file;
static long file_bytes;

enum { RUNS_KEPT = 5, COPY_CHUNK = 16384 };   /* this run and the four before it */
static int fatal_said;                        /* port_fatal ran in this run: its end is on purpose */
static int hook_set;                          /* the atexit hook is set (once) */
static char log_path[1100];                   /* what this run opened */

/* the file of the run `n` back (0 is `path` itself): "x.log" gives "x-1.log" ... ; a name without ".log" gets ".1" ... */
static void numbered(const char *path, int n, char *out, size_t cap)
{
    const size_t len = strlen(path);
    if (n == 0) snprintf(out, cap, "%s", path);
    else if (len > 4 && strcmp(path + len - 4, ".log") == 0) snprintf(out, cap, "%.*s-%d.log", (int)(len - 4), path, n);
    else snprintf(out, cap, "%s.%d", path, n);
}

/* the file kept for a run that did not end: "x-unclean.log" ("x.unclean" without the ".log") */
static void unclean_name(const char *path, char *out, size_t cap)
{
    const size_t len = strlen(path);
    if (len > 4 && strcmp(path + len - 4, ".log") == 0) snprintf(out, cap, "%.*s-unclean.log", (int)(len - 4), path);
    else snprintf(out, cap, "%s.unclean", path);
}

/* 1 when the log at `path` has a line "exit: ..." (its last 4 KB are looked at: it is the last thing a run writes), 0 when it has none or is not there */
static int log_has_end(const char *path, int *exists)
{
    FILE *f = fopen(path, "rb");
    *exists = f != NULL;
    if (f == NULL) return 0;
    char tail[4096 + 1];
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, size > 4096 ? size - 4096 : 0, SEEK_SET);
    const size_t n = fread(tail, 1, 4096, f);
    fclose(f);
    tail[n] = '\0';
    for (const char *line = tail; line != NULL; line = strchr(line, '\n'), line = line != NULL ? line + 1 : NULL)
        if (strncmp(line, "exit: ", 6) == 0) return 1;
    return 0;
}

static void copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb");
    if (in == NULL) return;
    FILE *out = fopen(to, "wb");
    if (out != NULL) {
        char chunk[COPY_CHUNK];
        size_t n;
        while ((n = fread(chunk, 1, sizeof chunk, in)) > 0 && fwrite(chunk, 1, n, out) == n) {}
        fclose(out);
    }
    fclose(in);
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/'), *back = strrchr(path, '\\');
    if (back != NULL && (slash == NULL || back > slash)) slash = back;
    return slash != NULL ? slash + 1 : path;
}

/* `from` moved to `to`, which is replaced. On Windows MoveFileEx with MOVEFILE_REPLACE_EXISTING (the C library's rename does not replace), and again for up to half a second when it is
 * refused: a scanner (Defender) holds the file for an instant, or another copy of the game has the log open (as art_cache.c's replace_file). 1 when it moved. */
static int move_over(const char *from, const char *to)
{
#ifdef _WIN32
    enum { TRIES = 250, PAUSE_MS = 2 };
    for (int tries = 0; tries < TRIES; tries++) {
        if (MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING)) return 1;
        Sleep(PAUSE_MS);
    }
    return 0;
#else
    remove(to);   /* rename replaces on POSIX; a leftover that cannot be removed fails the rename below */
    return rename(from, to) == 0;
#endif
}

enum { ROTATED, ROTATED_UNCLEAN, HELD };   /* what became of the run that was `path`: moved aside (it ended or did not), or could not be moved */

/* the runs move up one place (the oldest goes). HELD when the current log could not be moved (another copy of the game has it open): the new run then follows it instead of truncating it,
 * and nothing is said of how it ended (it may not have) */
static int rotate(const char *path)
{
    int exists;
    const int ended = log_has_end(path, &exists);
    if (!exists) return ROTATED;
    char from[1100], to[1100];
    numbered(path, RUNS_KEPT - 1, to, sizeof to);
    remove(to);
    for (int n = RUNS_KEPT - 2; n >= 1; n--) {
        numbered(path, n, from, sizeof from);
        numbered(path, n + 1, to, sizeof to);
        move_over(from, to);   /* a failed one only leaves the older run where it was */
    }
    numbered(path, 1, to, sizeof to);
    if (!move_over(path, to)) return HELD;
    if (ended) return ROTATED;
    char kept[1100];
    unclean_name(path, kept, sizeof kept);
    remove(kept);
    copy_file(to, kept);   /* the log of the run that did not end stays beyond the five */
    return ROTATED_UNCLEAN;
}

int port_log_open_file(const char *path)
{
    port_log_close_file();
    if (path == NULL || path[0] == '\0') return 0;
    const int rotated = rotate(path);
    file = fopen(path, rotated == HELD ? "ab" : "wb");   /* binary: the same bytes, and the limit counts the same, on every system */
    if (file == NULL) return 0;
    setvbuf(file, NULL, _IOLBF, 0);   /* a crash keeps what was said */
    file_bytes = 0;
    fatal_said = 0;
    snprintf(log_path, sizeof log_path, "%s", path);
    if (!hook_set) hook_set = atexit(port_log_exit) == 0;
    if (rotated == HELD) port_log("log: the last run's log could not be moved aside (it is open elsewhere?): this run follows it\n");
    if (rotated == ROTATED_UNCLEAN) {
        char kept[1100];
        unclean_name(path, kept, sizeof kept);
        port_log("previous run: did not end cleanly (kept as %s)\n", base_name(kept));
    }
    return 1;
}

void port_log_close_file(void)
{
    if (file != NULL) fclose(file);
    file = NULL;
}

void port_log_exit(void)
{
    if (file == NULL) return;
    fputs(fatal_said ? "exit: after a fatal error\n" : "exit: clean\n", file);   /* past the size limit too: it is how the next run knows this one ended */
    port_log_close_file();
}

static char watch_text[64];
static int watch_found;

void port_log_watch(const char *text)
{
    snprintf(watch_text, sizeof watch_text, "%s", text != NULL ? text : "");
    watch_found = 0;
}

int port_log_watch_hit(void) { return watch_found; }

static void scan(const char *format, va_list args)
{
    char line[512];
    if (watch_text[0] == '\0' || watch_found) return;
    vsnprintf(line, sizeof line, format, args);
    if (strstr(line, watch_text) != NULL) watch_found = 1;
}

static void to_file(const char *format, va_list args)
{
    va_list again;
    va_copy(again, args);
    scan(format, again);
    va_end(again);
    if (file == NULL) return;
    if (file_bytes >= PORT_LOG_FILE_MAX) return;
    const int n = vfprintf(file, format, args);
    if (n > 0) file_bytes += n;
    if (file_bytes >= PORT_LOG_FILE_MAX) fputs("log: stopped at the size limit\n", file);
    fflush(file);   /* Windows' C library does not line-buffer: a crash would keep nothing */
}

int port_log_verbose(void)
{
    if (verbose < 0) {
        const char *const v = getenv("TNFS_VERBOSE");
        verbose = v != NULL && v[0] != '\0' && !(v[0] == '0' && v[1] == '\0');
    }
    return verbose;
}

void port_log_set_verbose(int on) { verbose = on != 0; }

void port_log(const char *format, ...)
{
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    to_file(format, copy);
    va_end(copy);
    if (port_log_verbose()) vfprintf(stderr, format, args);
    va_end(args);
}

void port_log_detail(const char *format, ...)
{
    if (!port_log_verbose()) return;
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    to_file(format, copy);
    va_end(copy);
    vfprintf(stderr, format, args);
    va_end(args);
}

void port_out(const char *format, ...)
{
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    to_file(format, copy);
    va_end(copy);
    if (port_log_verbose()) vprintf(format, args);
    va_end(args);
}

void port_fatal(const char *format, ...)
{
    fatal_said = 1;
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    to_file(format, copy);
    va_end(copy);
    vfprintf(stderr, format, args);
    va_end(args);
}
