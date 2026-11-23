/* Convert PSD through the app (editor/app_psd.c): a car kit made of plain files and small PSDs: refused with a clear status when there is no PSD folder,
 * converted into the project with the lists read again, a failure leaving the project alone. */
#include "app.h"
#include "app_dash.h"
#include "psd_fixture.h"
#include "psd_manifest.h"
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

static void put_psd(const char *rel)
{
    static const uint8_t M[4][4] = { { 255, 255, 255, 0 }, { 20, 40, 220, 255 }, { 20, 220, 40, 255 }, { 255, 0, 255, 255 } };
    FxLayer l[1] = { { "mask", NULL, 0, 0, 0, 4, 1, &M[0][0], 0, 1, 0 } };
    FxBuf f = fx_psd(4, 1, l, 1, 8, 3, 1);
    char p[700];
    path_of(p, sizeof p, rel);
    FILE *out = fopen(p, "wb");
    CHECK(out != NULL && fwrite(f.p, 1, f.n, out) == f.n);
    if (out != NULL) fclose(out);
    free(f.p);
}

static void run_to_end(App *a)
{
    for (int i = 0; i < 40000 && app_psd_poll(a); i++) usleep(5000);
}

static App *open_kit(void)
{
    const char *dirs[] = { "kit", "kit/SIMDATA", "kit/SIMDATA/CARFAMS", "kit/car_masks", "kit/car_masks/C84", "psd", "psd/c84" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) dir_of(dirs[i]);
    if (!put("kit/mod.ini", "id=boxcar\nname=Box\nversion=1\ntype=car\nstem=C84\nsound_from=911\n") || !put("kit/SIMDATA/CARFAMS/C84.CFM", "cfm") ||
        !put("kit/car_masks/C84/old_mask.png", "old"))
        return NULL;
    put_psd("psd/c84/side.psd");
    put_psd("psd/c84/frnt.psd");
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
    char psd[500], none[500];
    path_of(psd, sizeof psd, "psd"), path_of(none, sizeof none, "nowhere");
    CHECK_EQ_INT(a->car.mask_count, 1);

    /* no PSD folder for this car: said with the name and the root */
    app_psd_setup(a, none, none);
    CHECK(!app_psd_start(a) && strstr(a->status, "no PSD folder named C84") != NULL && strstr(a->status, "nowhere") != NULL);
    app_psd_setup(a, none, psd);
    /* a PSD folder spelled c84, the car's C84: found without case; converted; the lists read again */
    char dir[600];
    CHECK(app_psd_psd_dir(a, dir, sizeof dir) && strstr(dir, "/psd/c84") != NULL);
    CHECK(app_psd_start(a) && app_psd_busy(a));
    CHECK(!app_psd_start(a) && strstr(a->status, "already") != NULL);
    run_to_end(a);
    CHECK(!app_psd_busy(a) && a->psd.ok && strstr(a->status, "Converted 2 masks") != NULL);
    CHECK_EQ_INT(a->car.mask_count, 3);   /* old, side, frnt */

    /* a PSD that cannot be read: the status names it, the project is as it was */
    {
        char bad[700];
        path_of(bad, sizeof bad, "psd/c84/rear.psd");
        FILE *f = fopen(bad, "wb");
        CHECK(f != NULL);
        if (f != NULL) fputs("not a PSD", f), fclose(f);
    }
    CHECK(app_psd_start(a));
    run_to_end(a);
    CHECK(!a->psd.ok && strstr(a->status, "rear.psd") != NULL && !app_psd_busy(a));
    CHECK_EQ_INT(a->car.mask_count, 3);
    char p[700];
    path_of(p, sizeof p, "kit/car_masks/C84/rear_mask.png");
    CHECK(access(p, F_OK) != 0);
    /* another car opened in the same window: the folder is the window's, Convert PSD still works */
    char kit[500];
    path_of(kit, sizeof kit, "kit");
    CHECK(app_open_car(a, kit));
    CHECK(a->psd_root[0] != '\0' && app_psd_psd_dir(a, dir, sizeof dir));
    app_free(a);
    free(a);
}

/* a kit (dep100): its PSDs in art-source/masks, the masks made flat in art-generated/car_masks with a manifest, by itself when a PSD is newer than its mask */
static void test_kit(void)
{
    const char *dirs[] = { "kit2", "kit2/model", "kit2/art-source", "kit2/art-source/masks" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) dir_of(dirs[i]);
    CHECK(put("kit2/mod.ini", "id=kitcar\nname=Kit\nversion=1\ntype=car\nstem=C84\nsound_from=911\n"));
    put_psd("kit2/art-source/masks/side.psd");
    put_psd("kit2/art-source/masks/frnt.psd");
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    app_begin(a, "./tnfs", "", NULL);
    char kit[500], dir[600], why[300], p[700];
    path_of(kit, sizeof kit, "kit2");
    CHECK(app_open_car(a, kit) && app_car_is_kit(a));
    for (int i = 0; i < 40000 && car_build_job_busy(&a->car_build); i++) car_build_job_poll(&a->car_build), usleep(5000);   /* the unbuilt kit's own build, over before the PSDs are looked at */
    char none[500];
    path_of(none, sizeof none, "nowhere");
    app_psd_setup(a, none, none);   /* no PSD folder of the window's: the kit's own art-source/masks is the PSD folder */
    CHECK(app_psd_psd_dir(a, dir, sizeof dir) && strstr(dir, "kit2/art-source/masks") != NULL);
    (void)why;
    a->mask.me.width = a->mask.me.height = 2;   /* a mask open with paint that is not saved: a conversion would close it, so none starts by itself */
    a->mask.me.now = calloc(4, 1), a->mask.me.saved = calloc(4, 1);
    if (a->mask.me.now != NULL && a->mask.me.saved != NULL) a->mask.me.now[0] = 1;
    app_dash_stale_refresh(a);
    CHECK(a->stale_ok && a->stale.stale == 2 && !app_psd_busy(a));
    mask_edit_free(&a->mask.me);   /* saved (closed): the same state goes on */
    app_dash_stale_refresh(a);   /* the masks are not there: stale, so Convert PSD starts by itself */
    CHECK(a->stale_ok && a->stale.stale == 2 && app_psd_busy(a));
    run_to_end(a);
    CHECK(a->psd.ok && strstr(a->status, "Converted 2 masks") != NULL);
    path_of(p, sizeof p, "kit2/art-generated/car_masks/side_mask.png");
    CHECK(access(p, F_OK) == 0);   /* flat, in the kit's art-generated, not in build/piece */
    path_of(p, sizeof p, "kit2/art-generated/.generated.tsv");
    CHECK(access(p, F_OK) == 0);
    path_of(p, sizeof p, "kit2/build/piece/car_masks");
    CHECK(access(p, F_OK) != 0);
    app_dash_stale_refresh(a);
    CHECK(a->stale_ok && a->stale.stale == 0 && !app_psd_busy(a));   /* in sync: nothing more runs by itself */
    {   /* a slow, synced disk makes every open dear: with nothing changed on disk a look re-reads nothing, and the frames in between do not look at all */
        app_dash_stale_refresh(a);   /* the hashes of what the conversion wrote are made once */
        const unsigned before = manifest_opens();
        app_dash_stale_refresh(a);
        app_dash_stale_refresh(a);
        for (int i = 0; i < 100; i++) app_dash_poll(a);
        CHECK_EQ_INT((int)(manifest_opens() - before), 0);   /* neither the manifest nor a PNG was opened */
        put("kit2/art-generated/car_masks/side_mask.png", "a longer edit, so its size is not the one before");
        app_dash_stale_refresh(a);
        CHECK(manifest_opens() > before);   /* a file that changed is hashed again */
        char names[200];
        CHECK_EQ_INT(app_psd_edited(a, names, sizeof names), 1);
        CHECK(app_psd_regenerate(a) && app_psd_busy(a));   /* put back as it was for what follows */
        run_to_end(a);
        app_dash_stale_refresh(a);
        CHECK_EQ_INT(app_psd_edited(a, names, sizeof names), 0);
    }
    {   /* a PNG touched up by hand: said, kept by a conversion, replaced by Regenerate from PSD */
        char names[200];
        CHECK_EQ_INT(app_psd_edited(a, names, sizeof names), 0);
        CHECK(put("kit2/art-generated/car_masks/side_mask.png", "touched up in GIMP"));
        app_dash_stale_refresh(a);
        CHECK(app_psd_edited(a, names, sizeof names) == 1 && strcmp(names, "side_mask.png") == 0 && !app_psd_busy(a));
        CHECK(app_psd_start(a));   /* Convert PSD */
        run_to_end(a);
        CHECK(a->psd.ok && strstr(a->status, "Kept, not replaced: side_mask.png") != NULL);
        CHECK(app_psd_regenerate(a) && app_psd_busy(a));
        run_to_end(a);
        app_dash_stale_refresh(a);
        CHECK(a->psd.ok && app_psd_edited(a, names, sizeof names) == 0 && strstr(a->status, "Converted") != NULL);
    }
    CHECK(put("kit2/art-source/masks/rear.psd", "not a PSD"));   /* a PSD that cannot be read: converted by itself once, and not again while nothing changes */
    app_dash_stale_refresh(a);
    CHECK(app_psd_busy(a));
    run_to_end(a);
    CHECK(!a->psd.ok && strstr(a->status, "rear.psd") != NULL);
    app_dash_stale_refresh(a);   /* the failure is remembered by the next look... */
    app_dash_stale_refresh(a);
    CHECK(!app_psd_busy(a));     /* ...which does not start it again, however often it looks */
    CHECK(put("kit2/art-source/masks/rear.psd", "not a PSD, saved again"));
    {
        char rear[700];
        path_of(rear, sizeof rear, "kit2/art-source/masks/rear.psd");
        struct timeval later[2];
        gettimeofday(&later[0], NULL);
        later[0].tv_sec += 120, later[1] = later[0];
        utimes(rear, later);
    }
    app_dash_stale_refresh(a);   /* saved again: another state, tried again */
    CHECK(app_psd_busy(a));
    run_to_end(a);
    CHECK(!a->psd.ok);
    app_free(a);
    free(a);
}

/* «Reveal masks folder» shows what a person edits: a kit's art-source/masks (the PSDs), never art-generated/car_masks (Studio's); a piece's own PNGs */
static void test_reveal(void)
{
    App *a = open_kit();   /* a piece: its car_masks/<folder> are the PNGs */
    CHECK(a != NULL);
    if (a == NULL) return;
    CHECK(app_reveal_masks(a) && strstr(a->reveal, "kit/car_masks/C84") != NULL);
    app_free(a);
    free(a);
    const char *dirs[] = { "kit3", "kit3/model", "kit3/art-generated", "kit3/art-generated/car_masks" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) dir_of(dirs[i]);
    CHECK(put("kit3/mod.ini", "id=kitcar\nname=Kit\nversion=1\ntype=car\nstem=C84\nsound_from=911\n"));
    a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    app_begin(a, "./tnfs", "", NULL);
    char kit[500];
    path_of(kit, sizeof kit, "kit3");
    CHECK(app_open_car(a, kit) && app_car_is_kit(a));
    for (int i = 0; i < 40000 && car_build_job_busy(&a->car_build); i++) car_build_job_poll(&a->car_build), usleep(5000);
    CHECK(!car_build_job_busy(&a->car_build));
    CHECK(!app_reveal_masks(a) && a->reveal[0] == '\0' && strstr(a->status, "art-source/masks") != NULL);   /* no PSD folder yet: said, nothing shown */
    dir_of("kit3/art-source");
    dir_of("kit3/art-source/masks");
    CHECK(app_reveal_masks(a) && strstr(a->reveal, "kit3/art-source/masks") != NULL && strstr(a->reveal, "art-generated") == NULL);
    app_free(a);
    free(a);
}

static void remove_tree(void)
{
    char cmd[700];   /* kit3 and what its build wrote: a tree, not a list */
    snprintf(cmd, sizeof cmd, "find '%s/kit3' -depth -delete", root);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s/kit3\n", root);
    const char *files[] = { "kit/mod.ini", "kit/SIMDATA/CARFAMS/C84.CFM", "kit/car_masks/C84/old_mask.png", "kit/car_masks/C84/side_mask.png", "kit/car_masks/C84/frnt_mask.png",
                            "psd/c84/side.psd", "psd/c84/frnt.psd", "psd/c84/rear.psd" };
    char p[700];
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) path_of(p, sizeof p, files[i]), unlink(p);
    const char *dirs[] = { "kit/car_masks/C84", "kit/car_masks", "kit/SIMDATA/CARFAMS", "kit/SIMDATA", "kit", "psd/c84", "psd" };
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
    test_kit();
    test_reveal();
    remove_tree();
    return TEST_REPORT();
}
