/* dep71: a mesh's Blender OBJ back to the mesh's own frame (notes/dep71-blender-roundtrip-plan.md). mesh_export writes NAME.blender.obj
 * (mesh_exchange.h): metres, z negated, every textured corner's uv as 0..1 of its bitmap from the bottom. Blender keeps the vertices'
 * order but may write the faces in another order (by material) and turned, writes its own texture coordinates (equal ones merged, in
 * another order) and gives every face of a mesh with uvs one. mesh_blender_back turns the OBJ Blender exported into the model of NAME.obj (mesh_export's, read with obj_read), from the
 * original NAME.obj beside the tables: units and texels again, and the original's uv rows, so an unedited mesh comes back the same:
 *   - a vertex: the Blender one, x 130, z negated;
 *   - a corner's uv: in texels of its face's bitmap (the face's material t<i>_, else the original's), on the original corner's uv row;
 *     a row two corners shared that now differ gets a new row for the later corner; a face that had no uvs keeps none;
 *   - the faces: the original's, in its order: each Blender face is the original face on its vertices, the same way round (turned
 *     as Blender wrote it), with Blender's material and uvs.
 * Same topology only (the parts' and groups' lists name vertices and polygons by index): the vertex count and the face count must be
 * the original's, every face on an original face's vertices the same way round, no two on one face's; anything else is refused, naming
 * `car import` without --raw (a face flipped: flip it back). Refused too: a coordinate or a texel past 32 bits, a material naming a
 * texture the mesh does not have, a textured face whose bitmap is 0 x 0. */
#ifndef TNFS_MESH_BLENDER_H
#define TNFS_MESH_BLENDER_H

#include "formats/exchange.h"
#include "formats/obj_read.h"

#include <stdint.h>

/* a texture reference's bitmap: 1 x 1 for a reference with no name; 0 x 0 for a named bitmap the bank does not hold (a textured face on
 * it is refused: its uvs could not be turned back) */
typedef struct { uint32_t width, height; } MeshBlenderSize;

/* `blender` back on `original` into `out` (its own arrays: obj_free); `sizes` by texture reference index, `size_count` of them.
 * 0 with the reason in `err`, and nothing to free */
int mesh_blender_back(const ObjModel *blender, const ObjModel *original, const MeshBlenderSize *sizes, uint32_t size_count, ObjModel *out,
                      ExchangeError *err);

#endif
