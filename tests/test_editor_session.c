/* The editor's session (editor/session.c, and the parts of editor/app.c that open, close and confirm): the recent projects keep their order, never
 * repeat, never pass RECENT_MAX and survive a save; a trace that changed has another fingerprint; an action asked for with unsaved changes waits
 * for Save / Discard / Cancel; Open wants a project in the folder and New wants none; a failed open leaves the project as it was. */
#include "app.h"
#include "formats/pbs.h"
#include "formats/car_piece.h"
#include "session.h"
#include "test.h"

#include <ftw.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[512];

static void path_in(char *out, size_t cap, const char *name)
{
    snprintf(out, cap, "%s/%s", root, name);
}

static void check_recent(void)
{
    Recent r;
    memset(&r, 0, sizeof r);
    char name[16], dir[600];
    for (int i = 0; i < RECENT_MAX + 3; ++i) {
        snprintf(name, sizeof name, "p%d", i);
        path_in(dir, sizeof dir, name);
        recent_push(&r, dir, RECENT_TRACK);
    }
    CHECK_EQ_INT(r.count, RECENT_MAX);                       /* the oldest three fell off */
    path_in(dir, sizeof dir, "p10");
    CHECK(strcmp(r.dir[0], dir) == 0);                       /* the newest first */
    path_in(dir, sizeof dir, "p5");
    recent_push(&r, dir, RECENT_TRACK);                                    /* opened again: it moves to the front, no copy */
    CHECK(strcmp(r.dir[0], dir) == 0);
    CHECK_EQ_INT(r.count, RECENT_MAX);
    int copies = 0;
    for (int i = 0; i < r.count; ++i) copies += strcmp(r.dir[i], dir) == 0;
    CHECK_EQ_INT(copies, 1);
    recent_push(&r, "", RECENT_TRACK);                                     /* nothing */
    CHECK_EQ_INT(r.count, RECENT_MAX);

    char file[600];
    path_in(file, sizeof file, "recent.txt");
    CHECK(recent_save(&r, file));
    Recent back;
    recent_load(&back, file);
    CHECK_EQ_INT(back.count, r.count);
    for (int i = 0; i < r.count && i < back.count; ++i) CHECK(strcmp(back.dir[i], r.dir[i]) == 0);
    path_in(file, sizeof file, "none.txt");                  /* a missing file is an empty list */
    recent_load(&back, file);
    CHECK_EQ_INT(back.count, 0);
    path_in(file, sizeof file, "recent.txt");
    remove(file);
}

static void check_fingerprint(void)
{
    TracePoint p[3] = { { 0, 0 }, { 0, 40 }, { 5, 80 } };
    Trace t = { p, 3, 40.0 };
    const uint64_t first = session_fingerprint(&t);
    CHECK(session_fingerprint(&t) == first);                 /* the same trace, the same print */
    p[1].x = 0.5;
    CHECK(session_fingerprint(&t) != first);                 /* a point moved */
    p[1].x = 0;
    CHECK(session_fingerprint(&t) == first);                 /* and back */
    t.width = 42.0;
    CHECK(session_fingerprint(&t) != first);                 /* the width */
    t.width = 40.0;
    t.count = 2;
    CHECK(session_fingerprint(&t) != first);                 /* a point fewer */
}

static void check_road_width(void)
{
    CHECK(project_road_metres(40) == 10.0);                  /* the template's byte is a road of 10 m */
    CHECK(project_road_metres(PROJECT_WIDTH_MIN) == 2.0);
    CHECK(project_road_metres(PROJECT_WIDTH_MAX) == 30.0);
    CHECK(project_width_from_road(10.0) == 40.0);
    for (int byte = PROJECT_WIDTH_MIN; byte <= PROJECT_WIDTH_MAX; ++byte)
        CHECK(project_width_from_road(project_road_metres(byte)) == byte);   /* a quarter of a metre is one byte: nothing is lost */
    CHECK(project_width_from_road(10.1) == 40.4);            /* a value between two bytes converts exactly; the panel rounds it to a byte */
    CHECK(strstr(PROJECT_ROAD_TIP, "CD") == NULL);           /* the tip names no original game */
    CHECK(strstr(PROJECT_ROAD_TIP, "1/8") == NULL);          /* nor the byte's unit: that is in the docs */
    CHECK(strstr(PROJECT_ROAD_TIP, "Default 10 m") != NULL);
}

static void cleanup_project(const char *dir)
{
    char f[600];
    static const char *const names[] = { "project.ini", "trace.json", "trace.json.tmp", "project.ini.tmp" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) {
        snprintf(f, sizeof f, "%s/%s", dir, names[i]);
        remove(f);
    }
    rmdir(dir);
}

static void check_app_flow(void)
{
    char one[600], two[600], three[600];
    path_in(one, sizeof one, "one");
    path_in(two, sizeof two, "two");
    path_in(three, sizeof three, "three");
    mkdir(one, 0777);
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    CHECK(app_open(a, one, "./tnfs", "", NULL));
    char recent[600];
    path_in(recent, sizeof recent, "recent.txt");
    app_keep_recent(a, recent);
    CHECK_EQ_INT(a->recent.count, 1);                        /* the project that is open */

    /* nothing unsaved: the action goes at once */
    CHECK(!app_modified(a));
    app_ask(a, APP_ACT_QUIT, "");
    CHECK_EQ_INT(a->go, APP_ACT_QUIT);
    CHECK_EQ_INT(a->confirm, APP_ACT_NONE);
    a->go = APP_ACT_NONE;

    /* a point added: unsaved; the action waits, and a second one asked meanwhile is dropped */
    app_add_point(a);
    CHECK(app_modified(a));
    app_ask(a, APP_ACT_QUIT, "");
    CHECK_EQ_INT(a->go, APP_ACT_NONE);
    CHECK_EQ_INT(a->confirm, APP_ACT_QUIT);
    app_ask(a, APP_ACT_NEW, "");
    CHECK_EQ_INT(a->confirm, APP_ACT_QUIT);
    app_answer(a, APP_ANSWER_CANCEL);                        /* cancel: nothing happens, still unsaved */
    CHECK_EQ_INT(a->confirm, APP_ACT_NONE);
    CHECK_EQ_INT(a->go, APP_ACT_NONE);
    CHECK(app_modified(a));
    app_ask(a, APP_ACT_OPEN, "");
    app_answer(a, APP_ANSWER_DISCARD);                       /* discard: goes on, the file is as it was */
    CHECK_EQ_INT(a->go, APP_ACT_OPEN);
    CHECK_EQ_INT(a->confirm, APP_ACT_NONE);
    a->go = APP_ACT_NONE;
    app_ask(a, APP_ACT_RECENT, two);
    app_answer(a, APP_ANSWER_SAVE);                          /* save: written, goes on with the path it was asked with */
    CHECK_EQ_INT(a->go, APP_ACT_RECENT);
    CHECK(strcmp(a->go_path, two) == 0);
    CHECK(!app_modified(a));
    a->go = APP_ACT_NONE;

    /* Open wants a project; New wants none */
    CHECK(!app_open_project(a, two));                        /* no project.ini there yet */
    CHECK(strstr(a->status, "no project") != NULL);
    CHECK(strcmp(a->dir, one) == 0);                         /* the project is as it was */
    mkdir(two, 0777);
    CHECK(!app_open_project(a, two));                        /* an empty folder */
    CHECK(app_new_project(a, two));                          /* New makes the project there */
    CHECK(strcmp(a->dir, two) == 0);
    CHECK(a->fit_pending);
    CHECK_EQ_INT(a->recent.count, 2);                        /* both are in the list, the new one first */
    char real_one[PATH_MAX], real_two[PATH_MAX];   /* the list holds real paths (a symlinked TMPDIR is another name) */
    CHECK(realpath(one, real_one) != NULL && realpath(two, real_two) != NULL);
    CHECK(strcmp(a->recent.dir[0], real_two) == 0 && strcmp(a->recent.dir[1], real_one) == 0);
    CHECK(!app_new_project(a, one));                         /* a folder with a project is not new */
    CHECK(strstr(a->status, "already") != NULL);
    CHECK(strcmp(a->dir, two) == 0);
    CHECK(app_open_project(a, one));                         /* Open takes it, with the point that was saved */
    CHECK(strcmp(a->dir, one) == 0);
    CHECK_EQ_INT(a->trace.count, PROJECT_START_POINTS + 1);
    CHECK(!app_modified(a));

    Recent back;
    recent_load(&back, recent);                              /* what was kept is what is in memory */
    CHECK_EQ_INT(back.count, 2);
    CHECK(back.count == 2 && strcmp(back.dir[0], real_one) == 0);

    /* a save that fails keeps the question waiting, with the reason; once it can write, the same answer goes through */
    app_add_point(a);
    chmod(one, 0555);
    app_ask(a, APP_ACT_QUIT, "");
    app_answer(a, APP_ANSWER_SAVE);
    CHECK_EQ_INT(a->confirm, APP_ACT_QUIT);
    CHECK_EQ_INT(a->go, APP_ACT_NONE);
    CHECK(app_modified(a));
    chmod(one, 0777);
    app_answer(a, APP_ANSWER_SAVE);
    CHECK_EQ_INT(a->go, APP_ACT_QUIT);
    CHECK(!app_modified(a));
    a->go = APP_ACT_NONE;

    /* a folder whose project cannot be read: the open fails and the project is as it was */
    char broken[600], ini[700], trace[700];
    path_in(broken, sizeof broken, "broken");
    mkdir(broken, 0777);
    snprintf(ini, sizeof ini, "%s/project.ini", broken);
    snprintf(trace, sizeof trace, "%s/trace.json", broken);
    FILE *f = fopen(ini, "w");
    if (f != NULL) { fputs("id=broken\nname=broken\nauthor=\nstem=BRK\nart=GENERIC.FAM\nredistributable=1\n", f); fclose(f); }
    f = fopen(trace, "w");
    if (f != NULL) { fputs("{ this is not a trace", f); fclose(f); }
    const int points = a->trace.count;
    CHECK(!app_open_project(a, broken));
    CHECK(strcmp(a->dir, one) == 0);
    CHECK_EQ_INT(a->trace.count, points);
    CHECK(a->status[0] != '\0');
    cleanup_project(broken);

    app_free(a);
    free(a);
    remove(recent);
    mkdir(three, 0777);
    cleanup_project(one);
    cleanup_project(two);
    cleanup_project(three);
}

static void put(const char *name, const char *text)
{
    char p[700];
    path_in(p, sizeof p, name);
    FILE *f = fopen(p, "wb");
    if (f != NULL) fputs(text, f), fclose(f);
}

static void dir_in(const char *name)
{
    char p[700];
    path_in(p, sizeof p, name);
    mkdir(p, 0777);
}

/* cars and tracks in one recent list: each keeps its kind through a push, a save and a load; an old list (no marks) is tracks */
static void check_recent_kinds(void)
{
    Recent r;
    memset(&r, 0, sizeof r);
    recent_push(&r, "/home/p/a-track", RECENT_TRACK);
    recent_push(&r, "/home/p/a-car", RECENT_CAR);
    recent_push(&r, "/home/p/b-track", RECENT_TRACK);
    CHECK_EQ_INT(r.count, 3);
    CHECK(strcmp(r.dir[1], "/home/p/a-car") == 0 && r.kind[1] == RECENT_CAR && r.kind[0] == RECENT_TRACK && r.kind[2] == RECENT_TRACK);
    recent_push(&r, "/home/p/a-car", RECENT_CAR);            /* opened again: to the front, its kind with it, no copy */
    CHECK(r.count == 3 && strcmp(r.dir[0], "/home/p/a-car") == 0 && r.kind[0] == RECENT_CAR && r.kind[1] == RECENT_TRACK);
    for (int i = 0; i < RECENT_MAX + 2; ++i) {               /* the oldest fall off, whatever they are */
        char dir[64];
        snprintf(dir, sizeof dir, "/home/p/n%d", i);
        recent_push(&r, dir, i % 2 ? RECENT_CAR : RECENT_TRACK);
    }
    CHECK_EQ_INT(r.count, RECENT_MAX);
    CHECK(r.kind[0] == RECENT_CAR && r.kind[1] == RECENT_TRACK);   /* n9 then n8 */
    char file[600];
    path_in(file, sizeof file, "mixed.txt");
    CHECK(recent_save(&r, file));
    Recent back;
    recent_load(&back, file);
    CHECK_EQ_INT(back.count, r.count);
    for (int i = 0; i < r.count && i < back.count; ++i) CHECK(strcmp(back.dir[i], r.dir[i]) == 0 && back.kind[i] == r.kind[i]);
    put("old.txt", "/home/p/old-one\n/home/p/old-two\n");     /* the first Studio's file: one folder a line */
    path_in(file, sizeof file, "old.txt");
    recent_load(&back, file);
    CHECK(back.count == 2 && back.kind[0] == RECENT_TRACK && back.kind[1] == RECENT_TRACK && strcmp(back.dir[1], "/home/p/old-two") == 0);
    put("odd.txt", "car\t\ncar\t/home/p/c\n/home/p/c\n");     /* an empty car line is skipped; the same folder twice is one */
    path_in(file, sizeof file, "odd.txt");
    recent_load(&back, file);
    CHECK(back.count == 1 && back.kind[0] == RECENT_CAR);
    remove(file);
    path_in(file, sizeof file, "old.txt");
    remove(file);
    path_in(file, sizeof file, "mixed.txt");
    remove(file);

    /* a car is present while its mod.ini is; a track while its project.ini is */
    dir_in("kitp");
    dir_in("kitp/coche");
    put("kitp/coche/mod.ini", "id=a\n");
    dir_in("trk");
    put("trk/project.ini", "id=t\n");
    char kit[700], trk[700];
    path_in(kit, sizeof kit, "kitp");
    path_in(trk, sizeof trk, "trk");
    CHECK(recent_present(kit, RECENT_CAR) && !recent_present(kit, RECENT_TRACK));
    CHECK(recent_present(trk, RECENT_TRACK) && !recent_present(trk, RECENT_CAR));
    CHECK(!recent_present("/home/p/nowhere", RECENT_CAR) && !recent_present("/home/p/nowhere", RECENT_TRACK));
    path_in(kit, sizeof kit, "kitp/coche/mod.ini");
    remove(kit);
    path_in(kit, sizeof kit, "kitp/coche");
    rmdir(kit);
    path_in(kit, sizeof kit, "kitp");
    rmdir(kit);
    path_in(trk, sizeof trk, "trk/project.ini");
    remove(trk);
    path_in(trk, sizeof trk, "trk");
    rmdir(trk);
}

/* one project of either kind: nothing open, a car, a track, back; the track's actions do nothing for a car; a car has nothing unsaved */
static void check_kinds(void)
{
    char dirs[][40] = { "kit", "kit/coche", "kit/coche/SIMDATA", "kit/coche/SIMDATA/CARFAMS", "kit/modelo", "kit/modelo/c1", "tr" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; ++i) dir_in(dirs[i]);
    put("kit/coche/mod.ini", "id=carrera-84\nname=911\nversion=1\ntype=car\nstem=C84\nsound_from=911\n");
    put("kit/coche/SIMDATA/CARFAMS/C84.CFM", "cfm");
    put("kit/modelo/c1/0010_rsid.png", "x");
    char kit[700], coche[700], two[700], blank[700], recent[700], real_kit[PATH_MAX];
    path_in(kit, sizeof kit, "kit");
    path_in(coche, sizeof coche, "kit/coche");
    path_in(two, sizeof two, "tr");
    path_in(blank, sizeof blank, "blank");
    path_in(recent, sizeof recent, "kinds.txt");
    CHECK(realpath(kit, real_kit) != NULL);

    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    app_begin(a, "./tnfs", "", NULL);                        /* the start screen: nothing open, nothing to save, nothing that asks */
    CHECK_EQ_INT(a->kind, APP_KIND_NONE);
    CHECK(!app_modified(a));
    CHECK(!app_save(a) && !app_build(a) && !app_install(a));
    app_add_point(a);                                        /* nothing to add a point to */
    app_ask(a, APP_ACT_OPEN, "");
    CHECK_EQ_INT(a->go, APP_ACT_OPEN);                       /* so no question first */
    a->go = APP_ACT_NONE;
    app_keep_recent(a, recent);
    CHECK_EQ_INT(a->recent.count, 0);                        /* nothing open puts nothing in the list */

    CHECK(app_open_car(a, coche));                           /* given coche/, the kit is opened */
    CHECK_EQ_INT(a->kind, APP_KIND_CAR);
    CHECK(strcmp(a->dir, real_kit) == 0 && strcmp(a->car.info.stem, "C84") == 0 && a->car.texture_count == 1);
    CHECK_EQ_INT(a->recent.count, 1);
    CHECK(a->recent.kind[0] == RECENT_CAR && strcmp(a->recent.dir[0], real_kit) == 0);
    CHECK(!app_modified(a));
    CHECK(app_save(a) && strstr(a->status, "nothing to save") != NULL);   /* nothing changed: mod.ini is not touched */
    CHECK(!app_build(a) && !app_install(a));
    app_ask(a, APP_ACT_NEW, "");                             /* switching from a car with nothing unsaved asks nothing */
    CHECK_EQ_INT(a->go, APP_ACT_NEW);
    a->go = APP_ACT_NONE;

    /* a card with unsaved changes asks before it is left, and Save writes mod.ini only */
    CHECK_EQ_INT(car_edit_set(&a->edit, CE_YEAR, "1990"), 1);
    CHECK(app_modified(a));
    app_ask(a, APP_ACT_NEW, "");
    CHECK_EQ_INT(a->confirm, APP_ACT_NEW);
    CHECK_EQ_INT(a->go, APP_ACT_NONE);
    app_answer(a, APP_ANSWER_CANCEL);
    CHECK(app_modified(a) && a->kind == APP_KIND_CAR);
    CHECK_EQ_INT(car_edit_set(&a->edit, CE_YEAR, "12345678901234567"), 1);   /* a field with a problem: Save is refused and the question stays */
    app_ask(a, APP_ACT_NEW, "");
    app_answer(a, APP_ANSWER_SAVE);
    CHECK_EQ_INT(a->confirm, APP_ACT_NEW);
    CHECK(strstr(a->status, "Year") != NULL);
    CHECK_EQ_INT(car_edit_set(&a->edit, CE_YEAR, "1990"), 1);
    app_answer(a, APP_ANSWER_SAVE);
    CHECK_EQ_INT(a->go, APP_ACT_NEW);
    CHECK(!app_modified(a) && strstr(a->status, "saved mod.ini") != NULL);
    a->go = APP_ACT_NONE;
    {
        char ini[900];
        path_in(ini, sizeof ini, "kit/coche/mod.ini");
        FILE *card = fopen(ini, "rb");
        char text[400] = "";
        if (card != NULL) text[fread(text, 1, sizeof text - 1, card)] = '\0', fclose(card);
        CHECK(strcmp(text, "id=carrera-84\nname=911\nversion=1\ntype=car\nstem=C84\nsound_from=911\nyear=1990\n") == 0);   /* only that line was added */
    }

    CHECK(!app_open_car(a, two));                            /* not a car: the car stays, the status says what is missing */
    CHECK(a->kind == APP_KIND_CAR && strcmp(a->dir, real_kit) == 0 && strstr(a->status, "No car found") != NULL);
    CHECK(!app_open_car(a, blank));
    CHECK(strstr(a->status, "not a folder") != NULL);

    CHECK(app_new_project(a, two));                          /* a track after the car */
    CHECK_EQ_INT(a->kind, APP_KIND_TRACK);
    CHECK(a->trace.count == PROJECT_START_POINTS && a->car.info.id[0] == '\0');
    CHECK_EQ_INT(a->recent.count, 2);
    CHECK(a->recent.kind[0] == RECENT_TRACK && a->recent.kind[1] == RECENT_CAR);
    app_add_point(a);                                        /* a track with a change asks before a car replaces it */
    CHECK(app_modified(a));
    app_ask(a, APP_ACT_OPEN_CAR, "");
    CHECK_EQ_INT(a->confirm, APP_ACT_OPEN_CAR);
    app_answer(a, APP_ANSWER_CANCEL);
    CHECK(a->kind == APP_KIND_TRACK && app_modified(a));
    app_ask(a, APP_ACT_OPEN_CAR_AT, kit);
    app_answer(a, APP_ANSWER_DISCARD);
    CHECK_EQ_INT(a->go, APP_ACT_OPEN_CAR_AT);
    CHECK(strcmp(a->go_path, kit) == 0);
    a->go = APP_ACT_NONE;
    CHECK(app_open_recent(a, real_kit));                     /* a recent car opens as a car, a recent track as a track */
    CHECK_EQ_INT(a->kind, APP_KIND_CAR);
    char real_two[PATH_MAX];
    CHECK(realpath(two, real_two) != NULL && app_open_recent(a, real_two));
    CHECK_EQ_INT(a->kind, APP_KIND_TRACK);
    CHECK(a->recent.kind[0] == RECENT_TRACK && a->recent.kind[1] == RECENT_CAR);
    app_free(a);

    /* the command line's folder: a kit opens as a car, a project as a track, a folder with neither as a new track, a card that is not a car says so */
    CHECK(app_open_any(a, kit, "./tnfs", "", NULL) && a->kind == APP_KIND_CAR);
    app_free(a);
    CHECK(app_open_any(a, two, "./tnfs", "", NULL) && a->kind == APP_KIND_TRACK);
    app_free(a);
    dir_in("newtrack");
    path_in(blank, sizeof blank, "newtrack");
    CHECK(app_open_any(a, blank, "./tnfs", "", NULL) && a->kind == APP_KIND_TRACK);
    app_free(a);
    dir_in("notcar");
    put("notcar/mod.ini", "id=a\nname=A\nversion=1\ntype=track\nstem=TT\n");
    path_in(blank, sizeof blank, "notcar");
    CHECK(!app_open_any(a, blank, "./tnfs", "", NULL));      /* it is not made a track project: the card says what it is */
    CHECK(strstr(a->status, "type=track") != NULL && a->kind == APP_KIND_NONE);
    struct stat st;
    snprintf(two, sizeof two, "%s/project.ini", blank);
    CHECK(stat(two, &st) != 0);
    app_free(a);
    free(a);
    path_in(blank, sizeof blank, "newtrack");
    cleanup_project(blank);
    char cmd[1200];
    snprintf(cmd, sizeof cmd, "rm -rf '%s/kit' '%s/notcar'", root, root);
    if (system(cmd) != 0) fprintf(stderr, "could not clean up\n");
    path_in(two, sizeof two, "tr");
    cleanup_project(two);
    remove(recent);
}

/* Rehacer through the app: refused with no script and with unsaved card changes; run after a backup; the lists read again; nothing else opens while it runs */
static void check_rehacer(void)
{
    char dirs[][40] = { "rk", "rk/coche", "rk/coche/SIMDATA", "rk/coche/SIMDATA/CARFAMS", "rk/modelo", "rk/modelo/c1", "nk", "nk/coche", "nk/coche/SIMDATA", "nk/coche/SIMDATA/CARFAMS" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; ++i) dir_in(dirs[i]);
    static const char CARD[] = "id=carrera-84\nname=911\nversion=1\ntype=car\nstem=C84\nsound_from=911\n";
    put("rk/coche/mod.ini", CARD);
    put("rk/coche/SIMDATA/CARFAMS/C84.CFM", "the car file as it was");
    put("rk/modelo/c1/0010_rsid.png", "x");
    put("nk/coche/mod.ini", CARD);
    put("nk/coche/SIMDATA/CARFAMS/C84.CFM", "cfm");
    char rk[700], nk[700];
    path_in(rk, sizeof rk, "rk");
    path_in(nk, sizeof nk, "nk");
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    app_begin(a, "./tnfs", "", NULL);
    CHECK(!app_rehacer(a) && strstr(a->status, "open a car") != NULL);   /* nothing open */

    CHECK(app_open_car(a, nk));                              /* no script: said plainly, the button has nothing to run */
    CHECK(!app_rehacer_available(a));
    CHECK(!app_rehacer(a) && strstr(a->status, "no rehacer.command") != NULL);
    char p[900];
    struct stat st;
    snprintf(p, sizeof p, "%s/.studio-backup", nk);
    CHECK(stat(p, &st) != 0);                                /* and no backup was made for nothing */

    put("rk/rehacer.command", "#!/bin/sh\necho making the car\necho texture > modelo/c1/0020_new.png\necho changed > coche/SIMDATA/CARFAMS/C84.CFM\nexit ${REHACER_TEST_EXIT:-0}\n");
    CHECK(app_open_car(a, rk));
    CHECK(app_rehacer_available(a) && a->car.texture_count == 1);
    CHECK_EQ_INT(car_edit_set(&a->edit, CE_YEAR, "1990"), 1);   /* unsaved changes: refused, nothing run, no backup */
    CHECK(!app_rehacer(a) && strstr(a->status, "not saved") != NULL);
    snprintf(p, sizeof p, "%s/.studio-backup", rk);
    CHECK(stat(p, &st) != 0 && !app_rehacer_busy(a));
    CHECK(app_save(a));                                      /* saved: it may run */

    CHECK(app_rehacer(a));
    CHECK(app_rehacer_busy(a) && strstr(a->status, ".studio-backup/") != NULL);
    CHECK(!app_rehacer(a) && strstr(a->status, "already running") != NULL);
    app_ask(a, APP_ACT_QUIT, "");                            /* nothing else while it runs */
    CHECK_EQ_INT(a->go, APP_ACT_NONE);
    CHECK(strstr(a->status, "Rehacer is running") != NULL);
    for (int i = 0; i < 4000 && app_rehacer_poll(a); ++i) usleep(5000);
    CHECK(!app_rehacer_busy(a) && a->rehacer.finished && a->rehacer.exit_code == 0);
    CHECK(strstr(a->status, "finished: exit code 0") != NULL);
    CHECK(a->rehacer.log != NULL && strstr(a->rehacer.log, "making the car") != NULL);
    CHECK_EQ_INT(a->car.texture_count, 2);                   /* the lists were read again: the script added a texture */
    CHECK(!app_modified(a));
    {   /* the backup holds the car file as it was before the script changed it */
        char got[100] = "";
        char bp[1100];
        snprintf(bp, sizeof bp, "%s/coche/SIMDATA/CARFAMS/C84.CFM", a->rehacer.backup);
        FILE *b = fopen(bp, "rb");
        if (b != NULL) got[fread(got, 1, sizeof got - 1, b)] = '\0', fclose(b);
        CHECK(strcmp(got, "the car file as it was") == 0);
        snprintf(bp, sizeof bp, "%s/coche/mod.ini", a->rehacer.backup);
        b = fopen(bp, "rb");
        char card[200] = "";
        if (b != NULL) card[fread(card, 1, sizeof card - 1, b)] = '\0', fclose(b);
        CHECK(strstr(card, "year=1990") != NULL);            /* the saved card was in the backup too */
    }
    app_ask(a, APP_ACT_QUIT, "");                            /* it has ended: asking works again */
    CHECK_EQ_INT(a->go, APP_ACT_QUIT);
    a->go = APP_ACT_NONE;

    setenv("REHACER_TEST_EXIT", "3", 1);                     /* a failing script: its exit code is told */
    CHECK(app_rehacer(a));
    for (int i = 0; i < 4000 && app_rehacer_poll(a); ++i) usleep(5000);
    CHECK(a->rehacer.exit_code == 3 && strstr(a->status, "failed: exit code 3") != NULL);
    unsetenv("REHACER_TEST_EXIT");

    put("rk/rehacer.command", "#!/bin/sh\necho started\nsleep 30\n");   /* Stop */
    CHECK(app_rehacer(a));
    for (int i = 0; i < 400 && (a->rehacer.log == NULL || strstr(a->rehacer.log, "started") == NULL); ++i) app_rehacer_poll(a), usleep(5000);
    app_rehacer_stop(a);
    for (int i = 0; i < 4000 && app_rehacer_poll(a); ++i) usleep(5000);
    CHECK(!app_rehacer_busy(a) && a->rehacer.stopped && strstr(a->status, "stopped") != NULL);
    {   /* the backups: five are kept */
        char names[16][REHACER_STAMP_NAME];
        CHECK(rehacer_backups(rk, names, 16) >= 1);
        for (int i = 0; i < 7; ++i) {
            char stamp[32], out[1024], why[200];
            snprintf(stamp, sizeof stamp, "2026110%d-100000", i + 1);
            CHECK(rehacer_backup(rk, stamp, REHACER_KEEP, 100, out, sizeof out, why, sizeof why));
        }
        CHECK_EQ_INT(rehacer_backups(rk, names, 16), REHACER_KEEP);
    }
    app_free(a);
    free(a);
    char cmd[1200];
    snprintf(cmd, sizeof cmd, "rm -rf '%s/rk' '%s/nk'", root, root);
    if (system(cmd) != 0) fprintf(stderr, "could not clean up\n");
}

/* a synthetic mask: a PNG header and filler; Export original car copies files, it never decodes them */
static void put_mask(const char *name, uint32_t w, uint32_t h)
{
    uint8_t png[40] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 'I', 'H', 'D', 'R' };
    png[19] = (uint8_t)w, png[23] = (uint8_t)h, png[24] = 8;
    char p[700];
    path_in(p, sizeof p, name);
    FILE *f = fopen(p, "wb");
    if (f != NULL) fwrite(png, 1, sizeof png, f), fclose(f);
}

static int remove_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st, (void)flag, (void)ftw;
    return remove(path);
}

/* Export original car through the app: each kind opens as the project it is, a refusal leaves the open project as it was, a masks project has no card */
static void check_export(void)
{
    char dirs[][40] = { "ex", "ex/cd", "ex/cd/SIMDATA", "ex/cd/SIMDATA/CARFAMS", "ex/cd/SIMDATA/CARSPECS", "ex/data", "ex/data/car_masks", "ex/data/car_masks/Wagon",
                        "ex/data/car_masks/P911", "ex/out" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; ++i) dir_in(dirs[i]);
    put("ex/cd/SIMDATA/CARFAMS/WAGON.CFM", "cfm");
    put("ex/cd/SIMDATA/CARFAMS/P911.CFM", "cfm");
    put("ex/cd/SIMDATA/CARFAMS/PROBE.CFM", "cfm");
    put("ex/cd/SIMDATA/CARFAMS/P911.PDN", "pdn");
    uint8_t pbs[PBS_SIZE] = { 0 };
    char pbs_path[700];
    path_in(pbs_path, sizeof pbs_path, "ex/cd/SIMDATA/CARSPECS/P911.PBS");
    FILE *f = fopen(pbs_path, "wb");
    if (f != NULL) fwrite(pbs, 1, sizeof pbs, f), fclose(f);
    put_mask("ex/data/car_masks/Wagon/side_mask.png", 100, 50);
    put_mask("ex/data/car_masks/P911/side_mask.png", 64, 32);
    char out[700], cd[700], data[700];
    path_in(out, sizeof out, "ex/out");
    path_in(cd, sizeof cd, "ex/cd");
    path_in(data, sizeof data, "ex/data");

    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    app_begin(a, "./tnfs", "", NULL);
    CHECK_EQ_INT(a->export_pick, -1);
    ExportSource src;
    snprintf(src.cd, sizeof src.cd, "%s", cd);
    snprintf(src.data, sizeof src.data, "%s", data);
    app_set_export_source(a, &src);
    CHECK(!app_export_car(a, out));                                   /* nothing chosen */
    CHECK(strstr(a->open_error, "no car") != NULL);
    a->open_error[0] = '\0';
    for (int i = 0; car_export_car(i) != NULL; ++i) {
        if (strcmp(car_export_car(i)->folder, "probe") == 0) a->export_pick = i;
    }
    CHECK(!app_export_car(a, out));                                   /* probe has no masks */
    CHECK(strstr(a->open_error, "Could not export") != NULL && strstr(a->open_error, "no masks") != NULL);
    CHECK_EQ_INT(a->kind, APP_KIND_NONE);
    a->open_error[0] = '\0';
    for (int i = 0; car_export_car(i) != NULL; ++i) {
        if (strcmp(car_export_car(i)->folder, "Wagon") == 0) a->export_pick = i;
    }
    a->export_dialog = 1;
    CHECK(app_export_car(a, out));                                    /* a traffic car: a masks project, opened */
    CHECK_EQ_INT(a->kind, APP_KIND_CAR);
    CHECK(a->car.masks_only && strcmp(a->car.folder, "Wagon") == 0 && a->car.mask_count == 1 && !a->export_dialog);
    CHECK(strstr(a->status, "do not share") != NULL);
    CHECK(a->recent.count == 1 && a->recent.kind[0] == RECENT_CAR);
    CHECK(strcmp(a->export_src.data, data) == 0 && a->export_pick >= 0);   /* where the game's files are stays with the window */
    CHECK(!app_modified(a));
    CHECK(strcmp(car_race_track(a->race_track)->stem, "CL1") == 0);   /* a traffic car starts on a stage: the circuits have no traffic */
    snprintf(a->game, sizeof a->game, "%s", "/nonexistent/tnfs");
    CHECK(!app_race_busy(a) && !app_race(a, 1) && strstr(a->status, "game's program") != NULL);   /* no game where Studio looked */
    {   /* Race photo through the app, against a fake game */
        char fake[700];
        path_in(fake, sizeof fake, "ex/fake-tnfs");
        put("ex/fake-tnfs", "#!/bin/sh\nuser=; shot=\nwhile [ $# -gt 0 ]; do case \"$1\" in --user) user=$2; shift;; --screenshot) shot=$2; shift;; esac; shift; done\n"
                            "printf 'race car: slot 0 is TSupra\\nrace car: slot 3 is Wagon\\n' > \"$user/spirited-drive.log\"\n"
                            "printf 'P6\\n640 480\\n255\\n' > \"$shot\"; head -c 921600 /dev/zero >> \"$shot\"\n");
        chmod(fake, 0755);
        snprintf(a->game, sizeof a->game, "%s", fake);
        a->race_night = 1;
        const int started = app_race(a, 1);
        if (!started) fprintf(stderr, "app_race: %s\n", a->status);
        CHECK(started && app_race_busy(a));
        app_ask(a, APP_ACT_NEW, "");                                  /* nothing else is opened while the game runs */
        CHECK_EQ_INT(a->go, APP_ACT_NONE);
        CHECK(strstr(a->status, "race is running") != NULL || strstr(a->status, "photo") != NULL);
        for (int i = 0; i < 2000 && app_race_poll(a); ++i) usleep(5000);
        CHECK(!app_race_busy(a) && a->race.ok && a->race.photo != NULL && strstr(a->status, "slot 3") != NULL);
        a->race_track = car_race_track_find("TR5");
        CHECK(!app_race(a, 1) && strstr(a->status, "no traffic") != NULL);   /* a circuit has none */
        CHECK(!app_race_busy(a));
    }
    CHECK(!app_save(a) && strstr(a->status, "no card") != NULL);
    CHECK(!app_rehacer(a) && strstr(a->status, "no rehacer.command") != NULL);
    CHECK(!app_rehacer_available(a));
    char masks_dir[700];
    snprintf(masks_dir, sizeof masks_dir, "%s/wagon-masks", out);
    CHECK(!recent_is_project(masks_dir) && recent_present(masks_dir, RECENT_CAR) && car_project_present(masks_dir));
    CHECK(!app_open_project(a, masks_dir));                           /* it is not a track project, whatever the file is called */
    a->open_error[0] = '\0';
    CHECK(!app_export_car(a, out));                                   /* the same car into the same parent again: refused, the project stays open */
    CHECK(strstr(a->open_error, "already there") != NULL);
    CHECK(a->kind == APP_KIND_CAR && a->car.masks_only);
    a->open_error[0] = '\0';
    CHECK(app_open_any(a, masks_dir, "./tnfs", "", NULL) && a->kind == APP_KIND_CAR && a->car.masks_only);   /* the command line opens it as a car */
    for (int i = 0; car_export_car(i) != NULL; ++i) {
        if (strcmp(car_export_car(i)->folder, "P911") == 0) a->export_pick = i;
    }
    app_set_export_source(a, &src);
    CHECK(app_export_car(a, out));                                    /* a car you drive: a car project with a card to edit */
    CHECK(!a->car.masks_only && strcmp(a->car.info.id, "p911-original") == 0 && a->car.mask_count == 1);
    CHECK(strcmp(car_race_track(a->race_track)->stem, "TR5") == 0 && car_race_original_of(&a->car) == car_export_car(a->export_pick));   /* Rusty Springs, the car's own entry */
    CHECK(!app_modified(a) && car_edit_set_by_text(&a->edit, "year=1995") && app_modified(a));
    app_free(a);
    free(a);
    char scratch[700];
    path_in(scratch, sizeof scratch, "ex");
    nftw(scratch, remove_entry, 16, FTW_DEPTH | FTW_PHYS);   /* what this check made, and only it */
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/tnfs-editor-session-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(root) == NULL) {
        fprintf(stderr, "no scratch folder\n");
        return 1;
    }
    check_recent();
    check_fingerprint();
    check_road_width();
    check_app_flow();
    check_recent_kinds();
    check_kinds();
    check_rehacer();
    check_export();
    rmdir(root);
    return TEST_REPORT();
}
