/* The distance strip's runs (track_trace.h): for one road value (a TRACE_KEY_ bit), the stretches of control points over which it holds. A run starts at a
 * key that sets the value and lasts to the next key that sets it (the last to the end of the road); before the first such key the template's value holds,
 * a run of its own with `set` 0. */
#include "formats/track_trace.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { POINTS = 10 };

static TraceKey key_at(int point, unsigned has)
{
    TraceKey k;
    memset(&k, 0, sizeof k);
    k.point = point;
    k.has = has;
    return k;
}

static void test_no_keys(void)
{
    TraceStripRun runs[4];
    CHECK_EQ_INT(track_trace_strip_runs(NULL, 0, POINTS, TRACE_KEY_BANK, runs, 4), 1);
    CHECK_EQ_INT(runs[0].first, 0);
    CHECK_EQ_INT(runs[0].last, POINTS - 1);
    CHECK_EQ_INT(runs[0].set, 0);
}

static void test_runs(void)
{
    const TraceKey keys[] = { key_at(3, TRACE_KEY_BANK | TRACE_KEY_WIDTH), key_at(5, TRACE_KEY_WIDTH), key_at(8, TRACE_KEY_BANK) };
    TraceStripRun runs[8];
    // the bank: the template's 0..3, a key at 3 to 8, a key at 8 to the end
    CHECK_EQ_INT(track_trace_strip_runs(keys, 3, POINTS, TRACE_KEY_BANK, runs, 8), 3);
    CHECK_EQ_INT(runs[0].first, 0);
    CHECK_EQ_INT(runs[0].last, 3);
    CHECK_EQ_INT(runs[0].set, 0);
    CHECK_EQ_INT(runs[1].first, 3);
    CHECK_EQ_INT(runs[1].last, 8);
    CHECK_EQ_INT(runs[1].set, 1);
    CHECK_EQ_INT(runs[2].first, 8);
    CHECK_EQ_INT(runs[2].last, POINTS - 1);
    CHECK_EQ_INT(runs[2].set, 1);
    // the width: 0..3 template, 3..5, 5 to the end
    CHECK_EQ_INT(track_trace_strip_runs(keys, 3, POINTS, TRACE_KEY_WIDTH, runs, 8), 3);
    CHECK_EQ_INT(runs[1].last, 5);
    CHECK_EQ_INT(runs[2].first, 5);
    // a value no key sets is one template run
    CHECK_EQ_INT(track_trace_strip_runs(keys, 3, POINTS, TRACE_KEY_LANES, runs, 8), 1);
}

static void test_key_on_first_point(void)
{
    const TraceKey keys[] = { key_at(0, TRACE_KEY_LANES) };
    TraceStripRun runs[4];
    CHECK_EQ_INT(track_trace_strip_runs(keys, 1, POINTS, TRACE_KEY_LANES, runs, 4), 1);
    CHECK_EQ_INT(runs[0].set, 1);
    CHECK_EQ_INT(runs[0].first, 0);
    CHECK_EQ_INT(runs[0].last, POINTS - 1);
}

static void test_cap_and_bad_input(void)
{
    const TraceKey keys[] = { key_at(2, TRACE_KEY_BANK), key_at(4, TRACE_KEY_BANK), key_at(6, TRACE_KEY_BANK), key_at(20, TRACE_KEY_BANK) };
    TraceStripRun runs[2];
    // the count is the runs there are, `out` takes the first `cap`; a key off the points is skipped
    CHECK_EQ_INT(track_trace_strip_runs(keys, 4, POINTS, TRACE_KEY_BANK, runs, 2), 4);
    CHECK_EQ_INT(runs[1].first, 2);
    CHECK_EQ_INT(track_trace_strip_runs(keys, 4, POINTS, TRACE_KEY_BANK, NULL, 0), 4);
    // no points, or a bit that is not one value: no runs
    CHECK_EQ_INT(track_trace_strip_runs(keys, 4, 0, TRACE_KEY_BANK, runs, 2), 0);
    CHECK_EQ_INT(track_trace_strip_runs(keys, 4, 1, TRACE_KEY_BANK, runs, 2), 1);
    CHECK_EQ_INT(track_trace_strip_runs(keys, 4, POINTS, 0, runs, 2), 0);
    CHECK_EQ_INT(track_trace_strip_runs(keys, 4, POINTS, TRACE_KEY_BANK | TRACE_KEY_WIDTH, runs, 2), 0);
}

int main(void)
{
    test_no_keys();
    test_runs();
    test_key_on_first_point();
    test_cap_and_bad_input();
    return TEST_REPORT();
}
