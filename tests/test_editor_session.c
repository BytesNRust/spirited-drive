/* The editor's session (editor/session.c, and the parts of editor/app.c that open, close and confirm): the recent projects keep their order, never
 * repeat, never pass RECENT_MAX and survive a save; a trace that changed has another fingerprint; an action asked for with unsaved changes waits
 * for Save / Discard / Cancel; Open wants a project in the folder and New wants none; a failed open leaves the project as it was. */
#include "app.h"
#include "session.h"
#include "test.h"

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
        recent_push(&r, dir);
    }
    CHECK_EQ_INT(r.count, RECENT_MAX);                       /* the oldest three fell off */
    path_in(dir, sizeof dir, "p10");
    CHECK(strcmp(r.dir[0], dir) == 0);                       /* the newest first */
    path_in(dir, sizeof dir, "p5");
    recent_push(&r, dir);                                    /* opened again: it moves to the front, no copy */
    CHECK(strcmp(r.dir[0], dir) == 0);
    CHECK_EQ_INT(r.count, RECENT_MAX);
    int copies = 0;
    for (int i = 0; i < r.count; ++i) copies += strcmp(r.dir[i], dir) == 0;
    CHECK_EQ_INT(copies, 1);
    recent_push(&r, "");                                     /* nothing */
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
    rmdir(root);
    return TEST_REPORT();
}
