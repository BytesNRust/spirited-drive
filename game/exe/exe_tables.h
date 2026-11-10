/* port: the tables the original keeps in NFS_WIN.EXE's data and the port needs (the track renderer's LODs and polygons, the
 * viewports, the AI drivers, the split times, the in-race menu's map, the default records), read from the player's own exe at
 * start-up instead of being kept in the repository. Each extractor reads one family through an ExeImage and fills its
 * members of an ExeTables; exe_tables_load runs them all or none.
 *
 * An extractor is `int exe_tables_<family>(const ExeImage *img, ExeTables *t, char *err, size_t errsz)` in
 * game/exe/exe_tables_<family>.c: 0 on success, -1 with a message on err (an address in no section, a count over the room here).
 * It writes only its own members. The members have the types and sizes of the arrays the port reads (the headers named), so
 * that publishing them is a copy. */
#ifndef TNFS_EXE_TABLES_H
#define TNFS_EXE_TABLES_H

#include "game/exe/exe_image.h"
#include "game/frontend/config_records.h"
#include "game/race/ai_profiles.h"
#include "game/race/race_menu.h"
#include "game/race/race_split_times.h"
#include "game/race/race_view.h"
#include "game/race/track_tables.h"

#include <stddef.h>
#include <stdint.h>

enum {
    EXE_MENU_MAPS = RACE_MENU_TRACKS * RACE_MENU_VARIANTS,
    EXE_MENU_POOL_POINTS = 5918,   /* the in-race menu's map points of all tracks and variants, x and y words each */
    EXE_NAME_MAX = 32              /* a name read from the exe, with its NUL */
};

typedef struct {
    /* ai_profiles.h: 004c2850 */
    uint8_t ai_driver_data[AI_DRIVER_DATA_SIZE];
    /* race_split_times.h: 004c584c..004c5cc0 */
    uint8_t race_split_tables[RACE_SPLIT_TABLES_SIZE];
    /* race_view.h: 004b7080, 004b75d0, 004b790c */
    ViewLayout race_view_layouts[VIEW_SCREEN_MODES][VIEW_LAYOUTS];
    ViewRow race_view_rows[VIEW_SCREEN_MODES][VIEW_ROWS];
    int32_t race_view_model_y[VIEW_SCREEN_MODES][VIEW_MODELS][3];
    /* track_tables.h: 004bba64, 004c49d4, 004c3034, 004c3098, 004c2ea8, 004c2f20, 004b9594 */
    uint8_t track_lod_by_distance[TRACK_PROPERTIES][TRACK_LOD_DISTANCES];
    TrackLod track_lods[TRACK_LOD_COUNT];
    TrackUv track_uvs[TRACK_UV_COUNT];
    TrackPolygon track_polygons[TRACK_POLYGON_COUNT];
    uint8_t track_variant_near[TRACK_VARIANT_ROWS];
    uint8_t track_variant_far[TRACK_VARIANT_ROWS];
    int32_t track_subdivide_z[TRACK_SUBDIVIDE_LEVELS];
    /* race_menu.h: 00447410..004474ea and the maps' points. The points of every map are one pool; map_first / map_count say
     * where each track's variant starts and how many points it has (count 0: no such map). */
    int16_t menu_item_x[RACE_MENU_ITEMS];
    int16_t menu_item_y[RACE_MENU_ITEMS];
    int16_t menu_x_offset[RACE_MENU_OFFSETS];
    int16_t menu_y_offset[RACE_MENU_OFFSETS];
    char menu_track_name[RACE_MENU_TRACKS][EXE_NAME_MAX];
    char menu_marker_name[RACE_MENU_CARS][5];
    int16_t menu_pool[EXE_MENU_POOL_POINTS][2];
    int16_t menu_map_first[RACE_MENU_TRACKS][RACE_MENU_VARIANTS];
    int16_t menu_map_count[RACE_MENU_TRACKS][RACE_MENU_VARIANTS];
    /* config_records.h: 004b9638.. and the three names */
    char config_row_name[CONFIG_RECORD_ROW_COUNT][EXE_NAME_MAX];
    int32_t config_row_number[CONFIG_RECORD_ROW_COUNT][5];
    char config_name[3][EXE_NAME_MAX];
} ExeTables;

/* the extractors of the families whose files are in this directory (see the header's top for their contract) */
int exe_tables_menu(const ExeImage *img, ExeTables *t, char *err, size_t errsz);
int exe_tables_config(const ExeImage *img, ExeTables *t, char *err, size_t errsz);

#endif
