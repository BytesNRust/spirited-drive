/* The fence step in trace.json (version 4): "fence_height" low / tall and "fence_art" planks / rail / parapet (the road's), "barrier_l_custom" /
 * "barrier_r_custom" true / false, read and written back; another value refused by name; a file with none stays the version it was. A file from
 * before (version 3 or less) has no Custom: its barrier keys were the barrier from the start, so a side with one reads as Custom from point 0 and the
 * road is built as before. The editor sets and clears the values as unsaved changes, and undo takes them back. */
#include "app.h"
#include "session.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[512];

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

static void write_trace(const char *path, int version, const char *keys)
{
    char text[1024];
    snprintf(text, sizeof text, "{\"version\": %d, \"width\": 40.0, \"points\": [[0, 0], [0, 100], [0, 200], [0, 300], [0, 400]],\n \"keys\": [%s]}\n",
             version, keys);
    write_file(path, text);
}

static void make_project(char *dir, size_t cap, const char *name)
{
    char ini[700];
    snprintf(dir, cap, "%s/%s", root, name);
    mkdir(dir, 0777);
    snprintf(ini, sizeof ini, "%s/project.ini", dir);
    write_file(ini, "id=fence\nname=fence\nauthor=\nstem=FENC\nart=GENERIC.FAM\nredistributable=1\n");
}

static void check_round_trip(void)
{
    char dir[600], trace[700], why[256];
    make_project(dir, sizeof dir, "fence");
    snprintf(trace, sizeof trace, "%s/trace.json", dir);
    Project proj;
    Trace t;
    write_trace(trace, 4, "{\"point\": 1, \"fence_height\": \"tall\", \"fence_art\": \"rail\"}, {\"point\": 2, \"barrier_r_m\": 6.0, \"barrier_r_custom\": true},"
                          " {\"point\": 3, \"fence_art\": \"parapet\", \"barrier_r_custom\": false, \"barrier_l_custom\": false}");
    CHECK(project_open(dir, &proj, &t, why, sizeof why));
    CHECK_EQ_INT(t.key_count, 3);
    if (t.key_count == 3) {
        CHECK(t.keys[0].has == (TRACE_KEY_FENCE_HEIGHT | TRACE_KEY_FENCE_ART) && t.keys[0].fence_tall == 1 && t.keys[0].fence_art == TRACE_FENCE_RAIL);
        CHECK(t.keys[1].has == (TRACE_KEY_SIDE(TRACE_SIDE_BARRIER, TRACK_GEN_RIGHT) | TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT)) &&
              t.keys[1].barrier_custom[TRACK_GEN_RIGHT] == 1);
        CHECK(t.keys[2].has == (TRACE_KEY_FENCE_ART | TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_LEFT) | TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT)) &&
              t.keys[2].fence_art == TRACE_FENCE_PARAPET && t.keys[2].barrier_custom[TRACK_GEN_RIGHT] == 0);
        CHECK(project_save_trace(dir, &t, why, sizeof why));
        const char *text = read_file(trace);
        CHECK(strstr(text, "\"version\": 4") != NULL && strstr(text, "\"fence_height\": \"tall\"") != NULL && strstr(text, "\"fence_art\": \"rail\"") != NULL &&
              strstr(text, "\"fence_art\": \"parapet\"") != NULL && strstr(text, "\"barrier_r_custom\": true") != NULL &&
              strstr(text, "\"barrier_l_custom\": false") != NULL);
        Trace back;
        CHECK(project_open(dir, &proj, &back, why, sizeof why));
        CHECK(back.key_count == 3);   /* every key as it was, byte for byte (the undo compares keys so) */
        CHECK(back.key_count == 3 && memcmp(back.keys, t.keys, sizeof *t.keys * 3) == 0);
        free(back.points), free(back.keys);
    }
    free(t.points), free(t.keys);
    write_trace(trace, 4, "{\"point\": 1, \"fence_height\": \"medium\"}");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why));
    CHECK(strstr(why, "point 1") != NULL && strstr(why, "medium") != NULL);
    write_trace(trace, 4, "{\"point\": 1, \"fence_art\": \"glass\"}");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why));
    CHECK(strstr(why, "glass") != NULL);
    write_trace(trace, 4, "{\"point\": 1, \"barrier_l_custom\": 1}");
    CHECK(!project_open(dir, &proj, &t, why, sizeof why));
    CHECK(strstr(why, "barrier_l_custom") != NULL);
    write_trace(trace, 3, "{\"point\": 1, \"fence_height\": \"tall\"}");   /* a version 3 file has no fence height: not read */
    CHECK(project_open(dir, &proj, &t, why, sizeof why));
    CHECK_EQ_INT(t.key_count, 0);
    free(t.points), free(t.keys);
    write_trace(trace, 6, "{\"point\": 1, \"fence_height\": \"tall\"}");   /* newer than this Studio */
    CHECK(!project_open(dir, &proj, &t, why, sizeof why));
    cleanup_project(dir);
}

/* a version 3 file's barrier keys: Custom from point 0 on that side (added to point 0's key), so the barrier is the key's from the start as it was;
 * saved again it is version 4 with that Custom; a side with no barrier key gets none, and a file with no barrier key stays version 3 */
static void check_old_file(void)
{
    char dir[600], trace[700], why[256];
    make_project(dir, sizeof dir, "old");
    snprintf(trace, sizeof trace, "%s/trace.json", dir);
    Project proj;
    Trace t;
    write_trace(trace, 3, "{\"point\": 2, \"barrier_r_m\": 8.0, \"wall_l\": \"hard\"}");
    CHECK(project_open(dir, &proj, &t, why, sizeof why));
    CHECK_EQ_INT(t.key_count, 2);
    if (t.key_count == 2) {
        CHECK(t.keys[0].point == 0 && t.keys[0].has == TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT) && t.keys[0].barrier_custom[TRACK_GEN_RIGHT] == 1);
        TrackGenNode *nodes = NULL;
        int count = 0;
        ExchangeError err = { { 0 } };
        CHECK(track_trace_nodes_keyed(t.points, t.count, t.width, t.keys, t.key_count, &nodes, &count, &err));
        int same = nodes != NULL;   /* the right barrier blends from 3 m at the start to 8 m at point 2, as a version 3 Studio built it */
        for (int k = 0; same && k < count; ++k) same = (nodes[k].side[TRACK_GEN_RIGHT].has & TRACK_GEN_SIDE_BARRIER) != 0;
        CHECK(same && nodes != NULL && lround(nodes[count - 1].side[TRACK_GEN_RIGHT].barrier) == 64 && lround(nodes[0].side[TRACK_GEN_RIGHT].barrier) == 24);
        free(nodes);
        CHECK(project_save_trace(dir, &t, why, sizeof why));
        CHECK(strstr(read_file(trace), "\"version\": 4") != NULL && strstr(read_file(trace), "\"barrier_r_custom\": true") != NULL);
    }
    free(t.points), free(t.keys);
    write_trace(trace, 3, "{\"point\": 2, \"shoulder_l_m\": 5.0}");
    CHECK(project_open(dir, &proj, &t, why, sizeof why));
    CHECK_EQ_INT(t.key_count, 1);
    CHECK(project_save_trace(dir, &t, why, sizeof why));
    CHECK(strstr(read_file(trace), "\"version\": 3") != NULL);
    free(t.points), free(t.keys);
    cleanup_project(dir);
}

static void check_app(void)
{
    char dir[600];
    snprintf(dir, sizeof dir, "%s/fencekeyed", root);
    mkdir(dir, 0777);
    App *a = calloc(1, sizeof *a);
    CHECK(a != NULL);
    if (a == NULL) return;
    CHECK(app_open(a, dir, "./tnfs", "", NULL));
    CHECK(!app_modified(a));
    app_set_point_fence(a, 1, TRACE_KEY_FENCE_HEIGHT, 1);
    CHECK(app_modified(a));
    CHECK(app_save(a));
    app_set_point_fence(a, 1, TRACE_KEY_FENCE_HEIGHT, 1);
    CHECK(!app_modified(a));                                 /* the same: nothing changed */
    app_set_point_fence(a, 1, TRACE_KEY_FENCE_ART, 9);       /* no such picture: nothing */
    app_set_point_fence(a, 99, TRACE_KEY_FENCE_ART, 1);      /* no such point: nothing */
    CHECK(!app_modified(a));
    TraceFencePoint v;
    CHECK(app_point_fence(a, 1, &v) && v.tall == 1 && v.art == TRACE_FENCE_PLANKS && v.custom[0] == 0);
    /* Custom: an amber line for Problems names the points where a side's barrier is set apart from what is drawn */
    char note[256] = "";
    CHECK(!app_barrier_custom_note(a, note, sizeof note));
    app_set_point_fence(a, 2, TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT), 1);
    CHECK(app_barrier_custom_note(a, note, sizeof note));
    CHECK(strstr(note, "point 2") != NULL && strstr(note, "right") != NULL);
    app_track_commit(a);
    app_clear_point_fence(a, 2, TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT));
    app_track_commit(a);
    CHECK(!app_barrier_custom_note(a, note, sizeof note));
    CHECK(app_undo(a));                                      /* undo brings Custom back */
    CHECK(app_barrier_custom_note(a, note, sizeof note));
    app_free(a);
    free(a);
    cleanup_project(dir);
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/tnfs-editor-fence-XXXXXX", tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(root) == NULL) {
        fprintf(stderr, "no scratch folder\n");
        return 1;
    }
    check_round_trip();
    check_old_file();
    check_app();
    rmdir(root);
    return TEST_REPORT();
}
