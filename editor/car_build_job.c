#include "car_build_job.h"

#include "formats/exchange.h"
#include "studio/car_kit.h"
#include "studio/car_kit_check.h"

#include <stdio.h>
#include <string.h>

void car_build_job_init(CarBuildJob *job)
{
    memset(job, 0, sizeof *job);
}

static void *build_thread(void *arg)
{
    CarBuildJob *job = arg;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    const int ok = job->check ? car_kit_check(job->kit, &job->list) || (exchange_fail(&err, "out of memory"), 0) : car_kit_build_dir(job->kit, 0, &err);
    pthread_mutex_lock(&job->lock);
    job->ok = ok;
    snprintf(job->why, sizeof job->why, "%s", ok ? "" : err.message);
    job->running = 0;
    pthread_mutex_unlock(&job->lock);
    return NULL;
}

static int start(CarBuildJob *job, const char *kit, int check, char *why, size_t why_cap)
{
    if (job->started) {
        snprintf(why, why_cap, "a build runs already");
        return 0;
    }
    if (!job->ready) {
        pthread_mutex_init(&job->lock, NULL);
        job->ready = 1;
    }
    if (snprintf(job->kit, sizeof job->kit, "%s", kit) >= (int)sizeof job->kit) {
        snprintf(why, why_cap, "the kit's path is too long");
        return 0;
    }
    job->done = job->ok = 0;
    job->check = check;
    if (check) car_check_free(&job->list), job->checked = 0;
    job->why[0] = '\0';
    job->running = 1;
    const int err = pthread_create(&job->thread, NULL, build_thread, job);
    if (err != 0) {
        job->running = 0;
        snprintf(why, why_cap, "could not start the build: %s", strerror(err));
        return 0;
    }
    job->started = 1;
    return 1;
}

int car_build_job_start(CarBuildJob *job, const char *kit, char *why, size_t why_cap)
{
    return start(job, kit, 0, why, why_cap);
}

int car_build_job_start_check(CarBuildJob *job, const char *kit, char *why, size_t why_cap)
{
    return start(job, kit, 1, why, why_cap);
}

int car_build_job_busy(const CarBuildJob *job)
{
    return job->started;
}

int car_build_job_poll(CarBuildJob *job)
{
    if (!job->started) return 0;
    pthread_mutex_lock(&job->lock);
    const int running = job->running;
    pthread_mutex_unlock(&job->lock);
    if (running) return 1;
    pthread_join(job->thread, NULL);
    job->started = 0;
    job->done = 1;
    job->checked = job->check && job->ok;
    return 0;
}

void car_build_job_free(CarBuildJob *job)
{
    if (job->started) {
        pthread_join(job->thread, NULL);
        job->started = 0;
    }
    car_check_free(&job->list);
    job->checked = 0;
    if (job->ready) pthread_mutex_destroy(&job->lock);
    job->ready = 0;
}
