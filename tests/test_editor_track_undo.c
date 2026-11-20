/* A track project's undo and redo (editor/track_undo.h): whole-project steps, one per commit, the oldest falling off, and "modified" as
 * "not what the files have". Every byte of the points and the road keys is part of a step, so a field added to them later is undone
 * with no change here; trace.json written before an edit and after its undo is the same file. */
#include "track_undo.h"
#include "formats/track_gen.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void make(Project *p, Trace *t)
{
    memset(p, 0, sizeof *p);
    snprintf(p->id, sizeof p->id, "loop");
    snprintf(p->edge, sizeof p->edge, "grass");
    memset(t, 0, sizeof *t);
    t->count = 6;
    t->width = 40;
    t->points = calloc((size_t)t->count, sizeof *t->points);
    for (int i = 0; i < t->count; i++) t->points[i].z = 40.0 * i;
    CHECK(track_trace_key_set_width(&t->keys, &t->key_count, &t->key_cap, 1, 8.0));
    CHECK(track_trace_key_set_surface(&t->keys, &t->key_count, &t->key_cap, 2, TRACK_GEN_LEFT, TRACE_SURFACE_TARMAC));
    CHECK(track_trace_key_set_side(&t->keys, &t->key_count, &t->key_cap, 3, TRACE_SIDE_BARRIER, TRACK_GEN_RIGHT, 8.0));
}

static void drop(Trace *t)
{
    free(t->points), free(t->keys);
    memset(t, 0, sizeof *t);
}

static int same(const Trace *a, const Trace *b)
{
    return a->count == b->count && a->width == b->width && a->key_count == b->key_count && memcmp(a->points, b->points, sizeof *a->points * (size_t)a->count) == 0
           && (a->key_count == 0 || memcmp(a->keys, b->keys, sizeof *a->keys * (size_t)a->key_count) == 0);
}

static void copy(Trace *to, const Trace *from)
{
    *to = *from;
    to->points = malloc(sizeof *to->points * (size_t)from->count);
    memcpy(to->points, from->points, sizeof *to->points * (size_t)from->count);
    to->keys = malloc(sizeof *to->keys * (size_t)from->key_cap);
    memcpy(to->keys, from->keys, sizeof *to->keys * (size_t)from->key_count);
}

/* an edit, an undo, a redo, the saved state, and an undo back to it is not modified */
static void test_steps(void)
{
    Project p;
    Trace t, first;
    make(&p, &t);
    copy(&first, &t);
    TrackUndo u;
    CHECK(track_undo_open(&u, &p, &t));
    CHECK(!track_undo_modified(&u, &p, &t));
    CHECK(!track_undo_can_undo(&u, &p, &t) && !track_undo_can_redo(&u));
    CHECK(!track_undo_commit(&u, &p, &t));   /* nothing changed: no step */
    t.points[2].x = 12.5;                     /* a drag: many frames, one commit when it ends */
    CHECK(track_undo_modified(&u, &p, &t));
    CHECK(track_undo_can_undo(&u, &p, &t));   /* a change not yet taken can be undone */
    CHECK(track_undo_commit(&u, &p, &t));
    CHECK(!track_undo_commit(&u, &p, &t));
    snprintf(p.edge, sizeof p.edge, "sand");  /* project.ini's verge: part of the project */
    CHECK(track_undo_commit(&u, &p, &t));
    CHECK(track_undo_undo(&u, &p, &t));
    CHECK(strcmp(p.edge, "grass") == 0 && t.points[2].x == 12.5);
    CHECK(track_undo_undo(&u, &p, &t));
    CHECK(same(&t, &first) && !track_undo_modified(&u, &p, &t));   /* back to the files' state: nothing to save */
    CHECK(!track_undo_undo(&u, &p, &t));
    CHECK(track_undo_redo(&u, &p, &t) && t.points[2].x == 12.5);
    CHECK(track_undo_modified(&u, &p, &t));
    CHECK(track_undo_saved(&u, &p, &t));
    CHECK(!track_undo_modified(&u, &p, &t));   /* Save clears the mark */
    CHECK(track_undo_undo(&u, &p, &t));
    CHECK(track_undo_modified(&u, &p, &t));    /* an undo past the save brings it back */
    CHECK(track_undo_redo(&u, &p, &t) && track_undo_redo(&u, &p, &t) && strcmp(p.edge, "sand") == 0);
    CHECK(!track_undo_redo(&u, &p, &t));
    t.count--;                                 /* a new edit empties redo */
    CHECK(track_undo_undo(&u, &p, &t));        /* the edit not yet taken is taken, then undone */
    CHECK(t.count == 6 && track_undo_can_redo(&u));
    t.points[0].x = 1;
    CHECK(track_undo_commit(&u, &p, &t) && !track_undo_can_redo(&u));
    track_undo_free(&u);
    drop(&t), drop(&first);
}

/* every byte of every point and every key is undone: a field added to TracePoint or TraceKey later is too */
static void test_every_byte(void)
{
    Project p;
    Trace t, first;
    make(&p, &t);
    copy(&first, &t);
    TrackUndo u;
    CHECK(track_undo_open(&u, &p, &t));
    int missed = 0;
    const size_t sizes[2] = { sizeof *t.points * (size_t)t.count, sizeof *t.keys * (size_t)t.key_count };
    for (int a = 0; a < 2; a++) {
        for (size_t i = 0; i < sizes[a]; i++) {
            unsigned char *const bytes = a == 0 ? (unsigned char *)t.points : (unsigned char *)t.keys;   /* each undo makes the arrays again */
            bytes[i] ^= 0x5a;
            if (!track_undo_modified(&u, &p, &t) || !track_undo_commit(&u, &p, &t) || !track_undo_undo(&u, &p, &t) || !same(&t, &first)) missed++;
        }
    }
    if (missed != 0) fprintf(stderr, "every byte: %d not undone\n", missed);
    CHECK_EQ_INT(missed, 0);
    track_undo_free(&u);
    drop(&t), drop(&first);
}

/* trace.json before an edit and after its undo is the same file */
static void test_the_file(void)
{
    char dir[] = "/tmp/track_undo_XXXXXX", why[200], path[300];
    CHECK(mkdtemp(dir) != NULL);
    Project p;
    Trace t;
    make(&p, &t);
    TrackUndo u;
    CHECK(track_undo_open(&u, &p, &t));
    CHECK(project_save_trace(dir, &t, why, sizeof why));
    snprintf(path, sizeof path, "%s/trace.json", dir);
    FILE *f = fopen(path, "rb");
    char before[8192] = "", after[8192] = "";
    CHECK(f != NULL && fread(before, 1, sizeof before - 1, f) > 0);
    if (f != NULL) fclose(f);
    t.points[4].z += 30;
    CHECK(track_undo_commit(&u, &p, &t));
    CHECK(track_trace_key_set_width(&t.keys, &t.key_count, &t.key_cap, 4, 12.0));
    CHECK(track_trace_key_set_side(&t.keys, &t.key_count, &t.key_cap, 1, TRACE_SIDE_FENCE, TRACK_GEN_LEFT, 0.0));
    CHECK(track_undo_commit(&u, &p, &t));
    CHECK(track_undo_undo(&u, &p, &t) && track_undo_undo(&u, &p, &t));
    CHECK(project_save_trace(dir, &t, why, sizeof why));
    f = fopen(path, "rb");
    CHECK(f != NULL && fread(after, 1, sizeof after - 1, f) > 0);
    if (f != NULL) fclose(f);
    CHECK(strcmp(before, after) == 0);
    unlink(path), rmdir(dir);
    track_undo_free(&u);
    drop(&t);
}

/* the oldest step falls off; the newest TRACK_UNDO_MAX are kept */
static void test_bounded(void)
{
    Project p;
    Trace t;
    make(&p, &t);
    TrackUndo u;
    CHECK(track_undo_open(&u, &p, &t));
    for (int i = 1; i <= TRACK_UNDO_MAX + 10; i++) {
        t.points[1].x = i;
        CHECK(track_undo_commit(&u, &p, &t));
    }
    int undone = 0;
    while (track_undo_undo(&u, &p, &t)) undone++;
    CHECK_EQ_INT(undone, TRACK_UNDO_MAX);
    CHECK(t.points[1].x == 10);
    track_undo_free(&u);
    drop(&t);
}

int main(void)
{
    test_steps();
    test_every_byte();
    test_the_file();
    test_bounded();
    return TEST_REPORT();
}
