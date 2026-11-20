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

typedef struct RehacerJob RehacerJob;   /* the backup's own thread (car_rehacer.c) */

typedef struct {
    ProcRun run;               /* the script's process, its output (both streams, the last REHACER_LOG_MAX bytes) and its exit code (proc_run.h) */
    char backup[REHACER_PATH]; /* the backup made before this run, "" for none */
    int backing_up;            /* rehacer_begin: the kit is being copied in its own thread; poll starts the script when the copy has ended */
    int failed;                /* the backup failed, or Stop came before the script ran: `why` says, and the script did not run */
    char why[300];
    int runs;                  /* how many runs were begun with this struct: the window brings each new run's log forward */
    RehacerJob *job;           /* the backup's thread while it is not joined */
} Rehacer;

/* Runs kit/rehacer.command with sh, the kit as its folder, no input (a `read` ends at once), its output to the log. 1 when it started, 0 with `why`. */
int rehacer_start(Rehacer *r, const char *kit, char *why, size_t why_cap);
/* Rehacer as the window runs it (owner test R1: nothing holds the window): the backup of `kit` (rehacer_backup with `stamp`, `keep`, `max_mb`) in its own thread,
 * then, from rehacer_poll, the script (rehacer_start). Returns at once: 1 when the copy started (`runs` counts it, the last run's log is dropped), 0 with `why`
 * when a run is going, the kit has no script or the stamp is not one. A backup that fails, or a Stop before the script runs, ends it with `failed` and `why`. */
int rehacer_begin(Rehacer *r, const char *kit, const char *stamp, int keep, int max_mb, char *why, size_t why_cap);
/* 1 while the backup or the script runs */
int rehacer_busy(const Rehacer *r);
/* Takes what it has written and sees whether it ended: 1 while it runs (or, after rehacer_begin, while the kit is copied). When it ends `finished` and
 * `exit_code` are set (or `failed`, when the script never ran). */
int rehacer_poll(Rehacer *r);
/* asks it to end (SIGTERM to its whole group; during the backup, the copy stops and the script is not run); poll tells when it has */
void rehacer_stop(Rehacer *r);
/* ends it if it runs (and waits, for the copy too), frees the log */
void rehacer_free(Rehacer *r);

/* 1 when `log` holds the game's install saying a mod's files changed but its version did not ("raise version= in mod.ini"; owner test R2) */
int rehacer_version_unraised(const char *log);

#endif
