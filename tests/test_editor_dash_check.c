/* Check dash through the app (editor/app_dash.c): a car you drive, its PSDs newer than its masks (the warning), Convert PSD with small PSDs, and then
 * the cockpit photo at night with gauge light 5 from a fake game that writes down what it was asked; a PSD that cannot be read takes no photo; the Race window's own
 * choices are as they were. */
#include "app.h"
#include "app_dash.h"
#include "psd_fixture.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

static char root[400];

static void path_of(char *out, size_t cap, const char *rel)
{
    snprintf(out, cap, "%s/%s", root, rel);
}

static int put(const char *rel, const char *text)
{
    char p[700];
    path_of(p, sizeof p, rel);
    FILE *f = fopen(p, "wb");
    if (f == NULL) return 0;
    const size_t n = strlen(text);
    const int ok = fwrite(text, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

static void dir_of(const char *rel)
{
    char p[700];
    path_of(p, sizeof p, rel);
    mkdir(p, 0755);
}

static void age(const char *rel, long seconds_ago)
{
    char p[700];
    path_of(p, sizeof p, rel);
    struct timeval tv[2];
    gettimeofday(&tv[0], NULL);
    tv[0].tv_sec -= seconds_ago;
    tv[1] = tv[0];
    utimes(p, tv);
}

/* a PSD with a "mask" layer (a dash's too: the colours of its classes) at `rel` */
static int put_psd(const char *rel)
{
    static const uint8_t M[4][4] = { { 255, 255, 255, 0 }, { 20, 40, 220, 255 }, { 20, 220, 40, 255 }, { 255, 0, 255, 255 } };
    FxLayer l[1] = { { "mask", NULL, 0, 0, 0, 4, 1, &M[0][0], 0, 1, 0 } };
    FxBuf f = fx_psd(4, 1, l, 1, 8, 3, 1);
    char p[700];
    path_of(p, sizeof p, rel);
    FILE *out = fopen(p, "wb");
    const int ok = out != NULL && fwrite(f.p, 1, f.n, out) == f.n;
    if (out != NULL) fclose(out);
    free(f.p);
    return ok;
}

/* the fake game: writes down the environment and the look it was asked for, a 640x480 frame and the log the photo job reads */
static const char GAME[] =
    "#!/bin/sh\n"
    "user=; shot=\n"
    "while [ $# -gt 0 ]; do case \"$1\" in --user) user=$2; shift;; --screenshot) shot=$2; shift;; esac; shift; done\n"
    "env | grep '^TNFS_' | sort > \"$FAKE_GAME_DIR/env.txt\"\n"
    "cp \"$user/restomod.cfg\" \"$FAKE_GAME_DIR/restomod.txt\"\n"
    "printf 'race car: slot 0 is P911\\n' > \"$user/spirited-drive.log\"\n"
    "printf 'P6\\n640 480\\n255\\n' > \"$shot\"; head -c 921600 /dev/zero >> \"$shot\"\n";

static int contains_file(const char *rel, const char *needle)
{
    char p[700], text[4096];
    path_of(p, sizeof p, rel);
    FILE *f = fopen(p, "rb");
    if (f == NULL) return 0;
    const size_t n = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    text[n] = '\0';
    return strstr(text, needle) != NULL;
}

static void run_to_end(App *a)
{
    for (int i = 0; i < 6000 && (app_psd_busy(a) || a->dash_check == 1 || app_race_busy(a)); i++) {
        app_psd_poll(a);
        app_dash_poll(a);
        app_race_poll(a);
        usleep(5000);
    }
}

static App *open_kit(void)
{
    const char *dirs[] = { "kit", "kit/SIMDATA", "kit/SIMDATA/CARFAMS", "kit/car_masks", "kit/car_masks/P911", "psd", "psd/P911", "cd" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) dir_of(dirs[i]);
    if (!put("kit/mod.ini", "id=boxcar\nname=Box\nversion=1\ntype=car\nstem=P911\nsound_from=911\n") || !put("kit/SIMDATA/CARFAMS/P911.CFM", "cfm") ||
        !put("kit/car_masks/P911/frnt_mask.png", "old") ||
        !put_psd("psd/P911/frnt.psd") || !put_psd("psd/P911/dash.psd") || !put("game", GAME))
        return NULL;
    char game[500];
    path_of(game, sizeof game, "game");
    chmod(game, 0755);
    age("kit/car_masks/P911/frnt_mask.png", 3600);   /* the PSDs are newer: just written */
    App *a = calloc(1, sizeof *a);
    if (a == NULL) return NULL;
    app_begin(a, game, "", NULL);
    char kit[500];
    path_of(kit, sizeof kit, "kit");
    if (!app_open_car(a, kit)) return free(a), NULL;
    return a;
}

static void test_flow(void)
{
    App *a = open_kit();
    CHECK(a != NULL);
    if (a == NULL) return;
    char psd[500], cd[500], why[800];
    path_of(psd, sizeof psd, "psd"), path_of(cd, sizeof cd, "cd");
    char game_dir[500];
    path_of(game_dir, sizeof game_dir, "");
    setenv("FAKE_GAME_DIR", root, 1);
    app_psd_setup(a, "/nowhere", psd);
    snprintf(a->export_src.cd, sizeof a->export_src.cd, "%s", cd);
    /* the warning: the body PSD is newer than its mask, the dash PSD has no mask at all */
    app_dash_stale_refresh(a);
    CHECK(strstr(a->dash_stale, "frnt.psd is newer than frnt_mask.png") != NULL && strstr(a->dash_stale, "dash.psd has no dash_mask.png") != NULL);
    CHECK(app_mask_dash_state(a) != NULL && strcmp(app_mask_dash_state(a), "PSD found, no mask yet") == 0);   /* the Masks window's dash row says it */
    CHECK(app_psd_busy(a) && !app_dash_possible(a, why, sizeof why));   /* and Convert PSD started by itself: the PSDs are newer than the masks */
    run_to_end(a);
    char made[700];
    path_of(made, sizeof made, "kit/dash_masks/P911/dash_mask.png");
    CHECK(a->psd.ok && access(made, F_OK) == 0 && strstr(a->status, "Converted") != NULL);
    app_dash_stale_refresh(a);
    CHECK(strcmp(app_mask_dash_state(a), "in sync") == 0);
    CHECK(a->dash_stale[0] == '\0' && app_dash_possible(a, why, sizeof why));   /* in sync now, and nothing runs */

    /* one click: Convert PSD, then the cockpit photo at night with gauge light 5, this project's dash masks */
    const RaceLook look = a->race_look;
    const int night = a->race_night;
    CHECK(app_dash_check_start(a) && a->dash_check == 1 && !app_dash_possible(a, why, sizeof why));
    run_to_end(a);
    CHECK(a->psd.ok && a->dash_check == 0);
    CHECK(access(made, F_OK) == 0);   /* made from the PSD, in the project */
    CHECK(a->race.ok && a->race.photo != NULL && a->race.photo_w == 640);
    CHECK(contains_file("env.txt", "TNFS_GAUGE_LIGHT=5") && contains_file("env.txt", "TNFS_VIEW=0") && contains_file("env.txt", "dash_masks"));
    CHECK(contains_file("restomod.txt", "light_time=night"));
    CHECK(memcmp(&a->race_look, &look, sizeof look) == 0 && a->race_night == night);   /* the Race window's own choices are as they were */
    CHECK(a->dash_stale[0] == '\0');                                                   /* and nothing is older than its PSD now */

    /* a PSD that cannot be read: no photo, the status names it, the choices untouched */
    put("psd/P911/rear.psd", "not a PSD");
    const int serial = a->race.photo_serial;
    CHECK(app_dash_check_start(a));
    run_to_end(a);
    CHECK(!a->psd.ok && strstr(a->status, "rear.psd") != NULL && a->race.photo_serial == serial && a->dash_check == 0);
    app_dash_stale_refresh(a);
    CHECK(!app_psd_busy(a));   /* a state a conversion failed on is not tried again by itself... */
    CHECK(app_psd_start(a) && app_psd_busy(a));   /* ...until Convert PSD is pressed */
    run_to_end(a);
    CHECK(!a->psd.ok);
    put("psd/P911/rear.psd", "not a PSD, but another file");   /* saved again: another state, tried again */
    age("psd/P911/rear.psd", -120);
    app_dash_stale_refresh(a);
    CHECK(app_psd_busy(a));
    run_to_end(a);
    CHECK(memcmp(&a->race_look, &look, sizeof look) == 0 && a->race_night == night);
    /* a car's project with no PSD folder: refused with the folder named */
    app_psd_setup(a, "/nowhere", "/no-such-root");
    CHECK(!app_dash_possible(a, why, sizeof why) && strstr(why, "P911") != NULL && !app_dash_check_start(a));
    unsetenv("FAKE_GAME_DIR");
    app_free(a);
    free(a);
}

static void remove_tree(void)
{
    char cmd[900];
    snprintf(cmd, sizeof cmd, "find '%s' -depth -delete", root);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", root);
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/tnfs-editor-dash-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(root) == NULL) {
        fprintf(stderr, "no scratch folder\n");
        return 1;
    }
    test_flow();
    remove_tree();
    return TEST_REPORT();
}
