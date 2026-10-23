/* The console log (port_log.h): quiet unless TNFS_VERBOSE is set or asked for, fatal errors always. */
#include "game/sys/port_log.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* what `f` wrote to stderr / stdout, read back through a file */
static long written(void (*f)(void), int fd)
{
    char name[] = "/tmp/tnfs_port_log_XXXXXX";
    const int tmp = mkstemp(name);
    fflush(NULL);
    const int saved = dup(fd);
    dup2(tmp, fd);
    f();
    fflush(NULL);
    dup2(saved, fd);
    close(saved);
    const long n = (long)lseek(tmp, 0, SEEK_END);
    close(tmp);
    unlink(name);
    return n;
}
static void log_one(void) { port_log("log %d\n", 1); }
static void out_one(void) { port_out("out %d\n", 1); }
static void fatal_one(void) { port_fatal("fatal %d\n", 1); }

int main(void)
{
    unsetenv("TNFS_VERBOSE");
    port_log_set_verbose(0);
    CHECK_EQ_INT(port_log_verbose(), 0);
    CHECK_EQ_INT((int)written(log_one, 2), 0);   /* quiet */
    CHECK_EQ_INT((int)written(out_one, 1), 0);
    CHECK(written(fatal_one, 2) > 0);            /* but a fatal error is said */
    port_log_set_verbose(1);
    CHECK_EQ_INT(port_log_verbose(), 1);
    CHECK_EQ_INT((int)written(log_one, 2), 6);
    CHECK_EQ_INT((int)written(out_one, 1), 6);
    CHECK_EQ_INT((int)written(fatal_one, 2), 8);
    return TEST_REPORT();
}
