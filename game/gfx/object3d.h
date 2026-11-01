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
    int32_t camera[12];   /* port: the corners in camera space (x, y, z each), for the sink's camera calls */
    int corners;
    const float *normal;  /* port (body shine): the corners' unit normals in camera space, 3 floats each; NULL when the object keeps none (object3d_enable_normals) */
} Object3DCorners;

typedef struct {
    void *user;
    void (*textured)(void *user, Object3DTable table, int mode, const FshEntry *texture,
                     const Object3DCorners *c);
    /* 004a3390: 2 corners a line (00484254), 3 or 4 a flat polygon (0048434e).
     * port: the sink finds the slot's colour (00473140) at each draw; 00490cc0 keeps it in the slot. */
    void (*flat)(void *user, uint32_t rgb, const Object3DCorners *c);
    /* port (Modern 3D): both NULL the exe's. With textured_camera set a textured polygon goes to it in place of
     * `textured`, and with flat_camera a flat one of 3 or 4 corners in place of `flat`, from their camera-space
     * corners (c->camera): the polygons are not dropped for touching the near plane (the sink clips them) and are
     * culled by their facing in camera space instead of by the winding of their snapped screen corners. */
    void (*textured_camera)(void *user, Object3DTable table, int mode, const FshEntry *texture, const Object3DCorners *c);
    void (*flat_camera)(void *user, uint32_t rgb, const Object3DCorners *c);
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
    float (*normal)[3];           /* port (body shine): a camera-space unit normal a vertex, NULL when off */
} Object3D;

/* Returns 0 on a malformed mesh or allocation failure (object left empty). */
int object3d_create(Object3D *obj, const Mesh *mesh);
void object3d_free(Object3D *obj);
/* port (two_sided_scenery): the polygons of an open surface are drawn from behind too. A polygon with a corner edge no other polygon of the object shares
 * (the object is open there: a board, an arch, a tunnel mouth, an inverted U) gets POLY_NO_CULL; the polygons of a closed solid keep the culling, as their
 * back faces are inside it. Returns how many it set; polygons that already draw both ways are not counted. */
int object3d_both_sides(Object3D *obj);
/* port (body shine): gives the object a normal per vertex (0 when it cannot allocate). object3d_update_normals then sets them from the camera-space
 * corners (after object3d_transform): a vertex takes the mean of the faces it belongs to that the engine would draw (a one-sided one turned
 * away is left out; a two-sided one counts turned toward the eye), a vertex of none gets (0, 0, 0). The polygons handed to the sink carry them in
 * Object3DCorners.normal. An object without them draws as it always did. */
int object3d_enable_normals(Object3D *obj);
void object3d_update_normals(Object3D *obj);
/* 004fbad8 set (a mirrored track): POLY_REVERSED flipped on the first order_count polygons, as 00457d40 does for
 * the cars and 004576f0 for the scenery models, so their faces keep facing the mirrored camera. */
void object3d_mirror(Object3D *obj);
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
 * "right_tu", the front wheels' steering parts) whose name matches as
 * 00490b21's, or -1. */
int32_t object3d_part_named(const Object3D *obj, const char *name);

/* 004914b8: part `part` of the mesh's section A (a record: +8 the count,
 * +0xc its first vertex map slot, +0x10 its first offset vertex) moved by
 * `weight` (16.16): each of its vertices v in obj->model becomes the
 * mesh's v plus offset k x weight (each product rounded, 004a2ed0), the
 * offsets being the mesh vertices from +0x10 on; weight 0 copies v, 1.0
 * adds the offsets as they are. Nothing for a part outside the section. */
void object3d_morph_part(Object3D *obj, int32_t part, int32_t weight);

/* 004910fa: the value (+8) of the first section C record ("smok", "RR1",
 * "FL0") whose name matches as 00490b21's, or -1. */
int32_t object3d_point_named(const Object3D *obj, const char *name);

/* 00491176: the polygon (+8) of the first label ("bklr", "rt_rear")
 * whose name matches as 00490b21's, or -1. */
int32_t object3d_label_named(const Object3D *obj, const char *name);

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
