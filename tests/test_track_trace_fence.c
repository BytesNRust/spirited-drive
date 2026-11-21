/* The fence step from the road keys (track_trace.h): «Fence height» and «Fence picture» are the road's (both sides: the game has one fence byte a segment),
 * held from their key to the next one that sets them; the barrier follows what is drawn, the shoulder's end where the fence stands, unless a side's
 * «Custom» holds, where the key's «Barrier from edge» is used as before. Values out of range are refused by point. */
#include "formats/track_template.h"
#include "formats/track_trace.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { POINTS = 6 };
static const double WIDTH = 40;

static void straight(TracePoint *p)
{
    for (int i = 0; i < POINTS; ++i) p[i].x = 0, p[i].z = 100.0 * i;
}

static TraceKey key_at(int point)
{
    TraceKey k;
    memset(&k, 0, sizeof k);
    k.point = point;
    return k;
}

static TrackGenNode *nodes_of(const TraceKey *keys, int key_count, int *count, double **metres, char *why, size_t cap)
{
    TracePoint p[POINTS] = { { .x = 0 } };
    straight(p);
    TrackGenNode *nodes = NULL;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    *metres = NULL;
    if (!track_trace_nodes_keyed(p, POINTS, WIDTH, keys, key_count, &nodes, count, &err)) {
        if (why != NULL) snprintf(why, cap, "%s", err.message);
        return NULL;
    }
    *metres = malloc(sizeof **metres * (size_t)*count);
    if (*metres != NULL) track_trace_distances(nodes, *count, *metres);
    return nodes;
}

/* no fence keys: no node sets a look (the template's), and a barrier with no Custom follows the shoulder */
static void test_height_and_picture(void)
{
    TraceKey keys[3] = { key_at(1), key_at(2), key_at(4) };
    keys[0].has = TRACE_KEY_FENCE_ART, keys[0].fence_art = TRACE_FENCE_RAIL;
    keys[1].has = TRACE_KEY_FENCE_HEIGHT, keys[1].fence_tall = 1;
    keys[2].has = TRACE_KEY_FENCE_ART | TRACE_KEY_FENCE_HEIGHT, keys[2].fence_art = TRACE_FENCE_PARAPET, keys[2].fence_tall = 0;
    int count = 0;
    double *m = NULL;
    TrackGenNode *n = nodes_of(keys, 3, &count, &m, NULL, 0);
    CHECK(n != NULL && m != NULL);
    if (n == NULL || m == NULL) { free(n), free(m); return; }
    int ok = 1;
    for (int k = 0; k < count; ++k) {
        const double at = m[k] + 1e-6;
        const int art = at >= 400 ? TRACE_FENCE_PARAPET : at >= 100 ? TRACE_FENCE_RAIL : -1;
        const int tall = at >= 400 ? 0 : at >= 200 ? 1 : -1;
        const TrackGenFence *f = &n[k].fence;
        if (art < 0) ok &= !(f->has & TRACK_GEN_FENCE_SLOT);
        else ok &= (f->has & TRACK_GEN_FENCE_SLOT) && f->slot == art * TRACK_GENERIC_FENCE_PICTURE_STEP;
        if (tall < 0) ok &= !(f->has & TRACK_GEN_FENCE_HEIGHT);
        else ok &= (f->has & TRACK_GEN_FENCE_HEIGHT) && f->tall == tall;
        if (!ok) { fprintf(stderr, "node %d at %.1f m: has %u tall %d slot %d\n", k, m[k], f->has, f->tall, f->slot); break; }
    }
    CHECK(ok);
    free(n), free(m);
}

static int barrier_of(const TrackGenNode *n, int side) { return (n->side[side].has & TRACK_GEN_SIDE_BARRIER) ? (int)lround(n->side[side].barrier) : -1; }
static int shoulder_of(const TrackGenNode *n, int side) { return (n->side[side].has & TRACK_GEN_SIDE_SHOULDER) ? (int)lround(n->side[side].shoulder) : -1; }

/* a barrier key without Custom is not used: the barrier is the template's (where the shoulder ends, 3 m); a shoulder key moves it with the shoulder */
static void test_barrier_follows(void)
{
    TraceKey keys[2] = { key_at(1), key_at(3) };
    keys[0].has = TRACE_KEY_SIDE(TRACE_SIDE_BARRIER, TRACK_GEN_RIGHT), keys[0].side[TRACE_SIDE_BARRIER][TRACK_GEN_RIGHT] = 8.0;
    keys[1].has = TRACE_KEY_SIDE(TRACE_SIDE_SHOULDER, TRACK_GEN_LEFT), keys[1].side[TRACE_SIDE_SHOULDER][TRACK_GEN_LEFT] = 5.0;
    int count = 0;
    double *m = NULL;
    TrackGenNode *n = nodes_of(keys, 2, &count, &m, NULL, 0);
    CHECK(n != NULL && m != NULL);
    if (n == NULL || m == NULL) { free(n), free(m); return; }
    int ok = 1;
    for (int k = 0; k < count && ok; ++k) {
        ok &= barrier_of(&n[k], TRACK_GEN_RIGHT) == -1;                      /* the 8 m key is not used */
        ok &= barrier_of(&n[k], TRACK_GEN_LEFT) == shoulder_of(&n[k], TRACK_GEN_LEFT);   /* -1 (template) before the shoulder key, then with it */
        if (!ok) fprintf(stderr, "node %d: right %d, left %d, left shoulder %d\n", k, barrier_of(&n[k], 1), barrier_of(&n[k], 0), shoulder_of(&n[k], 0));
    }
    CHECK(ok);
    CHECK(shoulder_of(&n[count - 1], TRACK_GEN_LEFT) == 40 && barrier_of(&n[count - 1], TRACK_GEN_LEFT) == 40);
    free(n), free(m);
}

/* Custom on the right from point 1 to point 3: the key's 8 m there (with its blend from the template's 3 m), then it follows the shoulder again */
static void test_custom(void)
{
    TraceKey keys[2] = { key_at(1), key_at(3) };
    keys[0].has = TRACE_KEY_SIDE(TRACE_SIDE_BARRIER, TRACK_GEN_RIGHT) | TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT);
    keys[0].side[TRACE_SIDE_BARRIER][TRACK_GEN_RIGHT] = 8.0, keys[0].barrier_custom[TRACK_GEN_RIGHT] = 1;
    keys[1].has = TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT), keys[1].barrier_custom[TRACK_GEN_RIGHT] = 0;
    int count = 0;
    double *m = NULL;
    TrackGenNode *n = nodes_of(keys, 2, &count, &m, NULL, 0);
    CHECK(n != NULL && m != NULL);
    if (n == NULL || m == NULL) { free(n), free(m); return; }
    int ok = 1;
    for (int k = 0; k < count && ok; ++k) {
        const double at = m[k] + 1e-6;
        const int b = barrier_of(&n[k], TRACK_GEN_RIGHT);
        if (at < 100) ok &= b == -1;               /* Custom not yet: it follows */
        else if (at < 300) ok &= b == 64;   /* the key's 8 m from its point on (a width key holds from its own point, the blend is before it) */
        else if (at >= 300) ok &= b == -1;          /* Custom off: the template's, where the shoulder ends */
        ok &= barrier_of(&n[k], TRACK_GEN_LEFT) == -1;
        if (!ok) fprintf(stderr, "node %d at %.1f m: right barrier %d\n", k, m[k], b);
    }
    CHECK(ok);
    free(n), free(m);
}

/* what a point has, as the keys make it (the editor's rows): the template's before any key, then held */
static void test_point_values(void)
{
    TraceKey keys[2] = { key_at(2), key_at(4) };
    keys[0].has = TRACE_KEY_FENCE_HEIGHT | TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_LEFT), keys[0].fence_tall = 1, keys[0].barrier_custom[TRACK_GEN_LEFT] = 1;
    keys[1].has = TRACE_KEY_FENCE_ART, keys[1].fence_art = TRACE_FENCE_RAIL;
    TraceFencePoint v[POINTS];
    track_trace_point_fence(keys, 2, POINTS, v);
    for (int i = 0; i < POINTS; ++i) {
        CHECK_EQ_INT(v[i].tall, i >= 2);
        CHECK_EQ_INT(v[i].art, i >= 4 ? TRACE_FENCE_RAIL : TRACE_FENCE_PLANKS);
        CHECK_EQ_INT(v[i].custom[TRACK_GEN_LEFT], i >= 2);
        CHECK_EQ_INT(v[i].custom[TRACK_GEN_RIGHT], 0);
    }
}

static void test_refusals(void)
{
    struct { unsigned has; int tall, art, custom; const char *word; } cases[] = {
        { TRACE_KEY_FENCE_HEIGHT, 2, 0, 0, "fence height" },
        { TRACE_KEY_FENCE_ART, 0, TRACE_FENCE_PICTURES, 0, "fence picture" },
        { TRACE_KEY_FENCE_ART, 0, -1, 0, "fence picture" },
        { TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT), 0, 0, 2, "custom" },
    };
    for (size_t c = 0; c < sizeof cases / sizeof cases[0]; ++c) {
        TraceKey k = key_at(2);
        k.has = cases[c].has, k.fence_tall = cases[c].tall, k.fence_art = cases[c].art, k.barrier_custom[TRACK_GEN_RIGHT] = cases[c].custom;
        int count = 0;
        double *m = NULL;
        char why[256] = "";
        TrackGenNode *n = nodes_of(&k, 1, &count, &m, why, sizeof why);
        CHECK(n == NULL);
        CHECK(strstr(why, "point 2") != NULL && strstr(why, cases[c].word) != NULL);
        if (strstr(why, cases[c].word) == NULL) fprintf(stderr, "case %zu: %s\n", c, why);
        free(n), free(m);
    }
}

/* the key list's edits: set and clear each value; a key with nothing left goes */
static void test_key_edits(void)
{
    TraceKey *keys = NULL;
    int count = 0, cap = 0;
    CHECK(track_trace_key_set_fence(&keys, &count, &cap, 3, TRACE_KEY_FENCE_HEIGHT, 1));
    CHECK(track_trace_key_set_fence(&keys, &count, &cap, 3, TRACE_KEY_FENCE_ART, TRACE_FENCE_PARAPET));
    CHECK(track_trace_key_set_fence(&keys, &count, &cap, 1, TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_LEFT), 1));
    CHECK(!track_trace_key_set_fence(&keys, &count, &cap, 1, TRACE_KEY_FENCE_ART, 7));   /* not a picture */
    CHECK(!track_trace_key_set_fence(&keys, &count, &cap, 1, TRACE_KEY_WIDTH, 1));       /* not a fence value */
    CHECK_EQ_INT(count, 2);
    if (count == 2) {
        CHECK(keys[0].point == 1 && keys[0].has == TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_LEFT) && keys[0].barrier_custom[TRACK_GEN_LEFT] == 1);
        CHECK(keys[1].point == 3 && keys[1].fence_tall == 1 && keys[1].fence_art == TRACE_FENCE_PARAPET);
    }
    track_trace_key_clear_fence(keys, &count, 3, TRACE_KEY_FENCE_HEIGHT);
    CHECK(count == 2 && keys[1].has == TRACE_KEY_FENCE_ART);
    track_trace_key_clear_fence(keys, &count, 3, TRACE_KEY_FENCE_ART);
    CHECK_EQ_INT(count, 1);
    track_trace_key_clear_fence(keys, &count, 1, TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_LEFT));
    CHECK_EQ_INT(count, 0);
    free(keys);
}

int main(void)
{
    test_height_and_picture();
    test_barrier_follows();
    test_custom();
    test_point_values();
    test_refusals();
    test_key_edits();
    return TEST_REPORT();
}
