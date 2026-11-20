/* dep87 1b: paste, mirror and reverse on a run of control points (editor/point_ops.h), and the circuit they close: half a circuit copied,
 * pasted at the end, mirrored left/right and reversed lands on its start when the half starts and ends square to its axis. */
#include "point_ops.h"
#include "test.h"

#include <math.h>
#include <stdlib.h>

static int near(TracePoint a, double x, double z)
{
    const int ok = fabs(a.x - x) < 1e-6 && fabs(a.z - z) < 1e-6;
    if (!ok) printf("point (%.6f, %.6f), expected (%.6f, %.6f)\n", a.x, a.z, x, z);
    return ok;
}

static double dist(TracePoint a, TracePoint b) { return hypot(a.x - b.x, a.z - b.z); }

static void test_heading(void)
{
    const TracePoint p[3] = { { 0, 0 }, { 0, 10 }, { 10, 10 } };
    CHECK(near(pts_heading(p, 3, 0), 0, 1));   /* point 0: the chord leaving it */
    CHECK(near(pts_heading(p, 3, 1), 0, 1));   /* the chord arriving */
    CHECK(near(pts_heading(p, 3, 2), 1, 0));
    CHECK(near(pts_heading(p, 1, 0), 0, 0));
}

static void test_paste(void)
{
    const TracePoint road[3] = { { 0, 0 }, { 0, 10 }, { 0, 20 } };
    const TracePoint block[3] = { { 0, 0 }, { 0, 10 }, { 10, 20 } };
    TracePoint *q = pts_paste(road, 3, 2, block, 3);   /* at the end, the same heading: as it was, after point 2 */
    CHECK(q != NULL && near(q[2], 0, 20) && near(q[3], 0, 30) && near(q[4], 10, 40));
    free(q);
    const TracePoint east[2] = { { 0, 0 }, { 10, 0 } };
    q = pts_paste(east, 2, 1, block, 3);   /* the block turned from +z to +x (a quarter clockwise) */
    CHECK(q != NULL && near(q[1], 10, 0) && near(q[2], 20, 0) && near(q[3], 30, -10));
    free(q);
    const TracePoint straight[4] = { { 0, 0 }, { 0, 10 }, { 0, 20 }, { 0, 30 } };
    const TracePoint turn[3] = { { 0, 0 }, { 0, 10 }, { 10, 10 } };
    q = pts_paste(straight, 4, 1, turn, 3);   /* in the middle: the tail turns with the block's end (+z to +x) and follows it */
    CHECK(q != NULL && near(q[1], 0, 10) && near(q[2], 0, 20) && near(q[3], 10, 20) && near(q[4], 20, 20) && near(q[5], 30, 20));
    free(q);
    CHECK(pts_paste(straight, 4, 4, turn, 3) == NULL);   /* no such anchor */
    CHECK(pts_paste(straight, 4, 1, turn, 1) == NULL);   /* a block of one point is nothing to paste */
}

static void test_mirror(void)
{
    const TracePoint p[5] = { { 0, 0 }, { 0, 10 }, { 5, 20 }, { 10, 25 }, { 10, 35 } };
    TracePoint *q = pts_mirror_lr(p, 4, 1, 3);   /* about x = 0 (through point 1, along +z) */
    CHECK(q != NULL && near(q[0], 0, 0) && near(q[1], 0, 10) && near(q[2], -5, 20) && near(q[3], -10, 25));
    free(q);
    q = pts_mirror_lr(p, 5, 1, 3);   /* the tail turns with the stretch's end ((1,1) to (-1,1): a quarter anticlockwise) */
    CHECK(q != NULL && near(q[4], -20, 25));
    free(q);
    q = pts_mirror_fb(p, 3, 1, 2);   /* about z = 10, square to the heading at point 1: the stretch folds back */
    CHECK(q != NULL && near(q[1], 0, 10) && near(q[2], 5, 0));
    free(q);
    CHECK(pts_mirror_lr(p, 5, 3, 1) == NULL && pts_mirror_lr(p, 5, 0, 5) == NULL);
}

static void test_reverse(void)
{
    const TracePoint p[5] = { { 0, 0 }, { 0, 10 }, { 10, 20 }, { 10, 30 }, { 10, 40 } };
    TracePoint *q = pts_reverse(p, 5, 0, 4);   /* the whole road: driven the other way, nothing moves */
    CHECK(q != NULL && near(q[0], 10, 40) && near(q[2], 10, 20) && near(q[4], 0, 0));
    free(q);
    q = pts_reverse(p, 5, 1, 3);   /* in the middle: the new first point on point 1, going on along +z; the tail follows */
    CHECK(q != NULL && near(q[0], 0, 0) && near(q[1], 0, 10) && near(q[2], 0, 20) && near(q[3], 10, 30));
    CHECK(q != NULL && near(q[4], 10 + 10 * sqrt(0.5), 30 + 10 * sqrt(0.5)));   /* the old end arrived along +z, the new one along (1,1): the tail turns 45 clockwise */
    free(q);
    q = pts_reverse(p, 5, 0, 2);   /* at the start: the new last point stays on point 2 and arrives as the road did there; the tail stays */
    CHECK(q != NULL && near(q[2], 10, 20) && near(q[3], 10, 30) && near(q[4], 10, 40));
    if (q != NULL) {
        const TracePoint was = pts_heading(p, 5, 2), now = pts_heading(q, 5, 2);
        CHECK(near(now, was.x, was.z));
        CHECK(fabs(dist(q[0], q[1]) - dist(p[1], p[2])) < 1e-9 && fabs(dist(q[1], q[2]) - dist(p[0], p[1])) < 1e-9);   /* the chords, in the other order */
    }
    free(q);
}

/* half a circuit from S to T, then Copy (all of it), Paste after T, Mirror left/right and Reverse on the pasted run (T included) */
static TracePoint *close_half(const TracePoint *half, int k1, int *n)
{
    TracePoint *a = pts_paste(half, k1, k1 - 1, half, k1);
    TracePoint *b = a ? pts_mirror_lr(a, 2 * k1 - 1, k1 - 1, 2 * k1 - 2) : NULL;
    TracePoint *c = b ? pts_reverse(b, 2 * k1 - 1, k1 - 1, 2 * k1 - 2) : NULL;
    free(a), free(b);
    *n = 2 * k1 - 1;
    return c;
}

static void test_circuit_closes(void)
{
    const TracePoint square[8] = { { 0, 0 }, { 60, 0 }, { 140, 30 }, { 160, 120 }, { 110, 180 }, { 150, 260 }, { 60, 300 }, { 0, 300 } };
    int n = 0;
    TracePoint *c = close_half(square, 8, &n);   /* the half starts and ends square to its axis (x = 0): it closes, a mirror image */
    CHECK(c != NULL && near(c[n - 1], 0, 0));
    if (c != NULL) {
        const TracePoint end = pts_heading(c, n, n - 1);
        CHECK(near(end, 1, 0));   /* arriving at S the way the road leaves it */
        for (int i = 0; i < n; ++i) CHECK(near(c[i], -c[n - 1 - i].x, c[n - 1 - i].z));
    }
    free(c);
    TracePoint gon[9];   /* half a 16-gon with S and T on corners (each turns 22.5 degrees): the ends are not square, it misses */
    for (int i = 0; i < 9; ++i) gon[i] = (TracePoint){ 100 * sin(2 * M_PI * i / 16), 100 - 100 * cos(2 * M_PI * i / 16) };
    c = close_half(gon, 9, &n);
    CHECK(c != NULL && dist(c[n - 1], gon[0]) > 50);
    free(c);
}

int main(void)
{
    test_heading();
    test_paste();
    test_mirror();
    test_reverse();
    test_circuit_closes();
    return TEST_REPORT();
}
