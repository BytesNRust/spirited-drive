#include "formats/tri_reverse.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    NODES_OFFSET = 0x98C,
    NODE_SIZE = 36,
    BRIGHTNESS_OFFSET = 0x15B0C,
    BRIGHTNESS_SIZE = 3,
    HEADER_SLOPE = 0x18,
    HEADER_ORIENTATION = 0x20,
    ROW_OFFSET = 0x18,
    ROW_SIZE = 66,
    POINT_SIZE = 6,
    CHAIN_BYTES = 5 * POINT_SIZE,
    ANGLE_MASK = 0x3FFF,        /* the 14-bit angles: 0x4000 is a turn */
    HALF_TURN = 0x2000,
    POLICE_SPOT = 1,            /* node +0x1a, as ai_manager.c */
    POLICE_FAR_SIDE = 2,
    PROPERTY = 7,
    SCENERY_ORIENTATION = 5,
    SCENERY_X = 0x0a,
    SCENERY_Z = 0x0e,
    DESCRIPTOR_SIZE = 16,
    DESCRIPTOR_TYPE = 1,
    DESCRIPTOR_RESOURCE = 2,
    TYPE_MODEL = 1,
    MODEL_TO_OFFSET = 16,       /* a model unit is 1/16 m, an offset's 1/4096 of a 16.16 m: 256 of them */
    SCENERY_HALF_TURN = 128     /* the placement's angle is a byte, 256 a turn */
};

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }

static uint32_t get32(const uint8_t *p) { return (uint32_t)get16(p) | (uint32_t)get16(p + 2) << 16; }
static void put32(uint8_t *p, uint32_t v) { put16(p, v), put16(p + 2, v >> 16); }

static uint16_t negate_angle(uint16_t a) { return (uint16_t)((0x4000 - (a & ANGLE_MASK)) & ANGLE_MASK); }

static uint8_t swap_nibbles(uint8_t b) { return (uint8_t)(b << 4 | b >> 4); }

static void swap_bytes(uint8_t *a, uint8_t *b)
{
    const uint8_t t = *a;
    *a = *b, *b = t;
}

/* a node as seen from the other way (36 bytes in place); `next` is the old node before it, the new node's next */
static void turn_node(uint8_t *n, const uint8_t *next, int strip_property)
{
    swap_bytes(n + 0, n + 1);   /* verges */
    swap_bytes(n + 2, n + 3);   /* barriers */
    for (int i = 4; i < 7; i++) n[i] = swap_nibbles(n[i]);
    /* the slope and the heading are those of the way to the next node (the file's values are that, to a unit) */
    if (strip_property) n[PROPERTY] = next[PROPERTY];   /* the strip's tunnel, LOD and sound byte goes with the strip, as the slope does */
    put16(n + 0x14, negate_angle(get16(next + 0x14)));
    put16(n + 0x16, negate_angle(get16(n + 0x16)));                 /* slant_a: the bank of the node's own cross-section */
    put16(n + 0x18, (get16(next + 0x18) + HALF_TURN) & ANGLE_MASK);
    const uint16_t police = get16(n + 0x1a);
    if (police & POLICE_SPOT) put16(n + 0x1a, police ^ POLICE_FAR_SIDE);
    for (int i = 0x1c; i <= 0x20; i += 2) put16(n + i, (uint16_t)(0u - get16(n + i)));  /* z_orientation, slant_b, x_orientation */
}

typedef struct {
    uint32_t node;
    uint32_t order;
    uint8_t record[TRI_SCENERY_SIZE];
} Placement;

static int by_node(const void *a, const void *b)
{
    const Placement *x = a, *y = b;
    if (x->node != y->node) return x->node < y->node ? -1 : 1;
    return x->order < y->order ? -1 : x->order > y->order;
}

/* the heading the model is drawn at: the placement's byte (256 a turn) and the node's 14-bit angle, as a fraction of a turn */
static double heading_turns(uint8_t orientation, uint16_t node_angle)
{
    return ((double)orientation * 0x10000 + (double)(node_angle & ANGLE_MASK) * 0x400) / 16777216.0;
}

static uint16_t node_angle_of(const uint8_t *file, uint32_t node)
{
    return node == 0 ? get16(file + HEADER_ORIENTATION) : get16(file + NODES_OFFSET + (size_t)NODE_SIZE * node + 0x18);
}

static int16_t clamp16(double v)
{
    const long r = lround(v);
    return (int16_t)(r < -32768 ? -32768 : r > 32767 ? 32767 : r);
}

/* a model's centre (`pivot`, model units) turned by `turns` of a turn, as model_draw turns a vertex: [c -s; s c] over (x, z) */
static void turned_centre(const TriPivot *pivot, double turns, double out[2])
{
    const double a = turns * 2 * 3.14159265358979323846, c = cos(a), s = sin(a);
    out[0] = MODEL_TO_OFFSET * (c * pivot->x - s * pivot->z);
    out[1] = MODEL_TO_OFFSET * (s * pivot->x + c * pivot->z);
}

/* `record` is the placement already turned (its orientation a half turn on). The half turn about the corner left the model's centre on the far side of the road: the offset moves by what the centre
 * was (at the old heading) less what it now is (at the new one) */
static void keep_centre(const Tri *tri, const uint8_t *in, const uint8_t *out, const TriPivot *pivots, uint32_t count,
                        uint32_t old_node, uint32_t new_node, uint8_t *record)
{
    const uint8_t descriptor = record[4];
    if (pivots == NULL || descriptor >= tri->descriptor_count) return;
    const uint8_t *d = in + tri->descriptors_offset + (size_t)DESCRIPTOR_SIZE * descriptor;
    if (d[DESCRIPTOR_TYPE] != TYPE_MODEL || d[DESCRIPTOR_RESOURCE] >= count) return;
    const TriPivot *pivot = &pivots[d[DESCRIPTOR_RESOURCE]];
    double before[2], after[2];
    turned_centre(pivot, heading_turns((uint8_t)(record[SCENERY_ORIENTATION] + SCENERY_HALF_TURN), node_angle_of(in, old_node)), before);
    turned_centre(pivot, heading_turns(record[SCENERY_ORIENTATION], node_angle_of(out, new_node)), after);
    put16(record + SCENERY_X, (uint16_t)clamp16((int16_t)get16(record + SCENERY_X) + before[0] - after[0]));
    put16(record + SCENERY_Z, (uint16_t)clamp16((int16_t)get16(record + SCENERY_Z) + before[1] - after[1]));
}

static void turn_scenery(const Tri *tri, const uint8_t *in, uint8_t *out, uint32_t ring, const TriPivot *pivots, uint32_t count_pivots)
{
    Placement list[TRI_SCENERY_MAX];
    uint32_t count = 0;
    const uint8_t *src = in + tri->scenery_offset;
    for (; count < tri->scenery_count; count++) {
        Placement *p = &list[count];
        memcpy(p->record, src + count * TRI_SCENERY_SIZE, TRI_SCENERY_SIZE);
        const uint32_t node = get32(p->record);
        p->node = (ring - node % ring) % ring;
        p->order = tri->scenery_count - count;   /* the ones on a node come the other way round too */
        p->record[SCENERY_ORIENTATION] = (uint8_t)(p->record[SCENERY_ORIENTATION] + SCENERY_HALF_TURN);
        keep_centre(tri, in, out, pivots, count_pivots, node % ring, p->node, p->record);
    }
    qsort(list, count, sizeof list[0], by_node);
    uint8_t *dst = out + tri->scenery_offset;
    for (uint32_t i = 0; i < count; i++) {
        memcpy(dst + i * TRI_SCENERY_SIZE, list[i].record, TRI_SCENERY_SIZE);
        put32(dst + i * TRI_SCENERY_SIZE, list[i].node);
    }
}

TriResult tri_reverse_with(const uint8_t *in, size_t size, uint8_t *out, const TriReverseOptions *options)
{
    const TriPivot *pivots = options != NULL ? options->pivots : NULL;
    const uint32_t count_pivots = options != NULL && pivots != NULL ? options->pivot_count : 0;
    const int strip_property = options != NULL && options->strip_property;
    Tri tri;
    const TriResult opened = tri_open(&tri, in, size);
    if (opened != TRI_OK) return opened;
    if (!tri_is_circuit(&tri) || tri.circuit_segments != tri.segment_count) return TRI_ERR_HEADER;
    const uint32_t ring = tri.ring_nodes, segments = tri.segment_count;
    memcpy(out, in, size);

    /* node 0 as the loader has it: the header's slope and orientation are over its own */
    uint8_t first[NODE_SIZE];
    memcpy(first, in + NODES_OFFSET, NODE_SIZE);
    put16(first + 0x14, get16(in + HEADER_SLOPE));
    put16(first + 0x18, get16(in + HEADER_ORIENTATION));
    for (uint32_t j = 0; j < ring; j++) {   /* the closing node past the ring is never read: left as it is */
        uint8_t node[NODE_SIZE];
        const uint32_t old = (ring - j % ring) % ring, before = (old + ring - 1) % ring;
        memcpy(node, old == 0 ? first : in + NODES_OFFSET + (size_t)NODE_SIZE * old, NODE_SIZE);
        turn_node(node, before == 0 ? first : in + NODES_OFFSET + (size_t)NODE_SIZE * before, strip_property);
        memcpy(out + NODES_OFFSET + (size_t)NODE_SIZE * j, node, NODE_SIZE);
    }
    put16(out + HEADER_SLOPE, get16(out + NODES_OFFSET + 0x14));
    put16(out + HEADER_ORIENTATION, get16(out + NODES_OFFSET + 0x18));   /* node 0 as the loader will have it */

    for (uint32_t t = 0; t < segments; t++) {
        uint8_t *seg = out + tri.terrain_offset + (size_t)TRI_SEGMENT_SIZE * t;
        const uint8_t *from = in + tri.terrain_offset + (size_t)TRI_SEGMENT_SIZE * (segments - 1 - t);
        memcpy(seg, from, TRI_SEGMENT_SIZE);
        const uint8_t fence = from[0x0d];
        seg[0x0d] = (uint8_t)((fence & TRI_FENCE_TEXTURE_MASK) | ((fence & TRI_FENCE_LEFT) ? TRI_FENCE_RIGHT : 0) |
                              ((fence & TRI_FENCE_RIGHT) ? TRI_FENCE_LEFT : 0));
        for (int s = 0; s < 10; s++) seg[0x0e + s] = from[0x0e + (s + 5) % 10];
        for (uint32_t r = 0; r < TRI_NODES_PER_SEGMENT; r++) {
            const uint32_t old = (ring - (t * TRI_NODES_PER_SEGMENT + r) % ring) % ring;
            const uint8_t *row = in + tri.terrain_offset + (size_t)TRI_SEGMENT_SIZE * (old / TRI_NODES_PER_SEGMENT) +
                                 ROW_OFFSET + (size_t)ROW_SIZE * (old % TRI_NODES_PER_SEGMENT);
            uint8_t *dst = seg + ROW_OFFSET + (size_t)ROW_SIZE * r;
            memcpy(dst, row, POINT_SIZE);
            memcpy(dst + POINT_SIZE, row + POINT_SIZE + CHAIN_BYTES, CHAIN_BYTES);
            memcpy(dst + POINT_SIZE + CHAIN_BYTES, row + POINT_SIZE, CHAIN_BYTES);
        }
        memcpy(out + BRIGHTNESS_OFFSET + (size_t)BRIGHTNESS_SIZE * t,
               in + BRIGHTNESS_OFFSET + (size_t)BRIGHTNESS_SIZE * (segments - 1 - t), BRIGHTNESS_SIZE);
    }
    turn_scenery(&tri, in, out, ring, pivots, count_pivots);
    return TRI_OK;
}

TriResult tri_reverse(const uint8_t *in, size_t size, uint8_t *out)
{
    return tri_reverse_with(in, size, out, NULL);
}
