/* EA's 3D object engine as the race uses it for scenery models (and later
 * the cars): one instance per mesh, transformed, projected, depth sorted and
 * drawn polygon by polygon.
 *
 *   object3d_create     0048d798 with flags 1 (own vertex copy, 00491349),
 *                       the polygon order 004917f7 and texture slots 00490cc0
 *   object3d_bind       00490b9a: textures found by name in an FSH
 *   object3d_transform  00491737: model to camera space
 *   object3d_project    00491690 -> 0048cdd0
 *   object3d_sort       004919b4 -> 004918e1 + Shell sort 004a3110
 *   object3d_draw       00491d35 -> 00491c8a -> 00491a82 per polygon
 *
 * The engine's camera (004c7b20, 004c7b44) is never moved by the race: its
 * matrix stays the identity (0049160a has no callers) and its position is
 * set per object by 00458b7c, so both are arguments here.
 *
 * Pixels leave through a sink. Textured polygons name the mapper table the
 * original dispatches through; the mappers themselves live in gfx/quad.h and
 * gfx/object_mappers.h. */
#ifndef TNFS_OBJECT3D_H
#define TNFS_OBJECT3D_H

#include "formats/fsh.h"
#include "formats/mesh.h"
#include "game/gfx/palette_match.h"
#include "game/race/view.h"

#include <stdint.h>

enum {
    OBJECT3D_MAX_SORTED = 1000,      /* 004918e1 sorts at most 1000 */
    OBJECT3D_FLAG_NEAR = 0x02,       /* +0x10: polygons touching the near plane are dropped */
    OBJECT3D_FLAG_MATRIX = 0x04,     /* +0x10: matrix changed, 00491737 recombines */
    OBJECT3D_FLAG_ALT_MAPPERS = 0x40 /* +0x10: 0052f11c, the two-triangle tables */
};

/* Polygon flag word (ORIP polygon +0). */
enum {
    POLY_CORNERS = 0x7,
    POLY_TEXTURED = 0x80,
    POLY_TWO_SIDED = 0x100,
    POLY_REVERSED = 0x200,      /* drawn when its screen winding is negative */
    POLY_NO_CULL = 0x400,
    POLY_LINKED = 0x800,        /* draws the polygon named after its corners next */
    POLY_UV = 0x1000,           /* mapped with its own uvs, else the whole texture */
    POLY_TRIANGLES = 0x2000     /* quads as two triangles */
};

/* The five mapper tables of 00491a82, indexed by (flags >> 3) & 0xF. */
typedef enum {
    OBJECT3D_TABLE_TRIANGLE,          /* 004f4828: three corners */
    OBJECT3D_TABLE_QUAD_UV,           /* 004f4834 */
    OBJECT3D_TABLE_QUAD_UV_TRIANGLES, /* 004f4840 */
    OBJECT3D_TABLE_QUAD_WHOLE,        /* 004f484c */
    OBJECT3D_TABLE_QUAD_WHOLE_TRIANGLES /* 004f4858 */
} Object3DTable;

typedef struct {
    uint32_t flags;
    uint32_t vertex[4];
    uint32_t uv[4];       /* uv indexes; meaningful when the mesh stored a uv run */
    uint32_t link;        /* vertex map slot after the corners (POLY_LINKED) */
} Object3DPolygon;

typedef struct {
    uint32_t rgb;               /* +0: MeshTexture +0xc, bytes r, g, b */
    uint32_t colour;            /* +4: the palette entry nearest rgb (object3d_match_colours) */
    const FshEntry *texture;    /* +0xc, bound by object3d_bind */
    FshEntry entry;
} Object3DTexture;

typedef struct {
    int32_t screen[8];    /* corners, (x, y) per corner */
    int32_t uv[8];        /* the mesh's uvs for those corners, as stored */
    int corners;
} Object3DCorners;

typedef struct {
    void *user;
    void (*textured)(void *user, Object3DTable table, int mode, const FshEntry *texture,
                     const Object3DCorners *c);
    /* 004a3390: 2 corners a line (00484254), 3 or 4 a flat polygon (0048434e).
     * TODO(00473140): rgb to palette index happens in the sink for now. */
    void (*flat)(void *user, uint32_t rgb, const Object3DCorners *c);
} Object3DSink;

/* A run of polygon indexes drawn as one sorted group: the whole object
 * (+0x84) or one of the mesh's named sections (0049126b). Sorting reorders
 * it in place and the order carries over to the next frame. */
typedef struct {
    uint32_t count;
    uint32_t *order;
} Object3DList;

typedef struct {
    const Mesh *mesh;
    uint32_t flags;               /* +0x10 */
    uint32_t vertex_count;        /* +0x4 */
    uint32_t polygon_count;       /* mesh +0x24 */
    uint32_t order_count;         /* +0x8 */
    int32_t matrix[9];            /* +0x28 */
    int32_t combined[9];          /* +0x4c */
    int32_t position[3];          /* +0x1c, 0 from 0048dad1 */
    int32_t (*model)[3];          /* +0x70 */
    int32_t (*camera)[3];         /* +0x78 */
    int32_t (*screen)[2];         /* +0x7c */
    uint32_t *order;              /* +0x84, kept between frames as the original does */
    Object3DPolygon *polygons;
    Object3DTexture *textures;    /* +0x14 */
    uint32_t texture_count;
} Object3D;

/* Returns 0 on a malformed mesh or allocation failure (object left empty). */
int object3d_create(Object3D *obj, const Mesh *mesh);
void object3d_free(Object3D *obj);
/* Returns how many texture slots were bound. */
int object3d_bind(Object3D *obj, const Fsh *fsh);

/* 00490cc0: each texture slot takes its mesh record's +0xc as rgb and the
 * entry 00473140 finds for its bytes 0..2 (00490c38) as colour. 0048d798
 * runs it when the object is made and 004574ec again once the race palette
 * is built. Returns 0 on a texture record outside the mesh. */
int object3d_match_colours(Object3D *obj, const PaletteMatch *m);

/* 00490df3: texture slot `slot` draws `texture` from now on (the car's
 * wheel frames, 00457d40 and 00458614). A slot past the object's is left
 * alone (the original writes past its slots). */
void object3d_set_texture(Object3D *obj, int32_t slot, const FshEntry *texture);

/* 00490b21: the first texture ref whose label matches `label` (up to 8
 * characters or its NUL, either case, 004a2da2), or -1. */
int32_t object3d_texture_named(const Object3D *obj, const char *label);

/* 004912d0: the first of the mesh's parts (section A: "left_tur",
 * "right_tu", the turn signals) whose name matches as 00490b21's, or -1. */
int32_t object3d_part_named(const Object3D *obj, const char *name);

/* 0048da62 */
void object3d_set_matrix(Object3D *obj, const int32_t matrix[9]);
/* camera_matrix: 004c7b20; camera_position: 004c7b44 */
void object3d_transform(Object3D *obj, const int32_t camera_matrix[9], const int32_t camera_position[3],
                        int recombine_always);
void object3d_project(Object3D *obj, const View *view);
void object3d_sort(Object3D *obj);
/* near_z: 004c7b1c (30 in the race, set by 004585cd) */
void object3d_draw(const Object3D *obj, int32_t near_z, const Object3DSink *sink);

/* 004911f2: the section whose name matches the first 8 bytes, or -1.
 * 0049126b: its polygon list, copied out of the mesh (free with free()). */
int object3d_section(const Object3D *obj, const char *name);
int object3d_section_list(const Object3D *obj, int section, Object3DList *out);
void object3d_sort_list(const Object3D *obj, Object3DList *list);   /* 004918e1 */
void object3d_draw_list(const Object3D *obj, const Object3DList *list, int32_t near_z,
                        const Object3DSink *sink);                   /* 00491c8a */

#endif
