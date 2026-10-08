/* The textured-polygon mappers of the 3D object engine (NFS_WIN.EXE 00491a82),
 * and the dispatch that picks one.
 *
 * 00491c8a/00491a82 set globals and call through five tables of three code
 * pointers each, laid out back to back from 004f4828. The pointers are not the
 * mappers but 15 small thunks (004a34e7 to 004a4205) that read the globals
 * and call a mapper with explicit arguments:
 *   0052f12c  int32 list of vertex indices into the screen vertices (00530180,
 *             an array of int32 (x, y) pairs); advanced as it is read
 *   0052f124  int32 list of uv indices into the uv table (0053017c, an array of
 *             int32 (u, v) pairs in whole texels); the polygon's third word
 *   0052f130  the texture's FSH entry
 * Both lists run parallel: corner i is screen[vertex[i]] with uv[uv_index[i]].
 * Table 4f4858 (inset quads) reads only the vertex list, 4f484c (whole
 * texture) too; they ignore 0052f124.
 *
 * The column is the polygon mode, (flags >> 3) & 0xF. The race only ever uses
 * 0, 1 and 2 (opaque, colour 0xFF transparent, shadow: see MapMode); a larger
 * mode runs off its table into the next one, and past the fifth into zeros,
 * which crashes the original (obj_select returns 0 there).
 *
 *   table   when                                 thunks -> mapper (mode 0 / 1 / 2)
 *   4f4828  flags & 7 == 3 (triangle)            4a34e7 -> 00486400 / 004858f0 / 004aa510
 *   4f4834  quad, flags & 0x1000, and neither    4a3b05 -> 00487180 / 00486950 / 004aa600
 *           0x2000 nor the object flag 0052f11c  (uv shifted left 16 to 16.16)
 *   4f4840  quad, flags & 0x1000, and 0x2000 or  4a3763 -> 00486400 x2 / 004858f0 x2 /
 *           the object flag                      004aa510 x2: corners 0 1 2 then 2 3 0
 *   4f484c  quad, no 0x1000, neither             4a4033 -> 00487a10 / 004879b0 / 004ab060
 *                                                the whole texture, uv from the texture
 *   4f4858  quad, no 0x1000, 0x2000 or the flag  4a3f94 -> 00486804 / 004865d4 / 004aaf14
 *                                                the texture inset by one texel, as two
 *                                                triangles 0 1 2 and 2 3 0
 * 0052f11c is bit 6 of the object's flag word (param_1[4] of 00491c8a).
 *
 * Mode 2 (004aa510, 004aa600, 004ab060, 004aaf14) is a copy of mode 1 whose
 * row or span writer also handles colour 0xFE: the destination pixel is
 * replaced by shade[pixel], the 256-byte table at 004c6604. The geometry,
 * clipping and texel selection are the mode 1 code unchanged.
 *
 * Only 8-bit textures (FSH type 0x7B) drawn to an 8-bit surface are ported, as
 * in quad.h. TODO(0049738c): the inset thunks convert a texture whose type
 * differs from the surface's (004c78cd) into a temporary copy; here any other
 * type draws nothing. */
#ifndef TNFS_OBJECT_MAPPERS_H
#define TNFS_OBJECT_MAPPERS_H

#include "game/gfx/quad.h"

typedef enum {
    OBJ_TRIANGLE = 0,   /* 004f4828 */
    OBJ_QUAD = 1,       /* 004f4834 */
    OBJ_QUAD_TRIS = 2,  /* 004f4840 */
    OBJ_FULL = 3,       /* 004f484c */
    OBJ_INSET = 4       /* 004f4858 */
} ObjShape;

typedef struct {
    ObjShape shape;
    MapMode mode;
} ObjMapper;

enum {
    OBJ_FLAG_CORNERS = 7,         /* flags & 7 == 3: a triangle, anything else a quad */
    OBJ_FLAG_UV_QUAD = 0x1000,    /* the polygon has its own uv */
    OBJ_FLAG_SPLIT = 0x2000,      /* draw a quad as two triangles / inset */
    OBJ_MODE_SHIFT = 3
};

/* Which mapper 00491a82 calls for polygon word 0 (flags) and the object flag
 * 0052f11c (non-zero when set). Returns 0 and leaves out untouched when the
 * table entry does not exist. */
int obj_select(uint32_t flags, int object_flag, ObjMapper *out);

/* Explicit-argument forms of the thunks. uv is in whole texels (as in the uv
 * table), screen in pixels; both 4 x (x, y) for quads, 3 for OBJ_TRIANGLE.
 * shade is read for MAP_SHADE only. */
void obj_draw_triangle(const QuadTarget *target, const FshEntry *texture, const int32_t uv[6],
                       const int32_t screen[6], MapMode mode, const uint8_t *shade);
void obj_draw_quad(const QuadTarget *target, const FshEntry *texture, const int32_t uv[8],
                   const int32_t screen[8], MapMode mode, const uint8_t *shade);
void obj_draw_quad_tris(const QuadTarget *target, const FshEntry *texture, const int32_t uv[8],
                        const int32_t screen[8], MapMode mode, const uint8_t *shade);
/* The texture's own corners (uv ignored), 00487a10 / 004879b0 / 004ab060. */
void obj_draw_full(const QuadTarget *target, const FshEntry *texture, const int32_t screen[8],
                   MapMode mode, const uint8_t *shade);
/* Texels 1 .. size-2 only, 00486804 / 004865d4 / 004aaf14. */
void obj_draw_inset(const QuadTarget *target, const FshEntry *texture, const int32_t screen[8],
                    MapMode mode, const uint8_t *shade);

/* The whole dispatch. uv_table/uv_count and screen_table/screen_count are the
 * arrays 0053017c and 00530180 in (x, y) pairs; uv_index and vertex_index are
 * the polygon's lists (0052f124, 0052f12c), 3 or 4 entries by the flags. An
 * index outside its table, or a missing mapper, draws nothing and returns 0.
 * uv_index may be NULL for the tables that ignore it. Returns 1 when drawn. */
int obj_draw_polygon(const QuadTarget *target, const FshEntry *texture, uint32_t flags,
                     int object_flag, const int32_t *uv_table, int uv_count,
                     const int32_t *uv_index, const int32_t *screen_table, int screen_count,
                     const int32_t *vertex_index, const uint8_t *shade);

#endif
