/* port (track_wall.h): a reversed circuit's walls stand where the forward ones did. The LODs' right-flagged fence is on the left chain's second point in some
 * LODs and its third in others; with `inner` it is always the second. */
#include "game/race/track_tables.h"
#include "game/race/track_wall.h"
#include "test.h"

#include <string.h>

/* the point (0..10) a vertex of `lod` is, -1 if it is not in the rows; a vertex past the last row is the next segment's first row: all 11 points in order */
static int point_of(const TrackLod *lod, int vertex)
{
    int start = 0;
    for (int row = 0; row < 4; row++) {
        int n = 0;
        const uint8_t *s = lod->select;
        int at = 0, r = 0;
        while (r < row && at < TRACK_LOD_SELECT) { if (s[at++] == 255) r++; }
        while (at + n < TRACK_LOD_SELECT && s[at + n] != 255) n++;
        if (n == 0 || (row > 0 && at >= TRACK_LOD_SELECT)) break;
        if (vertex >= start && vertex < start + n) return s[at + (vertex - start)];
        start += n;
    }
    return vertex >= lod->vertex_count && vertex < lod->vertex_count + 11 ? vertex - lod->vertex_count : -1;
}

int main(void)
{
    int moved = 0;
    for (int n = 0; n < TRACK_LOD_COUNT; n++) {
        const TrackLod *lod = &TRACK_LODS[n];
        track_wall_set_inner(0);
        CHECK(track_wall_lod(n) == lod);                          /* not reversed: the exe's table itself */
        track_wall_set_inner(1);
        const TrackLod *inner = track_wall_lod(n);
        CHECK(inner != lod);
        CHECK_EQ_INT(inner->vertex_count, lod->vertex_count);
        CHECK_EQ_INT(inner->first_polygon, lod->first_polygon);
        CHECK(memcmp(inner->fence, lod->fence, 8) == 0);           /* the left-flagged wall is not touched */
        CHECK(memcmp(inner->select, lod->select, sizeof lod->select) == 0);
        for (int i = 8; i < 16; i++) {
            const int was = point_of(lod, lod->fence[i]), now = point_of(inner, inner->fence[i]);
            if (was == 8) { CHECK_EQ_INT(now, 7); moved++; }
            else CHECK_EQ_INT(inner->fence[i], lod->fence[i]);     /* the second point already, or no wall (0) */
            CHECK(now != 8);
        }
    }
    CHECK(moved > 0);   /* some LODs do put it on the third */
    return TEST_REPORT();
}
