/* Test in race and Race photo (editor/car_race.c, proc_run.c): the command line and environment built for each kind of car and each preset, the scratch player folder's
 * files, the log's lines, the slot a car has, and the whole job run against a fake game (a shell script that writes the lines the real one does), so no game data is
 * needed. */
#include "car_race.h"
#include "test.h"

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[512];

static int remove_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st, (void)flag, (void)ftw;
    return remove(path);
}

static int index_of(const char *folder)
{
    for (int i = 0; car_export_car(i) != NULL; ++i)
        if (strcmp(car_export_car(i)->folder, folder) == 0) return i;
    return -1;
}

static int has_arg(const RaceCommand *c, const char *word)
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

static int has_env_key(const RaceCommand *c, const char *key)
{
    const size_t n = strlen(key);
    for (int i = 0; i < c->envc; ++i)
        if (strncmp(c->envp[i], key, n) == 0) return 1;
    return 0;
}

/* 1 when `flag` is followed by `value` in the arguments */
static int arg_pair(const RaceCommand *c, const char *flag, const char *value)
{
    for (int i = 0; i + 1 < c->argc; ++i)
        if (strcmp(c->argv[i], flag) == 0 && strcmp(c->argv[i + 1], value) == 0) return 1;
    return 0;
}

static RaceSetup setup_of(const char *folder, const char *track, int night, int ticks)
{
    RaceSetup s = { 0 };
    s.game = "/g/tnfs", s.cd = "/cd", s.project = "/p/proj", s.user = "/tmp/u", s.car = car_export_car(index_of(folder)), s.track = track, s.night = night, s.photo_ticks = ticks, s.shot = "/tmp/f.ppm";
    return s;
}

static void test_command(void)
{
    char *base_env[] = { "HOME=/h", "TNFS_CAR_MASKS=/elsewhere", "TNFS_VIEW=1", "SDL_VIDEO_DRIVER=cocoa", "PATH=/bin", NULL };
    RaceCommand c;
    char why[300];
    /* a car you drive, a photo: the car's number, alone, the project's masks and dash masks, the headless drivers, the follow view */
    RaceSetup s = setup_of("P911", "TR5", 1, RACE_TICKS_DRIVEN);
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why));
    CHECK(strcmp(c.argv[0], "/g/tnfs") == 0 && c.argv[c.argc] == NULL && c.envp[c.envc] == NULL);
    CHECK(arg_pair(&c, "--data", "/cd") && arg_pair(&c, "--user", "/tmp/u") && arg_pair(&c, "--drive", "TR5") && arg_pair(&c, "--car", "2") && arg_pair(&c, "--racers", "1"));
    CHECK(arg_pair(&c, "--node", "200") && arg_pair(&c, "--screenshot", "/tmp/f.ppm") && !has_arg(&c, "--type") && !has_arg(&c, "--mods"));
    CHECK(has_env(&c, "TNFS_CAR_MASKS=/p/proj/car_masks") && has_env(&c, "TNFS_DASH_MASKS=/p/proj/dash_masks"));
    CHECK(has_env(&c, "TNFS_VIEW=3") && has_env(&c, "SDL_VIDEO_DRIVER=dummy") && has_env(&c, "SDL_AUDIO_DRIVER=dummy") && has_env(&c, "SDL_RENDER_DRIVER=software"));
    CHECK(has_env(&c, "HOME=/h") && has_env(&c, "PATH=/bin"));
    CHECK(!has_env(&c, "TNFS_CAR_MASKS=/elsewhere") && !has_env(&c, "TNFS_VIEW=1") && !has_env(&c, "SDL_VIDEO_DRIVER=cocoa"));   /* the person's own do not change the test */
    CHECK(!has_env_key(&c, "TNFS_FOLLOW_CAR") && !has_env_key(&c, "TNFS_DRIVE_SEED"));
    car_race_command_free(&c);
    /* the same in a window: no screenshot, no headless drivers (the person's SDL ones stay), no view kind */
    s = setup_of("MRX7", "TR2", 0, 0);
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why));
    CHECK(arg_pair(&c, "--car", "7") && !has_arg(&c, "--node") && !has_arg(&c, "--screenshot"));
    CHECK(has_env(&c, "SDL_VIDEO_DRIVER=cocoa") && !has_env_key(&c, "TNFS_VIEW") && has_env(&c, "TNFS_CAR_MASKS=/p/proj/car_masks"));
    car_race_command_free(&c);
    /* a traffic car: a race with traffic (type 1), the first car for the player, the slot followed and the seed of the draw, no dash masks */
    s = setup_of("Wagon", "CL1", 1, RACE_TICKS_TRAFFIC);
    s.slot = 4, s.seed = RACE_SEED_FIRST + 3;
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why));
    CHECK(arg_pair(&c, "--type", "1") && arg_pair(&c, "--car", "0") && !has_arg(&c, "--racers") && arg_pair(&c, "--node", "800"));
    CHECK(has_env(&c, "TNFS_FOLLOW_CAR=4") && has_env(&c, "TNFS_DRIVE_SEED=11115") && !has_env_key(&c, "TNFS_DASH_MASKS"));
    car_race_command_free(&c);
    /* the police car the same */
    s = setup_of("COPMUST", "AL2", 0, RACE_TICKS_POLICE);
    s.slot = 2;
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why) && arg_pair(&c, "--type", "1") && arg_pair(&c, "--node", "250"));
    CHECK(has_env(&c, "TNFS_POLICE_NEAR=15") && has_env(&c, "TNFS_FOLLOW_CAR=2"));   /* the police put behind the player, chasing: the camera on it has it at any tick */
    car_race_command_free(&c);
    s = setup_of("Wagon", "CL1", 0, 0);   /* the police are put near only for a photo of the police car */
    CHECK(car_race_command(&s, base_env, &c, why, sizeof why) && !has_env_key(&c, "TNFS_POLICE_NEAR"));
    car_race_command_free(&c);
    /* a circuit has no traffic: refused, with the way out; a track that is not one; a setup that is not whole */
    s = setup_of("Wagon", "TR5", 0, 0);
    CHECK(!car_race_command(&s, base_env, &c, why, sizeof why) && strstr(why, "no traffic") != NULL && strstr(why, "Rusty Springs") != NULL);
    s = setup_of("P911", "NOPE", 0, 0);
    CHECK(!car_race_command(&s, base_env, &c, why, sizeof why) && strstr(why, "not a track") != NULL);
    s = setup_of("P911", "TR5", 0, RACE_TICKS_DRIVEN);
    s.shot = NULL;
    CHECK(!car_race_command(&s, base_env, &c, why, sizeof why));
    s.game = NULL;
    CHECK(!car_race_command(&s, base_env, &c, why, sizeof why));
    /* a long environment (a shell's has a hundred words and more) goes through */
    static char words[300][32];
    char *big[301];
    for (int i = 0; i < 300; ++i) snprintf(words[i], sizeof words[i], "VAR%d=value", i), big[i] = words[i];
    big[300] = NULL;
    s = setup_of("P911", "TR5", 0, 0);
    CHECK(car_race_command(&s, big, &c, why, sizeof why) && c.envc >= 300);
    car_race_command_free(&c);
}

static void test_tracks(void)
{
    CHECK(car_race_track_count() == 16);
    CHECK(car_race_track(-1) == NULL && car_race_track(16) == NULL);
    int stages = 0;
    for (int i = 0; i < car_race_track_count(); ++i) stages += car_race_track(i)->stage;
    CHECK_EQ_INT(stages, 9);
    CHECK(car_race_track_find("tr5") == car_race_track_find("TR5") && car_race_track_find("TR5") >= 0 && car_race_track_find("XX9") < 0 && car_race_track_find(NULL) < 0);
    CHECK(strcmp(car_race_track(car_race_default_track(car_export_car(index_of("P911"))))->stem, "TR5") == 0);   /* Rusty Springs: the night reference */
    CHECK(strcmp(car_race_track(car_race_default_track(car_export_car(index_of("Wagon"))))->stem, "CL1") == 0);   /* a stage: the circuits have no traffic */
    CHECK(car_race_track(car_race_default_track(car_export_car(index_of("COPMUST"))))->stage);
    CHECK_EQ_INT(car_race_photo_ticks(car_export_car(index_of("P911"))), RACE_TICKS_DRIVEN);
    CHECK_EQ_INT(car_race_photo_ticks(car_export_car(index_of("Wagon"))), RACE_TICKS_TRAFFIC);
    CHECK_EQ_INT(car_race_photo_ticks(car_export_car(index_of("COPMUST"))), RACE_TICKS_POLICE);
}

static void read_all(const char *path, char *out, size_t cap)
{
    FILE *f = fopen(path, "rb");
    out[0] = '\0';
    if (f != NULL) {
        const size_t n = fread(out, 1, cap - 1, f);
        out[n] = '\0';
        fclose(f);
    }
}

static void test_files_and_temp(void)
{
    char temp[1024], user[1100], why[300], text[400], path[1200];
    CHECK(car_race_temp(temp, sizeof temp, user, sizeof user, why, sizeof why));
    CHECK(strstr(temp, "studio-race-") != NULL && strcmp(user + strlen(temp), "/user") == 0);
    struct stat st;
    CHECK(stat(user, &st) == 0 && S_ISDIR(st.st_mode));
    CHECK(car_race_user_files(user, 1, why, sizeof why));
    snprintf(path, sizeof path, "%s/port.cfg", user);
    read_all(path, text, sizeof text);
    CHECK(strcmp(text, "profile=restomod\n") == 0);
    snprintf(path, sizeof path, "%s/restomod.cfg", user);
    read_all(path, text, sizeof text);
    CHECK(strstr(text, "light_time=night\n") != NULL && strstr(text, "light_effects=1\n") != NULL && strstr(text, "render_scale=1\n") != NULL);
    CHECK(car_race_user_files(user, 0, why, sizeof why));
    read_all(path, text, sizeof text);
    CHECK(strstr(text, "light_time=day\n") != NULL && strstr(text, "light_effects") == NULL);
    CHECK(strncmp(user, getenv("TMPDIR"), strlen(getenv("TMPDIR"))) == 0);   /* in the temp folder, never the player's own */
    CHECK(!car_race_user_files("/nonexistent-folder/user", 0, why, sizeof why) && strstr(why, "cannot write") != NULL);
    CHECK(car_race_temp_remove(temp) && stat(temp, &st) != 0);
    CHECK(car_race_temp_remove(temp));   /* already gone: fine */
    /* only what car_race_temp made is ever removed */
    char other[1100];
    snprintf(other, sizeof other, "%s/keep-me", base);
    mkdir(other, 0777);
    CHECK(!car_race_temp_remove(other) && stat(other, &st) == 0);
    CHECK(!car_race_temp_remove(NULL) && !car_race_temp_remove("/"));
    rmdir(other);
}

static void test_log(void)
{
    const char *log = "line\nrace car: slot 0 is TSupra\nrace car: slot 1 is LDiabl\nrace car: slot 3 is Wagon\nrace car: slot 4 is axxess\n";
    CHECK_EQ_INT(car_race_find_slot(log, "WAGON"), 3);
    CHECK_EQ_INT(car_race_find_slot(log, "axxess"), 4);
    CHECK_EQ_INT(car_race_find_slot(log, "AXXESS"), 4);
    CHECK_EQ_INT(car_race_find_slot(log, "probe"), -1);
    CHECK_EQ_INT(car_race_find_slot("", "Wagon"), -1);
    CHECK_EQ_INT(car_race_find_slot("race car: slot x is Wagon\n", "Wagon"), -1);
    CHECK_EQ_INT(car_race_find_slot("race car: slot 2 is CopMust", "COPMUST"), 2);
    CHECK_EQ_INT(car_race_line_kind("tick 60 (phase 1): node 20", 26), RACE_LINE_PLAIN);
    CHECK_EQ_INT(car_race_line_kind("car mask: /p/car_masks/Wagon/side_mask.png", 42), RACE_LINE_MASK);
    CHECK_EQ_INT(car_race_line_kind("platform: cannot read /p/TSupra/frnt_mask.png: Couldn't open /p", 64), RACE_LINE_NOTE);
    CHECK_EQ_INT(car_race_line_kind("platform: cannot read /p/x.cfm: Couldn't open /p", 47), RACE_LINE_ERROR);
    CHECK_EQ_INT(car_race_line_kind("FATAL: no game data", 19), RACE_LINE_ERROR);
    CHECK_EQ_INT(car_race_line_kind("car mask: cannot read it", 24), RACE_LINE_ERROR);
    CHECK_EQ_INT(car_race_line_kind("track not found", 15), RACE_LINE_ERROR);
    CHECK_EQ_INT(car_race_line_kind("", 0), RACE_LINE_PLAIN);
    const char *noisy = "a\nplatform: cannot read /p/TSupra/a_mask.png: Couldn't open /p\nb\nplatform: cannot read /p/TSupra/b_mask.png: Couldn't open /p\nplatform: cannot read /p/x.cfm: Couldn't open /p\n";
    CHECK_EQ_INT(car_race_count_notes(noisy, strlen(noisy)), 2);   /* the two masks; the car file is an error */
    CHECK_EQ_INT(car_race_count_notes("", 0), 0);
    { const char *last = "platform: cannot read /p/q_mask.png: Couldn't open /p"; CHECK_EQ_INT(car_race_count_notes(last, strlen(last)), 1); }   /* the last line without a newline */
}

static void write_text_file(const char *path, const char *text, mode_t mode)
{
    FILE *f = fopen(path, "wb");
    if (f != NULL) fputs(text, f), fclose(f);
    chmod(path, mode);
}

static void test_ppm(void)
{
    char path[1100], why[200];
    uint8_t *rgba = NULL;
    int w = 0, h = 0;
    snprintf(path, sizeof path, "%s/f.ppm", base);
    FILE *f = fopen(path, "wb");
    fputs("P6\n640 480\n255\n", f);
    for (int i = 0; i < 640 * 480; ++i) fputc(10, f), fputc(20, f), fputc(30, f);
    fclose(f);
    CHECK(car_race_read_ppm(path, &rgba, &w, &h, why, sizeof why) && w == 640 && h == 480);
    CHECK(rgba != NULL && rgba[0] == 10 && rgba[1] == 20 && rgba[2] == 30 && rgba[3] == 255 && rgba[4 * (640 * 480 - 1) + 2] == 30);
    free(rgba);
    write_text_file(path, "P6\n320 200\n255\nxx", 0644);
    CHECK(!car_race_read_ppm(path, &rgba, &w, &h, why, sizeof why) && strstr(why, "640x480") != NULL);
    write_text_file(path, "P6\n640 480\n255\nshort", 0644);
    CHECK(!car_race_read_ppm(path, &rgba, &w, &h, why, sizeof why) && strstr(why, "cut short") != NULL);
    CHECK(!car_race_read_ppm("/nonexistent.ppm", &rgba, &w, &h, why, sizeof why));
    remove(path);
}

/* a fake game: it writes the lines the real one does into --user's log; FAKE_MODE says which traffic it draws */
static void make_fake_game(char *path, size_t cap)
{
    snprintf(path, cap, "%s/fake-tnfs", base);
    write_text_file(path,
        "#!/bin/sh\n"
        "user=; shot=\n"
        "while [ $# -gt 0 ]; do case \"$1\" in --user) user=$2; shift;; --screenshot) shot=$2; shift;; esac; shift; done\n"
        "echo \"fake game out follow=$TNFS_FOLLOW_CAR seed=$TNFS_DRIVE_SEED masks=$TNFS_CAR_MASKS\"\n"
        "case \"$FAKE_MODE\" in\n"
        "  fail) echo \"error: no such track\" >&2; exit 3;;\n"
        "  hang) sleep 30; exit 0;;\n"
        "esac\n"
        "case \"$FAKE_MODE\" in\n"
        "  old) ;;\n"
        "  never) printf 'race car: slot 0 is TSupra\\nrace car: slot 3 is BMW\\n' > \"$user/spirited-drive.log\";;\n"
        "  later) if [ \"$TNFS_DRIVE_SEED\" = 11113 ]; then printf 'race car: slot 0 is TSupra\\nrace car: slot 5 is Wagon\\n' > \"$user/spirited-drive.log\";\n"
        "         else printf 'race car: slot 0 is TSupra\\nrace car: slot 3 is BMW\\n' > \"$user/spirited-drive.log\"; fi;;\n"
        "  *) printf 'race car: slot 0 is TSupra\\nrace car: slot 3 is Wagon\\n' > \"$user/spirited-drive.log\";;\n"
        "esac\n"
        "if [ -n \"$shot\" ]; then printf 'P6\\n640 480\\n255\\n' > \"$shot\"; head -c 921600 /dev/zero >> \"$shot\"; fi\n"
        "exit 0\n", 0755);
}

static void run_job(RaceJob *job)
{
    for (int i = 0; i < 3000 && car_race_poll(job); ++i) usleep(5000);
}

static int temps_left(void)
{
    char cmd[700];
    snprintf(cmd, sizeof cmd, "ls %s | grep -q studio-race-", base);
    return system(cmd) == 0;
}

static void test_job(void)
{
    char game[600], why[300], project[600];
    make_fake_game(game, sizeof game);
    snprintf(project, sizeof project, "%s/proj", base);
    mkdir(project, 0777);
    RaceJob job;
    memset(&job, 0, sizeof job);
    job.run.fd = -1;
    const ExportCar *p911 = car_export_car(index_of("P911")), *wagon = car_export_car(index_of("Wagon")), *cop = car_export_car(index_of("COPMUST"));
    setenv("FAKE_MODE", "ok", 1);
    /* a car you drive: one run, a photo, the scratch folder gone after */
    CHECK(car_race_start_photo(&job, game, "/cd", project, p911, "TR5", 1, why, sizeof why));
    CHECK(car_race_busy(&job) && !car_race_start_photo(&job, game, "/cd", project, p911, "TR5", 1, why, sizeof why) && strstr(why, "already") != NULL);
    run_job(&job);
    CHECK(!car_race_busy(&job) && job.finished && job.ok && job.photo != NULL && job.photo_serial == 1 && job.photo_night == 1);
    CHECK(strstr(job.summary, "Porsche 911 Carrera on TR5, night") != NULL);
    CHECK(strstr(job.log, "fake game out") != NULL && strstr(job.log, "masks=") != NULL && strstr(job.log, "/proj/car_masks") != NULL);
    CHECK(job.temp[0] == '\0' && !temps_left());
    /* a traffic car found at once: the probe, then the shot following its slot */
    CHECK(car_race_start_photo(&job, game, "/cd", project, wagon, "CL1", 0, why, sizeof why));
    run_job(&job);
    CHECK(job.ok && job.photo_serial == 2 && job.photo_slot == 3 && strstr(job.summary, "slot 3") != NULL && strstr(job.summary, "800 ticks") != NULL);
    CHECK(strstr(job.log, "follow=3") != NULL && strstr(job.log, "the car is in it") != NULL && !temps_left());
    /* found in the third draw: the seeds are tried in order */
    setenv("FAKE_MODE", "later", 1);
    CHECK(car_race_start_photo(&job, game, "/cd", project, wagon, "CL1", 1, why, sizeof why));
    run_job(&job);
    CHECK(job.ok && job.photo_slot == 5 && job.photo_seed == 11113 && strstr(job.log, "seed=11112") != NULL && strstr(job.log, "follow=5") != NULL);
    CHECK(strstr(job.log, "the car is not in it") != NULL && job.probes == 2);
    /* in none of the draws: no photo, said plainly, nothing left */
    setenv("FAKE_MODE", "never", 1);
    const int serial = job.photo_serial;
    CHECK(car_race_start_photo(&job, game, "/cd", project, wagon, "CL1", 0, why, sizeof why));
    run_job(&job);
    CHECK(job.finished && !job.ok && strstr(job.summary, "none of") != NULL && strstr(job.summary, "no tick where it is on screen") != NULL);
    CHECK(job.probes == RACE_SEEDS + 1 && job.photo == NULL && job.photo_serial == serial && !temps_left());
    /* a game that does not say who is in which slot (an older one) */
    setenv("FAKE_MODE", "old", 1);
    CHECK(car_race_start_photo(&job, game, "/cd", project, cop, "AL1", 0, why, sizeof why));
    run_job(&job);
    CHECK(!job.ok && strstr(job.summary, "older") != NULL && !temps_left());
    /* the police car is in slot 3 of the fake: any car is found by its name */
    setenv("FAKE_MODE", "ok", 1);
    /* a game that fails: its exit code and its output are in the log, no photo */
    setenv("FAKE_MODE", "fail", 1);
    CHECK(car_race_start_photo(&job, game, "/cd", project, p911, "TR5", 0, why, sizeof why));
    run_job(&job);
    CHECK(!job.ok && strstr(job.summary, "exit code 3") != NULL && strstr(job.log, "no such track") != NULL && job.photo == NULL && !temps_left());
    /* Test in race: the game in its window, run to its end */
    setenv("FAKE_MODE", "ok", 1);
    CHECK(car_race_start_race(&job, game, "/cd", project, p911, "TR2", 1, why, sizeof why));
    run_job(&job);
    CHECK(job.ok && strstr(job.summary, "ended") != NULL && !temps_left());
    /* Stop ends a game that does not */
    setenv("FAKE_MODE", "hang", 1);
    CHECK(car_race_start_race(&job, game, "/cd", project, p911, "TR2", 0, why, sizeof why));
    usleep(100000);
    CHECK(car_race_busy(&job));
    car_race_stop(&job);
    run_job(&job);
    CHECK(!car_race_busy(&job) && strstr(job.summary, "Stopped") != NULL && !temps_left());
    /* refusals: no game, a car that is not an original, an unknown track */
    CHECK(!car_race_start_race(&job, "/nonexistent/tnfs", "/cd", project, p911, "TR2", 0, why, sizeof why) && strstr(why, "Locate game") != NULL);
    CHECK(!car_race_start_race(&job, game, "/cd", project, NULL, "TR2", 0, why, sizeof why) && strstr(why, "Export original car") != NULL);
    CHECK(!car_race_start_race(&job, game, "/cd", project, p911, "ZZ9", 0, why, sizeof why));
    CHECK(!car_race_start_race(&job, game, "/cd", project, wagon, "TR5", 0, why, sizeof why) && strstr(why, "no traffic") != NULL && !temps_left());   /* nothing made before the refusal */
    car_race_free(&job);
    rmdir(project);
    unlink(game);
}

static void test_original_of(void)
{
    CarProject c;
    memset(&c, 0, sizeof c);
    snprintf(c.info.stem, sizeof c.info.stem, "%s", "P911");
    CHECK(car_race_original_of(&c) == car_export_car(index_of("P911")));
    snprintf(c.info.stem, sizeof c.info.stem, "%s", "TSUPRA");
    CHECK(car_race_original_of(&c) == car_export_car(index_of("TSupra")));
    snprintf(c.info.stem, sizeof c.info.stem, "%s", "C84");   /* a car of somebody's own */
    CHECK(car_race_original_of(&c) == NULL);
    c.masks_only = 1;
    snprintf(c.folder, sizeof c.folder, "%s", "axxess");
    CHECK(car_race_original_of(&c) == car_export_car(index_of("axxess")));
    snprintf(c.folder, sizeof c.folder, "%s", "NOPE");
    CHECK(car_race_original_of(&c) == NULL);
    c.masks_only = 0;
    snprintf(c.info.stem, sizeof c.info.stem, "%s", "WAGON");   /* a traffic car's stem in a car project is not an original: they have no PBS */
    CHECK(car_race_original_of(&c) == NULL);
}

int main(void)
{
    snprintf(base, sizeof base, "%s/car_race_test_XXXXXX", getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    if (mkdtemp(base) == NULL) return 2;
    char real[512];
    if (realpath(base, real) != NULL) snprintf(base, sizeof base, "%s", real);
    setenv("TMPDIR", base, 1);   /* the scratch folders of the jobs are made here, and counted */
    test_command();
    test_tracks();
    test_files_and_temp();
    test_log();
    test_ppm();
    test_original_of();
    test_job();
    nftw(base, remove_entry, 16, FTW_DEPTH | FTW_PHYS);   /* what this test made, and only it */
    return TEST_REPORT();
}
