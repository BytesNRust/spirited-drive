#include "viewer/object_sink.h"

#include "game/gfx/object_mappers.h"
#include "game/gfx/quad.h"

#include <string.h>

/* 00491a82 indexes five tables of three thunks back to back, so a mode past
 * 2 lands in the next table; past the last one the original crashes. */
static void textured(void *user, Object3DTable table, int mode, const FshEntry *texture, const Object3DCorners *c)
{
    const ObjectSink *s = user;
    const int slot = (int)table * 3 + mode;
    if (slot >= 15) return;
    const ObjShape shape = (ObjShape)(slot / 3);
    const MapMode m = (MapMode)(slot % 3);
    switch (shape) {
    case OBJ_TRIANGLE:
        obj_draw_triangle(s->target, texture, c->uv, c->screen, m, s->shade);
        break;
    case OBJ_QUAD:
        obj_draw_quad(s->target, texture, c->uv, c->screen, m, s->shade);
        break;
    case OBJ_QUAD_TRIS:
        obj_draw_quad_tris(s->target, texture, c->uv, c->screen, m, s->shade);
        break;
    case OBJ_FULL:
        obj_draw_full(s->target, texture, c->screen, m, s->shade);
        break;
    case OBJ_INSET:
        obj_draw_inset(s->target, texture, c->screen, m, s->shade);
        break;
    }
}

/* TODO(00473140): the game's own palette match. */
static uint8_t nearest(const uint8_t palette[768], uint32_t rgb)
{
    const int r = (int)(rgb & 0xFF), g = (int)((rgb >> 8) & 0xFF), b = (int)((rgb >> 16) & 0xFF);
    int best = 0, best_d = 1 << 30;
    for (int i = 0; i < 256; i++) {
        const int dr = palette[3 * i] - r, dg = palette[3 * i + 1] - g, db = palette[3 * i + 2] - b;
        const int d = dr * dr + dg * dg + db * db;
        if (d < best_d) best = i, best_d = d;
    }
    return (uint8_t)best;
}

/* TODO(0048434e, 00484254): flat polygons and lines, here a one-texel
 * texture through the triangle mapper. */
static void flat(void *user, uint32_t rgb, const Object3DCorners *c)
{
    const ObjectSink *s = user;
    if (c->corners < 3) return;
    uint8_t texel[4];
    memset(texel, nearest(s->palette, rgb), sizeof texel);
    FshEntry tex;
    memset(&tex, 0, sizeof tex);
    tex.type = FSH_TYPE_BITMAP;
    tex.width = tex.height = 2;
    tex.payload = texel;
    tex.payload_size = sizeof texel;
    Object3DCorners z = *c;
    memset(z.uv, 0, sizeof z.uv);
    obj_draw_triangle(s->target, &tex, z.uv, z.screen, MAP_OPAQUE, NULL);
    if (c->corners == 4) {
        const int32_t scr[6] = { z.screen[0], z.screen[1], z.screen[4], z.screen[5], z.screen[6], z.screen[7] };
        obj_draw_triangle(s->target, &tex, z.uv, scr, MAP_OPAQUE, NULL);
    }
}

/* TODO(004c6604): the game's shade table; here each colour's nearest match
 * at half brightness. */
void object_sink_shade(const uint8_t palette[768], uint8_t shade[256])
{
    for (int i = 0; i < 256; i++) {
        const uint32_t rgb = (uint32_t)(palette[3 * i] / 2) | (uint32_t)(palette[3 * i + 1] / 2) << 8 |
                             (uint32_t)(palette[3 * i + 2] / 2) << 16;
        shade[i] = nearest(palette, rgb);
    }
}

Object3DSink object_sink(ObjectSink *s)
{
    return (Object3DSink){ s, textured, flat };
}
