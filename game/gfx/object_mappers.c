/* The 3D object engine's polygon mappers: the table dispatch and the thunks
 * behind it (see object_mappers.h). Everything here is argument shuffling over
 * quad_draw_mode and tri_draw_mode, which carry the pixel work. */
#include "game/gfx/object_mappers.h"

#include "game/gfx/x86_int.h"

enum {
    TABLES = 5,
    MODES_PER_TABLE = 3,
    ENTRIES = TABLES * MODES_PER_TABLE,
    MODE_MASK = 0xF
};

/* The thunks pick a table from these tests (00491a82), then the entry from
 * the mode, running into the next table for mode >= 3. */
int obj_select(uint32_t flags, int object_flag, ObjMapper *out)
{
    const int split = object_flag != 0 || (flags & OBJ_FLAG_SPLIT) != 0;
    int table;
    if ((flags & OBJ_FLAG_CORNERS) == 3) table = OBJ_TRIANGLE;
    else if (!(flags & OBJ_FLAG_UV_QUAD)) table = split ? OBJ_INSET : OBJ_FULL;
    else table = split ? OBJ_QUAD_TRIS : OBJ_QUAD;
    const int entry = table * MODES_PER_TABLE + (int)((flags >> OBJ_MODE_SHIFT) & MODE_MASK);
    if (entry >= ENTRIES) return 0;
    out->shape = (ObjShape)(entry / MODES_PER_TABLE);
    out->mode = (MapMode)(entry % MODES_PER_TABLE);
    return 1;
}

void obj_draw_triangle(const QuadTarget *target, const FshEntry *texture, const int32_t uv[6],
                       const int32_t screen[6], MapMode mode, const uint8_t *shade)
{
    tri_draw_mode(target, texture, uv, screen, mode, shade);
}

/* 004a3b05: the uv go to the 16.16 quad mapper as whole texels shifted up. */
void obj_draw_quad(const QuadTarget *target, const FshEntry *texture, const int32_t uv[8],
                   const int32_t screen[8], MapMode mode, const uint8_t *shade)
{
    int32_t fixed[8];
    for (int i = 0; i < 8; i++) fixed[i] = x_shl(uv[i], 16);
    quad_draw_mode(target, texture, fixed, screen, mode, shade);
}

/* 004a3763: corners 0 1 2, then 2 3 0 (the thunk keeps corner 0 at the end of
 * its arrays so the second triangle reads it from there). */
static void two_triangles(const QuadTarget *target, const FshEntry *texture, const int32_t uv[8],
                          const int32_t screen[8], MapMode mode, const uint8_t *shade)
{
    const int32_t uv_b[6] = { uv[4], uv[5], uv[6], uv[7], uv[0], uv[1] };
    const int32_t scr_b[6] = { screen[4], screen[5], screen[6], screen[7], screen[0], screen[1] };
    tri_draw_mode(target, texture, uv, screen, mode, shade);
    tri_draw_mode(target, texture, uv_b, scr_b, mode, shade);
}

void obj_draw_quad_tris(const QuadTarget *target, const FshEntry *texture, const int32_t uv[8],
                        const int32_t screen[8], MapMode mode, const uint8_t *shade)
{
    two_triangles(target, texture, uv, screen, mode, shade);
}

/* 00487a10, 004879b0, 004ab060: uv from (0, 0) to the last 16.16 value of the
 * texture's size. */
void obj_draw_full(const QuadTarget *target, const FshEntry *texture, const int32_t screen[8],
                   MapMode mode, const uint8_t *shade)
{
    if (texture == NULL) return;
    const int32_t right = x_sub(x_shl((int16_t)texture->width, 16), 1);
    const int32_t bottom = x_sub(x_shl((int16_t)texture->height, 16), 1);
    const int32_t uv[8] = { 0, 0, right, 0, right, bottom, 0, bottom };
    quad_draw_mode(target, texture, uv, screen, mode, shade);
}

/* 00486804, 004865d4, 004aaf14. */
void obj_draw_inset(const QuadTarget *target, const FshEntry *texture, const int32_t screen[8],
                    MapMode mode, const uint8_t *shade)
{
    if (texture == NULL || texture->type != FSH_TYPE_BITMAP) return;  /* TODO(0049738c) */
    const int32_t right = (int32_t)(int16_t)texture->width - 2;
    const int32_t bottom = (int32_t)(int16_t)texture->height - 2;
    const int32_t uv[8] = { 1, 1, right, 1, right, bottom, 1, bottom };
    two_triangles(target, texture, uv, screen, mode, shade);
}

static int gather(const int32_t *table, int count, const int32_t *index, int corners, int32_t *out)
{
    if (table == NULL || index == NULL) return 0;
    for (int i = 0; i < corners; i++) {
        if (index[i] < 0 || index[i] >= count) return 0;
        out[2 * i] = table[2 * index[i]];
        out[2 * i + 1] = table[2 * index[i] + 1];
    }
    return 1;
}

int obj_draw_polygon(const QuadTarget *target, const FshEntry *texture, uint32_t flags,
                     int object_flag, const int32_t *uv_table, int uv_count,
                     const int32_t *uv_index, const int32_t *screen_table, int screen_count,
                     const int32_t *vertex_index, const uint8_t *shade)
{
    ObjMapper m;
    if (!obj_select(flags, object_flag, &m)) return 0;
    const int corners = m.shape == OBJ_TRIANGLE ? 3 : 4;
    int32_t screen[8] = { 0 }, uv[8] = { 0 };
    if (!gather(screen_table, screen_count, vertex_index, corners, screen)) return 0;
    const int needs_uv = m.shape == OBJ_TRIANGLE || m.shape == OBJ_QUAD || m.shape == OBJ_QUAD_TRIS;
    if (needs_uv && !gather(uv_table, uv_count, uv_index, corners, uv)) return 0;

    switch (m.shape) {
    case OBJ_TRIANGLE: obj_draw_triangle(target, texture, uv, screen, m.mode, shade); break;
    case OBJ_QUAD: obj_draw_quad(target, texture, uv, screen, m.mode, shade); break;
    case OBJ_QUAD_TRIS: obj_draw_quad_tris(target, texture, uv, screen, m.mode, shade); break;
    case OBJ_FULL: obj_draw_full(target, texture, screen, m.mode, shade); break;
    case OBJ_INSET: obj_draw_inset(target, texture, screen, m.mode, shade); break;
    }
    return 1;
}
