/* The classic rasterizer pinned before it moves to the core (dep59 car editor, step 2a): a synthetic object, built with mesh_build, no CD data, drawn
 * through the object engine (object3d) into an 8-bit surface by its mappers (quad, object_mappers) and its flat fill (flat), and the surface's CRC-32
 * pinned. Whatever moves where, these pictures must not change a pixel. A: the whole object, depth sorted: textured quads and a triangle, opaque
 * (mode 0), colour 0xFF transparent (mode 1) and shadow (mode 2, colour 0xFE through a shade table), with their own uvs, the whole texture, and as two
 * triangles; flat polygons; a one-sided polygon turned away (culled), a two-sided one and a reversed one turned away (drawn). B: the sections in the
 * order the car is drawn, inside, surface, outside, each sorted on its own, nearest last inside a section but a nearer polygon of an earlier section
 * covered by a farther one of a later section. GOLDEN_DUMP=dir writes the pictures as PGMs. */
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "formats/mesh_build.h"
#include "game/gfx/flat.h"
#include "game/gfx/object3d.h"
#include "game/gfx/object_mappers.h"
#include "formats/fixed_math.h"
#include "game/race/view.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

enum { W = 160, H = 120, CLEAR = 0x20 };

static const uint32_t GOLDEN_WHOLE = 0xd13cdadfu, GOLDEN_SECTIONS = 0x5d992671u, GOLDEN_TURNED = 0xcf88f66eu;   /* set from the first run's print */

static uint32_t crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

/* ---- the textures: 8 x 8 8-bit bitmaps (FSH type 0x7B) ---- */
static uint8_t pix_opaque[64], pix_holes[64], pix_shadow[64];
static FshEntry tex[3];

static void make_textures(void)
{
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            pix_opaque[y * 8 + x] = (uint8_t)(0x40 + y * 8 + x);
            pix_holes[y * 8 + x] = (x + y) % 3 == 0 ? 0xFF : (uint8_t)(0x90 + x * 4 + y);
            pix_shadow[y * 8 + x] = x < 4 ? 0xFE : (uint8_t)(0xC0 + y);
        }
    const uint8_t *const pix[3] = { pix_opaque, pix_holes, pix_shadow };
    for (int i = 0; i < 3; i++)
        tex[i] = (FshEntry){ .type = FSH_TYPE_BITMAP, .width = 8, .height = 8, .payload = pix[i], .payload_size = 64 };
}

/* ---- the mesh: a model-space description ---- */
enum { U = 0x10000 };

typedef struct {
    MeshDesc d;
    MeshVertex v[64];
    MeshUv uv[4];
    MeshDescPolygon p[16];
    MeshTexture t[8];
    uint32_t inside[1], surface[1], outside[1];
    MeshDescGroup g[3];
} Desc;

static int nv, np;

/* a quad in the z = z plane, its corners clockwise from the top left on the screen, x0..x1 left to right, y0..y1 top to bottom; `back` winds it the other way */
static void quad(Desc *s, int x0, int y0, int x1, int y1, int z, uint8_t flags, uint8_t extra, uint8_t texture, int back, int uv)
{
    const int a = nv;
    const int cx[4] = { x0, x1, x1, x0 }, cy[4] = { y0, y0, y1, y1 };
    for (int i = 0; i < 4; i++) s->v[nv++] = (MeshVertex){ cx[i] * U / 4, -cy[i] * U / 4, z * U / 4 };
    MeshDescPolygon *p = &s->p[np++];
    *p = (MeshDescPolygon){ .flags = flags, .unknown_1 = extra, .texture = texture, .has_uv = (uint8_t)uv };
    for (int i = 0; i < 4; i++) p->vertex[i] = (uint32_t)(a + (back ? i : 3 - i)), p->uv[i] = (uint32_t)i;
}

static void triangle(Desc *s, int x0, int y0, int x1, int y1, int x2, int y2, uint8_t flags, uint8_t extra, uint8_t texture)
{
    const int a = nv;
    s->v[nv++] = (MeshVertex){ x0 * U / 4, -y0 * U / 4, 0 };
    s->v[nv++] = (MeshVertex){ x1 * U / 4, -y1 * U / 4, 0 };
    s->v[nv++] = (MeshVertex){ x2 * U / 4, -y2 * U / 4, 0 };
    MeshDescPolygon *p = &s->p[np++];
    *p = (MeshDescPolygon){ .flags = flags, .unknown_1 = extra, .texture = texture, .has_uv = 1 };
    for (int i = 0; i < 3; i++) p->vertex[i] = (uint32_t)(a + i), p->uv[i] = (uint32_t)i;
}

enum { TEXTURED = 0x80, MODE1 = 1 << 3, MODE2 = 2 << 3, OWN_UV = 0x10, TWO_SIDED = 0x01, REVERSED = 0x02, SPLIT = 0x20 };

static void describe(Desc *s)
{
    memset(s, 0, sizeof *s);
    nv = np = 0;
    s->uv[0] = (MeshUv){ 0, 0 }, s->uv[1] = (MeshUv){ 7, 0 }, s->uv[2] = (MeshUv){ 7, 7 }, s->uv[3] = (MeshUv){ 0, 7 };
    /* the screen is -8 .. 8 across and -6 .. 6 down in eighths of a unit: a 4 x 3 grid of 4-wide cells, z 0 (nearer is smaller z) */
    quad(s, -16, -12, -12, -8, 0, 4 | TEXTURED, OWN_UV, 0, 0, 1);               /* 0 opaque, own uv */
    quad(s, -11, -12, -7, -8, 0, 4 | TEXTURED | MODE1, OWN_UV, 1, 0, 1);        /* 1 colour 0xFF transparent */
    quad(s, -6, -12, -2, -8, 0, 4 | TEXTURED, 0, 0, 0, 0);                       /* 2 the whole texture */
    triangle(s, -1, -8, 3, -8, 1, -12, 3 | TEXTURED | OWN_UV, 0, 0);            /* 3 a triangle */
    quad(s, 4, -12, 8, -8, 0, 4 | TEXTURED, OWN_UV | SPLIT, 0, 0, 1);           /* 4 a quad as two triangles */
    quad(s, -16, -6, -12, -2, 0, 4, 0, 2, 0, 0);                                 /* 5 flat quad */
    triangle(s, -11, -2, -7, -2, -9, -6, 3, 0, 3);                               /* 6 flat triangle */
    quad(s, -6, -6, -2, -2, 0, 4, 0, 4, 1, 0);                                   /* 7 one-sided, turned away: culled */
    quad(s, -1, -6, 3, -2, 0, 4, TWO_SIDED, 5, 1, 0);                            /* 8 two-sided, turned away: drawn */
    quad(s, 4, -6, 8, -2, 0, 4, REVERSED, 6, 1, 0);                              /* 9 reversed, turned away: drawn */
    quad(s, -16, 2, -9, 9, -1, 4 | TEXTURED | MODE2, OWN_UV, 2, 0, 1);          /* 10 shadow (0xFE shaded) over 11 */
    quad(s, -12, 4, -4, 11, 0, 4, 0, 7, 0, 0);                                   /* 11 flat, behind 10 */
    /* three flat quads overlapping, drawn in section order whatever their depth: inside nearest, outside farthest */
    quad(s, 0, 3, 7, 9, -3, 4, 0, 4, 0, 0);                                      /* 12 inside, near */
    quad(s, 2, 4, 9, 10, 0, 4, 0, 5, 0, 0);                                      /* 13 surface */
    quad(s, 4, 5, 11, 11, 3, 4, 0, 6, 0, 0);                                     /* 14 outside, far */
    s->d.vertices = s->v, s->d.vertex_count = (uint32_t)nv;
    s->d.uvs = s->uv, s->d.uv_count = 4;
    s->d.polygons = s->p, s->d.polygon_count = (uint32_t)np;
    for (int i = 0; i < 8; i++) {
        s->t[i].unknown_1[0] = 0x00000000u | (uint32_t)(0x30 + 0x18 * i);   /* a flat polygon's colour: its low byte is the index drawn */
        memcpy(s->t[i].name, i < 3 ? "tex0" : "\0\0\0\0", 4), s->t[i].name[3] = i < 3 ? (char)('0' + i) : 0;
    }
    s->d.textures = s->t, s->d.texture_count = 8;
    s->inside[0] = 12, s->surface[0] = 13, s->outside[0] = 14;
    memcpy(s->g[0].name, "inside", 6), s->g[0].polygons = s->inside, s->g[0].count = 1;
    memcpy(s->g[1].name, "surface", 7), s->g[1].polygons = s->surface, s->g[1].count = 1;
    memcpy(s->g[2].name, "outside", 7), s->g[2].polygons = s->outside, s->g[2].count = 1;
    s->d.groups = s->g, s->d.group_count = 3;
}

/* ---- the sink: the game's, without its palette and lights ---- */
typedef struct {
    QuadTarget target;
    uint8_t shade[256];
} Sink;

static void sink_textured(void *user, Object3DTable table, int mode, const FshEntry *texture, const Object3DCorners *c)
{
    Sink *s = user;
    const int slot = (int)table * 3 + mode;
    if (slot >= 15) return;
    const MapMode m = (MapMode)(slot % 3);
    switch ((ObjShape)(slot / 3)) {
    case OBJ_TRIANGLE: obj_draw_triangle(&s->target, texture, c->uv, c->screen, m, s->shade); break;
    case OBJ_QUAD: obj_draw_quad(&s->target, texture, c->uv, c->screen, m, s->shade); break;
    case OBJ_QUAD_TRIS: obj_draw_quad_tris(&s->target, texture, c->uv, c->screen, m, s->shade); break;
    case OBJ_FULL: obj_draw_full(&s->target, texture, c->screen, m, s->shade); break;
    case OBJ_INSET: obj_draw_inset(&s->target, texture, c->screen, m, s->shade); break;
    }
}

static void sink_flat(void *user, uint32_t rgb, const Object3DCorners *c)
{
    Sink *s = user;
    if (c->corners == 2) flat_line(&s->target, c->screen[0], c->screen[1], c->screen[2], c->screen[3], rgb & 0xFF);
    else if (c->corners == 3 || c->corners == 4) flat_polygon(&s->target, rgb & 0xFF, c->corners, c->screen);
}

static void clear(uint8_t *surface, Sink *s)
{
    memset(surface, CLEAR, (size_t)W * H);
    s->target = (QuadTarget){ surface, W, 0, 0, W, H };
    for (int i = 0; i < 256; i++) s->shade[i] = (uint8_t)(i / 2 + 1);
}

static void dump(const char *name, const uint8_t *surface)
{
    const char *dir = getenv("GOLDEN_DUMP");
    if (dir == NULL) return;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.pgm", dir, name);
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;
    fprintf(f, "P5\n%d %d\n255\n", W, H);
    fwrite(surface, 1, (size_t)W * H, f);
    fclose(f);
}

static const int32_t IDENTITY[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };

typedef struct {
    Desc desc;
    uint8_t *chunk;
    size_t len;
    Chunk c;
    Mesh mesh;
    Object3D obj;
    View view;
    uint8_t surface[W * H];
    Sink sink;
} Scene;

/* the car's pose: turned about x then y from 10-bit angles (car_draw.c's turn_x, turn_y, body_matrix) */
static void turned(uint32_t yaw, uint32_t pitch, int32_t out[9])
{
    const int32_t sy = fx_sin(yaw), cy = fx_cos(yaw), sx = fx_sin(pitch), cx = fx_cos(pitch);
    const int32_t y[9] = { cy, 0, -sy, 0, 0x10000, 0, sy, 0, cy };
    const int32_t x[9] = { 0x10000, 0, 0, 0, cx, sx, 0, -sx, cx };
    mat3_mul(x, y, out);
}

static int open_scene(Scene *sc, const int32_t matrix[9])
{
    make_textures();
    describe(&sc->desc);
    ExchangeError err = { { 0 } };
    if (!mesh_build(&sc->desc.d, &sc->chunk, &sc->len, &err)) {
        fprintf(stderr, "mesh_build: %s\n", err.message);
        return 0;
    }
    if (chunk_open(&sc->c, sc->chunk, sc->len) != CHUNK_OK || mesh_open(&sc->mesh, &sc->c) != MESH_OK || !object3d_create(&sc->obj, &sc->mesh)) return 0;
    for (int i = 0; i < 3; i++) object3d_set_texture(&sc->obj, i, &tex[i]);
    for (uint32_t i = 0; i < sc->obj.texture_count; i++) sc->obj.textures[i].rgb = sc->desc.t[i].unknown_1[0];
    view_init(&sc->view, 0, 0, W, H, 50, 40);
    const int32_t eye[3] = { 0, 0, -10 * U };
    object3d_set_matrix(&sc->obj, matrix);
    object3d_transform(&sc->obj, IDENTITY, eye, 1);
    object3d_project(&sc->obj, &sc->view);
    return 1;
}

static void close_scene(Scene *sc)
{
    object3d_free(&sc->obj);
    free(sc->chunk);
}

static void test_the_whole_object(void)
{
    static Scene sc;
    CHECK(open_scene(&sc, IDENTITY));
    clear(sc.surface, &sc.sink);
    const Object3DSink sink = { .user = &sc.sink, .textured = sink_textured, .flat = sink_flat };
    object3d_sort(&sc.obj);
    object3d_draw(&sc.obj, sc.view.near_z, &sink);
    dump("whole", sc.surface);
    const uint32_t crc = crc32(sc.surface, sizeof sc.surface);
    fprintf(stderr, "whole object crc %08x\n", crc);
    CHECK_EQ_INT(crc, GOLDEN_WHOLE);
    int drawn = 0;
    for (int i = 0; i < W * H; i++) drawn += sc.surface[i] != CLEAR;
    CHECK(drawn > 2000);   /* something was drawn: the pin is not of a blank surface */
    close_scene(&sc);
}

static void test_the_sections_in_the_cars_order(void)
{
    static Scene sc;
    CHECK(open_scene(&sc, IDENTITY));
    clear(sc.surface, &sc.sink);
    const Object3DSink sink = { .user = &sc.sink, .textured = sink_textured, .flat = sink_flat };
    const char *const names[3] = { "inside", "surface", "outside" };
    for (int i = 0; i < 3; i++) {
        const int section = object3d_section(&sc.obj, names[i]);
        Object3DList list;
        CHECK(section >= 0 && object3d_section_list(&sc.obj, section, &list));
        object3d_sort_list(&sc.obj, &list);
        object3d_draw_list(&sc.obj, &list, sc.view.near_z, &sink);
        free(list.order);
    }
    dump("sections", sc.surface);
    const uint32_t crc = crc32(sc.surface, sizeof sc.surface);
    fprintf(stderr, "sections crc %08x\n", crc);
    CHECK_EQ_INT(crc, GOLDEN_SECTIONS);
    close_scene(&sc);
}

/* the object turned 20 and 12 degrees: slanted edges, uvs along a slope, polygons seen at an angle and the sort by depth */
static void test_the_object_turned(void)
{
    static Scene sc;
    int32_t m[9];
    turned(0x38, 0x22, m);   /* 1024 per turn: 0x38 about 20 degrees, 0x22 about 12 */
    CHECK(open_scene(&sc, m));
    clear(sc.surface, &sc.sink);
    const Object3DSink sink = { .user = &sc.sink, .textured = sink_textured, .flat = sink_flat };
    object3d_sort(&sc.obj);
    object3d_draw(&sc.obj, sc.view.near_z, &sink);
    dump("turned", sc.surface);
    const uint32_t crc = crc32(sc.surface, sizeof sc.surface);
    fprintf(stderr, "turned crc %08x\n", crc);
    CHECK_EQ_INT(crc, GOLDEN_TURNED);
    close_scene(&sc);
}

int main(void)
{
    test_the_whole_object();
    test_the_sections_in_the_cars_order();
    test_the_object_turned();
    return TEST_REPORT();
}
