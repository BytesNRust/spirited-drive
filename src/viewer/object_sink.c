#include "viewer/object_sink.h"

#include "game/gfx/quad.h"

#include <string.h>

/* Mode 1 of every table is the colour 0xFF transparent mapper.
 * TODO(004aa510, 004aa600, 004ab060, 004aaf14): mode 2, drawn as mode 1. */
static int transparent(int mode)
{
    return mode != 0;
}

static void triangle(const QuadTarget *t, const FshEntry *tex, const Object3DCorners *c, int a, int b, int d,
                     int mode)
{
    const int32_t uv[6] = { c->uv[2 * a], c->uv[2 * a + 1], c->uv[2 * b], c->uv[2 * b + 1], c->uv[2 * d],
                            c->uv[2 * d + 1] };
    const int32_t scr[6] = { c->screen[2 * a], c->screen[2 * a + 1], c->screen[2 * b], c->screen[2 * b + 1],
                             c->screen[2 * d], c->screen[2 * d + 1] };
    tri_draw(t, tex, uv, scr, transparent(mode));
}

static void textured(void *user, Object3DTable table, int mode, const FshEntry *texture, const Object3DCorners *c)
{
    const ObjectSink *s = user;
    switch (table) {
    case OBJECT3D_TABLE_TRIANGLE:  /* 00486400 / 004858f0 */
        triangle(s->target, texture, c, 0, 1, 2, mode);
        break;
    case OBJECT3D_TABLE_QUAD_UV: { /* 00487180 / 00486950 */
        /* TODO(0052f124): the uv units the table entries expect. */
        int32_t uv[8];
        for (int i = 0; i < 8; i++) uv[i] = c->uv[i] * 65536;
        quad_draw(s->target, texture, uv, c->screen, transparent(mode));
        break;
    }
    case OBJECT3D_TABLE_QUAD_UV_TRIANGLES:
        triangle(s->target, texture, c, 0, 1, 2, mode);
        triangle(s->target, texture, c, 0, 2, 3, mode);
        break;
    case OBJECT3D_TABLE_QUAD_WHOLE:  /* 00487a10 / 004879b0 */
        quad_draw_full(s->target, texture, c->screen, transparent(mode));
        break;
    case OBJECT3D_TABLE_QUAD_WHOLE_TRIANGLES:  /* TODO(00486804, 004865d4) */
        quad_draw_full_tri(s->target, texture, c->screen, transparent(mode));
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
    triangle(s->target, &tex, &z, 0, 1, 2, 0);
    if (c->corners == 4) triangle(s->target, &tex, &z, 0, 2, 3, 0);
}

Object3DSink object_sink(ObjectSink *s)
{
    return (Object3DSink){ s, textured, flat };
}
