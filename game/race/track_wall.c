#include "game/race/track_wall.h"

#include <string.h>

enum { RIGHT_PAIRS = 8, THIRD = 8, SECOND = 7, ROW_POINTS = 11 };

/* the vertex of the same row as `vertex` whose point is the second one of the left chain, or `vertex` when there is none */
static uint8_t second_of_row(const TrackLod *lod, uint8_t vertex)
{
    int start = 0;
    for (int row = 0, at = 0; row < 4 && at < TRACK_LOD_SELECT; row++) {
        int n = 0;
        while (at + n < TRACK_LOD_SELECT && lod->select[at + n] != 255) n++;
        if (n == 0) break;
        if (vertex >= start && vertex < start + n) {
            if (lod->select[at + (vertex - start)] != THIRD) return vertex;
            for (int k = 0; k < n; k++)
                if (lod->select[at + k] == SECOND) return (uint8_t)(start + k);
            return vertex;
        }
        start += n;
        at += n + 1;
    }
    /* past the rows: the next segment's first row, every point in order */
    if (vertex >= lod->vertex_count && vertex - lod->vertex_count == THIRD) return (uint8_t)(lod->vertex_count + SECOND);
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
            for (int i = RIGHT_PAIRS; i < 2 * RIGHT_PAIRS; i++)
                inner_lods[n].fence[i] = second_of_row(&TRACK_LODS[n], TRACK_LODS[n].fence[i]);
        }
        built = 1;
    }
    return &inner_lods[lod];
}
