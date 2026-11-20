/* Lanes on the editor's road keys (dep59 editor phase 2, step 4): trace.json's "lanes" ("1+1", "1+2", "2+1", version 3) reads and writes back, another
 * value is refused by name, a version 2 file has none; the lanes are part of the project's fingerprint; the editor sets and clears a point's lanes as an
 * unsaved change, the same again being none. */
#include "app.h"
#include "session.h"
#include "test.h"

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

static char *read_file(const char *path)
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

static void free_trace(Trace *t)
{
    free(t->points);
    free(t->keys);
}

static const char *const POINTS = "\"points\": [[0, 0], [0, 100], [0, 200], [0, 300], [0, 400]]";

static void write_trace(const char *path, int version, const char *keys)
{
    char text[1024];
    snprintf(text, sizeof text, "{\"version\": %d, \"width\": 40.0, %s,\n \"keys\": [%s]}\n", version, POINTS, keys);
    write_file(path, text);
}

static void check_lanes_file(void)
{
    char dir[600], ini[700], trace[700], why[256];
    path_in(dir, sizeof dir, "lanes");
    mkdir(dir, 0777);
    snprintf(ini, sizeof ini, "%s/project.ini", dir);
    snprintf(trace, sizeof trace, "%s/trace.json", dir);
    write_file(ini, "id=lanes\nname=lanes\nauthor=\nstem=LANE\nart=GENERIC.FAM\nredistributable=1\n");
    Project proj;
    Trace t;
    write_trace(trace, 3, "{\"point\": 1, \"lanes\": \"1+2\"}, {\"point\": 2, \"width_m\": 12.0, \"lanes\": \"2+1\"}, {\"point\": 3, \"lanes\": \"1+1\"}");
    CHECK(project_open(dir, &proj, &t, why, sizeof why));
    CHECK_EQ_INT(t.key_count, 3);
    if (t.key_count == 3) {
        CHECK(t.keys[0].has == TRACE_KEY_LANES && t.keys[0].lanes == TRACK_GEN_LANES_1_2);
        CHECK(t.keys[1].has == (TRACE_KEY_WIDTH | TRACE_KEY_LANES) && t.keys[1].lanes == TRACK_GEN_LANES_2_1 && t.keys[1].width_m == 12.0);
        CHECK(t.keys[2].has == TRACE_KEY_LANES && t.keys[2].lanes == TRACK_GEN_LANES_1_1);
        const uint64_t print = session_fingerprint(&t);
        t.keys[0].lanes = TRACK_GEN_LANES_2_1;
        CHECK(session_fingerprint(&t) != print);   /* the lanes are part of the project */
        t.keys[0].lanes = TRACK_GEN_LANES_1_2;
        CHECK(session_fingerprint(&t) == print);
        CHECK(project_save_trace(dir, &t, why, sizeof why));
        const char *text = read_file(trace);
        CHECK(strstr(text, "\"version\": 3") != NULL && strstr(text, "\"lanes\": \"1+2\"") != NULL && strstr(text, "\"lanes\": \"2+1\"") != NULL &&
              strstr(text, "\"lanes\": \"1+1\"") != NULL);
        Trace back;
        CHECK(project_open(dir, &proj, &back, why, sizeof why));   /* round trip */
        CHECK(back.key_count == 3 && session_fingerprint(&back) == print);
        free_trace(&back);
        /* the lanes gone (the width key stays): version 2 again */
        track_trace_key_clear_lanes(t.keys, &t.key_count, 3);
        track_trace_key_clear_lanes(t.keys, &t.key_count, 2);
        track_trace_key_clear_lanes(t.keys, &t.key_count, 1);
        CHECK_EQ_INT(t.key_count, 1);
        CHECK(project_save_trace(dir, &t, why, sizeof why));
        text = read_file(trace);
        CHECK(strstr(text, "\"version\": 2") != NULL && strstr(text, "lanes") == NULL && strstr(text, "\"width_m\": 12.00") != NULL);
    }
    free_trace(&t);
    write_trace(trace, 3, "{\"point\": 1, \"lanes\": \"2+2\"}");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why));   /* not lanes this version knows: refused, not read as 1+1 */
    CHECK(strstr(why, "trace.json") != NULL && strstr(why, "point 1") != NULL && strstr(why, "2+2") != NULL);
    write_trace(trace, 3, "{\"point\": 1, \"lanes\": 12}");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why));   /* no string */
    CHECK(strstr(why, "lanes") != NULL);
    write_trace(trace, 3, "{\"point\": 7, \"lanes\": \"1+2\"}");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why));
    CHECK(strstr(why, "point 7") != NULL);
    write_trace(trace, 2, "{\"point\": 1, \"lanes\": \"1+2\"}");
    CHECK(project_open(dir, &proj, &t, why, sizeof why));    /* lanes are version 3's: a version 2 file has none */
    CHECK_EQ_INT(t.key_count, 0);
    free_trace(&t);
    cleanup_project(dir);
}

static void check_lanes_app(void)
{
    char dir[600];
    path_in(dir, sizeof dir, "laneskeyed");
    mkdir(dir, 0777);
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    CHECK(app_open(a, dir, "./tnfs", "", NULL));
    CHECK(!app_modified(a));
    app_set_point_lanes(a, 1, TRACK_GEN_LANES_1_2);
    CHECK(app_modified(a));
    CHECK(a->trace.key_count == 1 && a->trace.keys[0].point == 1 && a->trace.keys[0].lanes == TRACK_GEN_LANES_1_2);
    CHECK(app_save(a));
    CHECK(!app_modified(a));
    app_set_point_lanes(a, 1, TRACK_GEN_LANES_1_2);
    CHECK(!app_modified(a));                                 /* the same lanes: nothing changed */
    app_set_point_lanes(a, 1, 0x22);                         /* not one of the three: nothing */
    app_set_point_lanes(a, 99, TRACK_GEN_LANES_2_1);         /* no such point: nothing */
    CHECK(!app_modified(a) && a->trace.key_count == 1 && a->trace.keys[0].lanes == TRACK_GEN_LANES_1_2);
    app_set_point_lanes(a, 1, TRACK_GEN_LANES_1_1);
    CHECK(app_modified(a) && a->trace.keys[0].lanes == TRACK_GEN_LANES_1_1);
    CHECK(app_save(a));
    app_clear_point_lanes(a, 1);
    CHECK(app_modified(a));
    CHECK_EQ_INT(a->trace.key_count, 0);                     /* nothing left on point 1: its key is gone */
    CHECK(app_save(a));
    app_clear_point_lanes(a, 1);
    CHECK(!app_modified(a));                                 /* nothing to clear: no change */
    /* a width key inside a 1+2 or 2+1 stretch: ignored, and Problems says so by point (amber); the point's lanes as the keys make them */
    char note[256] = "";
    CHECK(!app_lane_width_note(a, note, sizeof note));
    CHECK_EQ_INT(app_point_lanes(a, 1), TRACK_GEN_LANES_1_1);   /* no key: 1+1 */
    app_set_point_lanes(a, 1, TRACK_GEN_LANES_2_1);
    app_set_point_width(a, 2, 14.0);
    app_set_point_width(a, 0, 12.0);                         /* before the stretch: used */
    CHECK_EQ_INT(app_point_lanes(a, 0), TRACK_GEN_LANES_1_1);
    CHECK_EQ_INT(app_point_lanes(a, 2), TRACK_GEN_LANES_2_1);
    CHECK(app_lane_width_note(a, note, sizeof note));
    CHECK(strstr(note, "point 2 (2+1)") != NULL && strstr(note, "ignored") != NULL && strstr(note, " 0 ") == NULL);
    app_set_point_lanes(a, 2, TRACK_GEN_LANES_1_1);          /* the stretch ends on point 2: its width is used again */
    CHECK(!app_lane_width_note(a, note, sizeof note));
    /* undo and redo take a lanes change back and forth as any other key's */
    app_track_commit(a);
    app_set_point_lanes(a, 2, TRACK_GEN_LANES_1_2);
    app_track_commit(a);
    CHECK_EQ_INT(app_point_lanes(a, 2), TRACK_GEN_LANES_1_2);
    CHECK(app_undo(a));
    CHECK_EQ_INT(app_point_lanes(a, 2), TRACK_GEN_LANES_1_1);
    CHECK(app_redo(a));
    CHECK_EQ_INT(app_point_lanes(a, 2), TRACK_GEN_LANES_1_2);
    app_free(a);
    free(a);
    cleanup_project(dir);
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/tnfs-editor-lanes-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(root) == NULL) {
        fprintf(stderr, "no scratch folder\n");
        return 1;
    }
    check_lanes_file();
    check_lanes_app();
    rmdir(root);
    return TEST_REPORT();
}
