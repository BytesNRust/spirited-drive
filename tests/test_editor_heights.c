/* Heights on the trace's control points (dep95 step 1): trace.json's points read `[x, z]` (version 4 and before: height 0) or `[x, z, h]` (version 5), a project with no
 * height still writes the version and pairs it always did, a height is an unsaved change that undo and redo carry, a point put in takes the curve's height there, a point
 * taken out takes only its own, and the values are checked (finite, within TRACE_HEIGHT_MAX_M). */
#include "app.h"
#include "point_ops.h"
#include "session.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[512];

static void path_in(char *out, size_t cap, const char *name) { snprintf(out, cap, "%s/%s", root, name); }

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
}

static const char *read_file(const char *path)
{
    static char buf[8192];
    FILE *f = fopen(path, "r");
    buf[0] = '\0';
    if (f != NULL) {
        const size_t n = fread(buf, 1, sizeof buf - 1, f);
        buf[n] = '\0';
        fclose(f);
    }
    return buf;
}

static void cleanup_project(const char *dir)
{
    char f[600];
    static const char *const names[] = { "project.ini", "trace.json", "trace.json.tmp", "project.ini.tmp", "GENERIC.FAM" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) {
        snprintf(f, sizeof f, "%s/%s", dir, names[i]);
        remove(f);
    }
    rmdir(dir);
}

static void write_project(const char *dir, int version, const char *points)
{
    char ini[700], trace[700], text[1024];
    snprintf(ini, sizeof ini, "%s/project.ini", dir);
    snprintf(trace, sizeof trace, "%s/trace.json", dir);
    write_file(ini, "id=heights\nname=heights\nauthor=\nstem=HGHT\nart=GENERIC.FAM\nredistributable=1\n");
    snprintf(text, sizeof text, "{\"version\": %d, \"width\": 40.0, \"points\": %s}\n", version, points);
    write_file(trace, text);
}

static void free_trace(Trace *t)
{
    free(t->points);
    free(t->keys);
}

static void test_core_height_at(void)
{
    // a flat run is flat (no overshoot), the ends are the points, two points are a line
    TracePoint flat[4] = { { .x = 0, .z = 0, .h = 5 }, { .x = 0, .z = 100, .h = 5 }, { .x = 0, .z = 200, .h = 5 }, { .x = 0, .z = 300, .h = 5 } };
    for (int seg = 0; seg < 3; ++seg)
        for (double t = 0; t <= 1.0; t += 0.25) CHECK(track_trace_height_at(flat, 4, seg, t) == 5.0);
    TracePoint two[2] = { { .x = 0, .z = 0, .h = 0 }, { .x = 0, .z = 100, .h = 10 } };
    CHECK(fabs(track_trace_height_at(two, 2, 0, 0.5) - 5.0) < 1e-9 && track_trace_height_at(two, 2, 0, 0) == 0.0 && track_trace_height_at(two, 2, 0, 1) == 10.0);
    // heights 0, 0, 10, 10 on a straight road: a monotone run never leaves 0..10 (a cubic spline would dip below 0 and rise above 10)
    TracePoint step[4] = { { .x = 0, .z = 0, .h = 0 }, { .x = 0, .z = 100, .h = 0 }, { .x = 0, .z = 200, .h = 10 }, { .x = 0, .z = 300, .h = 10 } };
    double previous = -1;
    for (int seg = 0; seg < 3; ++seg)
        for (double t = 0; t <= 1.0; t += 0.05) {
            const double h = track_trace_height_at(step, 4, seg, t);
            CHECK(h >= -1e-9 && h <= 10 + 1e-9 && h >= previous - 1e-9);
            previous = h;
        }
    CHECK(fabs(track_trace_height_at(step, 4, 1, 0.5) - 5.0) < 1e-9);   // the middle of a symmetric step
    // a hill top: the point is the maximum, the height never goes past it
    TracePoint hill[3] = { { .x = 0, .z = 0, .h = 0 }, { .x = 0, .z = 100, .h = 8 }, { .x = 0, .z = 200, .h = 0 } };
    for (int seg = 0; seg < 2; ++seg)
        for (double t = 0; t <= 1.0; t += 0.1) CHECK(track_trace_height_at(hill, 3, seg, t) <= 8.0 + 1e-9);
    // out of range is a point of the road, not a crash
    CHECK(track_trace_height_at(hill, 3, -1, 0.5) == 0.0 && track_trace_height_at(hill, 3, 5, 0.5) == 0.0 && track_trace_height_at(hill, 1, 0, 0.5) == 0.0);
}

static void test_reading(void)
{
    char dir[600], why[256];
    path_in(dir, sizeof dir, "read");
    mkdir(dir, 0777);
    Project proj;
    Trace t;
    // an old file: pairs, no height
    write_project(dir, 4, "[[0, 0], [0, 100], [0, 200]]");
    CHECK(project_open(dir, &proj, &t, why, sizeof why) && t.count == 3 && t.points[1].h == 0.0 && t.points[1].z == 100.0);
    free_trace(&t);
    // version 5: triples, a pair is height 0
    write_project(dir, 5, "[[0, 0, 0], [0, 100, 12.5], [0, 200]]");
    CHECK(project_open(dir, &proj, &t, why, sizeof why) && t.count == 3 && t.points[0].h == 0.0 && t.points[1].h == 12.5 && t.points[2].h == 0.0);
    free_trace(&t);
    // refusals, by point, in the other refusals' words
    write_project(dir, 4, "[[0, 0], [0, 100, 3], [0, 200]]");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why) && strstr(why, "trace.json") != NULL && strstr(why, "version 5") != NULL);   // a height in a file that is not version 5's
    write_project(dir, 5, "[[0, 0, 0], [0, 100, 501], [0, 200, 0]]");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why) && strstr(why, "point 1") != NULL && strstr(why, "500") != NULL);
    write_project(dir, 5, "[[0, 0, 0], [0, 100, -501], [0, 200, 0]]");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why) && strstr(why, "point 1") != NULL);
    write_project(dir, 5, "[[0, 0, 0], [0, 100, nan], [0, 200, 0]]");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why) && strstr(why, "point 1") != NULL);
    write_project(dir, 5, "[[0, 0, 0], [0, 100, 1e999], [0, 200, 0]]");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why) && strstr(why, "point 1") != NULL);
    write_project(dir, 5, "[[0, 0, 0], [0, 100, 1, 2], [0, 200, 0]]");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why));   // four numbers
    write_project(dir, 6, "[[0, 0], [0, 100], [0, 200]]");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why) && strstr(why, "newer") != NULL);
    cleanup_project(dir);
}

static void test_point_ops_carry_heights(void)
{
    // a straight road north with heights 0, 1, 2, 3, 4 on points 0..4
    TracePoint road[5] = { { .x = 0 } };
    for (int i = 0; i < 5; ++i) road[i] = (TracePoint){ .x = 0, .z = 10.0 * i, .h = i };
    // the whole road reversed: the points turn round and each keeps its height
    TracePoint *q = pts_reverse(road, 5, 0, 4);
    CHECK(q != NULL);
    for (int i = 0; q != NULL && i < 5; ++i) CHECK(q[i].h == 4 - i);
    free(q);
    // a mirror on the plan moves no height
    q = pts_mirror_lr(road, 5, 1, 3);
    CHECK(q != NULL);
    for (int i = 0; q != NULL && i < 5; ++i) CHECK(q[i].h == i);
    free(q);
    // a block pasted after point 2 (height 2): its own heights are counted from the anchor's, the tail keeps its own
    const TracePoint block[3] = { { .x = 0, .z = 0, .h = 7 }, { .x = 0, .z = 10, .h = 9 }, { .x = 0, .z = 20, .h = 6 } };
    q = pts_paste(road, 5, 2, block, 3);
    CHECK(q != NULL);
    if (q != NULL) {
        CHECK(q[2].h == 2 && q[3].h == 4 && q[4].h == 1);   // 2 + (9 - 7), 2 + (6 - 7)
        CHECK(q[5].h == 3 && q[6].h == 4);                    // the tail's own
    }
    free(q);
}

static void test_app_flow(void)
{
    char dir[600], trace[700], why[256];
    path_in(dir, sizeof dir, "flow");
    mkdir(dir, 0777);
    snprintf(trace, sizeof trace, "%s/trace.json", dir);
    write_project(dir, 4, "[[0, 0], [0, 100], [0, 200], [0, 300], [0, 400]]");
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    CHECK(app_open(a, dir, "./tnfs", "", NULL));
    CHECK(!app_modified(a));
    // a height is an unsaved change; the value is kept to a tenth of a metre and inside the range
    app_set_point_height(a, 2, 12.34);
    CHECK(app_modified(a) && fabs(a->trace.points[2].h - 12.3) < 1e-9);
    app_track_commit(a);
    app_set_point_height(a, 2, 12.3);   // the same: nothing changed
    app_set_point_height(a, 3, 9999);
    CHECK(a->trace.points[3].h == TRACE_HEIGHT_MAX_M);
    app_set_point_height(a, 3, -9999);
    CHECK(a->trace.points[3].h == -TRACE_HEIGHT_MAX_M);
    app_set_point_height(a, 99, 5);     // no such point: nothing
    app_set_point_height(a, 3, 20);
    app_track_commit(a);
    // Save writes version 5 with triples; a reload gives the same heights
    CHECK(app_save(a) && !app_modified(a));
    const char *text = read_file(trace);
    CHECK(strstr(text, "\"version\": 5") != NULL && strstr(text, "[0.000, 200.000, 12.300]") != NULL && strstr(text, "[0.000, 0.000, 0.000]") != NULL);   // every point has its height once one does
    Project proj;
    Trace t;
    CHECK(project_open(dir, &proj, &t, why, sizeof why) && t.count == 5 && fabs(t.points[2].h - 12.3) < 1e-9 && t.points[3].h == 20.0 && t.points[0].h == 0.0);
    free_trace(&t);
    // undo and redo carry the height
    CHECK(app_modified(a) == 0);
    app_set_point_height(a, 1, 4);
    app_track_commit(a);
    CHECK(app_undo(a) && a->trace.points[1].h == 0.0 && !app_modified(a));
    CHECK(app_redo(a) && a->trace.points[1].h == 4.0 && app_modified(a));
    // a point put in after point 1 (height 4) before point 2 (12.3) takes the curve's height there, between the two
    a->selected = 1;
    app_insert_point(a);
    CHECK(a->trace.count == 6 && a->trace.points[2].h > 4.0 && a->trace.points[2].h < 12.3 && a->trace.points[3].h == 12.3);
    // one taken out takes only its own height
    a->selected = 2;
    app_remove_point(a);
    CHECK(a->trace.count == 5 && a->trace.points[1].h == 4.0 && fabs(a->trace.points[2].h - 12.3) < 1e-9 && a->trace.points[3].h == 20.0);
    // every height back to 0 and Save: the old version and pairs again
    for (int i = 0; i < a->trace.count; ++i) app_set_point_height(a, i, 0);
    CHECK(app_save(a));
    text = read_file(trace);
    CHECK(strstr(text, "\"version\": 2") != NULL && strstr(text, "\"version\": 5") == NULL);   // a project with no height is the file it always was
    CHECK(strstr(text, "[0.000, 100.000],\n") != NULL && strstr(text, "[0.000, 100.000, ") == NULL);
    app_free(a);
    free(a);
    cleanup_project(dir);
}

int main(void)
{
    snprintf(root, sizeof root, "/tmp/tnfs_heights_%d", (int)getpid());
    mkdir(root, 0777);
    test_core_height_at();
    test_reading();
    test_point_ops_carry_heights();
    test_app_flow();
    rmdir(root);
    return TEST_REPORT();
}
