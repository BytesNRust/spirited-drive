/* ORIP: mesh chunk inside a FAM or CFM container (see chunk.h).
 *
 * The game relocates the header with FUN_00490ea8 (00490ea8), which adds the
 * chunk base to the ten offset fields marked [rebased] below, and then walks
 * the polygon records (12 bytes each) rebasing their two index fields onto the
 * vertex map. So every offset is relative to the "ORIP" tag.
 *
 *   header (0x70 bytes)
 *   +00  "ORIP"
 *   +04  u32 total length of the chunk
 *   +08  u32 unknown (0x2BC in every sample)
 *   +0C  u32 always 0 in the file; the game stores a runtime value here
 *   +10  u32 vertex count
 *   +14  u32 unknown, <= vertex count (109 of 125 on DVIPER: looks like the
 *        count of the body vertices before the turn signal ones)
 *   +18  u32 vertex offset [rebased]          12 bytes each: i32 x, y, z
 *   +1C  u32 uv count
 *   +20  u32 uv offset [rebased]              8 bytes each: i32 u, v
 *   +24  u32 polygon count (the loop bound of FUN_00490ea8)
 *   +28  u32 polygon offset [rebased]         12 bytes each, see MeshPolygon
 *   +2C  char name[12], NUL padded            "_VIPER"
 *   +38  u32 texture reference count
 *   +3C  u32 texture reference offset [rebased]   20 bytes each, MeshTexture
 *   +40  u32 section A count
 *   +44  u32 section A offset [rebased]       20 bytes each, MeshSectionA
 *   +48  u32 section B count
 *   +4C  u32 section B offset [rebased]       28 bytes each, MeshSectionB
 *   +50  u32 vertex map offset [rebased]      flat u32 array to the chunk end
 *   +54  u32 section C count
 *   +58  u32 section C offset [rebased]       12 bytes each, MeshSectionC
 *   +5C  u32 label count
 *   +60  u32 label offset [rebased]           12 bytes each, MeshLabel
 *   +64  u32 unknown; the game takes a different path when it is nonzero
 *   +68  u32 unknown offset [rebased]; equals the polygon offset on DVIPER
 *   +6C  u32 runtime slot, 0 in the file
 *
 * The game's polygon records point into the vertex map: each polygon owns a run
 * of 4 u32 slots for vertex indexes and, unless the uv slot equals the vertex
 * slot, another run of 4 for uv indexes (a triangle leaves the fourth unused).
 *
 * Coordinates stay as stored. They are signed integers whose unit this module
 * does not decide (DVIPER spans about -100..110 on x). The OpenNFS1 notes say
 * the Z axis is flipped on import; that is a presentation choice, not stored.
 * Layout confidence: header, vertices, uvs, polygons, labels, texture names:
 * high (checked against 00490ea8 and a size walk of every chunk on the CD).
 * Sections A, B, C: field widths high, meaning low. All values the reader
 * calls unknown are carried raw. */
#ifndef TNFS_MESH_H
#define TNFS_MESH_H

#include "formats/chunk.h"

#include <stddef.h>
#include <stdint.h>

typedef enum {
    MESH_OK = 0,
    MESH_ERR_KIND,    /* chunk is not an ORIP chunk */
    MESH_ERR_BOUNDS,  /* a table lies outside the chunk */
    MESH_ERR_INDEX    /* record index, or a slot run, outside its table */
} MeshResult;

typedef struct {
    const uint8_t *base;        /* the "ORIP" tag */
    size_t size;                /* declared chunk length */
    char name[12];              /* NUL padded, not necessarily terminated */
    uint32_t unknown_08;
    uint32_t unknown_14;
    uint32_t unknown_64;
    uint32_t unknown_68_offset;
    uint32_t vertex_count;
    uint32_t uv_count;
    uint32_t polygon_count;
    uint32_t texture_count;
    uint32_t section_a_count;
    uint32_t section_b_count;
    uint32_t section_c_count;
    uint32_t label_count;
    uint32_t vertex_map_slots;  /* u32 slots from the vertex map to the chunk end */
    uint32_t vertex_offset, uv_offset, polygon_offset, texture_offset;
    uint32_t section_a_offset, section_b_offset, section_c_offset, label_offset;
    uint32_t vertex_map_offset;
} Mesh;

typedef struct { int32_t x, y, z; } MeshVertex;
typedef struct { int32_t u, v; } MeshUv;

typedef struct {
    uint8_t flags;          /* low 3 bits: corner count, 3 or 4 (one record with 2 exists,
                               TR6's "_MINE"); the other bits are unknown (bit 7 is common) */
    uint8_t unknown_1;
    uint8_t texture;        /* index into the texture references */
    uint8_t unknown_3;
    uint32_t vertex_slot;   /* first of 4 vertex index slots in the vertex map */
    uint32_t uv_slot;       /* first of 4 uv index slots; == vertex_slot means "no uvs" */
} MeshPolygon;

typedef struct {
    uint32_t unknown_0[2];
    char name[4];           /* FSH entry name; all zero means no texture. Not terminated */
    uint32_t unknown_1[2];  /* 0 and 0xEF on DVIPER */
} MeshTexture;

typedef struct {
    char name[8];           /* e.g. "left_tur", "right_tu"; NUL padded */
    uint32_t unknown[3];    /* DVIPER: 8, 0x240, first vertex index of the part */
} MeshSectionA;

typedef struct {
    char name[12];          /* e.g. "NON-SORT", "inside", "surface", "outside" */
    uint32_t unknown[4];
} MeshSectionB;

typedef struct {
    char name[8];           /* e.g. "smok", "RR1", "FL0"; NUL padded */
    uint32_t value;         /* small index, polygon-like. Some records are not text */
} MeshSectionC;

typedef struct {
    char name[8];           /* e.g. "bklr", "rt_rear"; NUL padded, not terminated */
    uint32_t polygon;       /* index into the polygons */
} MeshLabel;

/* Validates the header and that every table fits in the chunk. */
MeshResult mesh_open(Mesh *mesh, const Chunk *chunk);

MeshResult mesh_vertex(const Mesh *mesh, uint32_t index, MeshVertex *out);
MeshResult mesh_uv(const Mesh *mesh, uint32_t index, MeshUv *out);
MeshResult mesh_polygon(const Mesh *mesh, uint32_t index, MeshPolygon *out);
MeshResult mesh_texture(const Mesh *mesh, uint32_t index, MeshTexture *out);
MeshResult mesh_section_a(const Mesh *mesh, uint32_t index, MeshSectionA *out);
MeshResult mesh_section_b(const Mesh *mesh, uint32_t index, MeshSectionB *out);
MeshResult mesh_section_c(const Mesh *mesh, uint32_t index, MeshSectionC *out);
MeshResult mesh_label(const Mesh *mesh, uint32_t index, MeshLabel *out);

/* Raw u32 at a vertex map slot. */
MeshResult mesh_vertex_map(const Mesh *mesh, uint32_t slot, uint32_t *out);

/* The four vertex indexes and four uv indexes of a polygon as stored; the uv
 * values are only meaningful when uv_slot != vertex_slot. */
MeshResult mesh_polygon_corners(const Mesh *mesh, const MeshPolygon *polygon,
                                uint32_t vertices[4], uint32_t uvs[4]);

const char *mesh_result_str(MeshResult result);

#endif
