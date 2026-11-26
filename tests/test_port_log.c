/* The console log (port_log.h): quiet unless TNFS_VERBOSE is set or asked for, fatal errors always. */
#include "game/sys/port_log.h"
#include "test.h"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

/* the line a run opens with when the one before it did not end cleanly (the size of the file it makes is what the first test counts) */
#define UNCLEAN_LINE "previous run: did not end cleanly (kept as spirited-drive-unclean.log)\n"

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
    snprintf(path, sizeof path, "%s/spirited-drive.log", dir);
    snprintf(old, sizeof old, "%s/spirited-drive-1.log", dir);
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
    CHECK_EQ_INT((int)size_of(path), (int)strlen(UNCLEAN_LINE));   /* its first line says it did not end cleanly (it was not closed) */
    for (long n = 0; n < PORT_LOG_FILE_MAX / 1000 + 50; n++)
        port_log("%0999d\n", 7);
    CHECK(size_of(path) < PORT_LOG_FILE_MAX + 2000);   /* the limit holds (at most a line over), with its own line */
    CHECK(size_of(path) >= PORT_LOG_FILE_MAX);
    port_log_close_file();
    port_log("after close\n");   /* no file: no crash */
    CHECK_EQ_INT(port_log_open_file("/no/such/dir/spirited-drive.log"), 0);   /* the game goes on without */
    unlink(path);
    unlink(old);
    rmdir(dir);
}

/* the whole text of `path`, "" when it is not there */
static char *text_of(const char *path, char *buf, size_t cap)
{
    buf[0] = '\0';
    FILE *f = fopen(path, "rb");
    if (f == NULL) return buf;
    const size_t n = fread(buf, 1, cap - 1, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static void numbered(const char *dir, int n, char *out, size_t cap)
{
    if (n == 0) snprintf(out, cap, "%s/spirited-drive.log", dir);
    else if (n > 0) snprintf(out, cap, "%s/spirited-drive-%d.log", dir, n);
    else snprintf(out, cap, "%s/spirited-drive-unclean.log", dir);
}

/* five runs are kept: this one and the four before it, in files a bug report can attach (.log); a sixth run drops the oldest */
static void test_five_runs(void)
{
    char dir[] = "/tmp/tnfs_port_log_five_XXXXXX";
    CHECK(mkdtemp(dir) != NULL);
    char path[220], other[220], text[256];
    numbered(dir, 0, path, sizeof path);
    port_log_set_verbose(0);
    for (int run = 1; run <= 6; run++) {
        CHECK_EQ_INT(port_log_open_file(path), 1);
        port_log("run %d\n", run);
        port_log_exit();
    }
    CHECK(strstr(text_of(path, text, sizeof text), "run 6\n") != NULL);
    for (int n = 1; n <= 4; n++) {
        numbered(dir, n, other, sizeof other);
        char want[16];
        snprintf(want, sizeof want, "run %d\n", 6 - n);
        CHECK(strstr(text_of(other, text, sizeof text), want) != NULL);
    }
    snprintf(other, sizeof other, "%s/spirited-drive-5.log", dir);
    CHECK_EQ_INT((int)size_of(other), -1);   /* only five: the first run is gone */
    for (int n = -1; n <= 5; n++) {
        numbered(dir, n, other, sizeof other);
        unlink(other);
    }
    rmdir(dir);
}

/* the marker: a run that says "exit: clean" ended; one that does not (a crash, a kill, a power cut) is said to have not, in the next run's first line, and its log is kept apart */
static void test_unclean_marker(void)
{
    char dir[] = "/tmp/tnfs_port_log_unclean_XXXXXX";
    CHECK(mkdtemp(dir) != NULL);
    char path[220], one[220], unclean[220], text[512];
    numbered(dir, 0, path, sizeof path);
    numbered(dir, 1, one, sizeof one);
    numbered(dir, -1, unclean, sizeof unclean);
    port_log_set_verbose(0);
    CHECK_EQ_INT(port_log_open_file(path), 1);   /* the first run ever: nothing before it to be unclean */
    CHECK_EQ_INT((int)size_of(path), 0);
    port_log("run 1\n");
    port_log_exit();
    CHECK(strstr(text_of(path, text, sizeof text), "run 1\nexit: clean\n") != NULL);
    CHECK_EQ_INT(port_log_open_file(path), 1);   /* after a clean end: no marker */
    CHECK_EQ_INT((int)size_of(path), 0);
    CHECK_EQ_INT((int)size_of(unclean), -1);
    port_log("run 2\n");                        /* run 2 is never closed */
    CHECK_EQ_INT(port_log_open_file(path), 1);
    CHECK(strcmp(text_of(path, text, sizeof text), UNCLEAN_LINE) == 0);
    CHECK(strstr(text_of(unclean, text, sizeof text), "run 2\n") != NULL);   /* its log is kept apart */
    CHECK(strstr(text_of(one, text, sizeof text), "run 2\n") != NULL);       /* and in its place in the five */
    port_log("run 3\n");
    port_fatal("fatal\n");                       /* a fatal error ends the run on purpose: it says so, and that counts as an end */
    port_log_exit();
    CHECK(strstr(text_of(path, text, sizeof text), "exit: after a fatal error\n") != NULL);
    CHECK_EQ_INT(port_log_open_file(path), 1);
    CHECK_EQ_INT((int)size_of(path), 0);
    CHECK(strstr(text_of(unclean, text, sizeof text), "run 2\n") != NULL);   /* the unclean one stays until another comes */
    port_log("run 4\n");
    CHECK_EQ_INT(port_log_open_file(path), 1);   /* run 4 was not closed */
    CHECK(strstr(text_of(unclean, text, sizeof text), "run 4\n") != NULL);   /* the next unclean one replaced it */
    CHECK(strstr(text_of(unclean, text, sizeof text), "run 2\n") == NULL);
    for (int n = -1; n <= 4; n++) {
        numbered(dir, n, one, sizeof one);
        unlink(one);
    }
    rmdir(dir);
}

/* a log that cannot be moved aside (another copy of the game, a scanner, has it): the new run follows it instead of truncating it, and it is not called unclean (a running game has no end yet).
 * Here the folder cannot be written to, which no move survives (a file in it can still be opened; skipped as root, which ignores that) */
static void test_log_held(void)
{
    if (geteuid() == 0) return;
    char dir[] = "/tmp/tnfs_port_log_held_XXXXXX";
    CHECK(mkdtemp(dir) != NULL);
    char path[220], other[260], text[512];
    numbered(dir, 0, path, sizeof path);
    FILE *last = fopen(path, "wb");   /* the other copy's log, with no end yet */
    CHECK(last != NULL);
    if (last != NULL) fputs("other run\n", last), fclose(last);
    CHECK(chmod(dir, 0500) == 0);
    port_log_set_verbose(0);
    CHECK_EQ_INT(port_log_open_file(path), 1);
    port_log("this run\n");
    port_log_exit();
    CHECK(chmod(dir, 0700) == 0);
    text_of(path, text, sizeof text);
    CHECK(strncmp(text, "other run\n", 10) == 0);                   /* what was there is still there */
    CHECK(strstr(text, "could not be moved aside") != NULL);        /* and the log says so */
    CHECK(strstr(text, "this run\nexit: clean\n") != NULL);          /* this run follows it */
    CHECK(strstr(text, "did not end cleanly") == NULL);              /* not called a crash */
    snprintf(other, sizeof other, "%s/spirited-drive-unclean.log", dir);
    CHECK_EQ_INT((int)size_of(other), -1);
    unlink(path);
    rmdir(dir);
}

/* a log that stopped at the size limit still gets its end */
static void test_exit_after_limit(void)
{
    char dir[] = "/tmp/tnfs_port_log_limit_XXXXXX";
    CHECK(mkdtemp(dir) != NULL);
    char path[220], text[8];
    numbered(dir, 0, path, sizeof path);
    port_log_set_verbose(0);
    CHECK_EQ_INT(port_log_open_file(path), 1);
    for (long n = 0; n < PORT_LOG_FILE_MAX / 1000 + 50; n++)
        port_log("%0999d\n", 7);
    port_log_exit();
    FILE *f = fopen(path, "rb");
    CHECK(f != NULL);
    char tail[32] = "";
    fseek(f, -12, SEEK_END);
    CHECK(fread(tail, 1, 12, f) == 12);
    fclose(f);
    CHECK(strstr(tail, "exit: clean") != NULL);
    (void)text;
    unlink(path);
    rmdir(dir);
}

/* the details (port_log_detail): a line for a developer, not for the player or a bug report: in the file only with TNFS_VERBOSE, and on the console then too */
static void detail_one(void) { port_log_detail("detail %d\n", 1); }
static void test_detail(void)
{
    char dir[] = "/tmp/tnfs_port_log_detail_XXXXXX";
    CHECK(mkdtemp(dir) != NULL);
    char path[200], old[210];
    snprintf(path, sizeof path, "%s/spirited-drive.log", dir);
    snprintf(old, sizeof old, "%s/spirited-drive-1.log", dir);
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
    test_five_runs();
    test_unclean_marker();
    test_exit_after_limit();
    test_log_held();
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
