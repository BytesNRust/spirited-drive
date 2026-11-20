#include "raster/car_scene.h"

#include "raster/flat.h"
#include "raster/object_mappers.h"
#include "raster/palette_match.h"
#include "raster/shine_pass.h"
#include "raster/view.h"

#include <stdlib.h>
#include <string.h>

enum { LEVEL_HIGH = 0, FOV = 45, NEAR_FLAG_Z = 0x219 /* model_draw.h's MODEL_NEAR_FLAG_Z */ };

static const char *const SECTION_NAMES[CAR_SCENE_SECTIONS] = { "inside", "surface", "outside", "convex" };
static const int32_t IDENTITY[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };

static void palette_of(CarScene *s)
{
    uint32_t index;
    FshEntry e;
    memset(s->palette, 0, sizeof s->palette);
    if (fsh_find(&s->bank, "!PAL", &index) == FSH_OK && fsh_entry(&s->bank, index, &e) == FSH_OK) fsh_palette_rgb(&e, s->palette);
    for (int i = 0; i < 256; i++) {
        s->quad[4 * i] = s->palette[3 * i + 2], s->quad[4 * i + 1] = s->palette[3 * i + 1], s->quad[4 * i + 2] = s->palette[3 * i];
        s->enabled[i] = 1;
    }
}

/* car_model.c's tyres(): the front and rear tyre refs draw the fourth wheel frame, the wheels at rest */
static void wheels_at_rest(Object3D *obj)
{
    const int32_t front = object3d_texture_named(obj, "frntires"), rear = object3d_texture_named(obj, "rertires"), frame = object3d_texture_named(obj, "tire4");
    if (frame < 0 || (uint32_t)frame >= obj->texture_count) return;
    const FshEntry *shape = obj->textures[frame].texture;
    if (front >= 0) object3d_set_texture(obj, front, shape);
    if (rear >= 0) object3d_set_texture(obj, rear, shape);
}

int car_scene_open(CarScene *s, const uint8_t *cfm, size_t size)
{
    memset(s, 0, sizeof *s);
    Chunk mesh_chunk, bank_chunk;
    if (chunk_open(&s->file, cfm, size) != CHUNK_OK) return 0;
    if (chunk_child(&s->file, 2 * LEVEL_HIGH, &mesh_chunk) != CHUNK_OK || mesh_open(&s->mesh, &mesh_chunk) != MESH_OK) return 0;
    s->has_bank = chunk_child(&s->file, 2 * LEVEL_HIGH + 1, &bank_chunk) == CHUNK_OK && bank_chunk.kind == CHUNK_KIND_BITMAP &&
                  fsh_open(&s->bank, bank_chunk.data, bank_chunk.size) == FSH_OK;
    if (!s->has_bank) return 0;
    palette_of(s);
    if (!object3d_create(&s->obj, &s->mesh)) return 0;
    const PaletteMatch match = { NULL, s->quad, s->enabled };
    object3d_match_colours(&s->obj, &match);
    object3d_bind(&s->obj, &s->bank);
    wheels_at_rest(&s->obj);
    for (int i = 0; i < CAR_SCENE_SECTIONS; i++) {
        const int section = object3d_section(&s->obj, SECTION_NAMES[i]);
        if (section >= 0 && section < 8 && !object3d_section_list(&s->obj, section, &s->sections[i])) {
            car_scene_close(s);
            return 0;
        }
    }
    int64_t far = 0;
    for (uint32_t v = 0; v < s->obj.vertex_count; v++) {
        const int64_t x = s->obj.model[v][0], y = s->obj.model[v][1], z = s->obj.model[v][2], r2 = x * x + y * y + z * z;
        if (r2 > far) far = r2;
    }
    int64_t r = 0;
    while ((r + 1) * (r + 1) <= far) r++;
    s->radius = (int32_t)r;
    return 1;
}

void car_scene_close(CarScene *s)
{
    for (int i = 0; i < CAR_SCENE_SECTIONS; i++) free(s->sections[i].order);
    object3d_free(&s->obj);
    memset(s, 0, sizeof *s);
}

typedef struct {
    QuadTarget target;
    const CarScene *scene;
    int flat;   /* the colour of every flat polygon and line, -1: the palette's nearest */
    float up[3];   /* the world's up in the camera's space, for the shine */
} Sink;

static void textured(void *user, Object3DTable table, int mode, const FshEntry *texture, const Object3DCorners *c)
{
    const Sink *s = user;
    const int slot = (int)table * 3 + mode;
    if (slot >= 15) return;
    const MapMode m = (MapMode)(slot % 3);
    switch ((ObjShape)(slot / 3)) {
    case OBJ_TRIANGLE: obj_draw_triangle(&s->target, texture, c->uv, c->screen, m, NULL); break;
    case OBJ_QUAD: obj_draw_quad(&s->target, texture, c->uv, c->screen, m, NULL); break;
    case OBJ_QUAD_TRIS: obj_draw_quad_tris(&s->target, texture, c->uv, c->screen, m, NULL); break;
    case OBJ_FULL: obj_draw_full(&s->target, texture, c->screen, m, NULL); break;
    case OBJ_INSET: obj_draw_inset(&s->target, texture, c->screen, m, NULL); break;
    }
    if (s->scene->shine != NULL) shine_pass_affine(&s->target, (ObjShape)(slot / 3), texture, c, s->scene->shine, s->up);   /* the envmap's pass, as the race runs it after each textured polygon */
}

/* 004a3390: two corners a line, three or four a flat polygon, in the palette's colour nearest the polygon's */
static void flat(void *user, uint32_t rgb, const Object3DCorners *c)
{
    const Sink *s = user;
    const uint32_t colour = s->flat >= 0 ? (uint32_t)s->flat : (uint32_t)palette_match_nearest(s->scene->quad, s->scene->enabled, rgb);
    if (c->corners == 2) flat_line(&s->target, c->screen[0], c->screen[1], c->screen[2], c->screen[3], colour);
    else if (c->corners == 3 || c->corners == 4) flat_polygon(&s->target, colour, c->corners, c->screen);
}

static void render(CarScene *s, const Orbit *o, uint8_t *pixels, int w, int h, int flat_colour)
{
    if (w < 2 || h < 2 || h > QUAD_MAX_ROWS) return;
    /* the field of view across the smaller side, and square pixels: view_project scales both axes by the horizontal focal, so a view of a
     * square of that side, centred in the whole picture, shows more at the sides of a wide one (or above and below a tall one), never less */
    View view;
    const int side = w < h ? w : h;
    view_init(&view, 0, 0, side, side, FOV, FOV);
    view.centre_x = w / 2, view.centre_y = h / 2;
    int32_t matrix[9];
    orbit_matrix(o, matrix);
    const int32_t eye[3] = { 0, 0, (int32_t)(0u - (uint32_t)o->dist) };   /* the car `dist` in front: 004591d4's eye is the car's place negated */
    if (o->dist < NEAR_FLAG_Z) s->obj.flags |= OBJECT3D_FLAG_NEAR;
    else s->obj.flags &= ~(uint32_t)OBJECT3D_FLAG_NEAR;
    object3d_set_matrix(&s->obj, matrix);
    object3d_transform(&s->obj, IDENTITY, eye, 0);
    object3d_update_normals(&s->obj);   /* none kept unless the shine is on */
    object3d_project(&s->obj, &view);
    Sink sink = { { pixels, w, 0, 0, w, h }, s, flat_colour, { 0, 1, 0 } };
    if (s->shine != NULL) shine_world_up(matrix, sink.up);   /* the car stands on the world's ground: its up is the world's */
    const Object3DSink out = { .user = &sink, .textured = textured, .flat = flat };
    for (int i = 0; i < CAR_SCENE_SECTIONS; i++) {
        if (s->sections[i].order == NULL) continue;
        object3d_sort_list(&s->obj, &s->sections[i]);
        object3d_draw_list(&s->obj, &s->sections[i], CAR_SCENE_NEAR_Z, &out);
    }
}

int car_scene_shine(CarScene *s, const EnvShine *env)
{
    if (env != NULL && s->obj.normal == NULL && !object3d_enable_normals(&s->obj)) env = NULL;
    s->shine = env;
    return env != NULL;
}

void car_scene_render(CarScene *s, const Orbit *o, uint8_t *pixels, int w, int h)
{
    render(s, o, pixels, w, h, -1);
}

void car_scene_render_layer(CarScene *s, const Orbit *o, uint8_t *pixels, int w, int h, const FshEntry *const *layer, uint8_t flat)
{
    const uint32_t n = s->obj.texture_count;
    const FshEntry **own = n > 0 ? malloc(n * sizeof *own) : NULL;
    if (n > 0 && own == NULL) return;
    for (uint32_t i = 0; i < n; i++) {
        own[i] = s->obj.textures[i].texture;
        if (layer != NULL && layer[i] != NULL) s->obj.textures[i].texture = layer[i];
    }
    render(s, o, pixels, w, h, flat);
    for (uint32_t i = 0; i < n; i++) s->obj.textures[i].texture = own[i];
    free(own);
}

void car_scene_rgba(const CarScene *s, const uint8_t *pixels, size_t n, uint8_t *rgba)
{
    for (size_t i = 0; i < n; i++) {
        const uint8_t p = pixels[i];
        rgba[4 * i] = s->palette[3 * p], rgba[4 * i + 1] = s->palette[3 * p + 1], rgba[4 * i + 2] = s->palette[3 * p + 2];
        rgba[4 * i + 3] = p == CAR_SCENE_CLEAR ? 0 : 255;
    }
}
