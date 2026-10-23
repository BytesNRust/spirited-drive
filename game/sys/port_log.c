/* port: see port_log.h. */
#include "game/sys/port_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static int verbose = -1;   /* -1: not read yet */

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
    if (!port_log_verbose()) return;
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

void port_out(const char *format, ...)
{
    if (!port_log_verbose()) return;
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void port_fatal(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
