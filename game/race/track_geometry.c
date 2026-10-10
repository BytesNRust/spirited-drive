#include "game/race/track_geometry.h"

#include <string.h>

enum { DELTA_SHIFT = 9 };  /* * 0x200 */

static TrackPoint offset(TrackPoint base, TriPoint delta)
{
    return (TrackPoint){ base.x + delta.x * (1 << DELTA_SHIFT), base.y + delta.y * (1 << DELTA_SHIFT),
                         base.z + delta.z * (1 << DELTA_SHIFT) };
}

/* Each chain starts from the centre and walks outward one delta at a time. */
static void decode_row(const TriRow *row, TriVec32 node, TrackPoint out[TRACK_ROW_POINTS])
{
    const TrackPoint centre = offset((TrackPoint){ node.x, node.y, node.z }, row->middle);
    out[0] = centre;
    for (int i = 0; i < TRACK_CHAIN_POINTS; i++) {
        out[1 + i] = offset(i == 0 ? centre : out[i], row->right[i]);
        out[6 + i] = offset(i == 0 ? centre : out[5 + i], row->left[i]);
    }
}

TriResult track_decode_segment(const Tri *tri, uint32_t segment, TrackSegment *out)
{
    TriSegment chunk;
    const uint32_t mask = tri->ring_segments - 1;
    TriResult r = tri_segment(tri, segment & mask, &chunk);
    if (r != TRI_OK) return r;
    out->unknown_0c = chunk.unknown_0c;
    out->fence = chunk.fence;
    memcpy(out->texture, chunk.texture, sizeof out->texture);
    for (uint32_t row = 0; row < TRACK_SEGMENT_ROWS; row++) {
        TriNode node;
        r = tri_node(tri, ((segment * TRI_NODES_PER_SEGMENT) + row) & (tri->ring_nodes - 1), &node);
        if (r != TRI_OK) return r;
        decode_row(&chunk.rows[row], node.position, out->rows[row]);
    }
    return TRI_OK;
}
