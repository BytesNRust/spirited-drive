#include "game/race/track_build.h"

#include "game/gfx/x86_int.h"
#include "game/race/track_cache.h"
#include "game/race/track_tables.h"
#include "game/sys/fixed_math.h"

enum {
    ROW_POINTS = 11,
    MODEL = 1,           /* descriptor +1: one point */
    TWO_SIDED = 6,       /* two corners more */
    QUARTER_TURN = 0x400000
};

static int32_t rd32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

static int32_t rd16(const uint8_t *p)
{
    return (int16_t)(p[0] | p[1] << 8);
}

static void put_object(TrackBuild *b, int32_t x, int32_t y, int32_t z)
{
    const int32_t i = b->walk.vertex_total;
    if (i >= 0 && i < b->object_capacity) {  /* port guard */
        b->object_world[i][0] = x;
        b->object_world[i][1] = y;
        b->object_world[i][2] = z;
    }
    b->walk.vertex_total = i + 1;
}

/* The record under the walker on its node `node`. */
static void place_object(TrackBuild *b, int32_t node)
{
    TrackObjectWalk *w = &b->walk;
    const uint8_t *r = w->records + (uint32_t)w->cursor * TRACK_WALK_RECORD_SIZE;
    const uint8_t *n = b->nodes + ((uint32_t)node & w->node_mask) * TRACK_BUILD_NODE_SIZE;
    const uint8_t *d = w->descriptors + (uint32_t)r[4] * TRACK_WALK_RECORD_SIZE;
    int32_t pos[3];
    for (int k = 0; k < 3; k++) pos[k] = x_add(rd32(n + 8 + 4 * k), x_shl(rd16(r + 0xa + 2 * k), 8));
    if (d[1] == MODEL) {
        put_object(b, pos[0], pos[1], pos[2]);
        w->vertex_count++;
        return;
    }
    const int32_t spin = x_shl(-(int32_t)r[5], 16);
    const int32_t heading = x_shl(rd16(n + 0x18), 10);
    int32_t turn = x_sub(spin, heading);
    const int32_t half = (rd32(d + 4) / 2) >> 8;
    const int32_t hx = x_mul(half, fx_cos((uint32_t)(turn >> 14)) >> 8);
    const int32_t hz = x_mul(half, fx_sin((uint32_t)(turn >> 14)) >> 8);
    int32_t up = rd32(d + 0xc), ex = 0, ez = 0;
    if (b->knocked[(uint32_t)w->cursor * 2] != 0) {
        /* knocked over: flat, the height laid along the heading a quarter turn on */
        const int32_t lie = spin < -QUARTER_TURN ? x_add(spin, 2 * QUARTER_TURN) : spin;
        turn = x_sub(x_add(lie, QUARTER_TURN), heading);
        const int32_t e = up >> 8;
        ex = x_mul(fx_cos((uint32_t)(turn >> 14)) >> 8, e);
        ez = x_mul(e, fx_sin((uint32_t)(turn >> 14)) >> 8);
        up = 0;
    }
    const int32_t top = x_add(pos[1], up);
    put_object(b, x_add(x_sub(pos[0], hx), ex), top, x_add(x_sub(pos[2], hz), ez));
    put_object(b, x_add(x_add(pos[0], hx), ex), top, x_add(x_add(pos[2], hz), ez));
    put_object(b, x_add(pos[0], hx), pos[1], x_add(pos[2], hz));
    put_object(b, x_sub(pos[0], hx), pos[1], x_sub(pos[2], hz));
    w->vertex_count += 4;
    if (d[1] != TWO_SIDED) return;
    const int32_t side = x_add(turn, QUARTER_TURN) >> 14;
    const int32_t e = rd32(d + 8) >> 8;
    const int32_t bx = x_mul(e, fx_cos((uint32_t)side) >> 8), bz = x_mul(e, fx_sin((uint32_t)side) >> 8);
    /* from the third corner, as the original reads it back */
    const int32_t x = x_add(pos[0], hx), z = x_add(pos[2], hz);
    put_object(b, x_add(x, bx), x_add(pos[1], up), x_add(z, bz));
    put_object(b, x_add(x, bx), pos[1], x_add(z, bz));
    w->vertex_count += 2;
}

/* The walker one record on; past the last, record 0 on a circuit unless it
 * is on this same node, else none. */
static void next_object(TrackObjectWalk *w, int32_t node)
{
    w->count++;
    if (++w->cursor == w->record_count) {
        w->cursor = 0;
        w->node = w->circuit != 0 ? rd32(w->records) : -1;
        if (w->node == node) w->node = -1;
    } else {
        w->node = rd32(w->records + (uint32_t)w->cursor * TRACK_WALK_RECORD_SIZE);
    }
}

enum { EXE_SEGMENTS = 0x1c };  /* 00504220's top: 004bba64's last far column is EXE_SEGMENTS - 1 */

static int lod_of(const TrackBuild *b, int32_t first, int32_t segment, int32_t node)
{
    const uint8_t property = b->nodes[(uint32_t)node * TRACK_BUILD_NODE_SIZE + 7];
    int32_t distance = x_sub(segment, first);
    /* port: "ultra" builds past 004bba64's 28 columns: the far ones take its last far LOD (27), the single row at
     * the end the column the exe gives its own (28) */
    if (b->reach > EXE_SEGMENTS && distance >= EXE_SEGMENTS) distance = distance >= b->reach ? EXE_SEGMENTS : EXE_SEGMENTS - 1;
    if (property >= TRACK_PROPERTIES || distance < 0 || distance >= TRACK_LOD_DISTANCES) return 0;  /* port guard */
    return TRACK_LOD_BY_DISTANCE[property][distance];
}

/* 0043eca4 */
void track_build_segment(TrackBuild *b, int32_t first, int32_t segment, int32_t rows, int objects)
{
    TrackObjectWalk *w = &b->walk;
    int32_t node = (int32_t)((uint32_t)segment << 2 & w->node_mask);
    const int32_t start = b->world_next;
    const uint8_t *cache = b->cache + ((uint32_t)segment & b->segment_mask) * TRACK_BUILD_SEGMENT_SIZE % (uint32_t)track_cache_size();
    const int lod = lod_of(b, first, segment, node);
    const uint8_t *select = TRACK_LODS[lod].select;
    const uint8_t *end = select + TRACK_LOD_SELECT;
    for (int32_t row = 0; row < rows; row++) {
        const uint8_t *points = cache + 0xc + row * TRACK_BUILD_ROW_SIZE;
        for (int point = 0; select < end && *select != 0xFF && point < ROW_POINTS; point++) {
            if (*select != point) continue;
            if (b->world_next >= 0 && b->world_next < b->world_capacity)  /* port guard */
                for (int k = 0; k < 3; k++) b->world[b->world_next][k] = rd32(points + 12 * point + 4 * k);
            if (b->light_out != NULL && b->world_next >= 0 && b->world_next < b->world_capacity)
                b->light_out[b->world_next] = psx_light_at(b->light, (uint32_t)node, point);   /* port: the PS1's light */
            if (b->tunnel_out != NULL && b->world_next >= 0 && b->world_next < b->world_capacity)
                b->tunnel_out[b->world_next] = (uint8_t)psx_light_tunnel(b->light, (uint32_t)node);
            b->world_next++;
            select++;
        }
        while (select < end && *select != 0xFF) select++;  /* port guard: the tables end each row at its last point */
        select++;
        if (objects)
            while (node == w->node) {
                if (w->cursor < 0 || w->cursor >= w->record_count) break;  /* port guard */
                place_object(b, w->node);
                next_object(w, node);
            }
        node = (int32_t)((uint32_t)(node + 1) & w->node_mask);
    }
    TrackSegmentEntry *e = &b->segments[(uint32_t)segment & b->segment_mask];
    e->lod = (uint8_t)lod;
    e->first = (int16_t)start;
    e->count = (int16_t)(b->world_next - start);
    b->vertex_count = x_add(b->vertex_count, e->count);
}

/* 0043f48c */
void track_build(TrackBuild *b, int32_t node, int32_t segment, int32_t count)
{
    const int32_t first = node / 4;
    b->first_node = node;
    b->first_segment = first;
    b->world_next = 0;
    b->vertex_count = 0;
    track_objects_start(&b->walk, (int32_t)(b->segment_mask & (uint32_t)first));
    const int32_t with_objects = x_sub(count, 1);
    for (int32_t i = 0; i < with_objects; i++, segment = x_add(segment, 1))
        track_build_segment(b, first, segment, 4, 1);
    track_build_segment(b, first, segment, 4, 0);
    track_build_segment(b, first, x_add(segment, 1), 1, 0);
}
