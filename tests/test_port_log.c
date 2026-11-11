/* The console log (port_log.h): quiet unless TNFS_VERBOSE is set or asked for, fatal errors always. */
#include "game/sys/port_log.h"
#include "test.h"

#include <stdio.h>
#include <fcntl.h>
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

static long size_of(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return -1;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fclose(f);
    return n;
}

/* the bug-report file: every line goes into it, verbose or not; a new run keeps the last one beside it; it stops at its limit */
static void test_log_file(void)
{
    char dir[] = "/tmp/tnfs_port_log_dir_XXXXXX";
    CHECK(mkdtemp(dir) != NULL);
    char path[200], old[210];
    snprintf(path, sizeof path, "%s/tnfs.log", dir);
    snprintf(old, sizeof old, "%s.1", path);
    port_log_set_verbose(0);
    CHECK_EQ_INT(port_log_open_file(path), 1);
    CHECK_EQ_INT((int)written(log_one, 2), 0);   /* the console is as quiet as it was */
    port_log("log %d\n", 2);
    port_out("out %d\n", 3);
    port_fatal("fatal %d\n", 4);
    CHECK_EQ_INT((int)size_of(path), 6 + 6 + 6 + 8);   /* log_one's, then the three: all kept */
    CHECK_EQ_INT((int)size_of(old), -1);
    CHECK_EQ_INT(port_log_open_file(path), 1);   /* the next run */
    CHECK_EQ_INT((int)size_of(old), 26);          /* the last one kept */
    CHECK_EQ_INT((int)size_of(path), 0);
    for (long n = 0; n < PORT_LOG_FILE_MAX / 1000 + 50; n++)
        port_log("%0999d\n", 7);
    CHECK(size_of(path) < PORT_LOG_FILE_MAX + 2000);   /* the limit holds (at most a line over), with its own line */
    CHECK(size_of(path) >= PORT_LOG_FILE_MAX);
    port_log_close_file();
    port_log("after close\n");   /* no file: no crash */
    CHECK_EQ_INT(port_log_open_file("/no/such/dir/tnfs.log"), 0);   /* the game goes on without */
    unlink(path);
    unlink(old);
    rmdir(dir);
}

/* the details (port_log_detail): a line for a developer, not for the player or a bug report: in the file only with TNFS_VERBOSE, and on the console then too */
static void detail_one(void) { port_log_detail("detail %d\n", 1); }
static void test_detail(void)
{
    char dir[] = "/tmp/tnfs_port_log_detail_XXXXXX";
    CHECK(mkdtemp(dir) != NULL);
    char path[200], old[210];
    snprintf(path, sizeof path, "%s/tnfs.log", dir);
    snprintf(old, sizeof old, "%s.1", path);
    port_log_set_verbose(0);
    CHECK_EQ_INT(port_log_open_file(path), 1);
    CHECK_EQ_INT((int)written(detail_one, 2), 0);   /* quiet on the console */
    CHECK_EQ_INT((int)size_of(path), 0);            /* and not in the file */
    port_log_set_verbose(1);
    CHECK(written(detail_one, 2) > 0);              /* verbose: on the console */
    CHECK_EQ_INT((int)size_of(path), 9);            /* and in the file */
    port_log_set_verbose(0);
    port_log_close_file();
    unlink(path);
    unlink(old);
    rmdir(dir);
}

/* the scripted input's WAIT: a line containing the text, logged after the watch was armed, from any of the three, quiet or not */
static void test_watch(void)
{
    port_log_set_verbose(0);
    port_log("mark:before\n");
    port_log_watch("mark:before");
    CHECK_EQ_INT(port_log_watch_hit(), 0);                   /* lines logged before arming do not count */
    port_log("something else %d\n", 3);
    CHECK_EQ_INT(port_log_watch_hit(), 0);
    port_log("mark:before humans=2\n");
    CHECK_EQ_INT(port_log_watch_hit(), 1);                   /* a substring of a line, though the console is quiet */
    port_log_watch("mark:out");
    CHECK_EQ_INT(port_log_watch_hit(), 0);                   /* arming again clears it */
    port_out("mark:out\n");
    CHECK_EQ_INT(port_log_watch_hit(), 1);
    port_log_watch("mark:fatal");
    fflush(NULL);
    const int saved = dup(2);
    const int quiet = open("/dev/null", 1);
    dup2(quiet, 2);
    port_fatal("mark:fatal\n");
    dup2(saved, 2), close(saved), close(quiet);
    CHECK_EQ_INT(port_log_watch_hit(), 1);
    port_log_watch("");
    port_log("x\n");
    CHECK_EQ_INT(port_log_watch_hit(), 0);                   /* nothing watched */
}

int main(void)
{
    test_detail();
    test_watch();
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
    test_log_file();
    return TEST_REPORT();
}
