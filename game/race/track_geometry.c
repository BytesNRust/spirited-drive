#include "game/race/track_geometry.h"

#include "game/gfx/x86_int.h"

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

enum {
    CHUNK_OFFSETS = 0x2c,   /* the header's table */
    CHUNK_RECORD = 0xc,     /* the chunk's bytes the record starts with */
    RECORD_HEAD = 12,
    CHUNK_ROW = 0x42,       /* 11 points of 3 i16 */
    RECORD_ROW = 0x84,      /* 11 points of 3 i32 */
    NODE_SIZE = 0x24,
    NODE_POSITION = 8
};

static int32_t rd32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

static void wr32(uint8_t *p, int32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)((uint32_t)v >> (8 * i));
}

static int32_t delta(const uint8_t *p)
{
    return x_shl((int16_t)(p[0] | p[1] << 8), DELTA_SHIFT);
}

/* 0043dfd0 */
void track_segment_record(const TrackSegmentSource *src, uint32_t segment, uint8_t record[TRACK_SEGMENT_RECORD])
{
    const uint32_t offset = (uint32_t)rd32(src->header + CHUNK_OFFSETS + 4 * (segment & src->segment_mask));
    const uint8_t *data = src->terrain + offset + CHUNK_RECORD;
    memcpy(record, data, RECORD_HEAD);
    for (uint32_t row = 0; row < TRACK_SEGMENT_ROWS; row++) {
        const uint8_t *node = src->nodes + ((segment * TRI_NODES_PER_SEGMENT + row) & src->node_mask) * NODE_SIZE + NODE_POSITION;
        const uint8_t *d = data + RECORD_HEAD + row * CHUNK_ROW;
        uint8_t *out = record + RECORD_HEAD + row * RECORD_ROW;
        int32_t centre[3], p[3];
        for (int k = 0; k < 3; k++) {
            centre[k] = x_add(rd32(node + 4 * k), delta(d + 2 * k));
            wr32(out + 4 * k, centre[k]);
        }
        for (int i = 1; i < TRACK_ROW_POINTS; i++) {
            for (int k = 0; k < 3; k++) {
                /* the chains start again from the centre at points 1 and 6 */
                const int32_t from = i == 1 || i == 1 + TRACK_CHAIN_POINTS ? centre[k] : p[k];
                p[k] = x_add(from, delta(d + 6 * i + 2 * k));
                wr32(out + 12 * i + 4 * k, p[k]);
            }
        }
    }
}
