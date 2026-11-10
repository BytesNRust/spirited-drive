/* Fixed tables of the race's track renderer, extracted from NFS_WIN.EXE by
 * tools/gen_track_tables.py (track_tables.c).
 *
 * A segment is drawn at a level of detail picked by its road property and its
 * distance from the camera. The LOD says which of each row's 11 terrain points
 * become vertices (rows end with 0xFF) and which run of polygons joins them.
 * Polygon corners index the segment's vertices; indices past the segment's
 * own count reach into the next segment's first row. */
#ifndef TNFS_TRACK_TABLES_H
#define TNFS_TRACK_TABLES_H

#include "game/exe/exe_table_const.h"
#include <stdint.h>

enum {
    TRACK_PROPERTIES = 19,
    TRACK_LOD_DISTANCES = 32,
    TRACK_LOD_COUNT = 30,
    TRACK_LOD_SELECT = 48,
    TRACK_UV_COUNT = 5,
    TRACK_POLYGON_COUNT = 587,
    TRACK_VARIANT_ROWS = 32,
    TRACK_SUBDIVIDE_LEVELS = 3
};

typedef struct {
    int32_t vertex_count;   /* +0 */
    int32_t polygon_count;  /* +4 */
    uint8_t fence[16];      /* +8, fence corner data (0044a2b4), not decoded yet */
    uint8_t select[TRACK_LOD_SELECT];  /* +0x18, point indices per row, 0xFF ends a row */
    int32_t first_polygon;  /* +0x48 */
} TrackLod;

typedef struct {
    int32_t u_level;   /* the texture is split into 2^level cells along u */
    int32_t u_cell;    /* this polygon's cell */
    int32_t variant;   /* added to the distance variant (0..7) */
} TrackUv;

typedef struct {
    uint8_t corner[4];
    uint8_t strip;      /* 0-9: which of the segment's 10 texture slots */
    uint8_t flags;      /* unknown */
    uint8_t uv;         /* index into TRACK_UVS */
    uint8_t rows_done;  /* nonzero after the last polygon of a row band */
} TrackPolygon;

extern EXE_TABLE_CONST uint8_t TRACK_LOD_BY_DISTANCE[TRACK_PROPERTIES][TRACK_LOD_DISTANCES];
extern EXE_TABLE_CONST TrackLod TRACK_LODS[TRACK_LOD_COUNT];
extern EXE_TABLE_CONST TrackUv TRACK_UVS[TRACK_UV_COUNT];
extern EXE_TABLE_CONST TrackPolygon TRACK_POLYGONS[TRACK_POLYGON_COUNT];
extern EXE_TABLE_CONST uint8_t TRACK_VARIANT_NEAR[TRACK_VARIANT_ROWS];
extern EXE_TABLE_CONST uint8_t TRACK_VARIANT_FAR[TRACK_VARIANT_ROWS];
extern EXE_TABLE_CONST int32_t TRACK_SUBDIVIDE_Z[TRACK_SUBDIVIDE_LEVELS];
enum { TRACK_SUBDIVIDE_MAX = 2 };  /* 004b9590 */

#endif
