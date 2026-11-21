/* A car kit built in a thread (dep85 step 3): car build's work (studio/car_kit.h car_kit_build_dir: the CFM, PBS and PDN from the kit's sources, build/piece/ and
 * build/<id>.zip) off the window's thread, so a Preview that waits for the car file shows "building..." and the window stays alive. Open runs it when the kit has
 * no car file yet; the Build button runs it again. Polled every frame: when it ends, `done` and `ok` (and `why` when it is not). No window. */
#ifndef TNFS_TRACK_EDITOR_CAR_BUILD_JOB_H
#define TNFS_TRACK_EDITOR_CAR_BUILD_JOB_H

#include "formats/car_check.h"

#include <pthread.h>
#include <stddef.h>

typedef struct {
    pthread_t thread;
    int started;               /* the thread was made and not joined yet */
    int running;               /* set by the thread's end: read under `lock` */
    int done;                  /* a build ran and was told by car_build_job_poll */
    int ok;
    int check;                 /* the job that ran or runs is the kit's check (studio/car_kit_check.h), not a build */
    CarCheckList list;         /* the findings of the last check; checked says there are some to show, even none */
    int checked;
    char kit[4096];
    char why[300];
    pthread_mutex_t lock;
    int ready;                 /* `lock` is made */
} CarBuildJob;

/* a job that has run nothing (a zeroed one is the same) */
void car_build_job_init(CarBuildJob *job);
/* starts the build of `kit`; 0 with `why` when one is running already or the thread could not start */
int car_build_job_start(CarBuildJob *job, const char *kit, char *why, size_t why_cap);
/* starts the check of `kit` in the same thread: the findings are in `list` when it ends; `ok` is 0 only when it could not run */
int car_build_job_start_check(CarBuildJob *job, const char *kit, char *why, size_t why_cap);
/* every frame: 1 while it runs; when it ends it is joined and `done`, `ok` and `why` are told once (the next call returns 0 and changes nothing) */
int car_build_job_poll(CarBuildJob *job);
int car_build_job_busy(const CarBuildJob *job);
/* waits for a running build, then releases the job; ready for car_build_job_start again */
void car_build_job_free(CarBuildJob *job);

#endif
