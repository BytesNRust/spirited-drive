/* The Track light window's photo (editor/light_photo.h, dep98 step 2 part B): the command and environment for one photo, and the job against a fake game (a shell script
 * that records what it was given and writes a 1280 x 960 PPM whose grey depends on the time of day): the working copy is in the scratch folder the game sees, the
 * person's TNFS_* and SDL_* are dropped, the picture comes back, a failing or silent game is reported, a run can be stopped, and the scratch folder is gone after each. */
#include "light_photo.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char dir[512];

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
    "shot= mods= track= ticks=\n"
    "while [ $# -gt 0 ]; do case $1 in --screenshot) shot=$2; shift;; --mods) mods=$2; shift;; --drive) track=$2; shift;; --node) ticks=$2; shift;; esac; shift; done\n"
    "if [ -n \"$FAKE_RECORD\" ]; then {\n"
    "  echo \"track=$track ticks=$ticks time=$TNFS_LIGHT_TIME start=$TNFS_START_NODE video=$SDL_VIDEO_DRIVER person=$TNFS_PERSON kept=$KEEP_ME\"\n"
    "  ls \"$mods/track_light\"; cat \"$mods\"/track_light/*.txt; echo ---; cat \"$mods\"/track/*.txt 2>/dev/null\n"
    "} > \"$FAKE_RECORD\"; fi\n"
    "case $FAKE_MODE in\n"
    "  fail) echo 'the game says: no such track' >&2; exit 3;;\n"
    "  none) exit 0;;\n"
    "  slow) sleep 30; exit 0;;\n"
    "esac\n"
    "case $TNFS_LIGHT_TIME in dawn) v=30;; day) v=200;; dusk) v=90;; night) v=5;; esac\n"
    "o=$(printf '%03o' $v)\n"
    "printf 'P6\\n1280 960\\n255\\n' > \"$shot\"\n"
    "head -c 3686400 /dev/zero | LC_ALL=C tr '\\000' \"\\\\$o\" >> \"$shot\"\n";

static char fake[1024], record[1024];

static void run_to_end(LightPhotoJob *job)
{
    for (int i = 0; i < 2000 && light_photo_poll(job); ++i) usleep(10000);
}

static void test_time_words(void)
{
    CHECK(strcmp(light_photo_time_word('A'), "dawn") == 0);
    CHECK(strcmp(light_photo_time_word('B'), "day") == 0);
    CHECK(strcmp(light_photo_time_word('C'), "dusk") == 0);
    CHECK(strcmp(light_photo_time_word('N'), "night") == 0);
    CHECK(light_photo_time_word('X') == NULL && light_photo_time_word('n') == NULL && light_photo_time_word('\0') == NULL);
}

static int has_word(const RaceCommand *c, const char *word)
{
    for (int i = 0; i < c->argc; ++i)
        if (strcmp(c->argv[i], word) == 0) return 1;
    return 0;
}

static int has_env(const RaceCommand *c, const char *entry)
{
    for (int i = 0; i < c->envc; ++i)
        if (strcmp(c->envp[i], entry) == 0) return 1;
    return 0;
}

static void test_command(void)
{
    LightPhotoSetup s = { "/g/tnfs", "/cd", "/u", "/m", "CY1", 'N', 70, 0, 0, 4, "/s.ppm" };
    char *const base[] = { "HOME=/h", "TNFS_PERSON=1", "SDL_VIDEO_DRIVER=x11", "TNFS_LIGHT_TIME=day", NULL };
    RaceCommand c;
    char why[LIGHT_PHOTO_WHY];
    CHECK(light_photo_command(&s, base, &c, why, sizeof why));
    const char *const want[] = { "/g/tnfs", "--data", "/cd", "--user", "/u", "--mods", "/m", "--drive", "CY1", "--node", "4", "--screenshot", "/s.ppm" };
    CHECK_EQ_INT(c.argc, 13);
    for (int i = 0; i < 13 && i < c.argc; ++i) CHECK(strcmp(c.argv[i], want[i]) == 0);
    CHECK(c.argv[c.argc] == NULL);
    CHECK(has_env(&c, "HOME=/h"));                                   // the person's other variables stay
    CHECK(!has_env(&c, "TNFS_PERSON=1") && !has_env(&c, "TNFS_LIGHT_TIME=day") && !has_env(&c, "SDL_VIDEO_DRIVER=x11"));   // their TNFS_* and SDL_* go
    CHECK(has_env(&c, "TNFS_LIGHT_TIME=night") && has_env(&c, "TNFS_START_NODE=70") && has_env(&c, "TNFS_AUTOPILOT=1") && has_env(&c, "TNFS_FRAMES=1") &&
          has_env(&c, "TNFS_WEATHER=clear") && has_env(&c, "TNFS_VIEW=1") &&   /* view 0, the bumper: no car in the picture */
           has_env(&c, "SDL_VIDEO_DRIVER=dummy") && has_env(&c, "SDL_AUDIO_DRIVER=dummy") && has_env(&c, "SDL_AUDIODRIVER=dummy"));
    car_race_command_free(&c);
    CHECK_EQ_INT(c.argc, 0);
    s.letter = 'A';
    CHECK(light_photo_command(&s, NULL, &c, why, sizeof why) && has_env(&c, "TNFS_LIGHT_TIME=dawn"));
    car_race_command_free(&c);
    (void)has_word;
}

static void test_views(void)
{
    const char *const names[LIGHT_VIEWS] = { "Bumper", "Dash", "Chase near", "Chase mid", "Chase far", "High" };
    const int kinds[LIGHT_VIEWS] = { 1, 0, 2, 3, 4, 12 };   // the game's chase kinds: the bumper has no car, 12 is the highest eye
    for (int v = 0; v < LIGHT_VIEWS; ++v) {
        CHECK(strcmp(light_photo_view_name(v), names[v]) == 0);
        CHECK_EQ_INT(light_photo_view_kind(v), kinds[v]);
        CHECK_EQ_INT(light_photo_view_ticks(v), v >= 2 ? LIGHT_PHOTO_TICKS_CHASE : LIGHT_PHOTO_TICKS_DEFAULT);   // a chase camera glides to its place
        CHECK_EQ_INT(light_photo_view_clamp(v), v);
    }
    CHECK(light_photo_view_name(-1) == NULL && light_photo_view_name(LIGHT_VIEWS) == NULL);
    CHECK_EQ_INT(light_photo_view_kind(LIGHT_VIEWS), -1);
    CHECK_EQ_INT(light_photo_view_clamp(-3), LIGHT_VIEW_DEFAULT);
    CHECK_EQ_INT(light_photo_view_clamp(99), LIGHT_VIEW_DEFAULT);
    CHECK_EQ_INT(LIGHT_VIEW_DEFAULT, 0);   // the bumper is what a first run shows
    light_photo_view_pref_set(5);
    CHECK_EQ_INT(light_photo_view_pref(), 5);
    light_photo_view_pref_set(77);
    CHECK_EQ_INT(light_photo_view_pref(), LIGHT_VIEW_DEFAULT);

    LightPhotoSetup s = { "/g/tnfs", "/cd", "/u", "/m", "CY1", 'N', 70, 0, 0, 4, "/s.ppm" };
    RaceCommand c;
    char why[LIGHT_PHOTO_WHY];
    for (int v = 0; v < LIGHT_VIEWS; ++v) {
        s.view = v;
        s.ticks = light_photo_view_ticks(v);
        CHECK(light_photo_command(&s, NULL, &c, why, sizeof why));
        char want[24];
        snprintf(want, sizeof want, "TNFS_VIEW=%d", kinds[v]);
        CHECK(has_env(&c, want));
        car_race_command_free(&c);
    }
    s.view = 0;   // the HUD: off by default (the photo is for the light), on when asked; the person's own TNFS_NO_HUD never gets through
    char *const base[] = { "TNFS_NO_HUD=1", NULL };
    s.hud = 0;
    CHECK(light_photo_command(&s, NULL, &c, why, sizeof why) && has_env(&c, "TNFS_NO_HUD=1"));
    car_race_command_free(&c);
    s.hud = 1;
    CHECK(light_photo_command(&s, base, &c, why, sizeof why) && !has_env(&c, "TNFS_NO_HUD=1"));
    car_race_command_free(&c);
    light_photo_hud_pref_set(1);
    CHECK_EQ_INT(light_photo_hud_pref(), 1);
    light_photo_hud_pref_set(0);
    CHECK_EQ_INT(light_photo_hud_pref(), 0);
    s.view = LIGHT_VIEWS;
    CHECK(!light_photo_command(&s, NULL, &c, why, sizeof why) && why[0] != '\0');
    s.view = -1;
    CHECK(!light_photo_command(&s, NULL, &c, why, sizeof why));
}

static void test_command_refusals(void)
{
    const LightPhotoSetup good = { "/g/tnfs", "/cd", "/u", "/m", "CY1", 'N', 70, 0, 0, 4, "/s.ppm" };
    RaceCommand c;
    char why[LIGHT_PHOTO_WHY];
    LightPhotoSetup s = good;
    s.letter = 'X';
    CHECK(!light_photo_command(&s, NULL, &c, why, sizeof why) && why[0] != '\0');
    s = good;
    s.ticks = 0;
    CHECK(!light_photo_command(&s, NULL, &c, why, sizeof why));
    s.ticks = LIGHT_PHOTO_TICKS_MAX + 1;
    CHECK(!light_photo_command(&s, NULL, &c, why, sizeof why));
    s = good;
    memcpy(s.track, "cy1", 4);
    CHECK(!light_photo_command(&s, NULL, &c, why, sizeof why));   // a stem is two capitals and a digit, as the light files are named
    memcpy(s.track, "CY12", 5);
    CHECK(!light_photo_command(&s, NULL, &c, why, sizeof why));
    s = good;
    s.game = NULL;
    CHECK(!light_photo_command(&s, NULL, &c, why, sizeof why));
    s = good;
    s.cd = "";
    CHECK(!light_photo_command(&s, NULL, &c, why, sizeof why));
    s = good;
    s.shot = NULL;
    CHECK(!light_photo_command(&s, NULL, &c, why, sizeof why));
    s = good;
    s.mods = NULL;
    CHECK(!light_photo_command(&s, NULL, &c, why, sizeof why));
}

static const char NIGHT_FILE[] = "# tnfs track light 1\ntrack CY1 time N nodes 100 flags fff\nlamp 5 100\n";
static const char PLACES[] = "# tnfs track places 1\ntrack CY1\ntunnel 10 20\n";

static void test_job_done(void)
{
    LightPhotoJob job;
    memset(&job, 0, sizeof job);
    char why[LIGHT_PHOTO_WHY];
    setenv("FAKE_RECORD", record, 1);
    setenv("FAKE_MODE", "", 1);
    setenv("TNFS_PERSON", "1", 1);
    setenv("KEEP_ME", "yes", 1);
    CHECK(light_photo_start(&job, fake, dir, "CY1", 'N', 70, 0, 0, 4, NIGHT_FILE, strlen(NIGHT_FILE), PLACES, strlen(PLACES), why, sizeof why));
    CHECK(job.state == LIGHT_PHOTO_RUNNING);
    char temp[EXPORT_PATH];
    snprintf(temp, sizeof temp, "%s", job.temp);
    struct stat st;
    CHECK(stat(temp, &st) == 0);   // the scratch folder exists while it runs
    run_to_end(&job);
    CHECK(job.state == LIGHT_PHOTO_DONE);
    CHECK(job.rgba != NULL && job.w == 1280 && job.h == 960 && job.serial == 1);
    CHECK(job.rgba != NULL && job.rgba[0] == 5 && job.rgba[1] == 5 && job.rgba[2] == 5 && job.rgba[3] == 255);   // night: the fake's grey 5
    CHECK(job.rgba != NULL && job.rgba[4 * (1280 * 959 + 1279)] == 5);   // the last pixel too: the whole frame was read
    CHECK(stat(temp, &st) != 0 && job.temp[0] == '\0');   // the scratch folder is gone with the working copy in it
    const char *rec = read_all(record);
    CHECK(strstr(rec, "track=CY1 ticks=4 time=night start=70 video=dummy person= kept=yes") != NULL);   // the person's TNFS_* dropped, their other variables kept
    CHECK(strstr(rec, "CY1N.txt\n") != NULL);                                                            // the working copy is named as the game reads it
    CHECK(strstr(rec, NIGHT_FILE) != NULL && strstr(rec, "---\n# tnfs track places 1\ntrack CY1\ntunnel 10 20\n") != NULL);
    // a second photo, another time: the serial moves and the picture changes
    CHECK(light_photo_start(&job, fake, dir, "CY1", 'B', 70, 0, 0, 4, NIGHT_FILE, strlen(NIGHT_FILE), NULL, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.state == LIGHT_PHOTO_DONE && job.serial == 2 && job.rgba != NULL && job.rgba[0] == 200);
    rec = read_all(record);
    CHECK(strstr(rec, "time=day") != NULL && strstr(rec, "---\n") != NULL && strstr(rec, "tunnel 10 20") == NULL);   // no places text: none in the folder
    light_photo_free(&job);
    CHECK(job.rgba == NULL);
}

static void test_job_failures(void)
{
    LightPhotoJob job;
    memset(&job, 0, sizeof job);
    char why[LIGHT_PHOTO_WHY];
    setenv("FAKE_MODE", "fail", 1);
    CHECK(light_photo_start(&job, fake, dir, "CY1", 'N', 70, 0, 0, 4, NIGHT_FILE, strlen(NIGHT_FILE), NULL, 0, why, sizeof why));
    char temp[EXPORT_PATH];
    snprintf(temp, sizeof temp, "%s", job.temp);
    run_to_end(&job);
    struct stat st;
    CHECK(job.state == LIGHT_PHOTO_FAILED && strstr(job.why, "no such track") != NULL && strstr(job.why, "3") != NULL);   // the exit code and the game's last line
    CHECK(stat(temp, &st) != 0);
    CHECK(job.rgba == NULL);
    setenv("FAKE_MODE", "none", 1);   // exits 0 and writes nothing
    CHECK(light_photo_start(&job, fake, dir, "CY1", 'N', 70, 0, 0, 4, NIGHT_FILE, strlen(NIGHT_FILE), NULL, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.state == LIGHT_PHOTO_FAILED && strstr(job.why, "no picture") != NULL);
    // a good photo, then a failing one: the last good picture stays
    setenv("FAKE_MODE", "", 1);
    CHECK(light_photo_start(&job, fake, dir, "CY1", 'A', 70, 0, 0, 4, NIGHT_FILE, strlen(NIGHT_FILE), NULL, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.state == LIGHT_PHOTO_DONE && job.rgba != NULL && job.rgba[0] == 30);
    setenv("FAKE_MODE", "fail", 1);
    CHECK(light_photo_start(&job, fake, dir, "CY1", 'A', 70, 0, 0, 4, NIGHT_FILE, strlen(NIGHT_FILE), NULL, 0, why, sizeof why));
    run_to_end(&job);
    CHECK(job.state == LIGHT_PHOTO_FAILED && job.rgba != NULL && job.rgba[0] == 30);
    // a game that is not there, and a track or time that is not one: refused before anything runs, no folder left
    CHECK(!light_photo_start(&job, "/no/such/game", dir, "CY1", 'N', 70, 0, 0, 4, NIGHT_FILE, strlen(NIGHT_FILE), NULL, 0, why, sizeof why) && why[0] != '\0');
    CHECK(job.state == LIGHT_PHOTO_FAILED && job.temp[0] == '\0');
    CHECK(!light_photo_start(&job, fake, dir, "cy1", 'N', 70, 0, 0, 4, NIGHT_FILE, strlen(NIGHT_FILE), NULL, 0, why, sizeof why));
    CHECK(!light_photo_start(&job, fake, dir, "CY1", 'Q', 70, 0, 0, 4, NIGHT_FILE, strlen(NIGHT_FILE), NULL, 0, why, sizeof why));
    CHECK(!light_photo_start(&job, fake, dir, "CY1", 'N', 70, 0, 0, 4, NULL, 0, NULL, 0, why, sizeof why));   // no file text
    light_photo_free(&job);
}

static void test_job_stop(void)
{
    LightPhotoJob job;
    memset(&job, 0, sizeof job);
    char why[LIGHT_PHOTO_WHY];
    setenv("FAKE_MODE", "slow", 1);
    CHECK(light_photo_start(&job, fake, dir, "CY1", 'N', 70, 0, 0, 4, NIGHT_FILE, strlen(NIGHT_FILE), NULL, 0, why, sizeof why));
    char temp[EXPORT_PATH];
    snprintf(temp, sizeof temp, "%s", job.temp);
    CHECK(light_photo_poll(&job));   // still running
    light_photo_stop(&job);
    struct stat st;
    CHECK(job.state == LIGHT_PHOTO_FAILED && strstr(job.why, "stopped") != NULL && stat(temp, &st) != 0);
    // a new photo while one runs ends the first
    CHECK(light_photo_start(&job, fake, dir, "CY1", 'N', 70, 0, 0, 4, NIGHT_FILE, strlen(NIGHT_FILE), NULL, 0, why, sizeof why));
    snprintf(temp, sizeof temp, "%s", job.temp);
    setenv("FAKE_MODE", "", 1);
    CHECK(light_photo_start(&job, fake, dir, "CY1", 'B', 70, 0, 0, 4, NIGHT_FILE, strlen(NIGHT_FILE), NULL, 0, why, sizeof why));
    CHECK(stat(temp, &st) != 0);   // the first run's folder went with it
    run_to_end(&job);
    CHECK(job.state == LIGHT_PHOTO_DONE);
    light_photo_free(&job);
}

int main(void)
{
    const char *base = getenv("TMPDIR");
    snprintf(dir, sizeof dir, "%s/light_photo_test.XXXXXX", base != NULL && base[0] != '\0' ? base : "/tmp");
    if (mkdtemp(dir) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(fake, sizeof fake, "%s/fake_game.sh", dir);
    snprintf(record, sizeof record, "%s/record.txt", dir);
    write_text(fake, FAKE, 1);
    test_time_words();
    test_command();
    test_views();
    test_command_refusals();
    test_job_done();
    test_job_failures();
    test_job_stop();
    unlink(fake);
    unlink(record);
    rmdir(dir);
    return TEST_REPORT();
}
