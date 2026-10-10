/* ORIP mesh (see mesh.h) <-> OBJ + MTL + TSV. An export gives, for a stem NAME:
 *   NAME.obj          o <name>, v x y z, vt u v, one f (or l, for the 2-corner record) per polygon in file order,
 *                     usemtl <material> whenever the texture changes. Coordinates and uvs are the stored
 *                     integers, one OBJ unit per raw unit (no axis flip, no scale).
 *   NAME.mtl          one newmtl per texture reference, named t<index>_<FSH entry name> (map_Kd points at
 *                     <texture_dir>/<entry name>.png when a directory is given).
 *   NAME.header.tsv   header words and the 12-byte name;  NAME.polygons.tsv, NAME.textures.tsv, NAME.parts.tsv
 *   (section A), NAME.groups.tsv (section B), NAME.sectionc.tsv, NAME.labels.tsv: the tables the OBJ does not
 *   carry, one row per record.
 * An import overlays them on the original chunk and keeps its size: vertex, uv and polygon counts, corner
 * counts and every table length are the original's (moving vertices, changing uvs, textures and the other
 * fields is supported; adding or removing geometry is not yet). Float OBJ values are rounded to integers. */
#ifndef TNFS_MESH_EXCHANGE_H
#define TNFS_MESH_EXCHANGE_H

#include "formats/exchange.h"

/* `orip` starts at the "ORIP" tag; `size` is at least its declared length. */
int mesh_export(const uint8_t *orip, size_t size, const char *stem, const char *texture_dir, FileSet *out, ExchangeError *err);

/* Writes a copy of the declared chunk length in *out (malloc'd). */
int mesh_import(const uint8_t *orip, size_t size, const char *stem, const FileSet *in, uint8_t **out, size_t *out_len,
                ExchangeError *err);

#endif
