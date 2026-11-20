/* The road's bank as road keys say it (dep59 editor phase 2): a key's bank_deg at its point, from key to key a smoothstep over the arc length (from a level
 * road at the start when the first key is further on), held after the last key. Signed as the CD's: positive where the heading grows (a right-hand curve).
 * A key takes -60..60 degrees; the generator builds up to 35 (the limits are track_trace_flags'), so a trace with no bank keys builds as before. */
#include "formats/track_trace.h"
#include "formats/track_gen.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH = 40 };

static double smooth(double t) { return t * t * (3 - 2 * t); }

static void straight_points(TracePoint *p, int n, double gap)
{
    for (int i = 0; i < n; ++i) p[i].x = 0, p[i].z = gap * i;
}

static TrackGenNode *nodes_of(const TracePoint *p, int n, const TraceKey *keys, int key_count, int *count, ExchangeError *err)
{
    TrackGenNode *nodes = NULL;
    memset(err, 0, sizeof *err);
    track_trace_nodes_keyed(p, n, WIDTH, keys, key_count, &nodes, count, err);
    return nodes;
}

/* the bank's bit is 1 << 1: the width's is 1, the shoulder surface's 1 << 2, the sides' from 1 << 3 */
static void test_bit(void)
{
    CHECK(TRACE_KEY_BANK == 1u << 1);
    CHECK((TRACE_KEY_BANK & TRACE_KEY_WIDTH) == 0);
    for (int f = 0; f < TRACE_SIDE_FIELDS; ++f) CHECK((TRACE_KEY_BANK & (TRACE_KEY_SIDE(f, 0) | TRACE_KEY_SIDE(f, 1))) == 0);
    CHECK(TRACE_BANK_WARN_DEG == 25.0 && TRACE_BANK_MAX_DEG == TRACK_GEN_BANK_MAX_DEGREES && TRACE_BANK_INPUT_MAX_DEG == 60.0);
}

/* no bank key: every node level, whatever the width keys do */
static void test_no_bank_keys(void)
{
    TracePoint p[6];
    straight_points(p, 6, 60);
    const TraceKey width = { .point = 2, .has = TRACE_KEY_WIDTH, .width_m = 12.0 };
    int count = 0;
    ExchangeError err;
    TrackGenNode *n = nodes_of(p, 6, &width, 1, &count, &err);
    CHECK(n != NULL);
    int level = 1;
    for (int k = 0; n != NULL && k < count; ++k) level &= n[k].bank == 0;
    CHECK(level);
    free(n);
}

/* level at the start, 20 at point 2 and -10 at point 4 by smoothsteps over the arc length, then held; a width on the same key stays the width's */
static void test_interpolates(void)
{
    TracePoint p[6];
    straight_points(p, 6, 60);   /* 300 m: 48 spans of 6.25 m */
    const TraceKey keys[2] = { { .point = 2, .has = TRACE_KEY_BANK | TRACE_KEY_WIDTH, .width_m = 10.0, .bank_deg = 20.0 },
                               { .point = 4, .has = TRACE_KEY_BANK, .bank_deg = -10.0 } };
    int count = 0;
    ExchangeError err;
    TrackGenNode *n = nodes_of(p, 6, keys, 2, &count, &err);
    CHECK_EQ_INT(count, 49);
    if (n == NULL) return;
    int near = 1;
    for (int k = 0; k < count; ++k) {
        const double s = k * 6.25;
        const double want = s >= 240 ? -10 : s >= 120 ? 20 + (-10 - 20) * smooth((s - 120) / 120) : 20 * smooth(s / 120);
        if (fabs(n[k].bank - want) > 1e-6) {
            near = 0;
            fprintf(stderr, "  node %d: bank %g, want %g\n", k, n[k].bank, want);
        }
    }
    CHECK(near);
    CHECK(n[0].bank == 0 && n[count - 1].bank == -10);
    CHECK(fabs(n[0].width - WIDTH) < 1e-9 && fabs(n[count - 1].width - 40) < 1e-9);   /* the width key's 10 m is 40: held */
    free(n);
}

/* a key on point 0 is the start's bank */
static void test_key_on_start(void)
{
    TracePoint p[4];
    straight_points(p, 4, 60);
    const TraceKey key = { .point = 0, .has = TRACE_KEY_BANK, .bank_deg = -12.5 };
    int count = 0;
    ExchangeError err;
    TrackGenNode *n = nodes_of(p, 4, &key, 1, &count, &err);
    int all = n != NULL;
    for (int k = 0; n != NULL && k < count; ++k) all &= n[k].bank == -12.5;
    CHECK(all);
    free(n);
}

/* the input's range is -60..60: past it the trace fails naming the point; at it, it builds nodes (the generator's 35 is a red flag, not this) */
static void test_range(void)
{
    TracePoint p[4];
    straight_points(p, 4, 60);
    TraceKey key = { .point = 2, .has = TRACE_KEY_BANK, .bank_deg = 60.0 };
    int count = 0;
    ExchangeError err;
    TrackGenNode *n = nodes_of(p, 4, &key, 1, &count, &err);
    CHECK(n != NULL && n[count - 1].bank == 60.0);
    free(n);
    key.bank_deg = -60.5;
    n = nodes_of(p, 4, &key, 1, &count, &err);
    CHECK(n == NULL && strstr(err.message, "point 2") != NULL && strstr(err.message, "60") != NULL);
    key.bank_deg = NAN;
    n = nodes_of(p, 4, &key, 1, &count, &err);
    CHECK(n == NULL && strstr(err.message, "point 2") != NULL);
    key.bank_deg = 10, key.point = 4;   /* off the points */
    n = nodes_of(p, 4, &key, 1, &count, &err);
    CHECK(n == NULL && strstr(err.message, "point 4") != NULL);
}

/* the Points column's values: each point's bank as the keys make it */
static void test_point_banks(void)
{
    TracePoint p[6];
    straight_points(p, 6, 60);
    const TraceKey keys[2] = { { .point = 2, .has = TRACE_KEY_BANK, .bank_deg = 20.0 }, { .point = 4, .has = TRACE_KEY_BANK, .bank_deg = -10.0 } };
    double deg[6];
    track_trace_point_banks(p, 6, keys, 2, deg);
    CHECK(deg[0] == 0 && fabs(deg[1] - 10) < 1e-6 && deg[2] == 20 && fabs(deg[3] - 5) < 1e-6 && deg[4] == -10 && deg[5] == -10);
    track_trace_point_banks(p, 6, NULL, 0, deg);
    CHECK(deg[0] == 0 && deg[5] == 0);
    track_trace_point_banks(p, 1, keys, 2, deg);   /* no curve: level */
    CHECK(deg[0] == 0);
}

/* the key list's edits: a bank on a new point makes a key with nothing else set; on a width's key it adds its bit; clearing drops the bit, and the key
 * when nothing is left */
static void test_edits(void)
{
    TraceKey *keys = NULL;
    int count = 0, cap = 0;
    CHECK(track_trace_key_set_width(&keys, &count, &cap, 3, 12.0));
    CHECK(track_trace_key_set_bank(&keys, &count, &cap, 1, 15.0));
    CHECK(count == 2 && keys[0].point == 1 && keys[0].has == TRACE_KEY_BANK && keys[0].bank_deg == 15.0 && keys[0].width_m == 0);
    CHECK(track_trace_key_set_bank(&keys, &count, &cap, 3, -30.0));
    CHECK(count == 2 && keys[1].has == (TRACE_KEY_WIDTH | TRACE_KEY_BANK) && keys[1].bank_deg == -30.0 && keys[1].width_m == 12.0);
    track_trace_key_clear_bank(keys, &count, 3);
    CHECK(count == 2 && keys[1].has == TRACE_KEY_WIDTH && keys[1].bank_deg == 0);
    track_trace_key_clear_bank(keys, &count, 1);
    CHECK(count == 1 && keys[0].point == 3);
    track_trace_key_clear_bank(keys, &count, 7);   /* no key there: nothing */
    CHECK(count == 1);
    track_trace_key_clear_width(keys, &count, 3);
    CHECK(count == 0);
    free(keys);
}

int main(void)
{
    test_bit();
    test_no_bank_keys();
    test_interpolates();
    test_key_on_start();
    test_range();
    test_point_banks();
    test_edits();
    return TEST_REPORT();
}
