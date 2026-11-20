/* Check dash through the app (editor/app_dash.c): a car you drive, its PSDs newer than its masks (the warning), Convert PSD with fake converter scripts, and then
 * the cockpit photo at night with gauge light 5 from a fake game that writes down what it was asked; a failing converter takes no photo; the Race window's own
 * choices are as they were. */
#include "app.h"
#include "app_dash.h"
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

static const char BODY[] =
    "import os, sys\n"
    "src, dest = sys.argv[1], sys.argv[2]\n"
    "for car in sorted(os.listdir(src)):\n"
    "    for n in sorted(os.listdir(os.path.join(src, car))):\n"
    "        if n.endswith('.psd') and n != 'dash.psd':\n"
    "            os.makedirs(os.path.join(dest, car), exist_ok=True)\n"
    "            open(os.path.join(dest, car, n[:-4] + '_mask.png'), 'w').write('png')\n"
    "            if os.environ.get('FAKE_PSD_FAIL'):\n"
    "                print('psd-tools exploded'); sys.exit(3)\n";

static const char DASH[] =
    "import os, sys\n"
    "src, dest = sys.argv[1], sys.argv[2]\n"
    "for car in sorted(os.listdir(src)):\n"
    "    if os.path.exists(os.path.join(src, car, 'dash.psd')):\n"
    "        os.makedirs(os.path.join(dest, car), exist_ok=True)\n"
    "        open(os.path.join(dest, car, 'dash_mask.png'), 'w').write('png')\n";

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
    const char *dirs[] = { "kit", "kit/SIMDATA", "kit/SIMDATA/CARFAMS", "kit/car_masks", "kit/car_masks/P911", "tools", "psd", "psd/P911", "cd" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) dir_of(dirs[i]);
    if (!put("kit/mod.ini", "id=boxcar\nname=Box\nversion=1\ntype=car\nstem=P911\nsound_from=911\n") || !put("kit/SIMDATA/CARFAMS/P911.CFM", "cfm") ||
        !put("kit/car_masks/P911/frnt_mask.png", "old") || !put("tools/car_mask_from_psd.py", BODY) || !put("tools/dash_mask_from_psd.py", DASH) ||
        !put("psd/P911/frnt.psd", "psd") || !put("psd/P911/dash.psd", "psd") || !put("game", GAME))
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
    char tools[500], psd[500], cd[500], interp[500], why[800];
    path_of(tools, sizeof tools, "tools"), path_of(psd, sizeof psd, "psd"), path_of(cd, sizeof cd, "cd"), path_of(interp, sizeof interp, "py_fake");
    CHECK(put("py_fake", "#!/bin/sh\n[ \"$1\" = -c ] && exit 0\nexec python3 \"$@\"\n") && chmod(interp, 0755) == 0);
    setenv("TNFS_TOOLS", tools, 1);
    setenv("TNFS_PYTHON", interp, 1);
    char game_dir[500];
    path_of(game_dir, sizeof game_dir, "");
    setenv("FAKE_GAME_DIR", root, 1);
    app_psd_setup(a, "/nowhere", psd);
    snprintf(a->export_src.cd, sizeof a->export_src.cd, "%s", cd);
    /* the warning: the body PSD is newer than its mask, the dash PSD has no mask at all */
    app_dash_stale_refresh(a);
    CHECK(strstr(a->dash_stale, "frnt.psd is newer than frnt_mask.png") != NULL && strstr(a->dash_stale, "dash.psd has no dash_mask.png") != NULL);
    /* not yet: no python found */
    CHECK(!app_dash_possible(a, why, sizeof why) && strstr(why, "python") != NULL);
    for (int i = 0; i < 4000 && a->py.state != 2; i++) app_psd_poll(a), usleep(5000);
    CHECK(app_dash_possible(a, why, sizeof why));

    /* one click: Convert PSD, then the cockpit photo at night with gauge light 5, this project's dash masks */
    const RaceLook look = a->race_look;
    const int night = a->race_night;
    CHECK(app_dash_check_start(a) && a->dash_check == 1 && !app_dash_possible(a, why, sizeof why));
    run_to_end(a);
    CHECK(a->psd.ok && a->dash_check == 0);
    CHECK(contains_file("kit/dash_masks/P911/dash_mask.png", "png") || contains_file("kit/dash_masks/P911/dash_mask.png", "png"));
    CHECK(a->race.ok && a->race.photo != NULL && a->race.photo_w == 640);
    CHECK(contains_file("env.txt", "TNFS_GAUGE_LIGHT=5") && contains_file("env.txt", "TNFS_VIEW=0") && contains_file("env.txt", "dash_masks"));
    CHECK(contains_file("restomod.txt", "light_time=night"));
    CHECK(memcmp(&a->race_look, &look, sizeof look) == 0 && a->race_night == night);   /* the Race window's own choices are as they were */
    CHECK(a->dash_stale[0] == '\0');                                                   /* and nothing is older than its PSD now */

    /* a failing converter: no photo, the status says what it printed, the choices untouched */
    put("psd/P911/rear.psd", "psd");
    setenv("FAKE_PSD_FAIL", "1", 1);
    const int serial = a->race.photo_serial;
    CHECK(app_dash_check_start(a));
    run_to_end(a);
    unsetenv("FAKE_PSD_FAIL");
    CHECK(!a->psd.ok && strstr(a->status, "exploded") != NULL && a->race.photo_serial == serial && a->dash_check == 0);
    CHECK(memcmp(&a->race_look, &look, sizeof look) == 0 && a->race_night == night);
    /* a car's project with no PSD folder: refused with the folder named */
    app_psd_setup(a, "/nowhere", "/no-such-root");
    CHECK(!app_dash_possible(a, why, sizeof why) && strstr(why, "P911") != NULL && !app_dash_check_start(a));
    unsetenv("TNFS_PYTHON");
    unsetenv("TNFS_TOOLS");
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
