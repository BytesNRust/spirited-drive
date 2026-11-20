/* Convert PSD through the app (editor/app_psd.c): a car kit made of plain files and fake converter scripts (python, stdlib only): refused with a clear
 * status when there is no tools folder or no PSD folder, converted into the project with the lists read again, a failure leaving the project alone. */
#include "app.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static const char BODY[] =
    "import os, sys\n"
    "src, dest = sys.argv[1], sys.argv[2]\n"
    "for car in sorted(os.listdir(src)):\n"
    "    for n in sorted(os.listdir(os.path.join(src, car))):\n"
    "        if n.endswith('.psd'):\n"
    "            os.makedirs(os.path.join(dest, car), exist_ok=True)\n"
    "            open(os.path.join(dest, car, n[:-4] + '_mask.png'), 'w').write('png')\n"
    "            if os.environ.get('FAKE_PSD_FAIL'):\n"
    "                print('psd-tools exploded'); sys.exit(3)\n";

static void wait_for_python(App *a)
{
    for (int i = 0; i < 4000 && a->py.state != 2; i++) app_psd_poll(a), usleep(5000);
}

static void run_to_end(App *a)
{
    for (int i = 0; i < 4000 && app_psd_poll(a); i++) usleep(5000);
}

static App *open_kit(void)
{
    const char *dirs[] = { "kit", "kit/SIMDATA", "kit/SIMDATA/CARFAMS", "kit/car_masks", "kit/car_masks/C84", "tools", "psd", "psd/c84" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) dir_of(dirs[i]);
    if (!put("kit/mod.ini", "id=boxcar\nname=Box\nversion=1\ntype=car\nstem=C84\nsound_from=911\n") || !put("kit/SIMDATA/CARFAMS/C84.CFM", "cfm") ||
        !put("kit/car_masks/C84/old_mask.png", "old") || !put("tools/car_mask_from_psd.py", BODY) || !put("psd/c84/side.psd", "psd") || !put("psd/c84/frnt.psd", "psd"))
        return NULL;
    App *a = calloc(1, sizeof *a);
    if (a == NULL) return NULL;
    app_begin(a, "./tnfs", "", NULL);
    char kit[500];
    path_of(kit, sizeof kit, "kit");
    if (!app_open_car(a, kit)) return free(a), NULL;
    return a;
}

static void test_flows(void)
{
    App *a = open_kit();
    CHECK(a != NULL);
    if (a == NULL) return;
    char tools[500], psd[500], none[500];
    path_of(tools, sizeof tools, "tools"), path_of(psd, sizeof psd, "psd"), path_of(none, sizeof none, "nowhere");
    CHECK_EQ_INT(a->car.mask_count, 1);

    /* no tools folder found: said; nothing runs */
    unsetenv("TNFS_TOOLS");
    app_psd_setup(a, none, psd);
    CHECK(a->tools[0] == '\0' && !app_psd_start(a) && strstr(a->status, "tools folder") != NULL);
    /* tools found by the environment; no PSD folder for this car: said with the name and the root */
    setenv("TNFS_TOOLS", tools, 1);
    app_psd_setup(a, none, none);
    CHECK(a->tools[0] != '\0' && !app_psd_start(a) && strstr(a->status, "no PSD folder named C84") != NULL && strstr(a->status, "nowhere") != NULL);

    /* the python: TNFS_PYTHON is tried first; this one answers the import check and otherwise runs the real python */
    char interp[500];
    path_of(interp, sizeof interp, "py_fake");
    CHECK(put("py_fake", "#!/bin/sh\n[ \"$1\" = -c ] && exit 0\nexec python3 \"$@\"\n") && chmod(interp, 0755) == 0);
    setenv("TNFS_PYTHON", interp, 1);
    app_psd_setup(a, none, psd);
    CHECK(!app_psd_python_ready(a) && !app_psd_start(a) && strstr(a->status, "still looking") != NULL);   /* not looked for yet */
    wait_for_python(a);
    CHECK(app_psd_python_ready(a) && strcmp(a->py.python, interp) == 0);
    char note[400];
    app_psd_python_note(a, note, sizeof note);
    CHECK(strstr(note, "py_fake") != NULL);   /* the tooltip says which one */
    /* a PSD folder spelled c84, the car's C84: found without case; converted; the lists read again */
    char dir[600];
    CHECK(app_psd_psd_dir(a, dir, sizeof dir) && strstr(dir, "/psd/c84") != NULL);
    CHECK(app_psd_start(a) && app_psd_busy(a));
    CHECK(!app_psd_start(a) && strstr(a->status, "already") != NULL);
    run_to_end(a);
    CHECK(!app_psd_busy(a) && a->psd.ok && strstr(a->status, "Converted 2 masks") != NULL);
    CHECK_EQ_INT(a->car.mask_count, 3);   /* old, side, frnt */

    /* a failing converter: the status says what it printed, the project is as it was */
    put("psd/c84/rear.psd", "psd");
    setenv("FAKE_PSD_FAIL", "1", 1);
    CHECK(app_psd_start(a));
    run_to_end(a);
    unsetenv("FAKE_PSD_FAIL");
    CHECK(!a->psd.ok && strstr(a->status, "exploded") != NULL && !app_psd_busy(a));
    CHECK_EQ_INT(a->car.mask_count, 3);
    char p[700];
    path_of(p, sizeof p, "kit/car_masks/C84/rear_mask.png");
    CHECK(access(p, F_OK) != 0);
    /* a window whose probe has not finished opens a car: TNFS_PYTHON is still the first candidate */
    App *b = calloc(1, sizeof *b);
    CHECK(b != NULL);
    if (b != NULL) {
        app_begin(b, "./tnfs", "", NULL);
        CHECK(strcmp(b->py.cand[0], interp) == 0 && b->py.state == 0);
        char kit0[500];
        path_of(kit0, sizeof kit0, "kit");
        CHECK(app_open_car(b, kit0) && strcmp(b->py.cand[0], interp) == 0);
        app_free(b);
        free(b);
    }
    /* another car opened in the same window: the folders are the window's, Convert PSD still works */
    char kit[500];
    path_of(kit, sizeof kit, "kit");
    CHECK(app_open_car(a, kit));
    CHECK(a->tools[0] != '\0' && a->psd_root[0] != '\0' && app_psd_psd_dir(a, dir, sizeof dir));
    unsetenv("TNFS_PYTHON");
    app_free(a);
    free(a);
}

static void remove_tree(void)
{
    const char *files[] = { "kit/mod.ini", "kit/SIMDATA/CARFAMS/C84.CFM", "kit/car_masks/C84/old_mask.png", "kit/car_masks/C84/side_mask.png", "kit/car_masks/C84/frnt_mask.png",
                            "tools/car_mask_from_psd.py", "psd/c84/side.psd", "psd/c84/frnt.psd", "psd/c84/rear.psd", "py_fake" };
    char p[700];
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) path_of(p, sizeof p, files[i]), unlink(p);
    const char *dirs[] = { "kit/car_masks/C84", "kit/car_masks", "kit/SIMDATA/CARFAMS", "kit/SIMDATA", "kit", "tools", "psd/c84", "psd" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) path_of(p, sizeof p, dirs[i]), rmdir(p);
    rmdir(root);
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/tnfs-editor-psd-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(root) == NULL) {
        fprintf(stderr, "no scratch folder\n");
        return 1;
    }
    test_flows();
    remove_tree();
    return TEST_REPORT();
}
