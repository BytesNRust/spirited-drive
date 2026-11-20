#include "viewer/object_sink.h"

#include "game/gfx/flat.h"
#include "game/gfx/object_mappers.h"
#include "game/gfx/palette_match.h"
#include "game/gfx/quad.h"
#include "raster/shine_pass.h"
#include "game/sys/palette.h"
#include "viewer/modern_road.h"
#include "viewer/shine_class.h"

#include <string.h>

/* 00491a82 indexes five tables of three thunks back to back, so a mode past
 * 2 lands in the next table; past the last one the original crashes. */
/* port (the light effects' light): while set, the textures and flat colours of the objects drawn go through this 256-byte
 * table, the shadow's shade mode (2) excepted */
static const uint8_t *light_table;

void object_sink_set_light(const uint8_t *table) { light_table = table; }

static uint8_t lit_colour(uint8_t colour) { return light_table != NULL ? light_table[colour] : colour; }
static MapMode lit_mode(MapMode m) { return light_table == NULL || m == MAP_SHADE ? m : m == MAP_TRANSPARENT ? MAP_REMAP_TRANSPARENT : MAP_REMAP; }
static const uint8_t *lit_shade(MapMode m, const uint8_t *shade) { return light_table == NULL || m == MAP_SHADE ? shade : light_table; }

/* port (body shine): the world's up (in camera space; NULL: the camera's own y) of the car being drawn, the horizon the envmap's sphere map is
 * looked up against */
static float shine_up[3] = { 0, 1, 0 };

void object_sink_set_shine_up(const float *up)
{
    for (int k = 0; k < 3; k++) shine_up[k] = up != NULL ? up[k] : (k == 1 ? 1.0f : 0.0f);
}

/* port (body_shine 4): the envmap pass's tables and image, NULL for none (kept by pointer until the frame is flushed) */
static const EnvShine *env_shine;

void object_sink_set_envmap(const EnvShine *env) { env_shine = env; }

static void envmap_exe(const ObjectSink *s, ObjShape shape, const FshEntry *texture, const Object3DCorners *c);

/* port (body_shine 4): each corner's sphere-map uv of the envmap (car_envmap.h) from its normal and the line of sight, packed; 0 when the
 * polygon carries no normals */
static void textured(void *user, Object3DTable table, int mode, const FshEntry *texture, const Object3DCorners *c)
{
    const ObjectSink *s = user;
    const int slot = (int)table * 3 + mode;
    if (slot >= 15) return;
    const ObjShape shape = (ObjShape)(slot / 3);
    const MapMode base = (MapMode)(slot % 3);
    const MapMode m = lit_mode(base);
    const uint8_t *shade = lit_shade(base, s->shade);
    switch (shape) {
    case OBJ_TRIANGLE:
        obj_draw_triangle(s->target, texture, c->uv, c->screen, m, shade);
        break;
    case OBJ_QUAD:
        obj_draw_quad(s->target, texture, c->uv, c->screen, m, shade);
        break;
    case OBJ_QUAD_TRIS:
        obj_draw_quad_tris(s->target, texture, c->uv, c->screen, m, shade);
        break;
    case OBJ_FULL:
        obj_draw_full(s->target, texture, c->screen, m, shade);
        break;
    case OBJ_INSET:
        obj_draw_inset(s->target, texture, c->screen, m, shade);
        break;
    }
    envmap_exe(s, shape, texture, c);
}

/* 00473140 on an 8-bit screen without the inverse table: the nearest of
 * the palette's colours, all of them enabled. Its entries are 0052f178's,
 * B, G, R (00476460), so a packed colour's first byte is blue. */
static uint8_t nearest(const uint8_t palette[768], uint32_t rgb)
{
    uint8_t quad[PALETTE_MATCH_ENTRIES * PALETTE_MATCH_ENTRY_BYTES] = { 0 };
    uint8_t enabled[PALETTE_MATCH_ENTRIES];
    const uint8_t *hidden = palette_world_hidden();   /* port (car colours): not the slots the fit gave the cars */
    for (int i = 0; i < PALETTE_MATCH_ENTRIES; i++) {
        uint8_t *const q = &quad[i * PALETTE_MATCH_ENTRY_BYTES];
        q[0] = palette[3 * i + 2], q[1] = palette[3 * i + 1], q[2] = palette[3 * i];
        enabled[i] = i >= PALETTE_COLORS || !hidden[i];
    }
    return (uint8_t)palette_match_nearest(quad, enabled, rgb);
}

/* 004a3390: two corners a line (00484254), three or four a flat polygon
 * (0048434e). */
static void flat(void *user, uint32_t rgb, const Object3DCorners *c)
{
    const ObjectSink *s = user;
    const uint8_t colour = lit_colour(nearest(s->palette, rgb));
    if (c->corners == 2) {
        flat_line(s->target, c->screen[0], c->screen[1], c->screen[2], c->screen[3], colour);
    } else if (c->corners == 3 || c->corners == 4) {
        flat_polygon(s->target, colour, c->corners, c->screen);
    }
}

/* port (body_shine 4): the envmap's pass on the exe's path, over the polygon and uvs the mapper drew (raster/shine_pass.h) */
static void envmap_exe(const ObjectSink *s, ObjShape shape, const FshEntry *texture, const Object3DCorners *c)
{
    shine_pass_affine(s->target, shape, texture, c, env_shine, shine_up);
}

static void textured_camera(void *user, Object3DTable table, int mode, const FshEntry *texture, const Object3DCorners *c)
{
    const ObjectSink *s = user;
    const int slot = (int)table * 3 + mode;
    if (slot >= 15 || texture->type != FSH_TYPE_BITMAP) return;
    int32_t uv[8];
    if (!shine_shape_uv((ObjShape)(slot / 3), texture, c, uv)) return;
    const int32_t *const cam[4] = { &c->camera[0], &c->camera[3], &c->camera[6], &c->camera[9] };
    const MapMode base = (MapMode)(slot % 3);
    modern_polygon(s->target, s->view, texture, uv, cam, c->corners, lit_mode(base), lit_shade(base, s->shade));
    const ShineClass *cls;
    int32_t packed[4];
    if (env_shine != NULL && (cls = shine_class_find(texture->payload)) != NULL && shine_corner_uvs(c, shine_up, packed)) {   /* port (body_shine 4) */
        shine_draw_set_view(s->view);
        shine_draw_env_camera(s->target, cls, env_shine, uv, cam, packed, c->corners);
    }
}

static void flat_camera(void *user, uint32_t rgb, const Object3DCorners *c)
{
    const ObjectSink *s = user;
    const int32_t *const cam[4] = { &c->camera[0], &c->camera[3], &c->camera[6], &c->camera[9] };
    modern_flat(s->target, s->view, lit_colour(nearest(s->palette, rgb)), cam, c->corners);
}

/* port: a stand-in for 004c6604 where no race built it (the track viewer); a
 * race gives hud_colours' table (ported_view_set_shadows). Here each colour's
 * nearest match at half brightness. */
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
    if (s->view != NULL) return (Object3DSink){ s, textured, flat, textured_camera, flat_camera };
    return (Object3DSink){ .user = s, .textured = textured, .flat = flat };
}
