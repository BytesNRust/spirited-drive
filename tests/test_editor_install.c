/* The editor's install by hand (editor/build.c): the shell command it names where it does not run the game itself, with and without a
 * player folder; and the install refusing, without running anything, a game that is not there. */
#include "build.h"
#include "install_job.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

static void check_command_text(void)
{
    char out[256];
    install_command_text("/g/tnfs", "/p/a.zip", "/u", out, sizeof out);
    CHECK(strcmp(out, "/g/tnfs --user /u --install /p/a.zip") == 0);
    install_command_text("/g/tnfs", "/p/a.zip", "", out, sizeof out);   /* no player folder: the game's own, so no --user at all */
    CHECK(strcmp(out, "/g/tnfs --install /p/a.zip") == 0);
    char tiny[12];
    install_command_text("/g/tnfs", "/p/a.zip", "/u", tiny, sizeof tiny);   /* cut, never overrun */
    CHECK(strlen(tiny) == sizeof tiny - 1);
}

static void check_missing_game(void)
{
    char why[256];
    CHECK(!install_zip("/no/such/dir/tnfs", "/no/such.zip", "/u", why, sizeof why));
    CHECK(why[0] != '\0');
}

static double now_s(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + tv.tv_usec / 1e6;
}

/* a game whose --install takes a second: it makes the player folder's mods/<zip's name>, or fails with 4 for a zip named broken */
static void make_slow_game(const char *dir, char *game, size_t cap)
{
    snprintf(game, cap, "%s/slow-tnfs", dir);
    FILE *f = fopen(game, "w");
    CHECK(f != NULL);
    if (f == NULL) return;
    fputs("#!/bin/sh\nuser=; zip=\nwhile [ $# -gt 0 ]; do case \"$1\" in --user) user=$2; shift;; --install) zip=$2; shift;; esac; shift; done\n"
          "echo \"installing $zip\"; sleep 1\ncase \"$zip\" in *broken*) echo 'bad package' >&2; exit 4;; esac\n"
          "mkdir -p \"$user/mods/$(basename \"$zip\" .zip)\"; echo \"installed $(basename \"$zip\" .zip) 0.1\"\n", f);
    fclose(f);
    chmod(game, 0755);
}

/* the install in the background: started at once, polled while the game's installer runs, its end told (ok, or why) */
static void check_install_job(void)
{
    char dir[512], game[600], user[600], why[256];
    snprintf(dir, sizeof dir, "%s/install_job_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    CHECK(mkdtemp(dir) != NULL);
    make_slow_game(dir, game, sizeof game);
    snprintf(user, sizeof user, "%s/user", dir);
    mkdir(user, 0777);
    InstallJob job;
    install_job_init(&job);
    const double t0 = now_s();
    CHECK(install_job_start(&job, game, "/p/loop.zip", user, why, sizeof why));
    CHECK(now_s() - t0 < 0.5);   /* it does not wait for the installer */
    CHECK(install_job_busy(&job) && install_job_poll(&job));
    CHECK(!install_job_start(&job, game, "/p/loop.zip", user, why, sizeof why) && strstr(why, "already") != NULL);
    for (int i = 0; i < 1000 && install_job_poll(&job); ++i) usleep(10000);
    CHECK(!install_job_busy(&job) && job.done && job.ok);
    CHECK(strstr(job.run.log != NULL ? job.run.log : "", "installed loop 0.1") != NULL);
    char made[700];
    snprintf(made, sizeof made, "%s/mods/loop", user);
    struct stat st;
    CHECK(stat(made, &st) == 0 && S_ISDIR(st.st_mode));

    CHECK(install_job_start(&job, game, "/p/broken.zip", user, why, sizeof why));   /* the installer fails: said with its status, not ok */
    for (int i = 0; i < 1000 && install_job_poll(&job); ++i) usleep(10000);
    CHECK(job.done && !job.ok && strstr(job.why, "status 4") != NULL);

    CHECK(install_job_start(&job, game, "/p/loop.zip", user, why, sizeof why));   /* Stop: ended, not ok, said */
    install_job_stop(&job);
    for (int i = 0; i < 1000 && install_job_poll(&job); ++i) usleep(10000);
    CHECK(job.done && !job.ok && strstr(job.why, "stopped") != NULL);
    install_job_free(&job);
    CHECK(!install_job_start(&job, "/no/such/dir/tnfs", "/p/loop.zip", user, why, sizeof why) && why[0] != '\0');   /* no game: refused before anything runs */
    install_job_free(&job);
}

int main(void)
{
    check_command_text();
    check_missing_game();
    check_install_job();
    return TEST_REPORT();
}
