#include "game/race/track_wall.h"

#include <string.h>

enum { RIGHT_PAIRS = 8, ROW_POINTS = 11 };

/* where a point of a row stands once the row is turned */
static int turned(int point) { return point == 0 ? 0 : point <= 5 ? point + 5 : point - 5; }

/* the select list is four rows ended by 255; a row can be empty (two 255s: the LODs with vertex_count 20 have no second row) and the rows after it still count.
 * The point of the left chain or the right one that `vertex` is, and the vertex of its row that is `want`: both through the LOD's rows (select), and past
 * them the next segment's first row, every point in order. `point` is -1 for none. */
static int point_of(const TrackLod *lod, uint8_t vertex)
{
    int start = 0;
    for (int row = 0, at = 0; row < 4 && at < TRACK_LOD_SELECT; row++) {
        int n = 0;
        while (at + n < TRACK_LOD_SELECT && lod->select[at + n] != 255) n++;
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

static int rows_as_forward;
enum { STRIP_HALF = 5, STRIPS = 10, FENCE_BIT_RIGHT = 0x80, FENCE_BIT_LEFT = 0x40 };

void track_wall_set_inner(int inner) { reversed = inner != 0; }
void track_wall_set_rows(int on) { rows_as_forward = on != 0; }

static int forward_rows(void) { return rows_as_forward && reversed; }
int track_wall_point(int point) { return forward_rows() ? turned(point) : point; }
int track_wall_strip(int strip) { return forward_rows() ? (strip + STRIP_HALF) % STRIPS : strip; }
uint8_t track_wall_fence(uint8_t fence)
{
    if (!forward_rows()) return fence;
    return (uint8_t)((fence & ~(FENCE_BIT_RIGHT | FENCE_BIT_LEFT)) | ((fence & FENCE_BIT_RIGHT) ? FENCE_BIT_LEFT : 0) | ((fence & FENCE_BIT_LEFT) ? FENCE_BIT_RIGHT : 0));
}

const TrackLod *track_wall_lod(int lod)
{
    static TrackLod inner_lods[TRACK_LOD_COUNT];
    static int built;
    if (lod < 0 || lod >= TRACK_LOD_COUNT) lod = 0;
    if (!reversed || rows_as_forward) return &TRACK_LODS[lod];   /* forward rows: the exe's own fence pairs */
    if (!built) {
        for (int n = 0; n < TRACK_LOD_COUNT; n++) {
            inner_lods[n] = TRACK_LODS[n];
            /* a reversed circuit's left wall is the old right chain's, turned with the row (points 1..5 on 6..10, 6..10 on 1..5), and its right wall the old left's:
             * the second and third points the exe's chains stand on (2, 7, 8) are now 7, 2, 3, and the odd ends some far LODs have (0, 5, 6) turn the same way */
            for (int i = 0; i < RIGHT_PAIRS; i++) {
                const uint8_t left = TRACK_LODS[n].fence[i], right = TRACK_LODS[n].fence[RIGHT_PAIRS + i];
                if (left == 0 || right == 0) continue;
                const int was_left = point_of(&TRACK_LODS[n], left), was_right = point_of(&TRACK_LODS[n], right);
                if (was_right >= 0) inner_lods[n].fence[i] = same_row_vertex(&TRACK_LODS[n], left, turned(was_right));
                if (was_left >= 0) inner_lods[n].fence[RIGHT_PAIRS + i] = same_row_vertex(&TRACK_LODS[n], right, turned(was_left));
            }
        }
        built = 1;
    }
    return &inner_lods[lod];
}
