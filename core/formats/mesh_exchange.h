/* ORIP mesh (see mesh.h) <-> OBJ + MTL + TSV. An export gives, for a stem NAME:
 *   NAME.obj          o <name>, v x y z, vt u v, one f (or l, for the 2-corner record) per polygon in file order,
 *                     usemtl <material> whenever the texture changes. Coordinates and uvs are the stored
 *                     integers, one OBJ unit per raw unit (no axis flip, no scale).
 *   NAME.mtl          one newmtl per texture reference, named t<index>_<FSH entry name>. Its map_Kd names the PNG of
 *                     the first of `banks` holding a bitmap of that name, as shpi_export writes it (<dir>/NNNN_LABEL.png,
 *                     shpi_entry_file); a reference no bank holds has no map_Kd.
 *   NAME.header.tsv   header words and the 12-byte name;  NAME.polygons.tsv, NAME.textures.tsv, NAME.parts.tsv
 *   (section A), NAME.groups.tsv (section B), NAME.sectionc.tsv, NAME.labels.tsv: the tables the OBJ does not
 *   carry, one row per record.
 *   NAME.lists.tsv    what the parts' and groups' map slots point at: each part's vertex list and each group's polygon
 *                     list, one row per record (for an importer that builds a new mesh, dep59; the import here reads
 *                     the lists from the original and ignores this file).
 * An import overlays them on the original chunk and keeps its size: vertex, uv and polygon counts, corner
 * counts and every table length are the original's (moving vertices, changing uvs, textures and the other
 * fields is supported; adding or removing geometry is not yet). Float OBJ values are rounded to integers. */
#ifndef TNFS_MESH_EXCHANGE_H
#define TNFS_MESH_EXCHANGE_H

#include "formats/exchange.h"

/* A texture bank a mesh can see, and the folder of its export relative to the mesh's (its map_Kd prefix). */
typedef struct {
    const uint8_t *shpi;
    size_t size;
    const char *dir;
} MeshTextureBank;

/* `orip` starts at the "ORIP" tag; `size` is at least its declared length. `banks` in the order a texture is looked up
 * (the mesh's own first); none (NULL, 0) gives an MTL with no map_Kd. */
int mesh_export(const uint8_t *orip, size_t size, const char *stem, const MeshTextureBank *banks, size_t bank_count, FileSet *out,
                ExchangeError *err);

/* Writes a copy of the declared chunk length in *out (malloc'd). */
int mesh_import(const uint8_t *orip, size_t size, const char *stem, const FileSet *in, uint8_t **out, size_t *out_len,
                ExchangeError *err);

/* One of the tables mesh_export writes beside NAME.obj, read back as the chunk's own records (dep59, the car importer's --raw): the
 * header as its 0x70 bytes (the fields not in the table zero), the others one record a row, at the chunk's stride. `records`
 * malloc'd, `rows` the count; 0 with the reason in `err` (no such file, a bad row). */
typedef enum { MESH_TABLE_HEADER, MESH_TABLE_POLYGONS, MESH_TABLE_TEXTURES, MESH_TABLE_PARTS, MESH_TABLE_GROUPS, MESH_TABLE_SECTIONC,
               MESH_TABLE_LABELS } MeshTable;
enum { MESH_TABLE_MAX_ROWS = 1 << 16 };
int mesh_table_read(const FileSet *in, const char *stem, MeshTable table, uint8_t **records, size_t *rows, ExchangeError *err);

#endif
