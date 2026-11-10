/* The street lamps' halos kept between frames (halo_track.h): a blob that goes missing for a frame fades and comes back without a gap, the quota does not swap equal lamps from one
 * frame to the next, the radius follows the blob's in steps, a halo lost for good is gone in a few frames, and of two equal blobs the nearer board keeps the place. */
#include "game/viewer/halo_track.h"
#include "tests/test.h"

#include <string.h>

enum { QUOTA = 24 };

static HaloCand lamp(int cx, int cy, int area, int radius, int depth)
{
    return (HaloCand){ cx, cy, area, radius, 130, depth };
}

static int has_at(const HaloDraw *d, int n, int cx, int cy)
{
    for (int i = 0; i < n; i++)
        if (d[i].cx >= cx - 1 && d[i].cx <= cx + 1 && d[i].cy >= cy - 1 && d[i].cy <= cy + 1) return d[i].percent;
    return 0;
}

static void missing_for_a_frame(void)
{
    HaloTracker t;
    HaloDraw out[QUOTA + HALO_EXTRA];
    halo_track_clear(&t);
    const HaloCand a = lamp(100, 60, 2, 12, 0);
    int n = 0;
    for (int f = 0; f < 6; f++) n = halo_track_frame(&t, &a, 1, QUOTA, out, QUOTA + HALO_EXTRA);
    CHECK_EQ_INT(has_at(out, n, 100, 60), 100);   /* settled */
    n = halo_track_frame(&t, NULL, 0, QUOTA, out, QUOTA + HALO_EXTRA);   /* the sprite painted nothing */
    const int dim = has_at(out, n, 100, 60);
    CHECK(dim > 0 && dim < 100);                  /* still shown, thinner */
    n = halo_track_frame(&t, &a, 1, QUOTA, out, QUOTA + HALO_EXTRA);
    CHECK(has_at(out, n, 100, 60) > dim);         /* back, on the way up */
}

static void gone_for_good(void)
{
    HaloTracker t;
    HaloDraw out[QUOTA + HALO_EXTRA];
    halo_track_clear(&t);
    const HaloCand a = lamp(40, 40, 3, 12, 0);
    int n = 0;
    for (int f = 0; f < 6; f++) n = halo_track_frame(&t, &a, 1, QUOTA, out, QUOTA + HALO_EXTRA);
    for (int f = 0; f < 5; f++) n = halo_track_frame(&t, NULL, 0, QUOTA, out, QUOTA + HALO_EXTRA);
    CHECK_EQ_INT(n, 0);
}

static void quota_does_not_swap(void)
{
    HaloTracker t;
    HaloDraw out[QUOTA + HALO_EXTRA];
    HaloCand c[40];
    halo_track_clear(&t);
    int n = 0;
    for (int f = 0; f < 8; f++) {   /* 40 lamps of the same size, every one nudged a pixel each frame, in a different order */
        for (int i = 0; i < 40; i++) {
            const int j = f % 2 ? 39 - i : i;
            c[j] = lamp(10 + 12 * i + f, 30 + (i % 5) * 7, 1, 12, 0);
        }
        n = halo_track_frame(&t, c, 40, QUOTA, out, QUOTA + HALO_EXTRA);
    }
    int chosen[40] = { 0 }, full = 0;
    for (int i = 0; i < 40; i++) full += has_at(out, n, 10 + 12 * i + 7, 30 + (i % 5) * 7) == 100, chosen[i] = has_at(out, n, 10 + 12 * i + 7, 30 + (i % 5) * 7);
    CHECK_EQ_INT(full, QUOTA);
    for (int f = 8; f < 14; f++) {   /* the same lamps on: the same 24 stay lit, none of the others comes in, no fading halo appears */
        for (int i = 0; i < 40; i++) {
            const int j = f % 2 ? 39 - i : i;
            c[j] = lamp(10 + 12 * i + f, 30 + (i % 5) * 7, 1, 12, 0);
        }
        n = halo_track_frame(&t, c, 40, QUOTA, out, QUOTA + HALO_EXTRA);
        int same = 1;
        for (int i = 0; i < 40; i++) same = same && (has_at(out, n, 10 + 12 * i + f, 30 + (i % 5) * 7) == 100) == (chosen[i] == 100);
        CHECK(same);
        CHECK_EQ_INT(n, QUOTA);
    }
}

static void radius_in_steps(void)
{
    HaloTracker t;
    HaloDraw out[QUOTA + HALO_EXTRA];
    halo_track_clear(&t);
    HaloCand a = lamp(70, 50, 2, 12, 0);
    int n = 0;
    for (int f = 0; f < 4; f++) n = halo_track_frame(&t, &a, 1, QUOTA, out, QUOTA + HALO_EXTRA);
    CHECK_EQ_INT(out[0].radius, 12);
    a.area = 12, a.radius = 16;                  /* the sprite grew a pixel: the ask jumps by a third */
    n = halo_track_frame(&t, &a, 1, QUOTA, out, QUOTA + HALO_EXTRA);
    CHECK_EQ_INT(n, 1);
    CHECK(out[0].radius > 12 && out[0].radius < 16);
    for (int f = 0; f < 10; f++) n = halo_track_frame(&t, &a, 1, QUOTA, out, QUOTA + HALO_EXTRA);
    CHECK_EQ_INT(out[0].radius, 16);
}

static void nearer_board_keeps_place(void)
{
    HaloTracker t;
    HaloDraw out[QUOTA + HALO_EXTRA];
    halo_track_clear(&t);
    const HaloCand two[2] = { lamp(20, 20, 1, 12, 9000), lamp(60, 20, 1, 12, 3000) };
    int n = 0;
    for (int f = 0; f < 5; f++) n = halo_track_frame(&t, two, 2, 1, out, QUOTA + HALO_EXTRA);
    CHECK_EQ_INT(has_at(out, n, 60, 20), 100);   /* quota 1: the nearer board's lamp */
    CHECK_EQ_INT(has_at(out, n, 20, 20), 0);
}

int main(void)
{
    missing_for_a_frame();
    gone_for_good();
    quota_does_not_swap();
    radius_in_steps();
    nearer_board_keeps_place();
    return TEST_REPORT();
}
