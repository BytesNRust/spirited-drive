#include "viewer/object_sink.h"

#include "game/gfx/flat.h"
#include "game/gfx/object_mappers.h"
#include "game/gfx/palette_match.h"
#include "game/gfx/quad.h"


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

/* 00473140 on an 8-bit screen without the inverse table: the nearest of
 * the palette's colours, all of them enabled. Its entries are 0052f178's,
 * B, G, R (00476460), so a packed colour's first byte is blue. */
static uint8_t nearest(const uint8_t palette[768], uint32_t rgb)
{
    uint8_t quad[PALETTE_MATCH_ENTRIES * PALETTE_MATCH_ENTRY_BYTES] = { 0 };
    uint8_t enabled[PALETTE_MATCH_ENTRIES];
    for (int i = 0; i < PALETTE_MATCH_ENTRIES; i++) {
        uint8_t *const q = &quad[i * PALETTE_MATCH_ENTRY_BYTES];
        q[0] = palette[3 * i + 2], q[1] = palette[3 * i + 1], q[2] = palette[3 * i];
        enabled[i] = 1;
    }
    return (uint8_t)palette_match_nearest(quad, enabled, rgb);
}

/* 004a3390: two corners a line (00484254), three or four a flat polygon
 * (0048434e). */
static void flat(void *user, uint32_t rgb, const Object3DCorners *c)
{
    const ObjectSink *s = user;
    const uint8_t colour = nearest(s->palette, rgb);
    if (c->corners == 2) {
        flat_line(s->target, c->screen[0], c->screen[1], c->screen[2], c->screen[3], colour);
    } else if (c->corners == 3 || c->corners == 4) {
        flat_polygon(s->target, colour, c->corners, c->screen);
    }
}

/* TODO(004c6604): the game's shade table; here each colour's nearest match
 * at half brightness. */
void object_sink_shade(const uint8_t palette[768], uint8_t shade[256])
{
    for (int i = 0; i < 256; i++) {
        const uint32_t rgb = (uint32_t)(palette[3 * i + 2] / 2) | (uint32_t)(palette[3 * i + 1] / 2) << 8 |
                             (uint32_t)(palette[3 * i] / 2) << 16;
        shade[i] = nearest(palette, rgb);
    }
}

Object3DSink object_sink(ObjectSink *s)
{
    return (Object3DSink){ s, textured, flat };
}
