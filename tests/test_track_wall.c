/* port (track_wall.h): a reversed circuit's walls stand where the forward ones did. The LODs' right-flagged fence is on the left chain's second point in some
 * LODs and its third in others; with `inner` it is always the second. */
#include "game/race/track_tables.h"
#include "game/race/track_wall.h"
#include "test.h"

#include <string.h>

/* the point (0..10) a vertex of `lod` is, -1 if it is not in the rows; a vertex past the last row is the next segment's first row: all 11 points in order.
 * The select list has four rows, each ended by 255; a row can be empty (two 255s in a row: LODs 1, 6, 9 ... have no second row), and the rows after it still count. */
static int point_of(const TrackLod *lod, int vertex)
{
    int start = 0, at = 0;
    for (int row = 0; row < 4 && at < TRACK_LOD_SELECT; row++) {
        int n = 0;
        while (at + n < TRACK_LOD_SELECT && lod->select[at + n] != 255) n++;
        if (vertex >= start && vertex < start + n) return lod->select[at + (vertex - start)];
        start += n;
        at += n + 1;
    }
    return vertex >= lod->vertex_count && vertex < lod->vertex_count + 11 ? vertex - lod->vertex_count : -1;
}

static int turned(int p) { return p == 0 ? 0 : p <= 5 ? p + 5 : p - 5; }

/* every wall of a reversed circuit stands where the old one did: a turned row has the old points 1..5 on 6..10 and 6..10 on 1..5, so the left wall is the old right chain's turned (its points 7, 8 are now 2, 3), the right wall the old left's (2 is now 7); checked on every vertex of every LOD, the far LODs with an empty row among them */
static void test_every_lod_every_vertex(void)
{
    for (int n = 0; n < TRACK_LOD_COUNT; n++) {
        const TrackLod *lod = &TRACK_LODS[n];
        track_wall_set_inner(1);
        const TrackLod *inner = track_wall_lod(n);
        for (int i = 0; i < 8; i++) {
            if (lod->fence[i] == 0 || lod->fence[8 + i] == 0) continue;
            CHECK_EQ_INT(point_of(inner, inner->fence[i]), turned(point_of(lod, lod->fence[8 + i])));
            CHECK_EQ_INT(point_of(inner, inner->fence[8 + i]), turned(point_of(lod, lod->fence[i])));
        }
    }
    track_wall_set_inner(0);
}

static void test_rows_read_as_forward_only_on_a_reversed_circuit_with_the_option(void)
{
    track_wall_set_inner(0), track_wall_set_rows(1);
    CHECK_EQ_INT(track_wall_point(3), 3);                  /* forward circuit: untouched */
    CHECK_EQ_INT(track_wall_strip(2), 2);
    CHECK_EQ_INT(track_wall_fence(0xC5), 0xC5);
    track_wall_set_inner(1), track_wall_set_rows(0);
    CHECK_EQ_INT(track_wall_point(3), 3);                  /* option off: the swapped rows as the exe's reverse has them */
    CHECK_EQ_INT(track_wall_strip(2), 2);
    track_wall_set_rows(1);
    CHECK_EQ_INT(track_wall_point(0), 0);
    CHECK_EQ_INT(track_wall_point(1), 6);                  /* a chain point is read from the other chain */
    CHECK_EQ_INT(track_wall_point(5), 10);
    CHECK_EQ_INT(track_wall_point(8), 3);
    CHECK_EQ_INT(track_wall_strip(2), 7);                  /* and the strip is the other side's */
    CHECK_EQ_INT(track_wall_strip(9), 4);
    CHECK_EQ_INT(track_wall_fence(0x80 | 0x05), 0x40 | 0x05);   /* the fence bits go back to their sides, the texture bits stay */
    CHECK_EQ_INT(track_wall_fence(0xC1), 0xC1);
    CHECK(track_wall_lod(25) == &TRACK_LODS[25]);          /* the LOD's own fence pairs */
    track_wall_set_inner(0), track_wall_set_rows(0);
}

int main(void)
{
    for (int n = 0; n < TRACK_LOD_COUNT; n++) {
        const TrackLod *lod = &TRACK_LODS[n];
        track_wall_set_inner(0);
        CHECK(track_wall_lod(n) == lod);                          /* not reversed: the exe's table itself */
        track_wall_set_inner(1);
        const TrackLod *inner = track_wall_lod(n);
        CHECK(inner != lod);
        CHECK_EQ_INT(inner->vertex_count, lod->vertex_count);
        CHECK_EQ_INT(inner->first_polygon, lod->first_polygon);
        CHECK(memcmp(inner->select, lod->select, sizeof lod->select) == 0);
    }
    test_every_lod_every_vertex();
    test_rows_read_as_forward_only_on_a_reversed_circuit_with_the_option();
    return TEST_REPORT();
}
