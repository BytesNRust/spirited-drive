/* The Track light window's test drive (editor/light_drive.h, the light tuning design, piece 1): the command and environment of the live game, and the job against a fake
 * game (a shell script that records what it was given and waits): the working copy is in the scratch folder the game sees, the light editor is on, the game gets a real
 * window (no dummy drivers) and none of the person's TNFS_* or SDL_*, an update rewrites the copy whole, a run can be stopped, a failing game is reported, and the
 * scratch folder is gone after each. */
#include "light_drive.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char dir[512], fake[1024], record[1024];

static void write_text(const char *path, const char *text, int exec)
{
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if (f == NULL) return;
    fputs(text, f);
    fclose(f);
    if (exec) chmod(path, 0755);
}

static char *read_all(const char *path)
{
    static char buf[16384];
    buf[0] = '\0';
    FILE *f = fopen(path, "r");
    if (f != NULL) {
        const size_t n = fread(buf, 1, sizeof buf - 1, f);
        buf[n] = '\0';
        fclose(f);
    }
    return buf;
}

static const char FAKE[] =
    "#!/bin/sh\n"
    "mods= track= node=\n"
    "while [ $# -gt 0 ]; do case $1 in --mods) mods=$2; shift;; --drive) track=$2; shift;; --node) node=$2; shift;; esac; shift; done\n"
    "{\n"
    "  echo \"track=$track node=$node time=$TNFS_LIGHT_TIME start=$TNFS_START_NODE weather=$TNFS_WEATHER edit=$TNFS_LIGHT_EDIT video=$SDL_VIDEO_DRIVER audio=$SDL_AUDIO_DRIVER frames=$TNFS_FRAMES auto=$TNFS_AUTOPILOT person=$TNFS_PERSON kept=$KEEP_ME\"\n"
    "  ls \"$mods/track_light\"; cat \"$mods\"/track_light/*.txt; echo ---; cat \"$mods\"/track/*.txt 2>/dev/null\n"
    "  echo \"masks=$TNFS_TRACK_MASKS\"\n"
    "} > \"$FAKE_RECORD\"\n"
    "case $FAKE_MODE in\n"
    "  fail) echo 'the game says: no such track' >&2; exit 3;;\n"
    "  quit) exit 0;;\n"
    "esac\n"
    "sleep 30\n";

static void test_weather_words(void)
{
    CHECK(strcmp(light_drive_weather_word(0), "clear") == 0 && strcmp(light_drive_weather_word(1), "fog") == 0);
    CHECK(strcmp(light_drive_weather_word(2), "rain") == 0 && strcmp(light_drive_weather_word(3), "storm") == 0);
    CHECK(strcmp(light_drive_weather_name(0), "Clear") == 0 && strcmp(light_drive_weather_name(3), "Storm") == 0);
    CHECK(light_drive_weather_word(4) == NULL && light_drive_weather_word(-1) == NULL && light_drive_weather_name(4) == NULL);
    CHECK_EQ_INT(light_drive_weather_clamp(9), 0);
    CHECK_EQ_INT(light_drive_weather_clamp(2), 2);
}

static int has(char *const *v, const char *want)
{
    for (; *v != NULL; ++v)
        if (strcmp(*v, want) == 0) return 1;
    return 0;
}

static void test_command(void)
{
    char *const base[] = { "PATH=/usr/bin", "TNFS_PERSON=1", "SDL_VIDEO_DRIVER=dummy", "KEEP_ME=yes", NULL };
    LightDriveSetup s = { "/g/tnfs", "/cd", "/u", "/m", { 0 }, 'N', 1596, 2, NULL, NULL };
    snprintf(s.track, sizeof s.track, "CY1");
    RaceCommand c;
    char why[LIGHT_DRIVE_WHY] = "";
    CHECK(light_drive_command(&s, base, &c, why, sizeof why));
    CHECK(strcmp(c.argv[0], "/g/tnfs") == 0 && has(c.argv, "--data") && has(c.argv, "/cd") && has(c.argv, "--user") && has(c.argv, "/u") && has(c.argv, "--mods") && has(c.argv, "/m"));
    CHECK(has(c.argv, "--drive") && has(c.argv, "CY1"));
    CHECK(!has(c.argv, "--screenshot"));   /* a window, not a photo */
    CHECK(has(c.envp, "TNFS_LIGHT_EDIT=1") && has(c.envp, "TNFS_LIGHT_TIME=night") && has(c.envp, "TNFS_START_NODE=1596") && has(c.envp, "TNFS_WEATHER=rain"));
    CHECK(has(c.envp, "PATH=/usr/bin") && has(c.envp, "KEEP_ME=yes"));
    CHECK(!has(c.envp, "TNFS_PERSON=1") && !has(c.envp, "SDL_VIDEO_DRIVER=dummy"));   /* the person's own game variables do not change the drive */
    for (char *const *e = c.envp; *e != NULL; ++e) CHECK(strncmp(*e, "SDL_", 4) != 0 && strncmp(*e, "TNFS_FRAMES", 11) != 0 && strncmp(*e, "TNFS_AUTOPILOT", 14) != 0);
    car_race_command_free(&c);
    s.masks = "/scratch/masks";
    CHECK(light_drive_command(&s, NULL, &c, why, sizeof why) && has(c.envp, "TNFS_TRACK_MASKS=/scratch/masks"));
    car_race_command_free(&c);
    CHECK(light_drive_command(&s, NULL, &c, why, sizeof why) && !has(c.envp, "TNFS_WINDOW_POS=1500,40"));   /* no place given: the game's own */
    car_race_command_free(&c);
    s.window = "1500,40";
    CHECK(light_drive_command(&s, NULL, &c, why, sizeof why) && has(c.envp, "TNFS_WINDOW_POS=1500,40"));   /* beside Studio */
    car_race_command_free(&c);
    s.letter = 'X';
    CHECK(!light_drive_command(&s, NULL, &c, why, sizeof why) && strstr(why, "time") != NULL);
    s.letter = 'B', s.weather = 7;
    CHECK(!light_drive_command(&s, NULL, &c, why, sizeof why) && strstr(why, "weather") != NULL);
    s.weather = 0, snprintf(s.track, sizeof s.track, "cy1");
    CHECK(!light_drive_command(&s, NULL, &c, why, sizeof why) && strstr(why, "track") != NULL);
    s.mods = NULL, snprintf(s.track, sizeof s.track, "CY1");
    CHECK(!light_drive_command(&s, NULL, &c, why, sizeof why));
}

static void run_to_end(LightDriveJob *job)
{
    for (int i = 0; i < 2000 && light_drive_poll(job); ++i) usleep(10000);
}

static void wait_record(void)
{
    for (int i = 0; i < 500 && strstr(read_all(record), "masks=") == NULL; ++i) usleep(10000);   /* the script's last line: the file is written whole */
}

static void test_job(void)
{
    LightDriveJob job;
    memset(&job, 0, sizeof job);
    char why[LIGHT_DRIVE_WHY] = "";
    setenv("FAKE_RECORD", record, 1);
    setenv("TNFS_PERSON", "1", 1);
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    setenv("KEEP_ME", "yes", 1);
    unlink(record);
    unsetenv("FAKE_MODE");
    const char light[] = "# tnfs track light 2\ntrack CY1\ntunnel 3 4\ntime B\n  brightness 1 2 90\n";
    char player[512];   /* the person's own settings: the drive shows the game as they set it up */
    snprintf(player, sizeof player, "%s/drive_player_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    CHECK(mkdtemp(player) != NULL);
    char player_cfg[560];
    snprintf(player_cfg, sizeof player_cfg, "%s/restomod.cfg", player);
    FILE *pf = fopen(player_cfg, "wb");
    CHECK(pf != NULL && fputs("fov_adjust=10\nfullscreen=1\n", pf) >= 0);
    if (pf != NULL) fclose(pf);
    car_race_set_player(player);
    CHECK(light_drive_start(&job, fake, "/cd", "CY1", 'B', 77, 1, light, strlen(light), why, sizeof why));
    CHECK_EQ_INT(job.state, LIGHT_DRIVE_RUNNING);
    wait_record();
    char *seen = read_all(record);
    CHECK(strstr(seen, "track=CY1") != NULL && strstr(seen, "time=day") != NULL && strstr(seen, "start=77") != NULL && strstr(seen, "weather=fog") != NULL && strstr(seen, "edit=1") != NULL);
    CHECK(strstr(seen, "video= ") != NULL && strstr(seen, "audio=dummy") == NULL && strstr(seen, "frames= ") != NULL && strstr(seen, "auto= ") != NULL);   /* a real window, a person at the wheel */
    CHECK(strstr(seen, "person= ") != NULL && strstr(seen, "kept=yes") != NULL);
    CHECK(strstr(seen, "CY1.txt") != NULL);
    CHECK(strstr(seen, "brightness 1 2 90") != NULL && strstr(seen, "tunnel 3 4") != NULL);
    char file[EXPORT_PATH + 64], scratch[EXPORT_PATH];
    snprintf(scratch, sizeof scratch, "%s", job.temp);
    CHECK(scratch[0] != '\0' && access(scratch, F_OK) == 0);
    char cfg[EXPORT_PATH + 64];
    snprintf(cfg, sizeof cfg, "%s/user/restomod.cfg", scratch);
    CHECK(strstr(read_all(cfg), "fov_adjust=10\n") != NULL && strstr(read_all(cfg), "fullscreen=1") == NULL);
    car_race_set_player(NULL);
    unlink(player_cfg);
    rmdir(player);
    /* an edit: the copy is rewritten whole, the game sees it, nothing half written is left beside it */
    const char light2[] = "# tnfs track light 2\ntrack CY1\ntunnel 5 6\ntime B\n  brightness 1 2 50\n";
    CHECK(light_drive_update(&job, light2, strlen(light2), why, sizeof why));
    snprintf(file, sizeof file, "%s/track_light/CY1.txt", job.mods);
    CHECK(strcmp(read_all(file), light2) == 0);
    light_drive_stop(&job);
    CHECK(job.state != LIGHT_DRIVE_RUNNING && strcmp(job.why, "stopped") == 0);
    CHECK(access(scratch, F_OK) != 0);   /* the scratch folder is gone */
    CHECK(!light_drive_update(&job, light2, strlen(light2), why, sizeof why) && why[0] != '\0');   /* nothing runs: nothing to update */
    light_drive_free(&job);
}

static void test_ends(void)
{
    LightDriveJob job;
    memset(&job, 0, sizeof job);
    char why[LIGHT_DRIVE_WHY] = "";
    const char light[] = "# tnfs track light 2\ntrack CY1\n";
    setenv("FAKE_MODE", "fail", 1);
    CHECK(light_drive_start(&job, fake, "/cd", "CY1", 'B', 1, 0, light, strlen(light), why, sizeof why));
    run_to_end(&job);
    CHECK_EQ_INT(job.state, LIGHT_DRIVE_FAILED);
    CHECK(strstr(job.why, "code 3") != NULL && strstr(job.why, "no such track") != NULL);
    CHECK(job.temp[0] == '\0');
    setenv("FAKE_MODE", "quit", 1);   /* the person closed the game's window: not an error */
    CHECK(light_drive_start(&job, fake, "/cd", "CY1", 'B', 1, 0, light, strlen(light), why, sizeof why));
    run_to_end(&job);
    CHECK_EQ_INT(job.state, LIGHT_DRIVE_ENDED);
    CHECK(job.why[0] == '\0' && job.temp[0] == '\0');
    unsetenv("FAKE_MODE");
    CHECK(!light_drive_start(&job, "/no/such/game", "/cd", "CY1", 'B', 1, 0, light, strlen(light), why, sizeof why) && strstr(why, "game") != NULL);
    CHECK(!light_drive_start(&job, fake, "/cd", "CY1", 'B', 1, 0, NULL, 0, why, sizeof why) && strstr(why, "light file") != NULL);
    CHECK(!light_drive_start(&job, fake, "/cd", "CY1", 'Z', 1, 0, light, strlen(light), why, sizeof why) && strstr(why, "time") != NULL);
    CHECK_EQ_INT(job.state, LIGHT_DRIVE_FAILED);
    light_drive_free(&job);
}

int main(void)
{
    snprintf(dir, sizeof dir, "%s/tnfs_light_drive_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(dir) == NULL) return perror("mkdtemp"), 2;
    snprintf(fake, sizeof fake, "%s/fake_game", dir);
    snprintf(record, sizeof record, "%s/record", dir);
    write_text(fake, FAKE, 1);
    test_weather_words();
    test_command();
    test_job();
    test_ends();
    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", dir);
    return TEST_REPORT();
}
