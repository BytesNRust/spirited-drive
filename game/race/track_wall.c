#include "game/race/track_wall.h"

#include <string.h>

enum { RIGHT_PAIRS = 8, THIRD = 8, SECOND = 7, ROW_POINTS = 11 };

/* the point of the left chain or the right one that `vertex` is, and the vertex of its row that is `want`: both through the LOD's rows (select), and past
 * them the next segment's first row, every point in order. `point` is -1 for none. */
static int point_of(const TrackLod *lod, uint8_t vertex)
{
    int start = 0;
    for (int row = 0, at = 0; row < 4 && at < TRACK_LOD_SELECT; row++) {
        int n = 0;
        while (at + n < TRACK_LOD_SELECT && lod->select[at + n] != 255) n++;
        if (n == 0) break;
        if (vertex >= start && vertex < start + n) return lod->select[at + (vertex - start)];
        start += n;
        at += n + 1;
    }
    return vertex >= lod->vertex_count && vertex < lod->vertex_count + ROW_POINTS ? vertex - lod->vertex_count : -1;
}

static uint8_t same_row_vertex(const TrackLod *lod, uint8_t vertex, int want)
{
    int start = 0;
    for (int row = 0, at = 0; row < 4 && at < TRACK_LOD_SELECT; row++) {
        int n = 0;
        while (at + n < TRACK_LOD_SELECT && lod->select[at + n] != 255) n++;
        if (n == 0) break;
        if (vertex >= start && vertex < start + n) {
            for (int k = 0; k < n; k++)
                if (lod->select[at + k] == want) return (uint8_t)(start + k);
            return vertex;
        }
        start += n;
        at += n + 1;
    }
    if (vertex >= lod->vertex_count && vertex - lod->vertex_count < ROW_POINTS) return (uint8_t)(lod->vertex_count + want);
    return vertex;
}

static int reversed;

void track_wall_set_inner(int inner) { reversed = inner != 0; }

const TrackLod *track_wall_lod(int lod)
{
    static TrackLod inner_lods[TRACK_LOD_COUNT];
    static int built;
    if (lod < 0 || lod >= TRACK_LOD_COUNT) lod = 0;
    if (!reversed) return &TRACK_LODS[lod];
    if (!built) {
        for (int n = 0; n < TRACK_LOD_COUNT; n++) {
            inner_lods[n] = TRACK_LODS[n];
            /* the right-flagged pairs take the second point; the left-flagged ones, which are now the old right's, take the third where the exe's right chain did (the
             * left's own point two of the old chain is the right's second, the old right's third is its third) */
            for (int i = 0; i < RIGHT_PAIRS; i++) {
                const uint8_t was_right = TRACK_LODS[n].fence[RIGHT_PAIRS + i];
                if (was_right != 0 && point_of(&TRACK_LODS[n], was_right) == THIRD)
                    inner_lods[n].fence[i] = same_row_vertex(&TRACK_LODS[n], TRACK_LODS[n].fence[i], THIRD - 5);
            }
            for (int i = RIGHT_PAIRS; i < 2 * RIGHT_PAIRS; i++)
                if (point_of(&TRACK_LODS[n], TRACK_LODS[n].fence[i]) == THIRD) inner_lods[n].fence[i] = same_row_vertex(&TRACK_LODS[n], TRACK_LODS[n].fence[i], SECOND);
        }
        built = 1;
    }
    return &inner_lods[lod];
}
