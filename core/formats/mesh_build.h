/* A car's mesh (ORIP) as a description in memory, and written back from one (dep59 tools B1). mesh_describe reads a mesh of the CD
 * into a MeshDesc; mesh_build lays a MeshDesc out as the CD's are (docs/modding/car-model.md: the tables in order with no gap, the
 * vertex map per polygon its uv run then its vertex run, a slot a corner, then the parts' and the groups' lists). Describing a mesh
 * of the CD and building it again gives the same bytes. The fields with no known meaning travel in the description: a new mesh
 * takes them from a car of the CD (the donor). Names are the record's raw bytes (the CD keeps whatever was in memory after the
 * text); mesh_desc_name sets one from text. */
#ifndef TNFS_MESH_BUILD_H
#define TNFS_MESH_BUILD_H

#include "formats/exchange.h"
#include "formats/mesh.h"

#include <stddef.h>
#include <stdint.h>

enum { MESH_NAME_SIZE = 12, MESH_PART_NAME_SIZE = 8, MESH_MAX_TEXTURES = 256 };

typedef struct {
    uint8_t flags, unknown_1, texture, unknown_3;   /* as MeshPolygon; the corner count is flags & 7, 3 or 4 */
    uint8_t has_uv;                                  /* 0: the polygon has no uv run (its uv slot is its vertex slot) */
    uint32_t vertex[4], uv[4];                       /* corner indexes; a triangle's 4th is not stored (0 here) */
} MeshDescPolygon;

/* section A: a front wheel the steering turns. Its vertices (indexes of the model's own) and, from first_offset_vertex on, as many
 * vertices holding their offsets at full lock */
typedef struct {
    char name[MESH_PART_NAME_SIZE];
    uint32_t *vertices;
    uint32_t count;
    uint32_t first_offset_vertex;
} MeshDescPart;

/* section B: a group of polygons the race draws together (inside, surface, outside, NON-SORT) */
typedef struct {
    char name[MESH_NAME_SIZE];
    uint32_t unknown_0, unknown_3;
    uint32_t *polygons;
    uint32_t count;
} MeshDescGroup;

typedef struct {
    char name[MESH_NAME_SIZE];
    uint32_t unknown_08, real_vertices /* +0x14: the model's own vertices, before the wheels' offsets */, unknown_64;
    MeshVertex *vertices;       uint32_t vertex_count;
    MeshUv *uvs;                uint32_t uv_count;
    MeshDescPolygon *polygons;  uint32_t polygon_count;
    MeshTexture *textures;      uint32_t texture_count;
    MeshDescPart *parts;        uint32_t part_count;
    MeshDescGroup *groups;      uint32_t group_count;
    MeshSectionC *points;       uint32_t point_count;   /* each value a vertex index */
    MeshLabel *labels;          uint32_t label_count;   /* each value a polygon index */
} MeshDesc;

/* `mesh` as a description that owns its arrays (mesh_desc_free); 0 with the reason in `err` */
int mesh_describe(const Mesh *mesh, MeshDesc *out, ExchangeError *err);
/* The ORIP chunk of `desc` (malloc'd), laid out as the CD's are; 0 with the reason in `err`: an index outside its table, a polygon
 * with other than 3 or 4 corners, more than 256 textures, no vertex or no polygon, a polygon twice in a group or two groups of one
 * name, a part's offsets past the vertices */
int mesh_build(const MeshDesc *desc, uint8_t **out, size_t *out_len, ExchangeError *err);
void mesh_desc_free(MeshDesc *desc);
/* `field` (`size` bytes) set to `text`, NUL padded; 0 with `err` naming it when it does not fit */
int mesh_desc_name(char *field, size_t size, const char *text, ExchangeError *err);

#endif
