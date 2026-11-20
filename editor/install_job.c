/* The game's installer in the background (install_job.h). */
#include "install_job.h"

#include "build.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

extern char **environ;

void install_job_init(InstallJob *job)
{
    memset(job, 0, sizeof *job);
    job->run.fd = -1;
}

int install_job_start(InstallJob *job, const char *game, const char *zip, const char *user_dir, char *why, size_t why_cap)
{
    if (job->run.running) return snprintf(why, why_cap, "an install is running already"), 0;
#if defined(_WIN32)
    char command[1100];
    install_command_text(game, zip, user_dir, command, sizeof command);
    snprintf(why, why_cap, "install runs the game from a shell here: %s", command);
    return 0;
#else
    struct stat st;
    if (stat(game, &st) != 0 || !S_ISREG(st.st_mode)) return snprintf(why, why_cap, "no game program at %s", game), 0;
    char *argv[6];
    int n = 0;
    argv[n++] = (char *)game;
    if (user_dir[0] != '\0') argv[n++] = "--user", argv[n++] = (char *)user_dir;   /* "": the game's own player folder */
    argv[n++] = "--install", argv[n++] = (char *)zip;
    argv[n] = NULL;
    proc_free(&job->run);
    job->done = job->ok = 0;
    job->why[0] = '\0';
    return proc_start(&job->run, argv, environ, NULL, why, why_cap);
#endif
}

int install_job_poll(InstallJob *job)
{
    if (!job->run.running) return 0;
    if (proc_poll(&job->run)) return 1;
    job->done = 1;
    job->ok = !job->run.stopped && job->run.exit_code == 0;
    if (job->run.stopped) snprintf(job->why, sizeof job->why, "the install was stopped");
    else if (!job->ok) snprintf(job->why, sizeof job->why, "--install stopped with status %d", job->run.exit_code);
    return 0;
}

int install_job_busy(const InstallJob *job)
{
    return job->run.running;
}

void install_job_stop(InstallJob *job)
{
    if (job->run.running) proc_stop(&job->run);
}

void install_job_free(InstallJob *job)
{
    proc_free(&job->run);
    install_job_init(job);
}
