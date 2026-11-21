/* A program run from Studio in the background (dep59 car editor E2): its output (both streams) kept for a log panel, polled every frame, stopped by SIGTERM to its
 * whole process group, its exit code told. POSIX only, no window. */
#ifndef TNFS_TRACK_EDITOR_PROC_RUN_H
#define TNFS_TRACK_EDITOR_PROC_RUN_H

#include <stddef.h>
#include <sys/types.h>

enum { PROC_LOG_MAX = 65536 };

typedef struct {
    int running;
    int finished;              /* it ran and ended: `exit_code` is told */
    int exit_code;             /* its status; 128 + the signal when it was ended by one */
    int stopped;               /* Stop was asked for */
    pid_t pid;
    int fd;                    /* the read end of its output, -1 when closed */
    char *log;                 /* malloc'd, `log_len` bytes, NUL terminated: its output; the last PROC_LOG_MAX */
    size_t log_len;
    int log_cut;               /* the start of the output was dropped */
} ProcRun;

/* Runs argv[0] (a path) with `argv` and the environment `envp` (both NULL terminated), no input, in its own process group; `cwd` NULL keeps the folder.
 * 1 when it started, 0 with `why`. */
int proc_start(ProcRun *p, char *const argv[], char *const envp[], const char *cwd, char *why, size_t why_cap);
/* takes what it has written and sees whether it ended: 1 while it runs; when it ends `finished` and `exit_code` are set */
int proc_poll(ProcRun *p);
/* asks it to end (SIGTERM to its whole group); proc_poll tells when it has */
void proc_stop(ProcRun *p);
/* ends it if it runs (and waits), frees the log, leaves the struct ready for proc_start */
void proc_free(ProcRun *p);

#endif
