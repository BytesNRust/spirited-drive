/* port: see port_log.h. */
#include "game/sys/port_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static int verbose = -1;   /* -1: not read yet */
static FILE *file;
static long file_bytes;

int port_log_open_file(const char *path)
{
    port_log_close_file();
    if (path == NULL || path[0] == '\0') return 0;
    char old[1100];
    if (snprintf(old, sizeof old, "%s.1", path) < (int)sizeof old) rename(path, old);   /* the last run's, if there is one */
    file = fopen(path, "w");
    if (file == NULL) return 0;
    setvbuf(file, NULL, _IOLBF, 0);   /* a crash keeps what was said */
    file_bytes = 0;
    return 1;
}

void port_log_close_file(void)
{
    if (file != NULL) fclose(file);
    file = NULL;
}

static void to_file(const char *format, va_list args)
{
    if (file == NULL) return;
    if (file_bytes >= PORT_LOG_FILE_MAX) return;
    const int n = vfprintf(file, format, args);
    if (n > 0) file_bytes += n;
    if (file_bytes >= PORT_LOG_FILE_MAX) fputs("log: stopped at the size limit\n", file);
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
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    to_file(format, copy);
    va_end(copy);
    vfprintf(stderr, format, args);
    va_end(args);
}
