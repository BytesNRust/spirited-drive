#include "game/race/track_objects.h"

#include "game/sys/fixed_math.h"

enum {
    OFFSET_SHIFT = 8,     /* record offsets are * 0x100 */
    QUARTER_TURN = 0x400000
};

/* The descriptor's 32-bit fields at +4, +8 and +0xc, as 0043eca4 reads them. */
static int32_t width_of(const TriDescriptor *d)
{
    return (int32_t)((uint32_t)(uint16_t)d->width << 16 | d->unknown_04);
}

static int32_t depth_of(const TriDescriptor *d)
{
    return (int32_t)((uint32_t)d->unknown_0a << 16 | d->unknown_08);
}

static int32_t height_of(const TriDescriptor *d)
{
    return (int32_t)((uint32_t)(uint16_t)d->height << 16 | d->unknown_0c);
}

static TrackPoint add(TrackPoint p, int32_t x, int32_t y, int32_t z)
{
    return (TrackPoint){ p.x + x, p.y + y, p.z + z };
}

/* 0043eca4, an object standing up: a quad of the descriptor's width and
 * height turned by -(object angle + node heading); a two-sided one adds a
 * face of the descriptor's depth at a quarter turn from the last corner. */
static void place_billboard(TrackObject *o, const TriScenery *s, int16_t node_heading)
{
    const int32_t angle = -(int32_t)s->orientation * 0x10000 - node_heading * 0x400;
    const uint32_t a10 = (uint32_t)(angle >> 14);
    const int32_t half = width_of(&o->descriptor) / 2 >> 8;
    const int32_t dx = half * (fx_cos(a10) >> 8), dz = half * (fx_sin(a10) >> 8);
    const int32_t h = height_of(&o->descriptor);
    const TrackPoint b = o->origin;
    o->corners[0] = add(b, -dx, h, -dz);
    o->corners[1] = add(b, dx, h, dz);
    o->corners[2] = add(b, dx, 0, dz);
    o->corners[3] = add(b, -dx, 0, -dz);
    o->corner_count = 4;
    if (o->descriptor.type != TRACK_OBJECT_TWO_SIDED) return;
    const uint32_t side = (uint32_t)((angle + QUARTER_TURN) >> 14);
    const int32_t depth = depth_of(&o->descriptor) >> 8;
    const TrackPoint far = add(o->corners[3], depth * (fx_cos(side) >> 8), 0, depth * (fx_sin(side) >> 8));
    o->corners[4] = add(far, 0, h, 0);
    o->corners[5] = far;
    o->corner_count = 6;
}

TriResult track_object(const Tri *tri, uint32_t index, TrackObject *out)
{
    TriScenery s;
    TriNode node;
    TriResult r = tri_scenery(tri, index, &s);
    if (r != TRI_OK) return r;
    *out = (TrackObject){ 0 };
    out->node = (uint32_t)s.node & (tri->ring_nodes - 1);
    if ((r = tri_descriptor(tri, s.descriptor, &out->descriptor)) != TRI_OK) return r;
    if ((r = tri_node(tri, out->node, &node)) != TRI_OK) return r;
    out->origin = (TrackPoint){ node.position.x + s.x * (1 << OFFSET_SHIFT), node.position.y + s.y * (1 << OFFSET_SHIFT),
                                node.position.z + s.z * (1 << OFFSET_SHIFT) };
    if (out->descriptor.type == TRACK_OBJECT_MODEL) {
        /* 0044a4b0: no negation for the 3D objects. */
        out->heading = ((uint32_t)s.orientation << 16) + (uint32_t)(node.orientation * 0x400);
    } else {
        place_billboard(out, &s, node.orientation);
    }
    return TRI_OK;
}
