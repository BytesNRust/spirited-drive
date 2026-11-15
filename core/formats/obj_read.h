/* An OBJ and its MTL read into memory for the car importer (dep59 tools B2). The geometry as written: vertices, uvs, faces of 3 or
 * more corners (an importer splits the rest), the material and the object each face is in. An object is opened by `o` or `g`
 * and holds the vertices written after it (as Blender writes them): an object with vertices and no face is a point. Indexes are
 * stored from 0; the OBJ's are 1-based or, negative, from the last one read. Normals, smoothing groups and the other lines are
 * skipped; `l` and `p` are refused, since a car has no lines. */
#ifndef TNFS_OBJ_READ_H
#define TNFS_OBJ_READ_H

#include "formats/exchange.h"

#include <stddef.h>
#include <stdint.h>

enum { OBJ_NAME_SIZE = 64, OBJ_PATH_SIZE = 256 };

typedef struct { double x, y, z; } ObjVec3;
typedef struct { double u, v; } ObjVec2;

typedef struct {
    uint32_t first, corners;   /* corners: corner_vertex[first .. first + corners) */
    int32_t material;          /* index into materials; -1 none */
    uint32_t object;           /* the o/g name in effect */
    uint32_t line;             /* in the OBJ, for messages */
} ObjFace;

typedef struct { char name[OBJ_NAME_SIZE]; char texture[OBJ_PATH_SIZE]; } ObjMaterial;   /* newmtl and its map_Kd as written */
typedef struct { char name[OBJ_NAME_SIZE]; uint32_t first_vertex, vertex_count, face_count; } ObjObject;

typedef struct {
    ObjVec3 *vertices;      uint32_t vertex_count;
    ObjVec2 *uvs;           uint32_t uv_count;
    uint32_t *corner_vertex;
    int32_t *corner_uv;     /* -1: none */
    uint32_t corner_count;
    ObjFace *faces;         uint32_t face_count;
    ObjMaterial *materials; uint32_t material_count;
    ObjObject *objects;     uint32_t object_count;   /* object 0 is "" (before any o/g) */
    int from_exporter;      /* the first line is mesh_export's "# ORIP mesh" (mesh_exchange.c) */
} ObjModel;

/* `obj` and its `mtl` (NULL: none) into `out`, which owns its arrays (obj_free); 0 with the reason and the line in `err`, and
 * nothing to free. A `usemtl` naming no material of the MTL adds one with no texture. */
int obj_read(const uint8_t *obj, size_t obj_len, const uint8_t *mtl, size_t mtl_len, ObjModel *out, ExchangeError *err);
void obj_free(ObjModel *m);

#endif
