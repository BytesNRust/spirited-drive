#include "game/race/portal_faces.h"

#include "formats/tri.h"
#include "game/race/light_synth.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    MODEL_TYPE = 1,
    HALF_TURN = 128,            /* the placement's angle is a byte, 256 a turn */
    ANGLE_MASK = 0x3fff,        /* a node's heading, 0x4000 a turn */
    MODEL_TO_OFFSET = 16,       /* a model unit is 1/16 m, an offset's 1/4096 of a 16.16 m: 256 of them */
    POSITION_PER_OFFSET = 256,
    X_AT = 0x0a,
    Z_AT = 0x0e,
    MAX_ADDED = 64,
    REACH = 2                   /* nodes from a tunnel's end a portal is taken to be at */
};

typedef struct {
    uint8_t record[TRI_SCENERY_SIZE];
    uint32_t node;              /* where it goes */
} Copy;

static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static int16_t get16s(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static void put16(uint8_t *p, int16_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)((uint16_t)v >> 8); }

static int16_t clamp16(double v)
{
    const long r = lround(v);
    return (int16_t)(r < -32768 ? -32768 : r > 32767 ? 32767 : r);
}

/* [c -s; s c] over (x, z) by `turns` of a turn, as the models' own vertices are turned */
static void turn(double turns, double x, double z, double out[2])
{
    const double a = turns * 2 * 3.14159265358979323846, c = cos(a), s = sin(a);
    out[0] = c * x - s * z, out[1] = s * x + c * z;
}

static double heading(uint8_t orientation, int16_t node_angle)
{
    return ((double)orientation * 0x10000 + (double)(node_angle & ANGLE_MASK) * 0x400) / 16777216.0;
}

/* the record `src` of node `from`, as it stands at node `to` facing the other way, its model's centre keeping its place across the road */
static void face_copy(const uint8_t *src, const TriNode *from, const TriNode *to, const TriPivot *pivot, uint8_t out[TRI_SCENERY_SIZE])
{
    memcpy(out, src, TRI_SCENERY_SIZE);
    out[5] = (uint8_t)(src[5] + HALF_TURN);
    const double px = pivot != NULL ? pivot->x : 0, pz = pivot != NULL ? pivot->z : 0;
    double c[2], d[2], r[2];
    turn(heading(src[5], from->orientation), px, pz, c);
    c[0] = get16s(src + X_AT) + MODEL_TO_OFFSET * c[0], c[1] = get16s(src + Z_AT) + MODEL_TO_OFFSET * c[1];   /* the centre from the node, in offset units */
    turn((double)((to->orientation - from->orientation) & ANGLE_MASK) / 0x4000, c[0], c[1], d);              /* across the road, the same */
    turn(heading(out[5], to->orientation), px, pz, r);
    put16(out + X_AT, clamp16(d[0] - MODEL_TO_OFFSET * r[0]));
    put16(out + Z_AT, clamp16(d[1] - MODEL_TO_OFFSET * r[1]));
}

static int on_node(const Tri *tri, uint32_t index, uint32_t node)
{
    return (get32(tri->data + tri->scenery_offset + (size_t)index * TRI_SCENERY_SIZE) & (tri->ring_nodes - 1)) == node;
}

static int is_model(const Tri *tri, uint8_t descriptor)
{
    TriDescriptor d;
    return tri_descriptor(tri, descriptor, &d) == TRI_OK && d.type == MODEL_TYPE;
}

/* a copy of the model records on `from` onto `to`, unless `to` already has one of that descriptor */
static uint32_t collect(const Tri *tri, uint32_t from, uint32_t to, const TriPivot *pivots, uint32_t pivot_count, Copy *list, uint32_t n)
{
    TriNode a, b;
    if (tri_node(tri, from, &a) != TRI_OK || tri_node(tri, to, &b) != TRI_OK) return n;
    for (uint32_t i = 0; i < tri->scenery_count && n < MAX_ADDED; i++) {
        const uint8_t *rec = tri->data + tri->scenery_offset + (size_t)i * TRI_SCENERY_SIZE;
        if (!on_node(tri, i, from) || !is_model(tri, rec[4])) continue;
        int there = 0;
        for (uint32_t k = 0; k < tri->scenery_count; k++)
            there |= on_node(tri, k, to) && tri->data[tri->scenery_offset + (size_t)k * TRI_SCENERY_SIZE + 4] == rec[4];
        for (uint32_t k = 0; k < n; k++) there |= list[k].node == to && list[k].record[4] == rec[4];
        if (there) continue;
        TriDescriptor d;
        const TriPivot *pivot = tri_descriptor(tri, rec[4], &d) == TRI_OK && pivots != NULL && d.resource_id < pivot_count ? &pivots[d.resource_id] : NULL;
        face_copy(rec, &a, &b, pivot, list[n].record);
        list[n++].node = to;
    }
    return n;
}

static int by_node(const void *a, const void *b)
{
    const Copy *x = a, *y = b;
    return x->node < y->node ? -1 : x->node > y->node;
}

/* the records of `list` into the file's, each after the records on its node or before the first of a later one */
static void insert(uint8_t *file, const Tri *tri, Copy *list, uint32_t n)
{
    uint8_t *base = file + tri->scenery_offset;
    qsort(list, n, sizeof list[0], by_node);
    uint32_t count = tri->scenery_count;
    for (uint32_t k = 0; k < n; k++) {
        uint32_t at = 0;
        while (at < count && (int32_t)get32(base + (size_t)at * TRI_SCENERY_SIZE) <= (int32_t)list[k].node) at++;
        memmove(base + (size_t)(at + 1) * TRI_SCENERY_SIZE, base + (size_t)at * TRI_SCENERY_SIZE, (size_t)(count + 1 - at) * TRI_SCENERY_SIZE);   /* with the -1 */
        memcpy(base + (size_t)at * TRI_SCENERY_SIZE, list[k].record, TRI_SCENERY_SIZE);
        put32(base + (size_t)at * TRI_SCENERY_SIZE, list[k].node);
        count++;
    }
}

uint32_t portal_faces_add(uint8_t *file, size_t size, const TriPivot *pivots, uint32_t pivot_count)
{
    Tri tri;
    if (tri_open(&tri, file, size) != TRI_OK || tri.node_count == 0) return 0;
    uint8_t *property = malloc(tri.node_count), *tunnel = malloc(tri.node_count);
    Copy list[MAX_ADDED];
    uint32_t n = 0;
    if (property == NULL || tunnel == NULL) goto done;
    for (uint32_t i = 0; i < tri.node_count; i++) {
        TriNode node;
        property[i] = tri_node(&tri, i, &node) == TRI_OK ? node.property : 0;
    }
    const int ring = tri_is_circuit(&tri);
    light_synth_tunnels(property, tri.node_count, ring, tunnel);
    for (uint32_t i = 0; i < tri.node_count; i++) {
        if (!tunnel[i]) continue;
        const int has_before = ring || i > 0;
        const uint32_t before = (i + tri.node_count - 1) % tri.node_count;
        if (has_before && !tunnel[before]) {   /* a run's first node: its mouth is the node before; the run's last node gives the other */
            uint32_t last = i;
            while (tunnel[(last + 1) % tri.node_count] && (last + 1) % tri.node_count != i) last++;
            for (uint32_t k = 1; k <= REACH; k++) {   /* a portal stands a node or two off the run (a reversed circuit's lands one further) */
                const uint32_t in = (i + tri.node_count * REACH - k) % tri.node_count, out = (last + k) % tri.node_count;
                if (!ring && (i < k || last + k >= tri.node_count)) continue;
                n = collect(&tri, in, out, pivots, pivot_count, list, n);
                n = collect(&tri, out, in, pivots, pivot_count, list, n);
            }
        }
    }
    if (n > 0 && tri.scenery_count + n < TRI_SCENERY_MAX) insert(file, &tri, list, n);
    else n = 0;
done:
    free(property);
    free(tunnel);
    return n;
}
