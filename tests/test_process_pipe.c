/* SPDX-License-Identifier: GPL-3.0-or-later */
/* process_pipe: a program started and its standard output read (on Windows with CreateProcess and no shell, no window): this test program is its own child ("--child WORD" writes WORD and some error text; "--exit N" exits N). */
#include "game/platform/sdl3/process_pipe.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static const char *self;

static size_t run(const char *args, char *out, size_t cap, int *status)
{
    char line[1200];
    snprintf(line, sizeof line, "\"%s\" %s", self, args);
    ProcessPipe *p = process_pipe_open(line);
    if (p == NULL) return (size_t)-1;
    size_t n = 0, got;
    while (n + 1 < cap && (got = process_pipe_read(p, out + n, cap - 1 - n)) > 0) n += got;
    out[n] = '\0';
    *status = process_pipe_close(p);
    return n;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "--child") == 0) {
        fputs(argv[2], stdout);
        fputs("this is standard error", stderr);
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "--exit") == 0) return argv[2][0] - '0';
    self = argv[0];
    char out[256];
    int status = -2;
    CHECK(run("--child hello", out, sizeof out, &status) == 5);
    CHECK(strcmp(out, "hello") == 0);   /* its standard error is not in the output */
    CHECK_EQ_INT(status, 0);
    CHECK(run("--child \"two words\"", out, sizeof out, &status) == 9);   /* the quoting survives */
    CHECK(strcmp(out, "two words") == 0);
    CHECK(run("--exit 3", out, sizeof out, &status) == 0);
    CHECK_EQ_INT(status, -1);   /* a failing exit */
    ProcessPipe *none = process_pipe_open("\"/no/such/program-for-tnfs\" x");
    if (none != NULL) {   /* popen runs a shell: the failure is its exit status */
        char buf[16];
        CHECK_EQ_INT((int)process_pipe_read(none, buf, sizeof buf), 0);
        CHECK_EQ_INT(process_pipe_close(none), -1);
    }
    CHECK_EQ_INT((int)process_pipe_read(NULL, out, sizeof out), 0);
    CHECK_EQ_INT(process_pipe_close(NULL), -1);
    return TEST_REPORT();
}
