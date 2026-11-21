/* The game's installer run in the background (`GAME --user DIR --install ZIP`, as build.c's install_zip runs it and waits): Install and Test drive start it and
 * poll it every frame, so the window is not held while the game unpacks the package. Its output is in run.log. */
#ifndef TNFS_TRACK_EDITOR_INSTALL_JOB_H
#define TNFS_TRACK_EDITOR_INSTALL_JOB_H

#include "proc_run.h"

#include <stddef.h>

typedef struct {
    ProcRun run;
    int done;        /* an install ran and ended: `ok`, and `why` when it is not */
    int ok;
    char why[300];
} InstallJob;

/* a job that has run nothing (a zeroed one is not: its run.fd must be -1) */
void install_job_init(InstallJob *job);
/* Starts the install of `zip` into the player folder `user_dir` ("" for the game's own). 1 when the installer started; 0 with `why` (one runs already, no game,
 * a system where the editor does not run the game: the command to run by hand). */
int install_job_start(InstallJob *job, const char *game, const char *zip, const char *user_dir, char *why, size_t why_cap);
/* every frame: 1 while the installer runs; when it ends, `done`, `ok` and `why` */
int install_job_poll(InstallJob *job);
int install_job_busy(const InstallJob *job);
/* asks the installer to end: its end is told by install_job_poll, not ok, "stopped" */
void install_job_stop(InstallJob *job);
/* 1 when the installer's output says the package's files changed but its version did not (the game's words, as `--install` and the menu print them: "its files
 * changed but its version did not: raise version= in mod.ini"). `log` is the job's output (job->run.log); NULL is 0. */
int install_version_unraised(const char *log);
/* ends it if it runs, frees its output; ready for install_job_start */
void install_job_free(InstallJob *job);

#endif
