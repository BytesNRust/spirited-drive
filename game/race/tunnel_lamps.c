#include "game/race/tunnel_lamps.h"

#include "game/race/track_mask.h"

#include <math.h>
#include <string.h>

enum { STRIPS = 10 };   /* a segment's texture bytes: the chain strips 0-4 (one side), 5-9 (the other) */

static int is_lamp(uint8_t texture, const uint8_t *slots, int slot_count)
{
    for (int i = 0; i < slot_count; i++)
        if (slots[i] == texture) return 1;
    return 0;
}

/* the middle of strip s of the band between rows a and b (the viewer's strip_points: strip s joins chain points inner and outer) */
static void strip_middle(const TrackPoint *a, const TrackPoint *b, int s, double *x, double *z)
{
    const int side = s / 5, step = s % 5, inner = step == 0 ? 0 : 1 + side * 5 + step - 1, outer = 1 + side * 5 + step;
    *x = ((double)a[inner].x + a[outer].x + b[outer].x + b[inner].x) / 4.0 / 65536.0;
    *z = ((double)a[inner].z + a[outer].z + b[outer].z + b[inner].z) / 4.0 / 65536.0;
}

size_t tunnel_lamps_in_segment(const TrackSegment *segment, const TrackPoint *next_row, uint32_t segment_index, const uint8_t *slots, int slot_count, TunnelLamp *out, size_t max)
{
    int lamp_strips[2][STRIPS / 2], strips[2] = { 0, 0 };   /* the strips of each side of the road that hold a lamp texture */
    for (int s = 0; s < STRIPS; s++)
        if (is_lamp(segment->texture[s], slots, slot_count)) lamp_strips[s / 5][strips[s / 5]++] = s;
    size_t n = 0;
    for (int row = 0; row < TRACK_SEGMENT_ROWS; row += TUNNEL_LAMP_EVERY)
        for (int side = 0; side < 2 && strips[side] > 0 && n < max; side++) {
            const TrackPoint *const a = segment->rows[row], *const b = row + 1 < TRACK_SEGMENT_ROWS ? segment->rows[row + 1] : next_row;
            double sum_x = 0, sum_z = 0;
            for (int i = 0; i < strips[side]; i++) {
                double x, z;
                strip_middle(a, b, lamp_strips[side][i], &x, &z);
                sum_x += x, sum_z += z;
            }
            const double dx = ((double)b[0].x - a[0].x) / 65536.0, dz = ((double)b[0].z - a[0].z) / 65536.0;
            out[n].node = segment_index * TRI_NODES_PER_SEGMENT + (uint32_t)row;
            out[n].x = (float)(sum_x / strips[side]), out[n].z = (float)(sum_z / strips[side]);
            out[n].spacing = (float)sqrt(dx * dx + dz * dz);
            out[n].along[0] = out[n].spacing > 0.0f ? (float)(dx / out[n].spacing) : 0.0f;
            out[n].along[1] = out[n].spacing > 0.0f ? (float)(dz / out[n].spacing) : 1.0f;
            n++;
        }
    return n;
}

size_t tunnel_lamps_find(const Tri *tri, const char *track, TunnelLamp *out, size_t max)
{
    const TunnelLampTexture *lamps = NULL;
    const int count = track_lamps_for(track, &lamps);
    if (tri == NULL || count <= 0 || tri->node_count < TRI_NODES_PER_SEGMENT) return 0;
    uint8_t slots[8];
    int slot_count = 0;
    for (int i = 0; i < count && slot_count < 8; i++) slots[slot_count++] = (uint8_t)lamps[i].slot;
    const uint32_t segments = tri->node_count / TRI_NODES_PER_SEGMENT;
    size_t n = 0;
    for (uint32_t s = 0; s < segments && n < max; s++) {
        TrackSegment here, next;
        if (track_decode_segment(tri, s, &here) != TRI_OK || track_decode_segment(tri, (s + 1) % segments, &next) != TRI_OK) continue;
        n += tunnel_lamps_in_segment(&here, next.rows[0], s, slots, slot_count, out + n, max - n);
    }
    return n;
}
