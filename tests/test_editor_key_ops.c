/* dep87 1b: the road profile keys that go with Copy, Paste, Mirror and Reverse (editor/key_ops.h). A straight of 10 points 40 m apart with a
 * left shoulder of 5 m from point 2, the right shoulder tarmac from 3, a bank of 10 degrees at 4, lanes 1+2 from 5 and no left fence from 6:
 * each operation's result is read back point by point (what the Points table and the generator see) against the original's. */
#include "key_ops.h"
#include "point_ops.h"
#include "test.h"

#include "formats/track_gen.h"

#include <stdlib.h>
#include <string.h>

enum { N = 10 };

static TracePoint g_points[N];
static TraceKey g_keys[5];

static Trace road(void)
{
    for (int i = 0; i < N; ++i) g_points[i] = (TracePoint){ 0, 40.0 * i };
    memset(g_keys, 0, sizeof g_keys);
    g_keys[0].point = 2, g_keys[0].has = TRACE_KEY_SIDE(TRACE_SIDE_SHOULDER, TRACK_GEN_LEFT), g_keys[0].side[TRACE_SIDE_SHOULDER][TRACK_GEN_LEFT] = 5;
    g_keys[1].point = 3, g_keys[1].has = TRACE_KEY_SHOULDER_SURFACE, g_keys[1].shoulder_r_surface = TRACE_SURFACE_TARMAC;
    g_keys[2].point = 4, g_keys[2].has = TRACE_KEY_BANK, g_keys[2].bank_deg = 10;
    g_keys[3].point = 5, g_keys[3].has = TRACE_KEY_LANES, g_keys[3].lanes = TRACK_GEN_LANES_1_2;
    g_keys[4].point = 6, g_keys[4].has = TRACE_KEY_SIDE(TRACE_SIDE_FENCE, TRACK_GEN_LEFT), g_keys[4].side[TRACE_SIDE_FENCE][TRACK_GEN_LEFT] = 0;
    return (Trace){ g_points, N, 40.0, g_keys, 5, 5 };
}

typedef struct {
    KeyState st;
    TracePoint *points;
    TraceKey *keys;
} Result;

static int made(Result *r, TracePoint *points, int n, TraceKey *keys, int count)
{
    r->points = points, r->keys = keys;
    const Trace t = { points, n, 40.0, keys, count, count };
    return points != NULL && key_state_make(&t, &r->st);
}

static void done(Result *r)
{
    key_state_free(&r->st);
    free(r->points), free(r->keys);
}

/* after's point j against before's point i (seen from the other side when `swap`), for `mask` */
static int same_at(const KeyState *after, int j, const KeyState *before, int i, unsigned mask, int swap)
{
    const TraceKey a = key_state_at(after, j, mask);
    const TraceKey b0 = key_state_at(before, i, mask), b = swap ? key_swapped(b0) : b0;
    const int ok = key_same(&a, &b, mask);
    if (!ok) printf("point %d against %d%s (mask 0x%x) differs\n", j, i, swap ? " swapped" : "", mask);
    return ok;
}

static void test_swapped(void)
{
    TraceKey k;
    memset(&k, 0, sizeof k);
    k.has = TRACE_KEY_SIDE(TRACE_SIDE_SHOULDER, TRACK_GEN_LEFT) | TRACE_KEY_BANK | TRACE_KEY_LANES | TRACE_KEY_SHOULDER_SURFACE;
    k.side[TRACE_SIDE_SHOULDER][TRACK_GEN_LEFT] = 5, k.bank_deg = 10, k.lanes = TRACK_GEN_LANES_1_2, k.shoulder_l_surface = TRACE_SURFACE_TARMAC;
    const TraceKey w = key_swapped(k);
    CHECK(w.has == (TRACE_KEY_SIDE(TRACE_SIDE_SHOULDER, TRACK_GEN_RIGHT) | TRACE_KEY_BANK | TRACE_KEY_LANES | TRACE_KEY_SHOULDER_SURFACE));
    CHECK(w.side[TRACE_SIDE_SHOULDER][TRACK_GEN_RIGHT] == 5 && w.bank_deg == -10 && w.lanes == TRACK_GEN_LANES_2_1);
    CHECK(w.shoulder_r_surface == TRACE_SURFACE_TARMAC && w.shoulder_l_surface == TRACE_SURFACE_INHERIT);
    const TraceKey back = key_swapped(w);
    CHECK(key_same(&back, &k, ~0u));
}

static void test_mirror(void)
{
    const Trace t = road();
    KeyState before;
    CHECK(key_state_make(&t, &before));
    TraceKey *keys = NULL;
    int count = 0;
    CHECK(keys_mirror(&t, 2, 7, &keys, &count));
    Result r;
    CHECK(made(&r, pts_mirror_lr(t.points, N, 2, 7), N, keys, count));   /* on a straight the points stay */
    const unsigned held = before.used & KEY_HELD, ramped = before.used & KEY_RAMPED;
    for (int i = 2; i < 7; ++i) CHECK(same_at(&r.st, i, &before, i, held, 1));   /* the run's segments, from the other side */
    for (int i = 0; i < N; ++i)
        if (i < 2 || i >= 7) CHECK(same_at(&r.st, i, &before, i, held, 0));   /* outside, and the tail's first segment (7), as they were */
    const int run_keyed[] = { 2, 4, 7 }, out_keyed[] = { 1, 8 };
    for (int i = 0; i < 3; ++i) CHECK(same_at(&r.st, run_keyed[i], &before, run_keyed[i], ramped, 1));
    for (int i = 0; i < 2; ++i) CHECK(same_at(&r.st, out_keyed[i], &before, out_keyed[i], ramped, 0));
    CHECK(r.st.surface[TRACK_GEN_LEFT][3] == TRACE_SURFACE_TARMAC && r.st.surface[TRACK_GEN_RIGHT][3] == TRACE_SURFACE_VERGE);
    CHECK(r.st.lanes[5] == TRACK_GEN_LANES_2_1 && r.st.lanes[7] == TRACK_GEN_LANES_1_2 && r.st.bank_deg[4] == -10);
    CHECK(r.st.side[2][TRACE_SIDE_SHOULDER][TRACK_GEN_RIGHT] == 5 && r.st.side[6][TRACE_SIDE_FENCE][TRACK_GEN_RIGHT] == 0);
    CHECK(r.st.side[7][TRACE_SIDE_FENCE][TRACK_GEN_LEFT] == 0 && r.st.side[7][TRACE_SIDE_FENCE][TRACK_GEN_RIGHT] == 1);   /* the tail's own again */
    done(&r);
    key_state_free(&before);
}

static void test_reverse(void)
{
    const Trace t = road();
    KeyState before;
    CHECK(key_state_make(&t, &before));
    const unsigned held = before.used & KEY_HELD, ramped = before.used & KEY_RAMPED;
    TraceKey *keys = NULL;
    int count = 0;
    CHECK(keys_reverse(&t, 2, 7, &keys, &count));   /* a stretch in the middle */
    Result r;
    CHECK(made(&r, pts_reverse(t.points, N, 2, 7), N, keys, count));
    for (int j = 2; j < 7; ++j) CHECK(same_at(&r.st, j, &before, 2 + 7 - 1 - j, held, 1));
    for (int j = 0; j < N; ++j)
        if (j < 2 || j >= 7) CHECK(same_at(&r.st, j, &before, j, held, 0));
    const int run_keyed[] = { 2, 4, 7 };
    for (int i = 0; i < 3; ++i) CHECK(same_at(&r.st, 2 + 7 - run_keyed[i], &before, run_keyed[i], ramped, 1));
    CHECK(same_at(&r.st, 1, &before, 1, ramped, 0) && same_at(&r.st, 8, &before, 8, ramped, 0));
    done(&r);
    CHECK(keys_reverse(&t, 0, N - 1, &keys, &count));   /* the whole road, driven the other way */
    CHECK(made(&r, pts_reverse(t.points, N, 0, N - 1), N, keys, count));
    for (int j = 0; j < N - 1; ++j) CHECK(same_at(&r.st, j, &before, N - 2 - j, held, 1));
    const int keyed[] = { 0, 2, 4, N - 1 };
    for (int i = 0; i < 4; ++i) CHECK(same_at(&r.st, N - 1 - keyed[i], &before, keyed[i], ramped, 1));
    done(&r);
    key_state_free(&before);
}

static void test_copy_paste(void)
{
    const Trace t = road();
    KeyState before;
    CHECK(key_state_make(&t, &before));
    const unsigned held = before.used & KEY_HELD, ramped = before.used & KEY_RAMPED;
    TraceKey *clip = NULL, *keys = NULL;
    int clip_count = 0, count = 0;
    CHECK(keys_clip(&t, 2, 5, &clip, &clip_count));   /* points 2..5: the shoulder, the tarmac from 3, the bank at 4 */
    CHECK(keys_paste(&t, N - 1, clip, clip_count, 4, &keys, &count));   /* at the end: points 9..12 */
    Result r;
    CHECK(made(&r, pts_paste(t.points, N, N - 1, t.points + 2, 4), N + 3, keys, count));
    for (int i = 0; i < 3; ++i) CHECK(same_at(&r.st, N - 1 + i, &before, 2 + i, held, 0));   /* the block's segments as the copy's */
    for (int i = 0; i < N - 1; ++i) CHECK(same_at(&r.st, i, &before, i, held, 0));   /* the road before it, as it was */
    const int keyed[] = { 0, 2, 3 };   /* the block's first, its bank key, its last */
    for (int i = 0; i < 3; ++i) CHECK(same_at(&r.st, N - 1 + keyed[i], &before, 2 + keyed[i], ramped, 0));
    CHECK(same_at(&r.st, 8, &before, 8, ramped, 0) && same_at(&r.st, 4, &before, 4, ramped, 0));
    done(&r);
    CHECK(keys_paste(&t, 1, clip, clip_count, 4, &keys, &count));   /* in the middle, after point 1: the old 2.. are now 5.. */
    CHECK(made(&r, pts_paste(t.points, N, 1, t.points + 2, 4), N + 3, keys, count));
    for (int i = 0; i < 3; ++i) CHECK(same_at(&r.st, 1 + i, &before, 2 + i, held, 0));
    CHECK(same_at(&r.st, 0, &before, 0, held, 0));
    for (int j = 4; j < N + 2; ++j) CHECK(same_at(&r.st, j, &before, j - 3, held, 0));   /* the tail from the block's end: old segment 1 on */
    CHECK(same_at(&r.st, 0, &before, 0, ramped, 0) && same_at(&r.st, 5, &before, 2, ramped, 0) && same_at(&r.st, 7, &before, 4, ramped, 0));
    done(&r);
    free(clip);
    key_state_free(&before);
}

int main(void)
{
    test_swapped();
    test_mirror();
    test_reverse();
    test_copy_paste();
    return TEST_REPORT();
}
