/* Rehacer (dep59 car editor 1f): a car kit's own rehacer.command, run from Studio. Before it runs the kit is copied into .studio-backup/<time>/ (the last
 * REHACER_KEEP are kept); it runs with the kit as its folder, its output (both streams) is kept for a log panel, and its exit code is told. POSIX only, no
 * window. */
#ifndef TNFS_TRACK_EDITOR_CAR_REHACER_H
#define TNFS_TRACK_EDITOR_CAR_REHACER_H

#include "proc_run.h"

#include <stddef.h>
#include <sys/types.h>

enum {
    REHACER_KEEP = 5,                       /* backups kept */
    REHACER_LOG_MAX = PROC_LOG_MAX,         /* bytes of output kept (the last ones) */
    REHACER_PATH = 4096,
    REHACER_BACKUP_MAX_MB = 512             /* a kit bigger than this is not backed up, so Rehacer does not run */
};

extern const char REHACER_SCRIPT[];         /* "rehacer.command" */
extern const char REHACER_BACKUPS[];        /* ".studio-backup" */

/* 1 when `kit` holds rehacer.command (a file) */
int rehacer_script_present(const char *kit);

/* "YYYYMMDD-HHMMSS" of the local time now */
void rehacer_stamp(char *out, size_t cap);

/* Copies everything in `kit` (but .studio-backup, .DS_Store and links) into kit/.studio-backup/<stamp>/ (a -2, -3 ... after the stamp when it is taken), then
 * removes the oldest backups past `keep`. The folder is in `out` ("" when it failed). 0 with `why` when the kit is over `max_mb` or anything cannot be copied
 * (the partial copy is removed, and the old backups are left). Only folders named as a stamp are ever removed. */
int rehacer_backup(const char *kit, const char *stamp, int keep, int max_mb, char *out, size_t out_cap, char *why, size_t why_cap);

/* the backups of `kit` as stamps, oldest first, into `names` (`cap` of REHACER_STAMP_NAME bytes); how many there are */
enum { REHACER_STAMP_NAME = 40 };
int rehacer_backups(const char *kit, char names[][REHACER_STAMP_NAME], int cap);

typedef struct {
    ProcRun run;               /* the script's process, its output (both streams, the last REHACER_LOG_MAX bytes) and its exit code (proc_run.h) */
    char backup[REHACER_PATH]; /* the backup made before this run, "" for none */
} Rehacer;

/* Runs kit/rehacer.command with sh, the kit as its folder, no input (a `read` ends at once), its output to the log. 1 when it started, 0 with `why`. */
int rehacer_start(Rehacer *r, const char *kit, char *why, size_t why_cap);
/* Takes what it has written and sees whether it ended: 1 while it runs. When it ends `finished` and `exit_code` are set. */
int rehacer_poll(Rehacer *r);
/* asks it to end (SIGTERM to its whole group); poll tells when it has */
void rehacer_stop(Rehacer *r);
/* ends it if it runs (and waits), frees the log */
void rehacer_free(Rehacer *r);

#endif
