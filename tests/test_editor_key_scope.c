/* dep87 part 2: an edit of a road profile value changes only what is selected (editor/key_ops.h keys_set_scoped): the value is set on the
 * selection and the value that held before comes back right after each run of it. A straight of 10 points 40 m apart; every case is read back
 * point by point the way the Points table and the generator see it (KeyState). «Extend» (keys_extend) lets a value run on again. */
#include "key_ops.h"
#include "test.h"

#include "formats/track_gen.h"

#include <stdlib.h>
#include <string.h>

enum { N = 10 };

static TracePoint g_points[N];

static Trace road(TraceKey *keys, int count)
{
    for (int i = 0; i < N; ++i) g_points[i] = (TracePoint){ 0, 40.0 * i, 0 };
    return (Trace){ g_points, N, 40.0, keys, count, count };
}

static TraceKey lanes_key(int point, int lanes)
{
    TraceKey k;
    memset(&k, 0, sizeof k);
    k.point = point, k.has = TRACE_KEY_LANES, k.lanes = lanes;
    return k;
}

static TraceKey width_key(int point, double metres)
{
    TraceKey k;
    memset(&k, 0, sizeof k);
    k.point = point, k.has = TRACE_KEY_WIDTH, k.width_m = metres;
    return k;
}

/* t's keys replaced by the result: the state they make */
static int state_of(const Trace *t, TraceKey *keys, int count, KeyState *st)
{
    const Trace r = { t->points, t->count, t->width, keys, count, count };
    return key_state_make(&r, st);
}

static int has_key(const TraceKey *keys, int count, int point, unsigned bit)
{
    for (int i = 0; i < count; ++i)
        if (keys[i].point == point && (keys[i].has & bit)) return 1;
    return 0;
}

static void test_held_single(void)
{
    TraceKey keys[1] = { lanes_key(7, TRACK_GEN_LANES_2_1) };
    const Trace t = road(keys, 1);
    const TraceKey v = lanes_key(0, TRACK_GEN_LANES_1_2);
    const int sel[] = { 3 };
    TraceKey *out = NULL;
    int count = 0;
    KeyState st;
    CHECK(keys_set_scoped(&t, sel, 1, &v, &out, &count));
    CHECK(state_of(&t, out, count, &st));
    CHECK_EQ_INT(st.lanes[2], TRACK_GEN_LANES_1_1);
    CHECK_EQ_INT(st.lanes[3], TRACK_GEN_LANES_1_2);
    CHECK_EQ_INT(st.lanes[4], TRACK_GEN_LANES_1_1);   /* the value that held comes back at the next point */
    CHECK_EQ_INT(st.lanes[6], TRACK_GEN_LANES_1_1);
    CHECK_EQ_INT(st.lanes[7], TRACK_GEN_LANES_2_1);   /* the road's own key further on is not touched */
    CHECK_EQ_INT(count, 3);                           /* the edit, the restoring key and the old one */
    CHECK_EQ_INT(keys[0].point, 7);                   /* the input is left alone */
    key_state_free(&st);
    free(out);
}

static void test_held_range_and_loose(void)
{
    TraceKey keys[1] = { lanes_key(7, TRACK_GEN_LANES_2_1) };
    const Trace t = road(keys, 1);
    const TraceKey v = lanes_key(0, TRACK_GEN_LANES_1_2);
    TraceKey *out = NULL;
    int count = 0;
    KeyState st;
    const int range[] = { 3, 4, 5 };
    CHECK(keys_set_scoped(&t, range, 3, &v, &out, &count));
    CHECK(state_of(&t, out, count, &st));
    for (int i = 3; i <= 5; ++i) CHECK_EQ_INT(st.lanes[i], TRACK_GEN_LANES_1_2);
    CHECK_EQ_INT(st.lanes[2], TRACK_GEN_LANES_1_1);
    CHECK_EQ_INT(st.lanes[6], TRACK_GEN_LANES_1_1);   /* restored after the last of the range */
    CHECK_EQ_INT(st.lanes[7], TRACK_GEN_LANES_2_1);
    key_state_free(&st);
    free(out);
    const int loose[] = { 1, 4, 5, 8 };   /* 1, 4-5 and 8: each run is restored after itself */
    CHECK(keys_set_scoped(&t, loose, 4, &v, &out, &count));
    CHECK(state_of(&t, out, count, &st));
    CHECK_EQ_INT(st.lanes[1], TRACK_GEN_LANES_1_2);
    CHECK_EQ_INT(st.lanes[2], TRACK_GEN_LANES_1_1);
    CHECK_EQ_INT(st.lanes[4], TRACK_GEN_LANES_1_2);
    CHECK_EQ_INT(st.lanes[5], TRACK_GEN_LANES_1_2);
    CHECK_EQ_INT(st.lanes[6], TRACK_GEN_LANES_1_1);
    CHECK_EQ_INT(st.lanes[7], TRACK_GEN_LANES_2_1);
    CHECK_EQ_INT(st.lanes[8], TRACK_GEN_LANES_1_2);
    CHECK_EQ_INT(st.lanes[9], TRACK_GEN_LANES_2_1);   /* what held at 8 before: the key at 7 */
    key_state_free(&st);
    free(out);
}

static void test_held_keys_inside_and_after(void)
{
    TraceKey keys[2] = { lanes_key(4, TRACK_GEN_LANES_2_1), lanes_key(8, TRACK_GEN_LANES_1_2) };
    const Trace t = road(keys, 2);
    const TraceKey v = lanes_key(0, TRACK_GEN_LANES_1_1 + 0);
    TraceKey one = lanes_key(0, TRACK_GEN_LANES_1_2);
    TraceKey *out = NULL;
    int count = 0;
    KeyState st;
    const int before_key[] = { 3 };
    CHECK(keys_set_scoped(&t, before_key, 1, &one, &out, &count));   /* the next point has its own key: it already says what comes next */
    CHECK_EQ_INT(count, 3);
    CHECK(state_of(&t, out, count, &st));
    CHECK_EQ_INT(st.lanes[3], TRACK_GEN_LANES_1_2);
    CHECK_EQ_INT(st.lanes[4], TRACK_GEN_LANES_2_1);
    key_state_free(&st);
    free(out);
    const int inside[] = { 3, 4, 5, 6 };   /* a key inside the selection gives way to it, what held at the end comes back after it */
    CHECK(keys_set_scoped(&t, inside, 4, &v, &out, &count));
    CHECK(state_of(&t, out, count, &st));
    for (int i = 3; i <= 6; ++i) CHECK_EQ_INT(st.lanes[i], TRACK_GEN_LANES_1_1);
    CHECK_EQ_INT(st.lanes[7], TRACK_GEN_LANES_2_1);   /* what held at 7 before: the key at 4 */
    CHECK_EQ_INT(st.lanes[8], TRACK_GEN_LANES_1_2);
    CHECK(!has_key(out, count, 4, TRACE_KEY_LANES));
    key_state_free(&st);
    free(out);
    const int last[] = { N - 1 };   /* the last point: nothing after it to restore */
    CHECK(keys_set_scoped(&t, last, 1, &one, &out, &count));
    CHECK_EQ_INT(count, 3);
    free(out);
    CHECK(!keys_set_scoped(&t, NULL, 0, &one, &out, &count));   /* nothing selected, or a point that is not there, changes nothing */
    const int bad[] = { N };
    CHECK(!keys_set_scoped(&t, bad, 1, &one, &out, &count));
}

static void test_ramped(void)
{
    const Trace t = road(NULL, 0);
    KeyState before, st;
    CHECK(state_of(&t, NULL, 0, &before));
    const TraceKey v = width_key(0, 14);
    TraceKey *out = NULL;
    int count = 0;
    const int one[] = { 5 };
    CHECK(keys_set_scoped(&t, one, 1, &v, &out, &count));   /* a ramp rises to the point and falls after it */
    CHECK(state_of(&t, out, count, &st));
    CHECK(st.width_m[5] == 14);
    CHECK(st.width_m[4] == before.width_m[4] && st.width_m[6] == before.width_m[6]);
    CHECK(st.width_m[0] == before.width_m[0] && st.width_m[9] == before.width_m[9]);
    CHECK(has_key(out, count, 4, TRACE_KEY_WIDTH) && has_key(out, count, 6, TRACE_KEY_WIDTH));
    key_state_free(&st);
    free(out);
    const int range[] = { 3, 4, 5, 6 };
    CHECK(keys_set_scoped(&t, range, 4, &v, &out, &count));   /* a range: the value on all of it, ramps at its ends */
    CHECK(state_of(&t, out, count, &st));
    for (int i = 3; i <= 6; ++i) CHECK(st.width_m[i] == 14);
    CHECK(st.width_m[2] == before.width_m[2] && st.width_m[7] == before.width_m[7]);
    key_state_free(&st);
    free(out);
    const int first[] = { 0 };   /* the first point has no point before it to pin */
    CHECK(keys_set_scoped(&t, first, 1, &v, &out, &count));
    CHECK(state_of(&t, out, count, &st));
    CHECK(st.width_m[0] == 14 && st.width_m[1] == before.width_m[1]);
    key_state_free(&st);
    free(out);
    key_state_free(&before);
}

static void test_extend(void)
{
    TraceKey keys[1] = { lanes_key(7, TRACK_GEN_LANES_2_1) };
    const Trace t = road(keys, 1);
    const TraceKey v = lanes_key(0, TRACK_GEN_LANES_1_2);
    const int sel[] = { 3 };
    TraceKey *edited = NULL, *next = NULL, *end = NULL;
    int edited_count = 0, next_count = 0, end_count = 0;
    CHECK(keys_set_scoped(&t, sel, 1, &v, &edited, &edited_count));
    const Trace e = road(edited, edited_count);
    KeyState st;
    CHECK(keys_extend(&e, 3, TRACE_KEY_LANES, 0, &next, &next_count));   /* to the next change: the restoring key goes, the key at 7 stays */
    CHECK(state_of(&t, next, next_count, &st));
    for (int i = 3; i <= 6; ++i) CHECK_EQ_INT(st.lanes[i], TRACK_GEN_LANES_1_2);
    CHECK_EQ_INT(st.lanes[7], TRACK_GEN_LANES_2_1);
    CHECK_EQ_INT(st.lanes[2], TRACK_GEN_LANES_1_1);
    key_state_free(&st);
    CHECK(keys_extend(&e, 3, TRACE_KEY_LANES, 1, &end, &end_count));   /* to the end: every later key of this value goes */
    CHECK(state_of(&t, end, end_count, &st));
    for (int i = 3; i < N; ++i) CHECK_EQ_INT(st.lanes[i], TRACK_GEN_LANES_1_2);
    CHECK_EQ_INT(st.lanes[2], TRACK_GEN_LANES_1_1);
    key_state_free(&st);
    CHECK(!keys_extend(&e, N, TRACE_KEY_LANES, 0, &next, &next_count));   /* no such point */
    free(edited), free(next), free(end);
    TraceKey other[1] = { lanes_key(5, TRACK_GEN_LANES_2_1) };   /* another value's keys stay: a key that says both loses only the one */
    other[0].has |= TRACE_KEY_WIDTH, other[0].width_m = 9;
    const Trace o = road(other, 1);
    CHECK(keys_extend(&o, 3, TRACE_KEY_LANES, 0, &next, &next_count));
    CHECK(has_key(next, next_count, 5, TRACE_KEY_WIDTH) && !has_key(next, next_count, 5, TRACE_KEY_LANES));
    free(next);
}

static void test_reach(void)
{
    TraceKey keys[3] = { lanes_key(2, TRACK_GEN_LANES_1_2), width_key(4, 9), lanes_key(7, TRACK_GEN_LANES_2_1) };
    const Trace t = road(keys, 3);
    CHECK_EQ_INT(keys_reach(&t, 2, TRACE_KEY_LANES), 7);                       /* up to the next key that sets lanes */
    CHECK_EQ_INT(keys_reach(&t, 2, TRACE_KEY_WIDTH), 4);                       /* each value its own */
    CHECK_EQ_INT(keys_reach(&t, 2, TRACE_KEY_LANES | TRACE_KEY_WIDTH), 4);     /* the nearest of several */
    CHECK_EQ_INT(keys_reach(&t, 7, TRACE_KEY_LANES), N - 1);                   /* none after: to the end */
    CHECK_EQ_INT(keys_reach(&t, 8, TRACE_KEY_BANK), N - 1);
}

int main(void)
{
    test_held_single();
    test_held_range_and_loose();
    test_held_keys_inside_and_after();
    test_ramped();
    test_extend();
    test_reach();
    return TEST_REPORT();
}
