/* Chunked containers: the track families (*.FAM in NTRACKFM, ETRACKFM and
 * GTRACKFM) and the car models (*.CFM in CARFAMS). Files may be QFS-compressed;
 * decompress first (formats/qfs.h). The CD files are all stored plain.
 *
 * A chunk starts with a 4-byte tag. Three tags exist:
 *
 *   "wwww"  header chunk, a table of child chunks:
 *             +0   "wwww"
 *             +4   u32 count
 *             +8   count x u32 child offset, relative to this chunk's "wwww"
 *           An offset of 0 means the child is absent (CY3_M01.FAM child 1).
 *           Children follow the table; a child's extent runs to the next
 *           larger sibling offset, or to the end of the parent.
 *   "ORIP"  mesh chunk:   +0 "ORIP", +4 u32 total length (see mesh.h)
 *   "SHPI"  bitmap chunk: +0 "SHPI", +4 u32 total length. The same SHPI layout
 *           as *.FSH (see fsh.h); directory offsets are relative to the tag.
 *
 * The game loads the whole file and walks it once, turning every offset in a
 * "wwww" table into a pointer (FUN_004511a4 at 004511a4; the file read is
 * FUN_004510e8 at 004510e8 for FAM files via FUN_004511fc, and FUN_00451278
 * at 00451278 for CFM files, from the car loader FUN_00457d40). It never looks
 * at "ORIP" or "SHPI" tags: the position in the tree decides the type. We read
 * the tag as well, which is stricter and costs nothing. FUN_004511a4 also lets a
 * zero offset inherit the previous child's pointer; we report such a child as
 * absent instead.
 *
 * Known tree shapes (child indexes at the root):
 *   CFM  4 children: 0 ORIP high-res body, 1 SHPI its bitmaps, 2 ORIP low-res,
 *        3 SHPI its bitmaps. Read by FUN_00457d40 via FUN_004512f8(root, 0..3),
 *        which is "root + 8 + index * 4". Confidence: high.
 *   FAM  4 children: 0 wwww of SHPI (terrain textures), 1 wwww of SHPI (scenery
 *        textures), 2 SHPI (horizon), 3 wwww of wwww{ORIP, SHPI} (scenery
 *        objects). Child 3 is the one FUN_004511fc hands to FUN_004585e4, which
 *        calls FUN_004576f0(child, i) for each object (high confidence). The
 *        roles of 0 to 2 follow the OpenNFS1 layout notes and the data (medium
 *        confidence). Children 0 and 1 hold a single SHPI on the TRn tracks and
 *        12 to 42 SHPI on the AL/CL/CY tracks. The ORIP+SHPI pairs of child 3
 *        are also wwww chunks of two children, at depth 2 of the tree.
 * All integers are little-endian. */
#ifndef TNFS_CHUNK_H
#define TNFS_CHUNK_H

#include "formats/fsh.h"

#include <stddef.h>
#include <stdint.h>

enum {
    CHUNK_MAX_DEPTH = 8,   /* nesting limit for chunk_survey */
    /* Root child indexes, see the layout above. */
    CFM_CHILD_MESH_HIGH = 0,
    CFM_CHILD_BITMAPS_HIGH = 1,
    CFM_CHILD_MESH_LOW = 2,
    CFM_CHILD_BITMAPS_LOW = 3,
    FAM_CHILD_TERRAIN = 0,
    FAM_CHILD_SCENERY = 1,
    FAM_CHILD_HORIZON = 2,
    FAM_CHILD_OBJECTS = 3
};

typedef enum {
    CHUNK_KIND_ABSENT = 0, /* zero offset in the parent table */
    CHUNK_KIND_HEADER,     /* "wwww" */
    CHUNK_KIND_MESH,       /* "ORIP" */
    CHUNK_KIND_BITMAP      /* "SHPI" */
} ChunkKind;

typedef enum {
    CHUNK_OK = 0,
    CHUNK_ERR_BOUNDS,  /* table, offset or declared length outside the data */
    CHUNK_ERR_TAG,     /* not "wwww", "ORIP" or "SHPI" */
    CHUNK_ERR_INDEX,   /* child index past the table */
    CHUNK_ERR_KIND,    /* operation does not apply to this kind of chunk */
    CHUNK_ERR_DEPTH    /* nesting deeper than CHUNK_MAX_DEPTH */
} ChunkResult;

typedef struct {
    ChunkKind kind;
    const uint8_t *data;  /* start of the tag; NULL for an absent chunk */
    size_t size;          /* ORIP/SHPI: declared length. wwww: extent in the parent */
    uint32_t count;       /* wwww: number of children, else 0 */
} Chunk;

typedef struct {
    uint32_t headers;
    uint32_t meshes;
    uint32_t bitmaps;
    uint32_t absent;
    uint32_t max_depth;   /* the root is depth 0 */
} ChunkTotals;

/* Keeps a pointer to data; it must outlive every Chunk taken from it. */
ChunkResult chunk_open(Chunk *out, const uint8_t *data, size_t size);
ChunkResult chunk_child(const Chunk *header, uint32_t index, Chunk *out);

/* Walks the whole tree below root (root included), checking every table,
 * offset and declared length, and adds up what it finds. */
ChunkResult chunk_survey(const Chunk *root, ChunkTotals *totals);

/* Opens a bitmap chunk as an SHPI directory. */
FshResult chunk_bitmaps(const Chunk *bitmap, Fsh *out);

const char *chunk_result_str(ChunkResult result);

#endif
