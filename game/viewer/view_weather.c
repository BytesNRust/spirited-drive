/* port (weather, the horizon's filter, the wide boards): the weather's tables and its frame, the rain's mood bands, the mirror's weather and sky, and the full-box draws of the wide boards, taken out of ported_view.c.
 * ported_view.c makes the weather tables and calls these; what this file takes of the view is in view_shared.h. */

#include "game/race/weather_snow.h"
#include "game/sys/port_settings.h"
#include "game/viewer/cone_cells.h"
#include "game/viewer/reflect_pass.h"
#include "game/viewer/street_halo.h"
#include "game/viewer/tunnel_cells.h"
#include "game/viewer/view_lamps.h"
#include "game/viewer/weather_hooks.h"
#include "viewer/psx_sky_draw.h"
#include "viewer/view_rain.h"
#include "viewer/view_tables.h"
#include "viewer/view_weather.h"
#include "viewer/weather_draw.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* port (weather, re/notes/weather-design.md): WEATHER_CLEAR runs none of it. The tables are made again when the palette or the
 * weather changes. */
int weather;
int view_weather(void) { return weather; }   /* the frame's weather, for view_spray.c */
ModernLook look;   /* port: the Texture Filter and Fog's look (the PS1 look's dark fog too: ported_view_set_dark_fog) */

static int track_snow;   /* port (snow_weather): this race's track is a snowy one with the switch on (ported_view_set_snow) */
void ported_view_set_snow(int snow) { track_snow = snow != 0; }
int snow_level(void) { return weather_snow_level(weather, track_snow, 1); }   /* SNOW_NONE .. SNOW_HEAVY: what falls on this race */
void ported_view_set_weather(int w) { weather = w > WEATHER_CLEAR && w < WEATHER_COUNT ? w : WEATHER_CLEAR; }

const WeatherTables *weather_tables(const TrackArt *art) { return weather_tables_for(art, weather, snow_level(), port_weather_fog(), &look); }


void rain_mood_band(void *user, int top, int bottom)
{
    const RainMood *m = user;
    const int32_t box[4] = { m->box[0], top, m->box[2], bottom };
    weather_mood_rows(m->pixels, m->pitch, box, m->table, m->share);
}

WeatherFrame weather_frame(void);   /* port (weather): the frame's weather, as weather_hooks.h asks for it */

int ported_view_modern(void) { return modern; }
int ported_view_rain(void) { return weather_wet(weather); }

int rain_open = 16;  /* port (weather_zones) 0..16: how much weather there is where the eye is (rain_open_at), once a frame before the sky is drawn */
int look_filter, look_fog;  /* port: EXTRAS' Texture Filter and Fog (ported_view_set_look) */
void ported_view_set_dark_fog(int dark) { modern_look_set_dark(&look, dark); }

/* the rear-view mirror draws before the front view of the frame: its road gets the weather's style (not last frame's);
 * 0 (nothing set) in WEATHER_CLEAR or without Modern 3D */
int ported_view_mirror_weather_on(void) { return weather != WEATHER_CLEAR && modern; }

int ported_view_mirror_weather(const TrackArt *art)
{
    if (weather == WEATHER_CLEAR || !modern) return 0;
    weather_style(art, weather_tables(art), weather, snow_level(), port_weather_fog(), &look, look_filter, rain_open);
    return 1;
}

/* the mirror's own sky, drawn by now: toward the weather's colour, meeting the fog at its horizon (its camera `matrix`) */
void ported_view_mirror_sky(const TrackArt *art, const QuadTarget *target, const View *view, const int32_t matrix[9])
{
    if (!ported_view_mirror_weather(art)) return;
    const WeatherTables *wt = weather_tables(art);
    const WeatherSky sky = weather_sky(target->pixels, target->pitch, target->clip_left, target->clip_right, wt, view, matrix, storm_flash_level());
    weather_sky_band((void *)&sky, target->clip_top, target->clip_bottom);
}
PsxSkyDraw *psx_sky;        /* port: the PS1 sky (ported_view_set_psx_sky) */
void ported_view_set_psx_sky(PsxSkyDraw *sky) { psx_sky = sky; }
const TrackInfo *psx_sky_info;   /* port: the alternative sky's INFO (ported_view_set_psx_sky_info) */
void ported_view_set_psx_sky_info(const TrackInfo *info) { psx_sky_info = info; }
const FshEntry *ported_view_sky_in_palette(void) { return psx_sky != NULL ? psx_sky_draw_in_palette(psx_sky) : NULL; }
HorizonSmooth smooth;       /* port: the Texture Filter's table for the sky's palette (horizon_smooth.h) */

void ported_view_set_look(int filter, int fog) { look_filter = filter != 0, look_fog = fog != 0; }
int ported_view_filter_on(void) { return modern && look_filter; }
int ported_view_fog_on(void) { return modern && look_fog; }

/* port: a scenery board the exe drops for a corner past +-0x800 across (0044a450) while its middle shows (a big tree) is drawn clipped at the near
 * plane, perspective-correct; TNFS_WIDE_BOARD=0 the exe's drop */
int wide_board_on(void)
{
    const char *env = getenv("TNFS_WIDE_BOARD");
    return env == NULL || atoi(env) != 0;
}

/* port: the view the 3D objects' pixels clip and project by when Modern 3D is on, else NULL (the exe's mappers) */
const View *modern_view(const View *view) { return modern ? view : NULL; }

/* port: the near road clipped, unsnapped and perspective-correct (Modern 3D) */
static void draw_camera_base(void *user, const View *view, const FshEntry *texture, const int32_t uv[8],
                        const int32_t *const cam[4])
{
    if (lit_active && (lit_tinted || lit_corner[0] < 255 || lit_corner[1] < 255 || lit_corner[2] < 255 || lit_corner[3] < 255)) {
        /* the intensities at the polygon's corners: cam's corner i is the polygon's own corner i (uv is only the polygon's piece of its texture, a lane's quarter of a strip's, so the light is not
         * found by uv across the whole texture: it stopped short of the corners and the next polygon's edge started at another value, a hard step of the cone's light at every row) */
        float dark[4];
        for (int i = 0; i < 4; i++) dark[i] = (255.0f - lit_corner[i]) * (MODERN_FOG_LEVELS - 1) / 255.0f;
        if (track_light_xz_ok && (track_light_tunnel & 7) == 0 && cone_cells_draw(user, view, texture, uv, cam, transparent(texture), lit_tables_for(texture), psx_light, lit_corner, track_light_xz, track_light_node)) return;   /* port (headlight_cone): lit inside the polygon */
        const uint8_t (*tables)[256] = lit_tables_for(texture);
        float lamp_heat[4];
        const uint8_t (*lamp_top)[256] = NULL;
        if (tables == lit_tables && lamp_polygon(lamp_heat, &lamp_top)) {   /* port (`lamp`): the street lamps' light, radial and per pixel: the lit tables and the lamps' graded by the heat at each corner */
            modern_road_cell_lit_warm(user, view, texture, uv, cam, transparent(texture), lit_tables, dark, lamp_top, lamp_heat, LAMP_STEPS);
            return;
        }
        if (tables == tunnel_tables && tunnel_cells_draw(user, view, texture, uv, cam, transparent(texture), tunnel_tables, dark)) return;   /* port (tunnel_lamps): a polygon of a tunnel with lamps near, in pieces a node long, each graded by the lamps' heat at its own corners */
        if (tables == tunnel_tables && lamp_polygon_tunnel(lamp_heat, &lamp_top)) {   /* port (tunnel_lamps): a tunnel's lamps' pools, the tunnel's tables and the same with the lamp's light added, graded by the heat at each corner */
            modern_road_cell_lit_warm(user, view, texture, uv, cam, transparent(texture), tunnel_tables, dark, lamp_top, lamp_heat, LAMP_STEPS);
            return;
        }
        if (pool_ready && track_light_xz_ok && (track_light_tunnel & 7) != 1 && psx_light != NULL
            && (tables == lit_tables || ((track_light_tunnel & 7) >= 2 && tables == pool_tables[((track_light_tunnel & 7) - 2) & 3]))) {
            /* port (lamp_pool_tint): the road in a lamp's pool is the plain lit tables and the pool's warm ones by the warmth at each corner (its node's class), dithered per pixel as the
             * darkness is: the warmth fades along the road with the lamp's classes, no polygon with a table of its own */
            float heat[4];
            for (int k = 0; k < 4; k++) {
                const int cls = psx_light_tunnel_class(psx_light, track_light_node[k]);
                heat[k] = cls >= 2 ? (float)(cls - 1) / POOL_CLASSES : 0.0f;
            }
            if (heat[0] > 0 || heat[1] > 0 || heat[2] > 0 || heat[3] > 0) {   /* a polygon that only reaches into the pool (its weakest corner is out of it) grades into it too */
                modern_road_cell_lit_warm(user, view, texture, uv, cam, transparent(texture), lit_tables, dark, pool_tables[POOL_CLASSES - 1], heat, 0);
                return;
            }
        }
        modern_road_cell_lit(user, view, texture, uv, cam, transparent(texture), tables, dark);
    } else modern_road_cell(user, view, texture, uv, cam, transparent(texture));
}

void draw_camera(void *user, const View *view, const FshEntry *texture, const int32_t uv[8],
                        const int32_t *const cam[4])
{
    street_halo_note_cam(texture, view, cam);
    draw_camera_base(user, view, texture, uv, cam);
    if (!reflect_pass_active()) return;
    const FshEntry *copy = reflect_pass_copy(texture);
    const int level = copy != NULL ? reflect_level_here() : 0;
    if (level > 0) {   /* port (reflectors): the masked texels again, lit by the lamps and not by the dark */
        static const float no_dark[4] = { 0, 0, 0, 0 };
        modern_road_cell_lit(user, view, copy, uv, cam, 1, reflect_pass_rows(level, MODERN_FOG_LEVELS), no_dark);
    }
}

/* 004f9e9c = 00425590 -> 004251d0: the whole texture as two triangles */
static void draw_full_base(void *user, const FshEntry *texture, const int32_t screen[8])
{
    view_lamps_note_full(texture, screen);
    street_halo_note(texture, screen);
    const uint8_t *lit = lit_table_for(texture);
    if (lit != NULL) {  /* quad_draw_full_tri's two triangles, the texture inset by a texel (00486720 / 004864f0) */
        const MapMode mode = transparent(texture) ? MAP_REMAP_TRANSPARENT : MAP_REMAP;
        const int32_t right = (int32_t)(int16_t)texture->width - 2, bottom = (int32_t)(int16_t)texture->height - 2;
        const int32_t uv_a[6] = { 1, 1, right, 1, right, bottom }, uv_b[6] = { right, bottom, 1, bottom, 1, 1 };
        const int32_t scr_b[6] = { screen[4], screen[5], screen[6], screen[7], screen[0], screen[1] };
        tri_draw_mode(user, texture, uv_a, screen, mode, lit);
        tri_draw_mode(user, texture, uv_b, scr_b, mode, lit);
    } else quad_draw_full_tri(user, texture, screen, transparent(texture));
}

void draw_full(void *user, const FshEntry *texture, const int32_t screen[8])
{
    draw_full_base(user, texture, screen);
    if (!reflect_pass_active()) return;
    const FshEntry *copy = reflect_pass_copy(texture);
    const int level = copy != NULL ? reflect_level_here() : 0;
    if (level > 0) {   /* port (reflectors) */
        const uint8_t *lit = reflect_pass_table(level);
        const int32_t right = (int32_t)(int16_t)copy->width - 2, bottom = (int32_t)(int16_t)copy->height - 2;
        const int32_t uv_a[6] = { 1, 1, right, 1, right, bottom }, uv_b[6] = { right, bottom, 1, bottom, 1, 1 };
        const int32_t scr_b[6] = { screen[4], screen[5], screen[6], screen[7], screen[0], screen[1] };
        tri_draw_mode(user, copy, uv_a, screen, MAP_REMAP_TRANSPARENT, lit);
        tri_draw_mode(user, copy, uv_b, scr_b, MAP_REMAP_TRANSPARENT, lit);
    }
}

/* 004f9e90 = 00425558 -> 00425250: the whole texture as one quad */
static void draw_full_alt_base(void *user, const FshEntry *texture, const int32_t screen[8])
{
    view_lamps_note_full(texture, screen);
    street_halo_note(texture, screen);
    const uint8_t *lit = lit_table_for(texture);
    if (lit != NULL) {
        const int32_t right = ((int32_t)(int16_t)texture->width << 16) - 1, bottom = ((int32_t)(int16_t)texture->height << 16) - 1;
        const int32_t uv[8] = { 0, 0, right, 0, right, bottom, 0, bottom };
        quad_draw_mode(user, texture, uv, screen, transparent(texture) ? MAP_REMAP_TRANSPARENT : MAP_REMAP, lit);
    } else quad_draw_full(user, texture, screen, transparent(texture));
}

void draw_full_alt(void *user, const FshEntry *texture, const int32_t screen[8])
{
    draw_full_alt_base(user, texture, screen);
    if (!reflect_pass_active()) return;
    const FshEntry *copy = reflect_pass_copy(texture);
    const int level = copy != NULL ? reflect_level_here() : 0;
    if (level > 0) {   /* port (reflectors) */
        const int32_t right = ((int32_t)(int16_t)copy->width << 16) - 1, bottom = ((int32_t)(int16_t)copy->height << 16) - 1;
        const int32_t uv[8] = { 0, 0, right, 0, right, bottom, 0, bottom };
        quad_draw_mode(user, copy, uv, screen, MAP_REMAP_TRANSPARENT, reflect_pass_table(level));
    }
}


/* port (weather): the frame's weather as weather_hooks.h asks for it; the thunder, the voices of the followed car and the lightning are there */
WeatherFrame weather_frame(void) { return (WeatherFrame){ weather, snow_level(), rain_open, shadow_race, anim_tick }; }

void storm_thunder_update(void) { const WeatherFrame f = weather_frame(); weather_hooks_thunder(&f); }
void rain_roll_update(void) { const WeatherFrame f = weather_frame(); weather_hooks_rain_roll(&f); }
void snow_roll_update(void) { const WeatherFrame f = weather_frame(); weather_hooks_snow_roll(&f); }
int storm_flash_level(void) { const WeatherFrame f = weather_frame(); return weather_hooks_flash_level(&f); }
