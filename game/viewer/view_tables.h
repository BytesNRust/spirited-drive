/* the light's tables and the draw hooks of the road (view_tables.c); ported_view.c calls these */
#ifndef TNFS_VIEW_TABLES_H
#define TNFS_VIEW_TABLES_H

#include "formats/fsh.h"
#include "game/race/psx_light.h"
#include "game/race/track_art.h"
#include "viewer/ported_view.h"
#include "viewer/modern_road.h"

#include <stdint.h>

enum { LAMP_CACHE = 128, LAMP_BUCKETS = 5, LAMP_NEAR = 24, LAMP_KEY_STEPS = 16, LAMP_STEPS = 12 };
enum { POOL_CLASSES = 4 };   /* the lamp pool's classes 1..4 (psx_light_tunnel_class 2..5): the warmth at class c is c / POOL_CLASSES */
enum { STREET_BOARDS = 3 };

int lamp_polygon(float heat[4], const uint8_t (**top)[256]);
extern int lit_active;
extern uint8_t lit_corner[4];
extern uint8_t lit_tables[MODERN_FOG_LEVELS][256];
extern int modern;
const uint8_t (*lit_tables_for(const FshEntry *texture))[256];
const uint8_t *lit_table(void);
const uint8_t *lit_table_for(const FshEntry *texture);
extern int pool_ready;
extern uint8_t pool_tables[4][MODERN_FOG_LEVELS][256];
int reflect_level_here(void);
int reflect_thin_here(void);   /* reflector_thin of the polygon reflect_level_here saw last */
extern const FshEntry *street_bitmap[STREET_BOARDS];
extern int street_scenery[STREET_BOARDS], street_count;
extern uint8_t street_entry[3];
extern uint8_t tunnel_tables[MODERN_FOG_LEVELS][256];
extern int lamp_cache_used, lamp_cache_next;
void street_halo_sync(void);
extern uint8_t street_tables[MODERN_FOG_LEVELS][256];
extern uint8_t tone_made[PSX_LIGHT_TONES][2];
extern const FshEntry *window_bitmap[WINDOW_BOARDS_MAX];
extern int window_scenery[WINDOW_BOARDS_MAX], window_count;
extern uint8_t window_entry;
extern uint8_t window_tables[MODERN_FOG_LEVELS][256];
extern const float NO_CAST[3];
extern float tone_lit_tint[3], tone_tunnel_tint[3], tone_cast[3];
extern int tone_have_cast;
extern float tone_min_light;
extern uint8_t tone_palette[768];
void draw_uv(void *user, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8]);
void light_hook(const uint8_t corners[4]);
extern int lit_level;
void reflect_frame(const TrackArt *art);
extern int tables_made;
extern float tone_mean;

#endif
