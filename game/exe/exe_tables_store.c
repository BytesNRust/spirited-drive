/* port: the storage of the tables read from the exe : the arrays the port reads under
 * their own names, filled once by exe_tables_publish before anything reads them. */
#include "game/exe/exe_tables.h"

#include <string.h>

uint8_t AI_DRIVER_DATA[AI_DRIVER_DATA_SIZE];
uint8_t RACE_SPLIT_TABLES[RACE_SPLIT_TABLES_SIZE];
ViewLayout race_view_layouts[VIEW_SCREEN_MODES][VIEW_LAYOUTS];
ViewRow race_view_rows[VIEW_SCREEN_MODES][VIEW_ROWS];
int32_t race_view_model_y[VIEW_SCREEN_MODES][VIEW_MODELS][3];
uint8_t TRACK_LOD_BY_DISTANCE[TRACK_PROPERTIES][TRACK_LOD_DISTANCES];
TrackLod TRACK_LODS[TRACK_LOD_COUNT];
TrackUv TRACK_UVS[TRACK_UV_COUNT];
TrackPolygon TRACK_POLYGONS[TRACK_POLYGON_COUNT];
uint8_t TRACK_VARIANT_NEAR[TRACK_VARIANT_ROWS];
uint8_t TRACK_VARIANT_FAR[TRACK_VARIANT_ROWS];
int32_t TRACK_SUBDIVIDE_Z[TRACK_SUBDIVIDE_LEVELS];
int16_t race_menu_item_x[RACE_MENU_ITEMS];
int16_t race_menu_item_y[RACE_MENU_ITEMS];
int16_t race_menu_x_offset[RACE_MENU_OFFSETS];
int16_t race_menu_y_offset[RACE_MENU_OFFSETS];
const char *race_menu_track_names[RACE_MENU_TRACKS];
char race_menu_marker_names[RACE_MENU_CARS][5];
RaceMenuMap race_menu_maps[RACE_MENU_TRACKS][RACE_MENU_VARIANTS];
ConfigRecordRow CONFIG_RECORD_ROWS[CONFIG_RECORD_ROW_COUNT];
const char *CONFIG_RECORD_NAMES[3];

/* what the pointers above point at: the maps' points, and the names */
static int16_t menu_points[EXE_MENU_POOL_POINTS][2];
static char track_names[RACE_MENU_TRACKS][EXE_NAME_MAX];
static char row_names[CONFIG_RECORD_ROW_COUNT][EXE_NAME_MAX];
static char record_names[3][EXE_NAME_MAX];

/* 00447564's maps: x and y words divided by 25 on tracks 0..2 (three variants), by 6 on the others (one) */
static int16_t map_divisor(int track) { return track < 3 ? 25 : 6; }

void exe_tables_publish(const ExeTables *t)
{
    memcpy(AI_DRIVER_DATA, t->ai_driver_data, sizeof AI_DRIVER_DATA);
    memcpy(RACE_SPLIT_TABLES, t->race_split_tables, sizeof RACE_SPLIT_TABLES);
    memcpy(race_view_layouts, t->race_view_layouts, sizeof race_view_layouts);
    memcpy(race_view_rows, t->race_view_rows, sizeof race_view_rows);
    memcpy(race_view_model_y, t->race_view_model_y, sizeof race_view_model_y);
    memcpy(TRACK_LOD_BY_DISTANCE, t->track_lod_by_distance, sizeof TRACK_LOD_BY_DISTANCE);
    memcpy(TRACK_LODS, t->track_lods, sizeof TRACK_LODS);
    memcpy(TRACK_UVS, t->track_uvs, sizeof TRACK_UVS);
    memcpy(TRACK_POLYGONS, t->track_polygons, sizeof TRACK_POLYGONS);
    memcpy(TRACK_VARIANT_NEAR, t->track_variant_near, sizeof TRACK_VARIANT_NEAR);
    memcpy(TRACK_VARIANT_FAR, t->track_variant_far, sizeof TRACK_VARIANT_FAR);
    memcpy(TRACK_SUBDIVIDE_Z, t->track_subdivide_z, sizeof TRACK_SUBDIVIDE_Z);
    memcpy(race_menu_item_x, t->menu_item_x, sizeof race_menu_item_x);
    memcpy(race_menu_item_y, t->menu_item_y, sizeof race_menu_item_y);
    memcpy(race_menu_x_offset, t->menu_x_offset, sizeof race_menu_x_offset);
    memcpy(race_menu_y_offset, t->menu_y_offset, sizeof race_menu_y_offset);
    memcpy(race_menu_marker_names, t->menu_marker_name, sizeof race_menu_marker_names);
    memcpy(track_names, t->menu_track_name, sizeof track_names);
    for (int i = 0; i < RACE_MENU_TRACKS; i++) race_menu_track_names[i] = track_names[i];
    memcpy(menu_points, t->menu_pool, sizeof menu_points);
    memset(race_menu_maps, 0, sizeof race_menu_maps);
    for (int tr = 0; tr < RACE_MENU_TRACKS; tr++)
        for (int v = 0; v < RACE_MENU_VARIANTS; v++) {
            if (t->menu_map_count[tr][v] == 0) continue;
            race_menu_maps[tr][v].points = (const int16_t (*)[2])menu_points[t->menu_map_first[tr][v]];
            race_menu_maps[tr][v].count = t->menu_map_count[tr][v];
            race_menu_maps[tr][v].divisor = map_divisor(tr);
        }
    memcpy(row_names, t->config_row_name, sizeof row_names);
    for (int r = 0; r < CONFIG_RECORD_ROW_COUNT; r++) {
        CONFIG_RECORD_ROWS[r].name = row_names[r];
        memcpy(CONFIG_RECORD_ROWS[r].number, t->config_row_number[r], sizeof CONFIG_RECORD_ROWS[r].number);
    }
    memcpy(record_names, t->config_name, sizeof record_names);
    for (int i = 0; i < 3; i++) CONFIG_RECORD_NAMES[i] = record_names[i];
}
