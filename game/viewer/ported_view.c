#include "viewer/ported_view.h"
#include "game/race/car_progress.h"
#include "game/race/race_chase.h"
#include "game/sys/fixed_math.h"
#include "game/sys/bands.h"
#include "game/gfx/raster_record.h"

#include "game/gfx/quad.h"
#include "game/race/horizon.h"
#include "game/race/police_lights.h"
#include "game/race/sky_ring.h"
#include "game/race/sky_stretch.h"
#include "game/race/track_render.h"
#include "game/gfx/sprite_scaled.h"
#include "game/race/view.h"
#include "game/viewer/cone_cells.h"
#include "game/race/track_fence.h"
#include "game/race/track_object_draw.h"
#include "game/race/reflector.h"
#include "game/race/trail_rise.h"
#include "game/race/track_mask.h"
#include "game/viewer/reflect_pass.h"
#include "game/viewer/street_halo.h"
#include "game/viewer/view_lamps.h"
#include "game/viewer/view_screen.h"
#include "game/viewer/car_lamp_halo.h"
#include "game/viewer/weather_sky.h"
#include "game/viewer/weather_hooks.h"
#include "game/viewer/screen_layout.h"
#include "game/race/car_draw.h"
#include "game/race/car_road.h"
#include "game/race/car_shadow.h"
#include "game/race/car_tag.h"
#include "game/race/car_lights.h"
#include "game/race/psx_lgs.h"
#include "game/race/psx_light_synth.h"
#include "game/race/car_body_draw.h"
#include "game/race/car_render.h"
#include "game/gfx/x86_int.h"
#include "game/race/car_wheels.h"
#include "game/gfx/flat.h"
#include "game/race/model_draw.h"
#include "game/race/row_cars.h"
#include "game/race/skid_marks.h"
#include "game/race/tyre_smoke.h"
#include "game/race/dust_trail.h"
#include "game/race/rally_gravel.h"
#include "game/race/weather_snow.h"
#include "game/race/rain_roll.h"
#include "game/race/snow_roll.h"
#include "game/viewer/follow_arm.h"
#include "game/viewer/follow_behind.h"
#include "viewer/horizon_smooth.h"
#include "viewer/modern_look.h"
#include "viewer/modern_road.h"
#include "viewer/object_sink.h"
#include "viewer/body_shine.h"
#include "game/sys/port_settings.h"
#include "game/viewer/drive_audio.h"
#include "viewer/soft_shadow.h"
#include "viewer/raster.h"
#include "viewer/sky_smooth.h"
#include "viewer/psx_sky_draw.h"
#include "viewer/drive_light.h"
#include "viewer/drive_lamps.h"
#include "game/race/lamp_emissive.h"
#include "game/race/lamp_flare.h"
#include "game/sys/port_log.h"
#include "game/sys/palette.h"
#include "viewer/weather_draw.h"
#include "viewer/wiper_draw.h"
#include "game/race/wiper.h"
#include "viewer/drive_light.h"
#include "game/race/weather.h"
#include "game/race/race_sim.h"
#include "viewer/view_spray.h"
#include "viewer/view_rain.h"
#include "viewer/view_shared.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    WORLD_SHIFT = 11,      /* 004c52bc */
    EYE_HEIGHT = 111411,   /* camera mode 2 (004b6bb0): 1.7 m */
    SKY_WORLD_SHIFT = 8,   /* 004bb8d4 */
    SKY_POSITION_SHIFT = 10  /* 0043af80: -position >> 10 */
};

/* The race's default mapper set (00449b90): every wrapper passes textures
 * of type 0x45 to the transparent mapper and the rest to the opaque one. */
static int transparent(const FshEntry *texture)
{
    return texture->type == TRACK_TEXTURE_TRANSPARENT_TYPE;
}

/* port (the PS1 look's light): the terrain polygon being drawn carries one darkness level, the mean of its corners'
 * intensities in the fog tables' 16 steps (0 = full light, nothing changes); a lit polygon is drawn through the mappers'
 * remap mode with that level's table, which the recorder keeps by pointer (the tables live until the next frame) */
static uint8_t lit_tables[MODERN_FOG_LEVELS][256];
static uint8_t pool_tables[4][MODERN_FOG_LEVELS][256];   /* port (lamp_pool_tint): the tables of a street lamp's pool, by class: the lit ones with a warm tint, the more the nearer the lamp */
static int pool_ready;
enum { POOL_CLASSES = 4 };   /* the lamp pool's classes 1..4 (psx_light_tunnel_class 2..5): the warmth at class c is c / POOL_CLASSES */
static uint8_t tunnel_tables[MODERN_FOG_LEVELS][256];   /* the same with the tunnel's cast, for the polygons all in a tunnel's rows */
static int lit_level;
static int tables_made;          /* the tables above have been made by a render (the mirror's follow pass draws with them) */
static int lit_active;           /* a polygon or object of the light's is being drawn (between the hook's two calls) */
static int lit_tinted;           /* the ambient colour is not neutral: even full light goes through the tables */
static float lit_tint[3] = { 1, 1, 1 };
static uint8_t lit_corner[4];   /* the polygon's corners' intensities, for the Modern 3D road's per-pixel gradient */


static void light_hook(const uint8_t corners[4])
{
    lit_active = corners != NULL;
    if (corners == NULL) {
        lit_level = 0;
        memset(lit_corner, 255, sizeof lit_corner);
        return;
    }
    memcpy(lit_corner, corners, sizeof lit_corner);
    const int mean = (corners[0] + corners[1] + corners[2] + corners[3] + 2) / 4;
    lit_level = ((255 - mean) * (MODERN_FOG_LEVELS - 1) + 127) / 255;
}

static const float NO_CAST[3] = { 1, 1, 1 };   /* (the tunnel's own cast and least light: psx_light_tunnel_look) */

/* port (night, psx_headlights; street_lamp.h): the street lamp's board, whose entries of its own the light tables leave alone as the lamps' colours are */
enum { STREET_BOARDS = 3 };
static const FshEntry *street_bitmap[STREET_BOARDS];
static int street_scenery[STREET_BOARDS], street_count;
static uint8_t street_entry[3];
static uint8_t street_tables[MODERN_FOG_LEVELS][256];
static void street_halo_sync(void);

void ported_view_set_street_lamps(const int scenery[], int n, const uint8_t entry[3])
{
    street_count = entry != NULL && scenery != NULL && n > 0 ? (n < STREET_BOARDS ? n : STREET_BOARDS) : 0;
    for (int i = 0; i < STREET_BOARDS; i++) street_bitmap[i] = NULL;
    for (int i = 0; i < street_count; i++) street_scenery[i] = scenery[i];
    if (street_count > 0) memcpy(street_entry, entry, sizeof street_entry);
    street_halo_sync();
}

static const FshEntry *window_bitmap[WINDOW_BOARDS_MAX];
static int window_scenery[WINDOW_BOARDS_MAX], window_count;
static uint8_t window_entry;
static uint8_t window_tables[MODERN_FOG_LEVELS][256];

void ported_view_set_window_boards(const int scenery[], int n, uint8_t entry)
{
    window_count = scenery != NULL && n > 0 ? (n < WINDOW_BOARDS_MAX ? n : WINDOW_BOARDS_MAX) : 0;
    for (int i = 0; i < window_count; i++) window_scenery[i] = scenery[i], window_bitmap[i] = NULL;
    window_entry = entry;
    street_halo_sync();
}

/* port (lamp_halos): the halo's module is told of the lamp boards, their entries and the window entry whenever one changes */
static void street_halo_sync(void) { street_halo_boards(street_bitmap, street_count, street_entry, window_entry); }


/* port (track light file, `tone`): the tables of a scene tone, one set for the open road and one for a tunnel's polygons, each made when its first polygon asks (a track has 16 tones at most:
 * 4 KB a set, about 2.5 ms to make) from what the table block below saved (the palette and the tints it was made with); the block drops them all when any of that changes */
static uint8_t tone_tables[PSX_LIGHT_TONES][2][MODERN_FOG_LEVELS][256];
static uint8_t tone_made[PSX_LIGHT_TONES][2];
static uint8_t tone_palette[768];
static float tone_lit_tint[3], tone_tunnel_tint[3], tone_cast[3];
static int tone_have_cast;
static float tone_min_light;
static float tone_mean;   /* the time's mean tint (drive_light_tint), which a zone's own light is taken over (psx_light_tunnel_look) */

/* the tint a tone gives: each channel 1 at no strength, the tone's own (0..1) at full */
static void tone_tint_of(const PsxLightTone *tone, float out[3])
{
    const float p = (float)tone->percent / 100.0f;
    for (int k = 0; k < 3; k++) out[k] = 1.0f + ((float)tone->rgb[k] / 255.0f - 1.0f) * p;
}

static uint8_t (*tone_get(int id, int in_tunnel))[256]
{
    if (psx_light == NULL || id < 0 || id >= psx_light->tone_count) return NULL;
    const PsxLightTone *const slot = &psx_light->tone[id];
    if (slot->look && !in_tunnel) return NULL;   /* port (tunnel zone looks): a zone's look is a tunnel's tables; a polygon that is not in a tunnel has the road's */
    uint8_t (*t)[256] = tone_tables[id][in_tunnel != 0];
    if (tone_made[id][in_tunnel != 0]) return t;
    if (slot->look) {   /* the zone's own light and colour, as the time's look is made (psx_light_tunnel_look): the light over the time's own tint, the colour as the cast */
        const float gain = (float)(tone_mean > 0.05f ? (double)slot->light / 100.0 / (double)tone_mean : (double)slot->light / 100.0);
        const float grey[3] = { gain, gain, gain };
        modern_tunnel_tables(tone_palette, grey, slot->cast, 0.0f, t);
        tone_made[id][1] = 1;
        return t;
    }
    float tint[3];
    tone_tint_of(&psx_light->tone[id], tint);
    if (in_tunnel && tone_have_cast) {   /* a tunnel's polygon: its own cast through the tone */
        const float cast[3] = { tone_cast[0] * tint[0], tone_cast[1] * tint[1], tone_cast[2] * tint[2] };
        modern_tunnel_tables(tone_palette, tone_tunnel_tint, cast, tone_min_light, t);
    } else {
        const float lit[3] = { tone_lit_tint[0] * tint[0], tone_lit_tint[1] * tint[1], tone_lit_tint[2] * tint[2] };
        modern_light_tables(tone_palette, lit, t);
    }
    tone_made[id][in_tunnel != 0] = 1;
    return t;
}

/* port (track light file, `lamp`): the tables a street lamp's light is: the lit tables with the lamp's colour ADDED (a light, not a tint: nothing is darkened), for a colour (to 1/LAMP_KEY_STEPS a channel, its strongest
 * channel 1) and a strength bucket (the most the lamps give there, in lamps at 100 percent). Made when a polygon asks (2 ms each), from what the table block saved for the tones (the palette and the lit tint), and kept
 * in a ring of LAMP_CACHE: plain lamps all give one colour, so a handful serve, but neons of other colours mixed in make every polygon's mean colour another one (a stretch of them wants some 60: at 12 every polygon
 * rebuilt a table, 45 ms a frame); the block drops them when it makes its own again. */
enum { LAMP_CACHE = 128, LAMP_BUCKETS = 5, LAMP_NEAR = 24, LAMP_KEY_STEPS = 16, LAMP_STEPS = 12 };
static const float LAMP_BUCKET[LAMP_BUCKETS] = { 1.0f, 2.0f, 3.0f, 5.0f, 10.0f };
/* port (`lamp`): no dither between the lit tables and the lamp's: the pool is graded in LAMP_STEPS even steps of the colour added (step s: s / LAMP_STEPS of it), a pixel taking the step its heat rounds to, as the
 * headlights' pool grades (cone_cells: levels, not a random choice). A slot holds a block per step, each made the first time a polygon needs it (`made` is a bit a step). */
static struct { uint8_t key[4]; uint32_t made; uint8_t tables[LAMP_STEPS][MODERN_FOG_LEVELS][256]; } lamp_cache[LAMP_CACHE];
static int lamp_cache_used, lamp_cache_next;

/* the blocks of steps 1 .. steps for a colour and bucket, in one run (block s - 1 the step s); NULL when steps is 0 */
static const uint8_t (*lamp_add_get(const float rgb[3], int bucket, int steps))[256]
{
    uint8_t key[4];
    for (int k = 0; k < 3; k++) key[k] = (uint8_t)(rgb[k] * LAMP_KEY_STEPS + 0.5f);
    key[3] = (uint8_t)bucket;
    int slot = -1;
    for (int i = 0; i < lamp_cache_used && slot < 0; i++)
        if (memcmp(lamp_cache[i].key, key, sizeof key) == 0) slot = i;
    if (slot < 0) {
        slot = lamp_cache_used < LAMP_CACHE ? lamp_cache_used++ : lamp_cache_next;
        if (slot == lamp_cache_next && lamp_cache_used == LAMP_CACHE) lamp_cache_next = (lamp_cache_next + 1) % LAMP_CACHE;
        memcpy(lamp_cache[slot].key, key, sizeof key);
        lamp_cache[slot].made = 0;
    }
    for (int s = 1; s <= steps; s++) {
        if (lamp_cache[slot].made >> s & 1) continue;
        float add[3];
        for (int k = 0; k < 3; k++) add[k] = 255.0f * LAMP_ADD * LAMP_BUCKET[bucket] * (float)key[k] / LAMP_KEY_STEPS * (float)s / LAMP_STEPS;
        modern_light_tables_add(tone_palette, tone_lit_tint, add, lamp_cache[slot].tables[s - 1]);
        lamp_cache[slot].made |= 1u << s;
    }
    return lamp_cache[slot].tables[0];
}

/* port (`lamp`): the heat of the lamps at the polygon being drawn's four corners (the sum of their falloffs at each corner's place in the world, 0 none; a bucket's share of 1), and the step tables of the lamps'
 * colour at the strength the polygon's strongest corner takes (as many as its hottest corner reaches). 0 when no lamp reaches it, or it is a tunnel's or a tone's: those keep their own tables. The light then
 * grades per pixel through the steps as the heat does between the corners. */
static int lamp_polygon(float heat[4], const uint8_t (**top)[256])
{
    if (psx_light == NULL || psx_light->lamp_n <= 0 || !track_light_xz_ok || (track_light_tunnel & 7) == 1 || !tables_made) return 0;
    int idx[LAMP_NEAR];
    const int n = psx_light_lamps_near(psx_light, track_light_node, idx, LAMP_NEAR);
    if (n == 0) return 0;
    float rgb[4][3], strongest = 0.0f, mean[3] = { 0, 0, 0 };
    for (int k = 0; k < 4; k++) {
        heat[k] = psx_light_lamp_at(psx_light, idx, n, track_light_xz[k][0], track_light_xz[k][1], rgb[k]);
        strongest = heat[k] > strongest ? heat[k] : strongest;
        for (int c = 0; c < 3; c++) mean[c] += rgb[k][c] * heat[k];
    }
    if (strongest < 0.02f) return 0;   /* a pool's last sliver is not worth the warm path */
    float top_channel = 0.0f;
    for (int c = 0; c < 3; c++) top_channel = mean[c] > top_channel ? mean[c] : top_channel;
    for (int c = 0; c < 3; c++) mean[c] /= top_channel;
    int bucket = 0;
    while (bucket < LAMP_BUCKETS - 1 && LAMP_BUCKET[bucket] < strongest) bucket++;
    float hottest = 0.0f;
    for (int k = 0; k < 4; k++) {
        heat[k] = heat[k] / LAMP_BUCKET[bucket] < 1.0f ? heat[k] / LAMP_BUCKET[bucket] : 1.0f;
        hottest = heat[k] > hottest ? heat[k] : hottest;
    }
    const int steps = (int)(hottest * LAMP_STEPS + 0.5f);
    if (steps < 1) return 0;   /* under half a step everywhere: the lit tables, as the pixels would take them */
    *top = lamp_add_get(mean, bucket, steps);
    return 1;
}

/* the tables of the polygon being drawn: its scene tone's (`tone`, above the class's 3 bits), a tunnel's (track_light_tunnel 1), a lamp pool's (2..5), the road's */
static uint8_t (*light_tables(void))[256]
{
    const int base = track_light_tunnel & 7, tone = track_light_tunnel >> 3;
    if (tone > 0) {
        uint8_t (*t)[256] = tone_get(tone - 1, base == 1);
        if (t != NULL) return t;
    }
    if (base == 1) return tunnel_tables;
    if (base >= 2 && pool_ready) return pool_tables[(base - 2) & 3];
    return base ? tunnel_tables : lit_tables;
}

static const uint8_t (*lit_tables_for(const FshEntry *texture))[256]
{
    const int lamp = view_lamps_of(texture);
    for (int i = 0; i < street_count; i++)
        if (street_bitmap[i] != NULL && texture == street_bitmap[i]) return street_tables;
    for (int i = 0; i < window_count; i++)
        if (window_bitmap[i] != NULL && texture == window_bitmap[i]) return window_tables;
    return lamp >= 0 ? view_lamps_table(lamp) : light_tables();
}

static const uint8_t *lit_table_for(const FshEntry *texture)
{
    return lit_active && (lit_level > 0 || lit_tinted) ? lit_tables_for(texture)[lit_level] : NULL;
}

static const uint8_t *lit_table(void) { return lit_active && (lit_level > 0 || lit_tinted) ? light_tables()[lit_level] : NULL; }

/* port (reflectors): the masks come from the track (track_mask.h's track_mask_for, which the loader fills); the textures are registered once per loaded art, the lamps' state each frame */
static const uint8_t *(*reflect_mask_for)(const TrackTexture *t) = track_mask_for;
static int reflect_lamps_on, reflect_dark;
static double reflect_x, reflect_z, reflect_hx, reflect_hz;

void ported_view_set_reflect_masks(const uint8_t *(*mask_for)(const TrackTexture *t))
{
    reflect_mask_for = mask_for;
    reflect_pass_forget_art();
}

static void reflect_register(const TrackArt *art)
{
    const int count = reflect_pass_register_art(art, reflect_mask_for);
    if (count >= 0) port_log("reflectors: %d textures with a mask\n", count);
}

/* the frame's state, before the track is drawn */
static void reflect_frame(const TrackArt *art)
{
    if (!port_reflectors() || art == NULL) {
        reflect_pass_forget_art();
        reflect_pass_frame(0, 0, 0, 1, 0, 0, NULL);
        return;
    }
    reflect_register(art);
    const LampEntries lamps = drive_lamps_get_entries();
    reflect_pass_set_entries(street_count > 0 ? street_entry : NULL, window_count > 0 ? window_entry : 0, lamps.brake_lit, lamps.position, lamps.headlight, lamps.reverse);
    reflect_pass_frame(reflect_lamps_on, reflect_x, reflect_z, reflect_hx, reflect_hz, reflect_dark, art->palette);
}

/* port (reflectors, reflect_pass.h): the step of shine (0 none) of the polygon being drawn: an object at its own place, a road polygon at the mean of its corners' ground places */
static int reflect_level_here(void)
{
    if (!reflect_pass_active()) return 0;
    double x, z, height = 0.0;
    if (track_port_object_at != NULL) {
        x = track_port_object_at[0] / 65536.0, z = track_port_object_at[2] / 65536.0;
        height = 1.0;   /* a sign's face is about this far over the foot the object is placed by */
    } else if (track_light_xz_ok) {
        x = (track_light_xz[0][0] + track_light_xz[1][0] + track_light_xz[2][0] + track_light_xz[3][0]) / 4.0;
        z = (track_light_xz[0][1] + track_light_xz[1][1] + track_light_xz[2][1] + track_light_xz[3][1]) / 4.0;
    } else return 0;
    return reflector_level(reflect_pass_gain(x, z, height));
}

/* 004f9e94 = 004254e8 -> 00425210: 00486950 / 00487180 */
static void draw_uv_base(void *user, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8])
{
    view_lamps_note(texture, uv, screen);
    street_halo_note(texture, screen);
    const uint8_t *lit = lit_table_for(texture);
    if (lit != NULL) quad_draw_mode(user, texture, uv, screen, transparent(texture) ? MAP_REMAP_TRANSPARENT : MAP_REMAP, lit);
    else quad_draw(user, texture, uv, screen, transparent(texture));
}

static void draw_uv(void *user, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8])
{
    draw_uv_base(user, texture, uv, screen);
    if (!reflect_pass_active()) return;
    const FshEntry *copy = reflect_pass_copy(texture);
    const int level = copy != NULL ? reflect_level_here() : 0;
    if (level > 0) quad_draw_mode(user, copy, uv, screen, MAP_REMAP_TRANSPARENT, reflect_pass_table(level));   /* port (reflectors) */
}

static int modern;  /* port: EXTRAS' Modern 3D (ported_view_set_modern) */

void ported_view_set_modern(int on) { modern = on != 0; }

/* port (weather, re/notes/weather-design.md): WEATHER_CLEAR runs none of it. The tables are made again when the palette or the
 * weather changes. */
static int weather;
int view_weather(void) { return weather; }   /* the frame's weather, for view_spray.c */
static ModernLook look;   /* port: the Texture Filter and Fog's look (the PS1 look's dark fog too: ported_view_set_dark_fog) */

static int track_snow;   /* port (snow_weather): this race's track is a snowy one with the switch on (ported_view_set_snow) */
void ported_view_set_snow(int snow) { track_snow = snow != 0; }
static int snow_level(void) { return weather_snow_level(weather, track_snow, 1); }   /* SNOW_NONE .. SNOW_HEAVY: what falls on this race */
void ported_view_set_weather(int w) { weather = w > WEATHER_CLEAR && w < WEATHER_COUNT ? w : WEATHER_CLEAR; }

static const WeatherTables *weather_tables(const TrackArt *art) { return weather_tables_for(art, weather, snow_level(), port_weather_fog(), &look); }

typedef struct {
    uint8_t *pixels;
    int32_t pitch, box[4];
    const uint8_t *table;
    int share;
} RainMood;

static void rain_mood_band(void *user, int top, int bottom)
{
    const RainMood *m = user;
    const int32_t box[4] = { m->box[0], top, m->box[2], bottom };
    weather_mood_rows(m->pixels, m->pitch, box, m->table, m->share);
}

static WeatherFrame weather_frame(void);   /* port (weather): the frame's weather, as weather_hooks.h asks for it */
static void storm_thunder_update(void);   /* port (rain_v2): the thunder after each strike */
static void rain_roll_update(void);       /* port (rain_tyre_roll): the wet-tyre roll of the followed car */
static void snow_roll_update(void);       /* port (snow crunch): the snowy rally's crunch of the followed car */
static int storm_flash_level(void);   /* port (WEATHER_STORM): the lightning's brightness this frame, 0 none */

int ported_view_modern(void) { return modern; }
int ported_view_rain(void) { return weather_wet(weather); }

static int rain_open = 16;  /* port (weather_zones) 0..16: how much weather there is where the eye is (rain_open_at), once a frame before the sky is drawn */
static int look_filter, look_fog;  /* port: EXTRAS' Texture Filter and Fog (ported_view_set_look) */
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
static PsxSkyDraw *psx_sky;        /* port: the PS1 sky (ported_view_set_psx_sky) */
void ported_view_set_psx_sky(PsxSkyDraw *sky) { psx_sky = sky; }
static const TrackInfo *psx_sky_info;   /* port: the alternative sky's INFO (ported_view_set_psx_sky_info) */
void ported_view_set_psx_sky_info(const TrackInfo *info) { psx_sky_info = info; }
const FshEntry *ported_view_sky_in_palette(void) { return psx_sky != NULL ? psx_sky_draw_in_palette(psx_sky) : NULL; }
static HorizonSmooth smooth;       /* port: the Texture Filter's table for the sky's palette (horizon_smooth.h) */

void ported_view_set_look(int filter, int fog) { look_filter = filter != 0, look_fog = fog != 0; }
int ported_view_filter_on(void) { return modern && look_filter; }
int ported_view_fog_on(void) { return modern && look_fog; }

/* port: a scenery board the exe drops for a corner past +-0x800 across (0044a450) while its middle shows (a big tree) is drawn clipped at the near
 * plane, perspective-correct; TNFS_WIDE_BOARD=0 the exe's drop */
static int wide_board_on(void)
{
    const char *env = getenv("TNFS_WIDE_BOARD");
    return env == NULL || atoi(env) != 0;
}

/* port: the view the 3D objects' pixels clip and project by when Modern 3D is on, else NULL (the exe's mappers) */
static const View *modern_view(const View *view) { return modern ? view : NULL; }

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

static void draw_camera(void *user, const View *view, const FshEntry *texture, const int32_t uv[8],
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

static void draw_full(void *user, const FshEntry *texture, const int32_t screen[8])
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

static void draw_full_alt(void *user, const FshEntry *texture, const int32_t screen[8])
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

/* port (the PS1 look's light): the track's light, or NULL for none (the frames are then the exe's, byte for byte) */
const PsxLight *psx_light;   /* the track's light (set below) */
static int light_changed;   /* the light editor's reload put new rows in the same light: the built track is built again */

void ported_view_light_changed(void) { light_changed = 1; }

void ported_view_set_light(const PsxLight *light, const float tint[3])
{
    psx_light = light;
    for (int k = 0; k < 3; k++) lit_tint[k] = light != NULL && tint != NULL ? tint[k] : 1.0f;
    lit_tinted = lit_tint[0] != 1.0f || lit_tint[1] != 1.0f || lit_tint[2] != 1.0f;
}

/* port (the PS1 light on the cars): the ambient colour by segment (a node is 4 to a segment) over the world's tint is the ratio each car's
 * colours are scaled by, as a table by the nearest palette entry; the ratio is kept to 1/16 and the tables cached by it */
enum { AMBIENT_TABLES = 8, AMBIENT_STEP = 16 };
static PsxLgs ambient;
static int ambient_on;
static int synth_on;   /* the light is one psx_light_synth made: the cars and the dash take its tunnel factor */
static uint32_t ambient_nodes;
static float ambient_tint[3] = { 1, 1, 1 };

void ported_view_set_psx_ambient(const PsxLgs *lgs, uint32_t ring_nodes, const float tint[3])
{
    ambient_on = lgs != NULL && tint != NULL && ring_nodes >= 4;
    if (!ambient_on) return;
    ambient = *lgs, ambient_nodes = ring_nodes;
    for (int k = 0; k < 3; k++) ambient_tint[k] = tint[k];
}

RaceState *shadow_race;

void ported_view_set_psx_synth(int on) { synth_on = on; }

/* port (the PS1 light on the cars and the dash): the exe's light of the road's centre at the car's own node (psx_lgs_car_ratio; the exe gives every car the
 * viewer's, the port its own node's), the one factor of the cars and the dash; 0 and `out` untouched when there is no light script. A context's colour is
 * its own tunnel's tint, and a RAMP context's odd / even nodes are the lamps' alternation. */
static int ratio_of(int32_t node, float out[3]);

/* port (psx_headlights): the nodes the road itself marks a tunnel (its property bit, runs of PSX_SYNTH_MIN_RUN and more: the wooden tunnels and sheds the CD's light
 * rows do not all mark), made once for the track; NULL when it has no data */
/* two callers ask with different counts (the light script's and the ring's): a slot each, or they would remake the flags every call, every frame;
 * dropped when the track is forgotten (the next track's race may sit at the same address with as many nodes) */
enum { TUNNEL_SLOTS = 2 };
static struct { uint8_t *flags; uint32_t have; const RaceState *race; } tunnel_slot[TUNNEL_SLOTS];
static int tunnel_next;

static uint8_t *rain_cover_flags;   /* rain_cover, below */
static void road_tunnels_forget(void)
{
    free(rain_cover_flags);
    rain_cover_flags = NULL;
    for (int i = 0; i < TUNNEL_SLOTS; i++) free(tunnel_slot[i].flags), tunnel_slot[i].flags = NULL;
}

const uint8_t *road_tunnels(const RaceState *race, uint32_t nodes)
{
    if (nodes == 0) return NULL;
    for (int i = 0; i < TUNNEL_SLOTS; i++)
        if (tunnel_slot[i].flags != NULL && tunnel_slot[i].have == nodes && tunnel_slot[i].race == race) return tunnel_slot[i].flags;
    const int at = tunnel_next;
    tunnel_next = (tunnel_next + 1) % TUNNEL_SLOTS;
    free(tunnel_slot[at].flags);
    tunnel_slot[at].flags = NULL;
    uint8_t *flags = malloc(nodes);
    uint8_t *property = malloc(nodes);
    if (flags == NULL || property == NULL) { free(property); free(flags); return NULL; }
    for (uint32_t i = 0; i < nodes; i++) property[i] = race_node(race, (int32_t)i).property;
    psx_light_synth_tunnels(property, nodes, 1, flags);
    free(property);
    tunnel_slot[at].flags = flags, tunnel_slot[at].have = nodes, tunnel_slot[at].race = race;
    return flags;
}

/* port (rain_fx): the nodes with a roof over the road for the rain (psx_light_synth_rain_cover), made once for the track like road_tunnels' */
static uint32_t rain_cover_have;
static const RaceState *rain_cover_race;

const uint8_t *rain_cover(const RaceState *race, uint32_t nodes)
{
    if (nodes == 0) return NULL;
    if (rain_cover_flags != NULL && rain_cover_have == nodes && rain_cover_race == race) return rain_cover_flags;
    free(rain_cover_flags);
    rain_cover_flags = malloc(nodes);
    uint8_t *property = malloc(nodes);
    if (rain_cover_flags == NULL || property == NULL) { free(property); free(rain_cover_flags); rain_cover_flags = NULL; return NULL; }
    for (uint32_t i = 0; i < nodes; i++) property[i] = race_node(race, (int32_t)i).property;
    psx_light_synth_rain_cover(property, nodes, 1, rain_cover_flags);
    free(property);
    rain_cover_have = nodes, rain_cover_race = race;
    return rain_cover_flags;
}

/* how far the car is on from its node to the next, out of 256 (its place projected on the road's step) */
static uint8_t headlight_frac(const RaceState *race, const Car *car)
{
    const TriNode a = race_node(race, car->node), b = race_node(race, car->node + 1);
    const double sx = (double)b.position.x - a.position.x, sz = (double)b.position.z - a.position.z;
    const double len2 = sx * sx + sz * sz;
    if (len2 <= 0.0) return 0;
    const double t = (((double)car->x - a.position.x) * sx + ((double)car->z - a.position.z) * sz) / len2;
    return (uint8_t)(t <= 0.0 ? 0 : t >= 1.0 ? 255 : t * 256.0);
}

/* port (headlight_cone): the nodes' centres (x, z in 16.16 metres), once per track: what the objects and the cars, lit by the node, take the cones at */
static const int32_t *cone_node_xz(const RaceState *race, uint32_t nodes)
{
    static int32_t *xz;
    static uint32_t have;
    static const RaceState *for_race;
    if (nodes == 0) return NULL;
    if (xz != NULL && have == nodes && for_race == race) return xz;
    free(xz);
    xz = malloc(sizeof *xz * 2 * nodes);
    if (xz == NULL) return NULL;
    for (uint32_t i = 0; i < nodes; i++) {
        const TriNode n = race_node(race, (int32_t)i);
        xz[2 * i] = n.position.x, xz[2 * i + 1] = n.position.z;
    }
    have = nodes, for_race = race;
    return xz;
}

/* port (headlight_cone): a car's heading as a unit vector on the ground from its yaw (24 bits a turn) */
static void cone_heading(const Car *car, double *hx, double *hz)
{
    const double a = (double)(car->yaw & 0xffffff) * (6.283185307179586 / 16777216.0);
    *hx = sin(a), *hz = cos(a);
}

/* port (psx_headlights): the tick each car's body was last drawn (its draw distance, the render scale's): a car that is not drawn does not light the road, so no lit road
 * shows far from a car that is not there yet. Stored as tick + 1: 0 is never */
int32_t body_drawn_tick[RACE_CARS];
static HeadlightPool *pool_fed;   /* the pool headlights_update last fed: its tunnel flags and node table are freed with the track, so it lets go of them then */

/* port (psx_headlights): the cars with their lights on, each frame - the active cars of the race, the player first - are the pools the road is lit in */
static void headlights_update(void)
{
    if (psx_light == NULL || psx_light->pool == NULL) return;
    HeadlightPool *pool = psx_light->pool;
    pool_fed = pool;
    static int32_t seen[RACE_CARS];
    static int8_t travel[RACE_CARS];
    static uint32_t of_ring;
    pool->count = 0;
    pool->ring = psx_light->nodes;
    reflect_lamps_on = 0;   /* port (reflectors): only the player's lamps */
    if (shadow_race == NULL) return;
    const int32_t ring = (int32_t)pool->ring;
    if (of_ring != pool->ring) of_ring = pool->ring, memset(seen, 0, sizeof seen), memset(travel, 0, sizeof travel);   /* another track */
    pool->tunnel = road_tunnels(shadow_race, psx_light->nodes);
    pool->cone = port_headlight_cone();
    pool->node_xz = pool->cone ? cone_node_xz(shadow_race, psx_light->nodes) : NULL;
    const int32_t n = shadow_race->car_count < RACE_CARS ? shadow_race->car_count : RACE_CARS;
    for (int32_t i = 0; i < n && pool->count < HEADLIGHT_MAX; i++) {
        const Car *car = shadow_race->cars[i];
        if (car == NULL || !(car->flags_51a & 4)) continue;
        if (i != race_slot(shadow_race->player)) {
            const int drawn = !(body_drawn_tick[i] == 0 || shadow_race->tick + 1 - body_drawn_tick[i] > 4 || shadow_race->tick + 1 < body_drawn_tick[i]);
            const Car *const player = shadow_race->cars[race_slot(shadow_race->player)];
            int32_t behind = player != NULL ? player->node - car->node : -1;
            if (ring > 0) behind = behind > ring / 2 ? behind - ring : behind < -ring / 2 ? behind + ring : behind;
            if (!headlight_car_lights(drawn, behind, pool->cone)) continue;   /* not drawn lately and not just behind the camera (headlight_cone): no light */
        }
        int32_t moved = car->node - seen[i];   /* which way the car travels: the nodes it moved over, the gear on the first frame */
        if (ring > 0) moved = moved > ring / 2 ? moved - ring : moved < -ring / 2 ? moved + ring : moved;
        if (travel[i] == 0) travel[i] = car->gear == LAMP_REVERSE_GEAR ? -1 : 1;
        if (moved != 0 && moved > -4 && moved < 4) travel[i] = moved > 0 ? 1 : -1;
        seen[i] = car->node;
        if (pool->cone) {   /* port (headlight_cone): the cone from the car's place along its nose */
            pool->cone_x[pool->count] = car->x / 65536.0, pool->cone_z[pool->count] = car->z / 65536.0;
            cone_heading(car, &pool->cone_hx[pool->count], &pool->cone_hz[pool->count]);
        }
        if (i == race_slot(shadow_race->player)) {   /* port (reflectors) */
            double hx, hz;
            cone_heading(car, &hx, &hz);
            reflect_lamps_on = 1, reflect_x = car->x / 65536.0, reflect_z = car->z / 65536.0, reflect_hx = hx, reflect_hz = hz;
            reflect_dark = drive_light_night() || headlight_pool_tunnel(pool, (uint32_t)car->node);
        }
        pool->cone_car[pool->count] = (int8_t)i;
        pool->sub[pool->count] = headlight_frac(shadow_race, car);
        pool->dir[pool->count] = travel[i];
        pool->node[pool->count++] = car->node;
    }
}

/* xz: the car's own place on the ground in metres, or NULL (the node's) */
static int ratio_at_xz(int32_t node, const double *xz, int car, float out[3])
{
    int found = ratio_of(node, out);
    found = headlight_pool_car_ratio_start(found, psx_light != NULL && psx_light->pool != NULL, port_car_night_lift(), out);
    /* port (car_night_lift): a light with no ratio for the car (no ambient, no synth: CL1's night) left `out` unset, and car_light made the car's table from 0: every texel but the paint's black, a wheel a flat disc. The car's own light is full, and the pool's gain below lights it as it lights the road */
    /* port (car_tunnel_match): a made tunnel with a look keeps the light its look gives (the world's tables are not scaled by the headlight pool's floor there, light_at_dark), so the car is not either: the pool's share took a further 60 % off the car */
    const int match_tunnel = found && port_car_tunnel_match() >= 1 && psx_light != NULL && psx_light->look_set && psx_light_tunnel(psx_light, psx_ring_node(node, psx_light->nodes));
    if (found && !match_tunnel && psx_light != NULL && psx_light->pool != NULL) {   /* port (psx_headlights): a car in the dark or in a pool takes its light, as the road under it does */
        const uint32_t at = psx_ring_node(node, psx_light->nodes);
        int g;
        if (psx_light->pool->cone && xz != NULL) {   /* port (headlight_cone): by where the car is in every car's patch of light, its own included (the glow round it): rivals, police and the player alike */
            const int lift = headlight_pool_car_lift_xz(psx_light->pool, car, xz[0], xz[1]);   /* its own patch limited, the others' added (i097) */
            g = headlight_pool_car_gain(headlight_pool_gain_at_lift(psx_light_tunnel(psx_light, at) ? TUNNEL_FLOOR : HEADLIGHT_FLOOR, lift));
        } else {
            g = headlight_pool_car_at(psx_light->pool, at) ? 256 : headlight_pool_car_gain(psx_light_tunnel(psx_light, at) ? headlight_pool_gain_tunnel(psx_light->pool, at) : headlight_pool_gain(psx_light->pool, at));   /* a car with its lights on keeps its own light: the beam is ahead of it */
        }
        g = headlight_pool_car_night_lift(g, port_car_night_lift());   /* port (car_night_lift) */
        const float gain = (float)g / 256.0f;
        for (int k = 0; k < 3; k++) out[k] *= gain;
        if (port_lamp_car_shade()) (void)psx_light_car_pool(psx_light, at, out);   /* port (lamp_car_shade): and the street lamp's pool lights it */
    }
    if (!found && psx_light != NULL && psx_light->car_pool != NULL && port_lamp_car_shade()) {   /* port (lamp_car_shade): a made light with no tunnels gives the car no ratio, so the lamps' pools had nothing to lift: the car's own is neutral, the pool's warm gain on it */
        static int mode = -1;
        if (mode < 0) mode = getenv("TNFS_LAMP_CAR_MODE") ? atoi(getenv("TNFS_LAMP_CAR_MODE")) : 1;
        const uint32_t at = psx_ring_node(node, psx_light->nodes);
        out[0] = out[1] = out[2] = 1.0f;
        if (mode == 2) {   /* steady: the nearest lamp is within 8 nodes either side, the lift is the weakest pool's all the way along the lamps' stretch */
            int near = 0;
            for (int d = -8; d <= 8 && !near; d++) near = psx_light->car_pool[psx_ring_node((int32_t)at + d, psx_light->nodes)] != 0;
            if (near) {
                PsxLight steady = *psx_light;
                uint8_t keep = 2;
                steady.car_pool = &keep;
                steady.nodes = 1;
                return psx_light_car_pool(&steady, 0, out);
            }
            return 0;
        }
        return psx_light_car_pool(psx_light, at, out);
    }
    if (psx_light != NULL && psx_light->lamp_n > 0 && xz != NULL) {   /* port (`lamp`): the street lamps add their light to the car by its distance to each */
        float lit[3] = { found ? out[0] : 1.0f, found ? out[1] : 1.0f, found ? out[2] : 1.0f };
        if (psx_light_lamp_car(psx_light, psx_ring_node(node, psx_light->nodes), xz[0], xz[1], lit)) {
            memcpy(out, lit, sizeof lit);
            return 1;
        }
    }
    return found;
}

/* a car's ratio at its own place */
static int ratio_of_car(const Car *car, float out[3])
{
    const double xz[2] = { car->x / 65536.0, car->z / 65536.0 };
    return ratio_at_xz(car->node, xz, car->index, out);
}

static const float CANYON_SHADE_FACTOR = 0.70f;   /* port (canyon_shade): the owner's pick, 30 % darker */

static int ratio_of(int32_t node, float out[3])
{
    if (synth_on && psx_light != NULL) {   /* port: a light made from the road (psx_light_synth.h): no script, the tunnel's lamp alternation */
        const uint32_t at = psx_ring_node(node, psx_light->nodes);
        if (port_car_tunnel_light() || psx_light->from_file) psx_light_synth_car_tunnel_match(psx_light, at, port_car_tunnel_match(), (lit_tint[0] + lit_tint[1] + lit_tint[2]) / 3.0f, out);   /* port (car_tunnel_light, car_tunnel_match) */
        else psx_light_synth_car(psx_light, at, out);
        if (psx_light->canyon != NULL && psx_light->canyon[psx_ring_node(node, psx_light->nodes)]) {   /* port (canyon_shade): the walls shade the car in the canyon */
            const float f = port_canyon_shade() || psx_light->from_file ? CANYON_SHADE_FACTOR : 1.0f;
            for (int k = 0; k < 3; k++) out[k] *= f;
        }
        return 1;
    }
    if (!ambient_on || psx_light == NULL) return 0;
    const int32_t at = (int32_t)psx_ring_node(node, ambient_nodes);
    /* the script's blocks are of the file's direction: a reversed track asks them at the node's old place */
    psx_lgs_car_ratio(&ambient, ambient_nodes, (int32_t)psx_light_old_node(psx_light, (uint32_t)at),
                      psx_light_byte(psx_light, (uint32_t)at, 0), AMBIENT_STEP, out);
    return 1;
}

int ported_view_car_ratio(const Car *car, float out[3])
{
    return car != NULL && ratio_of_car(car, out);
}

static int police_flash_on;   /* port (psx_light): the police car's lights flash (police_lights.h) */
void ported_view_set_police_lights(int on) { police_flash_on = on; }

/* the table for a car at `node`, NULL when its ambient is the world's */
static const uint8_t *car_light(int32_t node, const double *xz, int car, const uint8_t palette[768])
{
    if (psx_light == NULL || palette == NULL || !(ambient_on || synth_on || (port_lamp_car_shade() && psx_light->car_pool != NULL) || psx_light->lamp_n > 0 || psx_light->car_tone != NULL)) return NULL;   /* port (lamp_car_shade): a made light with no tunnels has the lamps' pools to give its cars too */
    static struct { int key[8]; uint8_t table[256]; int used; } cache[AMBIENT_TABLES];   /* the ratio and the car's tone (track light file's car_tone) */
    static uint8_t made_for[768];
    static int next, valid;
    float ratio[3] = { 0.0f, 0.0f, 0.0f };   /* what a light with no ratio for the car left in it (found 0 left it unset: read as 0, so the car was black), now said */
    ratio_at_xz(node, xz, car, ratio);
    int key[8] = { 0 }, neutral = 1;
    const int floor_level = psx_light->pool != NULL ? port_car_night_lift() : 0;   /* port (car_night_lift): at night (the headlight pool), a floor of light on the body */
    key[7] = floor_level, neutral = floor_level == 0;
    const int match_bright = (port_car_tunnel_match() >= 1 || port_car_night_lift() >= 1) && (ratio[0] > 1.0f || ratio[1] > 1.0f || ratio[2] > 1.0f);   /* port (car_tunnel_match): above full light the table has no family */
    for (int k = 0; k < 3; k++) {
        key[k] = (int)(ratio[k] * AMBIENT_STEP + 0.5f);
        neutral = neutral && key[k] == AMBIENT_STEP;
    }
    if (psx_light->car_tone != NULL && psx_light->nodes > 0) {   /* port (track light file, car_tone): the car's own tone at this node */
        const uint8_t *tone = psx_light->car_tone + 4 * (size_t)psx_ring_node(node, psx_light->nodes);
        if (tone[3] > 0) {
            for (int k = 0; k < 4; k++) key[3 + k] = tone[k];
            neutral = 0;
        }
    }
    if (neutral) return NULL;
    if (!valid || memcmp(made_for, palette, sizeof made_for) != 0) {   /* a fade changed the palette */
        memset(cache, 0, sizeof cache);
        memcpy(made_for, palette, sizeof made_for), valid = 1, next = 0;
    }
    for (int i = 0; i < AMBIENT_TABLES; i++)
        if (cache[i].used && memcmp(cache[i].key, key, sizeof key) == 0) return cache[i].table;
    const int slot = next;
    next = (next + 1) % AMBIENT_TABLES;
    memcpy(cache[slot].key, key, sizeof key), cache[slot].used = 1;
    static const uint8_t FLOOR[3][3] = { { 0, 0, 0 }, { 8, 9, 12 }, { 16, 18, 24 } };   /* a cool light, as the night's */
    psx_lit_table_floor(palette, ratio, match_bright, floor_level > 0 && floor_level < 3 ? FLOOR[floor_level] : NULL, cache[slot].table);
    if (key[6] > 0) {
        const uint8_t rgb[3] = { (uint8_t)key[3], (uint8_t)key[4], (uint8_t)key[5] };
        psx_car_tone_table(palette, cache[slot].table, rgb, key[6], cache[slot].table);   /* in place: each entry reads only its own */
    }
    return cache[slot].table;
}

/* the table `car` is drawn through: its ambient's (car_light), and for the police car of a chase on, with its lights going to pure red and blue
 * past the light (police_lights.h) */
static const uint8_t *car_table(const Car *car, const uint8_t palette[768])
{
    const double xz[2] = { car->x / 65536.0, car->z / 65536.0 };
    const uint8_t *base = car_light(car->node, xz, car->index, palette);
    if (!police_flash_on || palette == NULL || !police_lights_on(shadow_race, car)) return base;
    static uint8_t table[256];
    police_lights_table(base, palette, shadow_race->tick, table);
    return table;
}

/* Scenery models through the ported 3D engine (00458c04), one object per
 * model as the race keeps them. */
static const ViewModel *models;
static Object3D objects[PORTED_VIEW_MODELS];
static int object_ok[PORTED_VIEW_MODELS];
ModelView model_view;
ObjectSink object_pixels;
static int models_mirrored;

static void draw_model(void *user, int model, const int32_t position[3], uint32_t heading)
{
    (void)user;
    if (model < 0 || model >= PORTED_VIEW_MODELS || !object_ok[model]) return;
    const Object3DSink sink = object_sink(&object_pixels);
    object_sink_set_light(lit_table());
    model_draw_object(&objects[model], position, (int32_t)heading, &model_view, &sink);
    object_sink_set_light(NULL);
}

void ported_view_set_models(const ViewModel *list, const int *ok, int mirrored, int two_sided, int close_houses)
{
    if (list == models && mirrored == models_mirrored) return;
    for (int i = 0; i < PORTED_VIEW_MODELS; i++) {
        if (object_ok[i]) object3d_free(&objects[i]);
        object_ok[i] = list != NULL && ok[i] && object3d_create(&objects[i], &list[i].mesh);
        if (object_ok[i] && list[i].has_textures) object3d_bind(&objects[i], &list[i].textures);
        if (object_ok[i] && close_houses) object3d_close_houses(&objects[i]);   /* port (close_houses): before the mirror, so the new faces follow it */
        if (object_ok[i] && mirrored) object3d_mirror(&objects[i]);   /* 004576f0 on 004fbad8 */
        if (object_ok[i] && two_sided) object3d_both_sides(&objects[i]);   /* port (two_sided_scenery): the open models from behind too */
    }
    models = list, models_mirrored = mirrored;
}

/* The cars on the road, each drawn with its row (0045a848). Slot 0 is
 * also the parked car of ported_view_render. */
typedef struct {
    Object3D objects[2];
    Object3D *high, *low;   /* objects, or the borrowed ones */
    CarModel model;
    int ok;
    int borrowed;
    CarPose pose;
    int32_t node;
    Car *car;
} ViewCar;

static ViewCar cars[PORTED_VIEW_CARS];
static int car_count, car_ahead;

static void release_car(ViewCar *c)
{
    if (c->ok) {
        car_model_free(&c->model);
        if (!c->borrowed) {
            object3d_free(&c->objects[0]);
            object3d_free(&c->objects[1]);
        }
    }
    c->ok = c->borrowed = 0;
    c->high = c->low = NULL;
}

void ported_view_set_car_slot(int slot, const ViewModel *high, const ViewModel *low)
{
    if (slot < 0 || slot >= PORTED_VIEW_CARS) return;
    ViewCar *c = &cars[slot];
    release_car(c);
    if (high == NULL || low == NULL) return;
    if (!object3d_create(&c->objects[0], &high->mesh)) return;
    if (!object3d_create(&c->objects[1], &low->mesh)) {
        object3d_free(&c->objects[0]);
        return;
    }
    if (high->has_textures) object3d_bind(&c->objects[0], &high->textures);
    if (low->has_textures) object3d_bind(&c->objects[1], &low->textures);
    c->high = &c->objects[0], c->low = &c->objects[1];
    c->ok = car_model_init(&c->model, c->high, c->low);
}

void ported_view_set_car_objects(int slot, Object3D *high, Object3D *low)
{
    if (slot < 0 || slot >= PORTED_VIEW_CARS) return;
    ViewCar *c = &cars[slot];
    release_car(c);
    if (high == NULL || low == NULL) return;
    c->high = high, c->low = low, c->borrowed = 1;
    c->ok = car_model_init(&c->model, high, low);
    body_shine_enable(port_body_shine());   /* port (body shine): the bodies keep vertex normals for the shine's reflections */
    if (c->ok && body_shine_enabled()) object3d_enable_normals(high), object3d_enable_normals(low);
}

Object3D *ported_view_car_object(int slot)
{
    return slot >= 0 && slot < PORTED_VIEW_CARS && cars[slot].ok ? cars[slot].high : NULL;
}

void ported_view_set_car(const ViewModel *high, const ViewModel *low, int ahead)
{
    ported_view_set_car_slot(0, high, low);
    car_ahead = ahead;
}

RaceState *shadow_race;
static const uint8_t *shadow_remap;

void ported_view_set_shadows(RaceState *race, const uint8_t *remap)
{
    shadow_race = race, shadow_remap = remap;
}

static void shadow_contact(void *ctx, Car *car)
{
    (void)ctx;
    car_road_contact(car, shadow_race, 1);  /* 00428918(car, 1) */
}

/* 00517620..: the followed car's skid marks; 0051c7cf the rows holding one. */
static SkidMarks skid_marks;

SkidMarks *ported_view_skids(void)
{
    return &skid_marks;
}
static int32_t skid_node;   /* the camera's node (chase +0x38) */

enum { SKID_HIGH_RES = 1 };  /* 004fa0c0, as drive_hud's HIGH_RES */

static SkidRing skid_ring(void)
{
    return (SkidRing){ 0, (int32_t)shadow_race->tri->ring_nodes - 1, shadow_race->segments };
}

static SkidView skid_view(void)
{
    return (SkidView){ object_pixels.target, shadow_remap, &model_view, SKID_HIGH_RES, 0 };
}

/* 004596a8's 00459eb8(index): the newest marks, as they are laid. */
static void skid_draw_new(void *ctx, int32_t index)
{
    (void)ctx;
    const SkidView v = skid_view();
    const SkidRing ring = skid_ring();
    skid_draw(&v, &ring, &skid_marks, index, 0, 0, 0, NULL, NULL);
}

static void shadow_followed(void *ctx, Car *car, const int32_t f[3], const int32_t s[3], const int32_t p[3])
{
    (void)ctx;
    const int32_t surface = car->surface;
    const SkidInputs in = { 0, shadow_race->tick, shadow_race->surfaces[surface].off_road };
    const SkidOps ops = { NULL, skid_draw_new };
    skid_record(&ops, &skid_marks, car, &in, f, s, p);
}

/* 0044b87d: 00459eb8(-999, camera node, row; s, t). */
static void draw_skids(void *user, int32_t row, const int32_t s[2], const int32_t t[2])
{
    (void)user;
    if (shadow_race == NULL || shadow_remap == NULL) return;
    const SkidView v = skid_view();
    const SkidRing ring = skid_ring();
    skid_draw(&v, &ring, &skid_marks, -999, skid_node, row, 0, s, t);
}

/* 0051c7ac: the rows ahead of the camera holding a car (00459d70). */
static uint8_t ahead_rows[ROW_CARS_AHEAD];
/* port (port_follow_camera): the rows behind the camera's node holding a car, for the arm of road behind it (row_cars.h), and whether the arm is drawing */
static uint8_t behind_rows[ROW_CARS_BEHIND];
static int arm_drawing;

void ported_view_frame_rows(uint8_t behind[MIRROR_CAR_ROWS])
{
    if (shadow_race == NULL) return;
    const SkidRing ring = skid_ring();
    skid_rows(&skid_marks, &ring, shadow_race->chase.node);
    car_rows_ahead(shadow_race->cars, shadow_race->car_count, shadow_race->segments, shadow_race->chase.node, ahead_rows,
                   behind);
    if (shadow_race->port_follow_camera)
        car_rows_behind(shadow_race->cars, shadow_race->car_count, shadow_race->segments, shadow_race->chase.node, behind_rows);
}

/* port: the mirror's rows again for the camera's node of this frame, the cars not moved on (mirror_car_rows_at) */
void ported_view_mirror_rows_again(uint8_t behind[MIRROR_CAR_ROWS])
{
    if (shadow_race == NULL) return;
    mirror_car_rows_at(shadow_race->cars, shadow_race->car_count, shadow_race->segments, shadow_race->chase.node, behind);
}

const Car *followed_car(void)
{
    return shadow_race->cars[race_slot(shadow_race->player)];
}

/* 00513e88 / 00514298, *004c795c: NULL until the race has its smoke. */
const PuffArt *puff_art;
const PuffBlend *puff_blend;
static int32_t (*puff_rand)(void *ctx, int32_t n);
static void *puff_rand_ctx;
static CrashTrail *puff_trail;   /* 005146ac */

void ported_view_set_puffs(const PuffArt *art, const PuffBlend *blend, int32_t (*rand_below)(void *ctx, int32_t n),
                           void *ctx, CrashTrail *trail)
{
    puff_art = art, puff_blend = blend, puff_rand = rand_below, puff_rand_ctx = ctx, puff_trail = trail;
}

PuffView puff_view(void)
{
    PuffView v = { object_pixels.target, puff_blend->rows, { 0 }, { 0 }, SKID_HIGH_RES, puff_rand_ctx, puff_rand };
    for (int k = 0; k < PUFF_SHAPES; k++) v.smoke[k] = puff_art->smoke[k], v.dirt[k] = puff_art->dirt[k];
    return v;
}

static void puff_1(void *ctx, int32_t x, int32_t y, int32_t size)
{
    (void)ctx;
    const PuffView v = puff_view();
    puff_smoke(&v, x, y, size);
}

static void puff_other(void *ctx, int32_t x, int32_t y, int32_t size)
{
    (void)ctx;
    const PuffView v = puff_view();
    puff_dirt(&v, x, y, size);
}

/* 004588ac */
static void smoke_puffs(void *ctx, int32_t corner, int32_t size)
{
    (void)ctx;
    const SkidPuffOps ops = { NULL, puff_1, puff_other };
    skid_puffs(&ops, &skid_marks, shadow_race->tick, corner, size);
}

/* 004587ac */
static int32_t smoke_view_angle(void *ctx, const Car *car)
{
    (void)ctx;
    return car_view_angle(car, shadow_race);
}

/* port (rally_gravel.h): the dust of the followed car, a trail (dust_trail.h): puffs born at the rear wheels that stay where they were dropped, low,
 * rising and spreading a little, fading behind it. Each is a smoke puff over a few of the exe's own gravel puffs (puff_dirt, dir1..4), so
 * it reads as grit in a haze; the screen's place is the car's own world-to-screen (render_point). */
static int32_t last_near_dir[3];   /* the main view's near plane (set by the render, used by render_point and the dust) */
static int32_t last_near;

int world_screen(const int32_t place[3], int32_t camera[3], int32_t screen[2])
{
    TrackCamera cam = { { 0 }, { 0 }, 0 };
    memcpy(cam.position, model_view.camera_position, sizeof cam.position);
    memcpy(cam.matrix, model_view.camera_matrix, sizeof cam.matrix);
    DrawContext dc = { 0 };
    dc.view = model_view.view, dc.world_shift = WORLD_SHIFT;
    memcpy(dc.near_dir, last_near_dir, sizeof dc.near_dir);
    dc.near = last_near;
    track_point_screen(&cam, &dc, place, camera, screen);
    return camera[2] > 0;
}


/* 00458988 */
static void smoke_draw(void *ctx, int in_car)
{
    const TyreSmokeOps ops = { puff_rand_ctx, smoke_view_angle, puff_rand, smoke_puffs };
    tyre_smoke(&ops, (const Car *)ctx, in_car, shadow_race->camera_roll_add);
    const Car *const car = ctx;
    int slot = -1;
    for (int32_t i = 0; i < RACE_CARS && slot < 0; i++)
        if (shadow_race->cars[i] == car) slot = (int)i;
    if (slot >= 0 && (car == followed_car() || (shadow_race->port_rally_road != 0 && port_rally_spray()))) view_spray_dust_draw(car, slot);   /* port (rally_spray): every car's dust, not only the followed one's */
}



/* port (psx_shadow): the soft shadow's flag and its half-way remap, made from the palette and the exe's shade table when either changes */
static int soft_shadow_on;

void ported_view_set_soft_shadow(int on) { soft_shadow_on = on != 0; }

static const uint8_t *soft_half(void)
{
    static uint8_t half[256], made_from[256], made_for[768];
    static int valid;
    if (!valid || memcmp(made_from, shadow_remap, sizeof made_from) != 0 || memcmp(made_for, object_pixels.palette, sizeof made_for) != 0) {
        soft_shadow_half(object_pixels.palette, shadow_remap, half);
        memcpy(made_from, shadow_remap, sizeof made_from), memcpy(made_for, object_pixels.palette, sizeof made_for);
        valid = 1;
    }
    return half;
}

/* port (lamp_shadow): the half-way remap made in the palette as it is shown (drive_light_palette), not the art's. The night palette leaves some entries (the lit board's cream) as they are while it darkens the rest,
 * so the entry nearest the half-way colour in the art's palette can be a bright one on screen: under a lamp, where the road's indexes are the bright ones, that made the shadow a flat cream box. */
static const uint8_t *soft_half_shown(void)
{
    static uint8_t half[256], made_from[256], made_for[768];
    static int valid;
    uint8_t shown[768];
    drive_light_palette(object_pixels.palette, shown);
    if (!valid || memcmp(made_from, shadow_remap, sizeof made_from) != 0 || memcmp(made_for, shown, sizeof made_for) != 0) {
        soft_shadow_half(shown, shadow_remap, half);
        memcpy(made_from, shadow_remap, sizeof made_from), memcpy(made_for, shown, sizeof made_for);
        valid = 1;
    }
    return half;
}

/* port (lamp_shadow): the car stands in a street lamp's light (the same test as the car's own lamp light, psx_light_lamp_car) */
static int lamp_on_car(const Car *car)
{
    if (psx_light == NULL || psx_light->lamp_n <= 0) return 0;
    float lit[3] = { 1.0f, 1.0f, 1.0f };
    return psx_light_lamp_car(psx_light, psx_ring_node(car->node, psx_light->nodes), car->x / 65536.0, car->z / 65536.0, lit);
}

/* port (tunnel look): the car stands in a tunnel whose look lights the road (a file's tunnel_tone): its road's indexes are the shown palette's bright ones too, so the half-way remap is made in the shown palette as lamp_shadow's is
 * (above full light the road was the palette's brightest entries and the shadow a white box) */
static int look_tunnel_on_car(const Car *car)
{
    if (psx_light == NULL) return 0;   /* the soft shadow needs no light (psx_shadow is its own switch): none loaded, no tunnel look */
    return psx_light_look_tunnel(psx_light, psx_ring_node(car->node, psx_light->nodes));
}

/* 0045a724 */
static void row_prepare(void *ctx, Car *car, int pass)
{
    (void)ctx;
    const CarShadowOps ops = { NULL, shadow_contact, shadow_followed, NULL, 0 };
    int32_t screen[8];
    if (soft_shadow_on && shadow_remap != NULL && object_pixels.palette != NULL) {   /* port: the quad's corners, the shade not drawn (remap NULL) */
        const CarShadowOps cut = { NULL, shadow_contact, shadow_followed, follow_arm_clip_quad, 1 };   /* and the quad cut at the near plane, not lost */
        if (car_prepare(&cut, car, &model_view, pass, car == followed_car(), object_pixels.target, NULL, screen))
            soft_shadow_draw(object_pixels.target, shadow_remap, lamp_on_car(car) || look_tunnel_on_car(car) ? soft_half_shown() : soft_half(), soft_shadow_level(psx_light_node(psx_light, (uint32_t)car->node)), screen);
        return;
    }
    car_prepare(&ops, car, &model_view, pass, car == followed_car(), object_pixels.target, shadow_remap, screen);
}

/* 0050b7d4 / 0050b7b4 */
static const uint8_t *tag_colours;
static int tag_colour_count;
static uint32_t tag_outline;

void ported_view_set_tags(const uint8_t *colours, int count, uint32_t outline)
{
    tag_colours = colours, tag_colour_count = count, tag_outline = outline;
}

static void tag_polygon(void *ctx, uint32_t colour, int count, const int32_t *points)
{
    flat_polygon(ctx, colour, count, points);
}

static void tag_line(void *ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t colour)
{
    flat_line(ctx, x0, y0, x1, y1, colour);
}

/* 004591d4 / 00458d4c on the slot's CarModel: car +0x52e / +0x666 hold no handles in the port, so the model is the
 * far one while +0x78e is set, and the parts come in 00458d4c's order (inside, surface, outside, convex, those below
 * 8), as CarModel keeps its sections. */
typedef struct {
    ViewCar *vc;
    Car *car;
    const Object3DSink *sink;
    const ModelView *mv;        /* the main view's, or the mirror's (0040586c sets 00511fe0.. for either) */
    const QuadTarget *target;   /* where the marks go */
    int32_t eye[3];
    int part;
} BodyCtx;

static const int32_t IDENTITY[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };

static Object3D *body_model(const BodyCtx *b) { return b->car->far != 0 ? b->vc->model.far : b->vc->model.body; }

static void body_eye(void *ctx, const int32_t eye[3]) { memcpy(((BodyCtx *)ctx)->eye, eye, sizeof ((BodyCtx *)ctx)->eye); }
static void body_matrix(void *ctx, uint32_t object, const void *m) { (void)object, object3d_set_matrix(body_model(ctx), m); }
static void body_transform(void *ctx, uint32_t object)
{
    (void)object, object3d_transform(body_model(ctx), IDENTITY, ((BodyCtx *)ctx)->eye, 0);
    object3d_update_normals(body_model(ctx));   /* port (body shine): none kept unless the flag is on */
}
static void body_project(void *ctx, uint32_t object) { (void)object, object3d_project(body_model(ctx), ((BodyCtx *)ctx)->mv->view); }
static void body_sort(void *ctx, uint32_t object) { (void)object, object3d_sort(body_model(ctx)); }
static void body_draw(void *ctx, uint32_t object)
{
    (void)object, object3d_draw(body_model(ctx), MODEL_NEAR_Z, ((BodyCtx *)ctx)->sink);
}

static int32_t part_of(const Car *car, int k)
{
    const int32_t parts[CAR_SECTIONS] = { car->inside_part, car->surface_part, car->outside_part, car->convex_part };
    return parts[k];
}

/* the next part below 8 from b->part on (004918e1 and 00491c8a come in pairs) */
static Object3DList *body_list(BodyCtx *b)
{
    while (b->part < CAR_SECTIONS && (uint32_t)part_of(b->car, b->part) >= 8) b->part++;
    return b->part < CAR_SECTIONS ? &b->vc->model.sections[b->part] : NULL;
}
static void body_sort_list(void *ctx, uint32_t object, int32_t count, uint32_t list)
{
    (void)object, (void)count, (void)list;
    Object3DList *l = body_list(ctx);
    if (l != NULL && l->order != NULL) object3d_sort_list(body_model(ctx), l);
}
static void body_draw_list(void *ctx, uint32_t object, int32_t count, uint32_t list)
{
    (void)object, (void)count, (void)list;
    BodyCtx *b = ctx;
    const Object3DList *l = body_list(b);
    if (l != NULL && l->order != NULL) object3d_draw_list(body_model(b), l, MODEL_NEAR_Z, b->sink);
    b->part++;
}

/* 00458614 / 00458818: the viewer's hooks (track_viewer.c keeps when the wheels turn). */
static void (*wheel_frames_hook)(void *ctx, Car *car);
static void (*steer_hook)(void *ctx, Car *car);
static void *wheel_ctx;

void ported_view_set_wheels(void (*frames)(void *ctx, Car *car), void (*steer)(void *ctx, Car *car), void *ctx)
{
    wheel_frames_hook = frames, steer_hook = steer, wheel_ctx = ctx;
}

static void body_wheels(void *ctx, Car *car) { (void)ctx; if (wheel_frames_hook != NULL) wheel_frames_hook(wheel_ctx, car); }
static void body_steer(void *ctx, Car *car) { (void)ctx; if (steer_hook != NULL) steer_hook(wheel_ctx, car); }

static void body_smoke(void *ctx, Car *car, int32_t in_car)
{
    (void)ctx;
    if (puff_art != NULL && puff_blend != NULL && puff_rand != NULL) smoke_draw(car, in_car);
}

/* 00476460 and 0051c798; 0051c7f8 / 004c5820 the police flash's last turn and phase */
static void (*light_write)(void *ctx, int32_t first, int32_t count, const uint8_t *colours);
static void *light_ctx;
static const uint8_t *light_slots;
static int32_t flash_tick, flash = 1;

void ported_view_set_lights(void (*write)(void *ctx, int32_t first, int32_t count, const uint8_t *colours), void *ctx,
                            const uint8_t *slot_colours)
{
    light_write = write, light_ctx = ctx, light_slots = slot_colours;
}

static void body_palette(void *ctx, int32_t first, int32_t count, const uint8_t *colours)
{
    (void)ctx;
    if (light_write != NULL) light_write(light_ctx, first, count, colours);
}

static const uint8_t NO_SLOT_COLOURS[RACE_CARS + 1];

/* port (handbrake_lamps): the brake lamps are lit by the brake above LAMP_BRAKE_ON, but not by one that is the handbrake's alone (the rally's handbrake is a full brake, car_physics.c) */
static int brake_lamp(const Car *car)
{
    if (car->brake <= LAMP_BRAKE_ON) return 0;
    const int by_handbrake = shadow_race != NULL && car->index >= 0 && car->index < RACE_CARS && shadow_race->port_hb_lamp[car->index];
    return !(by_handbrake && port_handbrake_lamps());
}

/* port (emissive lamps): the car's lamp texels as the light at the car is (the day's, or glowing, lit by its brake), before it is drawn */
void ported_view_set_flares(int on) { view_lamps_set_flares(on); }

static void lamp_state(const Car *car)
{
    float ratio[3] = { 1, 1, 1 }, tint[3] = { 1, 1, 1 };
    ratio_of_car(car, ratio);
    drive_light_tint(tint);
    if (view_lamps_flares() && weather == WEATHER_FOG)   /* port (psx_flares): the lamps glow in fog too */
        drive_lamps_frame(car->index, 1, brake_lamp(car), car->gear == LAMP_REVERSE_GEAR);
    else   /* port (rain_lamps): and in rain, day included: by day or dusk only the texels (force 2), no halo; at night as the light says */
        drive_lamps_light_forced(car->index, car->node, tint, ratio, brake_lamp(car), car->gear == LAMP_REVERSE_GEAR,
                                 weather_wet(weather) && port_rain_lamps() ? (drive_light_night() ? 1 : 2) : 0);
}

/* 00458d4c(car, 0051c774, +0x391) */
static unsigned body_draws;   /* port: the bodies drawn so far; a pass that drew none left no screen corners to flare from */
static void render_body(void *ctx, Car *car, const int32_t matrix[9], const int32_t position[3])
{
    BodyCtx *b = ctx;
    const RaceState *race = shadow_race;
    const CarBodyOps ops = { b, body_eye, body_matrix, body_transform, body_project, body_sort, body_draw, body_sort_list,
                             body_draw_list, body_wheels, body_steer, body_smoke, body_palette };
    const CarLightInputs in = { { race->stop_notice[0], race->stop_notice[1] },
                                light_slots != NULL ? light_slots : NO_SLOT_COLOURS, race->finish_cars, race->tick,
                                &flash_tick, &flash };
    lamp_state(car);
    body_draws++;
    if (car->index >= 0 && car->index < RACE_CARS) body_drawn_tick[car->index] = race->tick + 1;   /* port (psx_headlights): this car was drawn: its pool may light the road */
    b->part = 0;
    car_body_draw(&ops, &in, car, followed_car(), matrix, position);
}

/* the object's +0x10 bit 1, on both models (the port's handles do not tell them apart) */
static void render_near(void *ctx, uint32_t object, int on)
{
    (void)object;
    const BodyCtx *b = ctx;
    Object3D *models[2] = { b->vc->model.body, b->vc->model.far };
    for (int i = 0; i < 2; i++) {
        if (models[i] == NULL) continue;
        if (on) models[i]->flags |= OBJECT3D_FLAG_NEAR;
        else models[i]->flags &= ~(uint32_t)OBJECT3D_FLAG_NEAR;
    }
}

/* the near model's projected point (its +0x7c) */
static void render_vertex(void *ctx, uint32_t object, int32_t index, int32_t out[2])
{
    (void)object;
    const Object3D *body = ((BodyCtx *)ctx)->vc->model.body;
    const int in_range = index >= 0 && (uint32_t)index < body->vertex_count;
    out[0] = in_range ? body->screen[index][0] : 0;
    out[1] = in_range ? body->screen[index][1] : 0;
}

/* 0044a1c4 with the pass's camera; the near plane is the main view's last (the mirror's own is not kept here) */
static void render_point(void *ctx, Car *car, int32_t camera[3], int32_t screen[2])
{
    const ModelView *mv = ((BodyCtx *)ctx)->mv;
    TrackCamera cam = { { 0 }, { 0 }, 0 };
    memcpy(cam.position, mv->camera_position, sizeof cam.position);
    memcpy(cam.matrix, mv->camera_matrix, sizeof cam.matrix);
    DrawContext dc = { 0 };
    dc.view = mv->view, dc.world_shift = WORLD_SHIFT;
    memcpy(dc.near_dir, last_near_dir, sizeof dc.near_dir);
    dc.near = last_near;
    const int32_t place[3] = { car->x, car->y, car->z };
    track_point_screen(&cam, &dc, place, camera, screen);
}

/* 00456f34(005146ac, offset) */
static void render_trail(void *ctx, const int32_t offset[2])
{
    const ModelView *mv = ((BodyCtx *)ctx)->mv;
    if (puff_trail == NULL || puff_art == NULL || puff_art->smok == NULL) return;
    const CrashTrailView v = { mv->view, mv->camera_position, mv->camera_matrix, WORLD_SHIFT, NULL, view_spray_trail_puff };
    crash_trail_draw(puff_trail, offset, &v);
}

static uint8_t tags_hidden;  /* 004fb78b */

void ported_view_set_tags_hidden(int hidden)
{
    tags_hidden = (uint8_t)hidden;
}

/* 00459064 */
static void render_tag(void *ctx, int32_t x, int32_t y, int32_t slot, int32_t z)
{
    if (tag_colours == NULL || slot < 0 || slot >= tag_colour_count) return;
    const CarTagOps ops = { (void *)((BodyCtx *)ctx)->target, tag_polygon, tag_line };
    const CarTagView v = { tag_colours, tag_outline, SKID_HIGH_RES, shadow_race->humans, tags_hidden };
    car_tag(&ops, &v, x, y, slot, z);
}

/* 0051c774 */
static int32_t car_world[9];

/* port: how far the car's model reaches from its centre (car_render.h's port_reach) */
static int32_t car_reach(const Object3D *o)
{
    int64_t most = 0;
    for (uint32_t v = 0; v < o->vertex_count; v++) {
        const int64_t x = o->model[v][0], y = o->model[v][1], z = o->model[v][2];
        if (x * x + y * y + z * z > most) most = x * x + y * y + z * z;
    }
    return (int32_t)sqrt((double)most) + 1;
}

static void draw_flare(const ViewCar *vc, const Car *car, const QuadTarget *t, const uint8_t palette[768])
{
    const Object3D *body = car->far != 0 ? vc->model.far : vc->model.body;   /* the model this pass drew: the other one's screen corners are another view's */
    if (!view_lamps_flares()) return;
    car_lamp_halo_draw(body, car, shadow_race, police_flash_on, t, palette);
}

static void render_slot(ViewCar *vc, Car *car, int pass, const ModelView *mv, const Object3DSink *sink,
                        const QuadTarget *target)
{
    BodyCtx b = { vc, car, sink, mv, target, { 0, 0, 0 }, 0 };
    const RaceState *race = shadow_race;
    const CarRenderOps ops = { &b, render_body, render_near, render_vertex, render_point, render_trail, render_tag };
    const CarRenderInputs in = { mv->camera_matrix, race->player, race->replay, race->race_type, race->racers, SKID_HIGH_RES,
                                 mv->port_all_sides ? car_reach(vc->model.body) : 0 };
    car_render(&ops, &in, car, pass, car_world);
}

/* 004591d4 */
static void row_draw(void *ctx, Car *car, int pass)
{
    (void)ctx;
    const Object3DSink sink = object_sink(&object_pixels);
    for (int i = 0; i < car_count; i++)
        if (cars[i].ok && cars[i].car == car) {
            object_sink_set_light(car_table(car, object_pixels.palette));
            body_shine_begin(object_pixels.palette, model_view.camera_matrix);
            const unsigned drawn = body_draws;
            render_slot(&cars[i], car, pass, &model_view, &sink, object_pixels.target);
            body_shine_end();
            if (body_draws != drawn) draw_flare(&cars[i], car, object_pixels.target, object_pixels.palette);
            object_sink_set_light(NULL);
        }
}

int ported_view_render_car(Car *car, const ModelView *mv, int mirror, const QuadTarget *target, const uint8_t palette[768])
{
    if (shadow_race == NULL) return 0;
    static uint8_t shade[256];
    static const uint8_t *shaded;
    if (shadow_remap == NULL && shaded != palette) object_sink_shade(palette, shade), shaded = palette;
    ObjectSink pixels = { target, palette, shadow_remap != NULL ? shadow_remap : shade, modern_view(mv->view) };
    const Object3DSink sink = object_sink(&pixels);
    const int32_t slot = car->index;  /* the slots are the race's (ported_view_set_car_slot) */
    if (slot < 0 || slot >= PORTED_VIEW_CARS || !cars[slot].ok) return 0;
    object_sink_set_light(car_table(car, palette));
    body_shine_begin(palette, mv->camera_matrix);
    const unsigned drawn = body_draws;
    render_slot(&cars[slot], car, mirror, mv, &sink, target);
    body_shine_end();
    if (body_draws != drawn) draw_flare(&cars[slot], car, target, palette);
    object_sink_set_light(NULL);
    return 1;
}

/* 0045a768 */
static void row_unseen(void *ctx, Car *car)
{
    (void)ctx;
    const CarShadowOps ops = { NULL, shadow_contact, shadow_followed, NULL, 0 };
    car_skid_unseen(&ops, car);
}

/* 0045a848 in a race; else each listed car on the node. */
static void draw_cars(void *user, int32_t node)
{
    (void)user;
    if (shadow_race != NULL && shadow_remap != NULL) {
        const RaceState *race = shadow_race;
        const RowCarsOps ops = { NULL, row_prepare, row_draw, row_unseen };
        const RowCarsInputs in = { race->chase.node, race->segments, ahead_rows, race->drawn, race->drawn_count, followed_car(),
                                   race->chase.kind, 0 };
        view_spray_row(node, 1);   /* the spray and the dust farther off than their car, first */
        cars_on_row(&ops, &in, node);
        if (arm_drawing) {   /* port: the rows behind the camera's node, which the exe's pass reads as the end of the ring */
            const RowCarsBehindInputs back = { race->chase.node, race->segments, behind_rows, race->drawn, race->drawn_count, followed_car() };
            cars_on_row_behind(&ops, &back, node);
        }
        view_spray_row(node, 0);   /* and the nearer, after the cars */
        return;
    }
    const Object3DSink sink = object_sink(&object_pixels);
    for (int i = 0; i < car_count; i++)
        if (cars[i].ok && cars[i].node == node) {
            object_sink_set_light(car_light(node, NULL, -1, object_pixels.palette));
            car_draw(&cars[i].model, &cars[i].pose, &model_view, 0, &sink);
            object_sink_set_light(NULL);
        }
}

typedef struct {
    const QuadTarget *quad;        /* the box (a band's rows of it when banded) */
    const RasterTarget *raster;
    const HorizonSmooth *smooth;   /* port: EXTRAS' Texture Filter on the ring's slices (sky_smooth.h), NULL off */
    const QuadTarget *full;        /* port (render bands): the whole box, for the draws whose edges a narrowed clip would move */
    int banded, top, bottom;       /* and, when banded, the rows this band stores */
} SkyTargets;

/* 004f9e98 = 00425520 -> 004252ec */
static void sky_triangle(void *user, const FshEntry *texture, const int32_t uv[6], const int32_t screen[6])
{
    const SkyTargets *t = user;
    if (t->smooth != NULL && !transparent(texture)) sky_smooth_triangle(t->smooth, t->quad, texture, uv, screen);
    else tri_draw(t->quad, texture, uv, screen, transparent(texture));
}

static void sky_fill(void *user, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t colour)
{
    const SkyTargets *t = user;
    if (t->banded) {   /* a rectangle's rows: the band's part of it */
        const int64_t y0 = y < t->top ? t->top : y, y1 = (int64_t)y + h > t->bottom ? t->bottom : (int64_t)y + h;
        if (y1 <= y0) return;
        y = (int32_t)y0, h = (int32_t)(y1 - y0);
    }
    raster_fill(t->raster, x, y, w, h, t->smooth != NULL ? t->smooth->map[colour] : colour);   /* port: the PS1 / PC alt sky's bitmap index is of its own palette */
}

/* 00484389 */
static void sky_polygon(void *user, uint8_t colour, const int32_t points[8])
{
    const SkyTargets *t = user;
    const uint8_t shown = t->smooth != NULL ? t->smooth->map[colour] : colour;
    if (t->banded) flat_polygon_rows(t->full, shown, 4, points, t->top, t->bottom);
    else flat_polygon(t->quad, shown, 4, points);
}

static int sky_off;  /* 004fb786 */

/* 004fbad8, the race's: the "m" tracks drawn turned over (004496ac, 00449f0c, 0043ae64, 0043b298) */
static int view_mirrored(void) { return shadow_race != NULL && shadow_race->mirrored != 0; }

void ported_view_set_sky_off(int off)
{
    sky_off = off;
}

static int32_t anim_tick;  /* 00513e70 */

void ported_view_set_tick(int32_t tick)
{
    anim_tick = tick;
}

/* 0043af80: the 3D sky ring of the tracks whose INFO asks for one. */
/* 0043af80's ring path: 0043b298 with the camera's yaw and roll (the follow
 * record's +0x44, +0x48); 0043b0c8 slopes the fills with the roll. */
/* 00512040..: the road direction the last frame's transform left (the mirror draws with it). */
static int32_t last_near_dir[3] = { 0, 0, 0x1999 };
static int32_t last_near = 62259 >> WORLD_SHIFT;  /* *(00511fe8) +4 as the last frame's transform left it */

void ported_view_near_dir(int32_t out[3])
{
    memcpy(out, last_near_dir, sizeof last_near_dir);
}

static SkyRing sky_ring;   /* built once a frame by the caller, read by every band */

static void draw_sky_ring(const SkyTargets *targets, const View *view, const FshEntry *horz, const TrackInfo *info,
                          const TrackCamera *cam, int32_t yaw, int32_t roll)
{
    const SkyRing *ring = &sky_ring;
    const int32_t rs = view_viewport.render_scale > 0 ? view_viewport.render_scale : 1;
    SkyFrame frame = { view, cam->matrix, { 0 }, yaw, roll, view_mirrored(), SKY_WORLD_SHIFT, 0, { 0 },
                       { view_viewport.scale, view_viewport.width, view_viewport.sky_left, view_viewport.sky_top, view_viewport.sky_width,
                         view_viewport.sky_height }, view_screen_w / rs, view_screen_w, 0, sky_off, 1, rs };   /* port: wide fields, render scale */
    for (int i = 0; i < 3; i++) frame.position[i] = -cam->position[i] >> SKY_POSITION_SHIFT;
    horizon_point(view, WORLD_SHIFT, yaw, cam->matrix, frame.horizon);
    const SkySink sink = { (void *)targets, sky_triangle, sky_fill, sky_polygon };
    sky_ring_draw(ring, info, &frame, horz, &sink);
}

/* port (render bands): the view's clear and the sky, one band's rows of them (everything they read is built before) */
typedef struct {
    uint8_t *pixels;
    const QuadTarget *quad;
    const RasterTarget *target;
    const View *view;
    const HorizonTables *horizon;
    const ClipRect *clip;
    const TrackArt *art;
    const TrackInfo *info;
    const TrackCamera *cam;
    const PortedCamera *pc;
    const HorizonSmooth *smooth;
    const FshEntry *horz;      /* the flat horizon's bitmap: the track's, or the PS1 sky's */
    const FshEntry *ring_horz; /* the ring's: the track's, or the alternative sky's */
    int32_t box_left, box_right, line;
    int ring, flat, banded;
} SkyStage;

static void sky_stage_band(void *user, int top, int bottom)
{
    const SkyStage *st = user;
    for (int32_t y = top; y < bottom; y++) {   /* 0044cc74 clears nothing; the view only, for the frames without one */
        const int32_t x0 = st->box_left < 0 ? 0 : st->box_left, x1 = st->box_right > view_screen_w ? view_screen_w : st->box_right;
        if (x1 > x0) memset(st->pixels + (size_t)y * view_screen_w + x0, 0, (size_t)(x1 - x0));
    }
    QuadTarget quad = *st->quad;   /* the box's rows this band owns */
    ClipRect clip = *st->clip;
    if (st->banded) {
        if (top > quad.clip_top) quad.clip_top = top, clip.y0 = top;
        if (bottom < quad.clip_bottom) quad.clip_bottom = bottom, clip.y1 = bottom;
        if (quad.clip_top >= quad.clip_bottom) return;
    }
    if (st->ring) {  /* the circuits' 3D ring */
        const SkyTargets targets = { &quad, st->target, st->smooth, st->quad, st->banded, top, bottom };
        draw_sky_ring(&targets, st->view, st->ring_horz, st->info, st->cam, st->pc->yaw, st->pc->roll);
    } else if (st->flat) {
        const HorizonTarget ht = { st->pixels, view_screen_w };
        if (st->smooth != NULL) {  /* the magnified flat sky blended */
            horizon_draw_flat_smooth(st->smooth, st->horizon, &ht, &clip, st->horz, st->pc->yaw, view_mirrored(),
                                     sky_off, st->line);
        } else if (st->banded) {  /* the box is the whole clip's, only the stores are the band's */
            horizon_draw_flat_rows(st->horizon, &ht, st->clip, st->horz, st->pc->yaw, view_mirrored(), sky_off,
                                   st->line, top, bottom);
        } else {
            horizon_draw_flat(st->horizon, &ht, &clip, st->horz, st->pc->yaw, view_mirrored(), sky_off, st->line);
        }
    }
}

/* The track from `cam`, with the car (set by ported_view_set_car) at `pose` drawn with the row of `car_node`; car_node -1 leaves it out. */
static void ported_view_render_from(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *pc,
                             const CarPose *pose, int32_t pose_node, uint8_t *pixels)
{
    const PortedCar one = { pose != NULL ? *pose : (CarPose){ { 0, 0, 0 }, 0, 0, 0, NULL, 1 },
                            pose != NULL ? pose_node : -1, NULL };
    ported_view_render_cars(tri, art, info, pc, &one, 1, pixels);
}

static TrackRender render;

/* 0043f540 on the race's scenery: the knocked-over bytes are the race's (004b9098, 0040fc64 sets them), and a knock's
 * 0043f51c (00504218 = -1) streams the scenery again at once; 0043f48c's 00504218 goes back to the race. */
static void stream(const Tri *tri, const TrackArt *art, int32_t camera_node)
{
    if (shadow_race != NULL && render.tri == tri) {
        render.build.knocked = shadow_race->scenery_state;
        if (shadow_race->scenery_stream_node < 0) render.build.first_node = -1;
    }
    track_render_set_light(&render, psx_light);  /* before the build: it fills the light with the vertices */
    const int relit = light_changed;
    if (relit) {   /* the light editor's reload: the segments already built hold the old rows */
        light_changed = 0;
        render.build.first_node = -1;
    }
    track_render_set_ring(&render, shadow_race != NULL && shadow_race->port_follow_ring ? TRACK_CACHE_RECORDS_MAX : TRACK_CACHE_RECORDS);   /* port (port_follow_ring) */
    if (shadow_race != NULL && shadow_race->port_follow_camera) follow_arm_window(&render, follow_arm_segments());   /* the window's reach behind before it is filled */
    track_render_build(&render, tri, art, camera_node);
    if (relit && psx_light != NULL) {   /* the editor's proof: the sum of the built vertices' light */
        uint64_t sum = 0;
        const int32_t n = render.build.world_next < TRACK_MAX_VERTICES ? render.build.world_next : TRACK_MAX_VERTICES;
        for (int32_t i = 0; i < n; i++) sum += render.light_vertex[i];
        port_log("light edit: the track built again with the new light (%d vertices, light sum %llu)\n", (int)n, (unsigned long long)sum);
    }
    if (shadow_race != NULL) {
        render.build.knocked = shadow_race->scenery_state;  /* again, for a first build that bound the track */
        shadow_race->scenery_stream_node = render.build.first_node;
    }
}

void ported_view_forget_track(void)
{
    render.tri = NULL;
    reflect_pass_forget_art();   /* port (reflectors): the next track's art may stand at this one's address; its textures are not these */
    ported_view_set_models(NULL, NULL, 0, 0, 0);  /* the next track's models sit at the same address, bound to these textures */
    horizon_smooth_cache_reset();           /* port: the sky bands kept for this track's bitmap */
    road_tunnels_forget();
    if (pool_fed != NULL) pool_fed->tunnel = NULL, pool_fed->node_xz = NULL;   /* the next track's first build asks the pool before headlights_update feeds it again */
}

void ported_view_set_reach(int32_t segments)
{
    track_render_set_reach(&render, segments);
}

int32_t ported_view_draw_segments(void) { return render.draw_segments; }

const TrackRender *ported_view_stream(const Tri *tri, const TrackArt *art, int32_t camera_node)
{
    stream(tri, art, camera_node);
    return &render;
}

/* port (port_follow_camera): which way the camera looks along the track decides who draws it. The exe's track pass draws
 * the segments ahead of the camera's node (its near plane pushes along the road direction, which runs away from a
 * camera that faces down the track); a camera that looks more across the road is drawn by follow_behind.h's, which
 * draws both sides of the node, the segments' rails, scenery and cars as the exe's pass does. */
enum { FOLLOW_EXE_BELOW = 0x120000 };   /* a 24-bit angle: 25.3 degrees off the road, past which the bottom of the view looks at ground behind the camera's node, which the exe's pass leaves bare */

static int32_t follow_off_road(const PortedCamera *pc)
{
    int32_t off = (int32_t)(((uint32_t)pc->yaw - (uint32_t)race_node(shadow_race, pc->node).orientation * 0x400u) & 0xffffff);
    if (off > 0x800000) off = 0x1000000 - off;
    return off;
}

/* port: the followed car on a row behind the camera's node, which the track pass does not draw cars on */
static void draw_followed_car(const PortedCamera *pc, int ahead_drawn)
{
    const RaceState *race = shadow_race;
    Car *car = (Car *)followed_car();
    if (car == NULL || race->chase.kind == 0 || car->physics == 6) return;
    const int32_t ring = (int32_t)((uint32_t)race->segments << 2);
    const int32_t d = car->draw_node >= pc->node ? car->draw_node - pc->node : car->draw_node + ring - pc->node;
    if (ahead_drawn && d >= 0 && d < ROW_CARS_AHEAD && ahead_rows[d] != 0) return;   /* the pass drew it */
    row_prepare(NULL, car, 0);
    row_draw(NULL, car, 0);
}

/* port (weather): the rain over the drawn view, under the 2D layer: the speed-following streaks and the lens at night (rain_fx), else the plain streaks */
static void rain_over_view(uint8_t *pixels, const int32_t box[4], const View *view, int32_t rs, const WeatherTables *wt)
{
    WiperPass wiper_now;
    const WeatherFrame frame = weather_frame();
    const WiperPass *const wipers = wt != NULL ? weather_hooks_wiper_pass(&wiper_now, box, &frame) : NULL;   /* port (wipers) */
    if (wt != NULL && weather_wet(weather) && port_rain_fx()) {   /* port (rain_fx): streaks that follow the speed, lit by the headlights at night */
        const int32_t speed = shadow_race != NULL && followed_car() != NULL ? followed_car()->speed : 0;
        const char *forced = getenv("TNFS_RAIN_SPEED");   /* dev: the speed the streaks are drawn for (metres a second), for captures of a car the harness has not got up to speed */
        const int32_t shown = forced != NULL ? (int32_t)(atoi(forced)) << 16 : speed;
        const int mood_share = (drive_light_night() ? RAIN_MOOD_SHARE_NIGHT : RAIN_MOOD_SHARE) * rain_open / 16;   /* port (tunnel_dry): the grey goes with the weather: none inside a closed zone */
        if (!rain_wet_off() && mood_share > 0 && weather_fog_snow_mood(snow_level(), port_weather_fog())) {   /* port (rain v4): the grey, low-contrast scene of rain; rows are independent, so in bands */
            const RainMood mood = { pixels, view_screen_w, { box[0], box[1], box[2], box[3] }, wt->mood, mood_share };
            bands_rows(box[1], box[3], rain_mood_band, (void *)&mood);
        }
        rain_cars_paint(pixels, view_screen_w, box, wt);
        int32_t vanish[2] = { view->centre_x, view->centre_y };
        rain_vanishing_point(vanish);
        if (port_rain_v2()) vanish[1] -= (box[3] - box[1]) / RAIN_V2_RAISE_DIV;   /* port (rain_v2): the rain falls from above: the streaks leave a point higher up */
        if (snow_level() != SNOW_NONE && port_snow_fx())   /* port (snow_fx): flakes that lean with the speed, a swinging storm wind, white marks on the lens */
            weather_snow_draw_fx(pixels, view_screen_w, box, rs, (uint32_t)anim_tick, snow_level(), rain_open, wt->rain4, shown, vanish[0] - box[0], vanish[1] - box[1]);
        else if (snow_level() != SNOW_NONE)   /* port (snow_weather): flakes for streaks and none on the lens; the wet road under them is the rain's */
            weather_snow_draw(pixels, view_screen_w, box, rs, (uint32_t)anim_tick, snow_level(), rain_open, wt->rain4[WEATHER_RAIN_LEVELS - 1]);
        else
        weather_rain_draw_fx(pixels, view_screen_w, box, rs, (uint32_t)anim_tick, shown, vanish[0] - box[0], vanish[1] - box[1], (float)view->focal_x,
                             drive_light_night(), rain_open, wt->rain4, weather == WEATHER_STORM, port_rain_v2());
        if (snow_level() != SNOW_NONE && port_snow_fx())
            weather_snow_lens_draw(pixels, view_screen_w, box, (uint32_t)anim_tick, shown, vanish[0] - box[0], vanish[1] - box[1], wt->rain4[WEATHER_RAIN_LEVELS - 1], rain_open, wipers);
        if (snow_level() == SNOW_NONE)
        weather_lens_draw(pixels, view_screen_w, box, (uint32_t)anim_tick, shown, vanish[0] - box[0], vanish[1] - box[1], weather_rain_spray_level(drive_light_night()), wt->rain4, wt->lens, rain_open, wipers);   /* port (rain v3): drops on the lens */
    } else if (wt != NULL && weather_wet(weather) && snow_level() != SNOW_NONE) {   /* port (snow_weather): flakes without rain_fx too */
        if (port_snow_fx()) {   /* port (snow_fx): no speed here (no rain_fx), the view's middle for the vanishing point */
            weather_snow_draw_fx(pixels, view_screen_w, box, rs, (uint32_t)anim_tick, snow_level(), rain_open, wt->rain4, 0, (box[2] - box[0]) / 2, (box[3] - box[1]) / 2);
            weather_snow_lens_draw(pixels, view_screen_w, box, (uint32_t)anim_tick, 0, (box[2] - box[0]) / 2, (box[3] - box[1]) / 2, wt->rain4[WEATHER_RAIN_LEVELS - 1], rain_open, wipers);
        } else
            weather_snow_draw(pixels, view_screen_w, box, rs, (uint32_t)anim_tick, snow_level(), rain_open, wt->rain4[WEATHER_RAIN_LEVELS - 1]);
    }
    else if (wt != NULL && weather_wet(weather))   /* port (weather): the streaks over the whole view, under the 2D layer */
        weather_rain_draw(pixels, view_screen_w, box, rs, (uint32_t)anim_tick, wt->rain, weather == WEATHER_STORM, port_rain_v2());
    if (wipers != NULL) wiper_draw(pixels, view_screen_w, box, wipers, (uint32_t)anim_tick, weather_hooks_wiper_dark(object_pixels.palette));   /* port (wipers): over the drops, under the dash */
}

/* port: the lamps', the street lamps' and the lit windows' tables from the world's tables just made, and the textures they are drawn on */
static void lamp_street_window_tables(const TrackArt *art)
{
    view_lamps_make_tables(art, (const uint8_t (*)[256])tunnel_tables);   /* the lamps' textures and tables */
    if (street_count > 0) {   /* the street lamps' boards: their entries kept */
        uint8_t exempt[256] = { 0 };
        for (int k = 0; k < 3; k++) exempt[street_entry[k]] = 1;
        for (int i = 0; i < street_count; i++)
            street_bitmap[i] = street_scenery[i] >= 0 && street_scenery[i] < TRACK_SCENERY_TEXTURES && art->scenery[street_scenery[i]].present
                                   ? &art->scenery[street_scenery[i]].bitmap : NULL;
        psx_lamp_tables(exempt, (const uint8_t (*)[256])lit_tables, MODERN_FOG_LEVELS, street_tables);
        street_halo_sync();
    }
    if (window_count > 0) {   /* the lit windows' boards: their entry kept */
        uint8_t exempt[256] = { 0 };
        exempt[window_entry] = 1;
        psx_lamp_tables(exempt, (const uint8_t (*)[256])lit_tables, MODERN_FOG_LEVELS, window_tables);
        for (int i = 0; i < window_count; i++)
            window_bitmap[i] = window_scenery[i] >= 0 && window_scenery[i] < TRACK_SCENERY_TEXTURES && art->scenery[window_scenery[i]].present
                                   ? &art->scenery[window_scenery[i]].bitmap : NULL;
    }
}

/* port (PS1 light): what the tables in force were made from; they are made again only when one of these moved (a fade changes the palette). One unit: the tone cache below
 * (tone_*) is reset together with it */
static struct {
    uint8_t palette[768];
    float tint[3];
    PsxTunnelLook look;
    int made, pool;
    PsxLightTone tone[PSX_LIGHT_TONES];
    int tone_count;
} light_made;

/* port (lamp_pool_tint): the lit tables again with the pool's warm tint, by class */
static void pool_tables_make(const TrackArt *art)
{
    static const float WARM[3] = { 1.0f, 0.9f, 0.55f };
    for (int k = 0; k < 4; k++) {
        float tint[3];
        for (int c = 0; c < 3; c++) tint[c] = lit_tint[c] * (1.0f + (WARM[c] - 1.0f) * (float)(k + 1) / 4.0f);
        modern_light_tables(art->palette, tint, pool_tables[k]);
    }
}

/* port (PS1 light): the lit, tunnel and pool tables again, and the scene tones' and the lamps' caches reset from the same inputs */
static void light_tables_make(const TrackArt *art, const PsxTunnelLook *look, int has_pool)
{
    modern_light_tables(art->palette, lit_tint, lit_tables);
    memcpy(light_made.tone, psx_light->tone, sizeof light_made.tone), light_made.tone_count = psx_light->tone_count;   /* port (`tone`): the scene tones' tables are made again when asked, from these */
    memset(tone_made, 0, sizeof tone_made);
    lamp_cache_used = lamp_cache_next = 0;   /* port (`lamp`): the lamps' tables are made again from the palette */
    memcpy(tone_palette, art->palette, sizeof tone_palette);
    memcpy(tone_lit_tint, lit_tint, sizeof tone_lit_tint), memcpy(tone_tunnel_tint, look->tint, sizeof tone_tunnel_tint);
    tone_have_cast = look->cast_on, tone_min_light = look->min_light;
    memcpy(tone_cast, look->cast, sizeof tone_cast);
    if (look->cast_on) modern_tunnel_tables(art->palette, look->tint, look->cast, look->min_light, tunnel_tables);
    else modern_light_tables_cast(art->palette, lit_tint, NO_CAST, tunnel_tables);
    if (has_pool) pool_tables_make(art);
    pool_ready = has_pool;
}

/* port (PS1 light): the tables of this frame's light (the lit, tunnel, pool, tone, lamp, street and window tables), made again only when the palette or the tint moved (a fade changes the palette), and the polygon hook set */
static void light_tables_prepare(const TrackArt *art)
{
    float shown[3];
    drive_light_tint(shown);
    PsxTunnelLook look;
    psx_light_tunnel_look(psx_light, shown, lit_tint, view_lamps_count(), &look);   /* the tunnel's tables: the file's look, else the made light's (psx_light.h) */
    const int has_pool = psx_light->pool_class != NULL;
    if (!light_made.made || light_made.pool != has_pool || light_made.tone_count != psx_light->tone_count || memcmp(light_made.tone, psx_light->tone, sizeof light_made.tone) != 0
        || memcmp(light_made.palette, art->palette, sizeof light_made.palette) != 0 || memcmp(light_made.tint, lit_tint, sizeof light_made.tint) != 0
        || memcmp(&light_made.look, &look, sizeof look) != 0) {
        light_made.look = look;
        tone_mean = (shown[0] + shown[1] + shown[2]) / 3.0f;
        light_tables_make(art, &look, has_pool);
        light_made.pool = has_pool;
        memcpy(light_made.palette, art->palette, sizeof light_made.palette), memcpy(light_made.tint, lit_tint, sizeof light_made.tint), light_made.made = 1;
    }
    lamp_street_window_tables(art);
    track_light_hook = light_hook;
    tables_made = 1;
}

/* port: the sky over the view before the track covers it: the horizon art (or the PS1 sky, or the ring), stretched up and down where the switches say, drawn in bands */
static void draw_sky(const TrackArt *art, const TrackInfo *info, const PortedCamera *pc, uint8_t *pixels, QuadTarget *quad, RasterTarget *target, View *view, HorizonTables *horizon,
                     DrawContext *ctx, TrackCamera *cam, const int32_t box[4])
{
    const int sky_filter = art->has_horizon && modern && look_filter;  /* port: EXTRAS' Texture Filter on the sky */
    if (sky_filter) horizon_smooth_palette(&smooth, art->palette);
    PsxSkyDraw *const own = psx_sky;   /* port: the PS1 sky (psx_sky_draw.h) in place of the track's, on the flat horizon */
    if (own != NULL) {
        uint8_t shown[768];
        drive_light_palette(art->palette, shown);
        psx_sky_draw_target(own, shown);
    }
    /* the PC's alternative sky (drive_light.c) is the morning / evening 3D ring of its INFO, on its own horizon */
    const int alt_ring = own != NULL && psx_sky_info != NULL && psx_sky_info->ring != 0;
    const TrackInfo *const sky_info = alt_ring ? psx_sky_info : info;
    const FshEntry *const ring_horz = alt_ring ? &own->horz : &art->horizon;
    const int ring = alt_ring || (own == NULL && art->has_horizon && info->ring != 0);
    if (ring) sky_ring_build(&sky_ring, sky_info, ring_horz);
    SkyStage stage = { pixels, quad, target, view, horizon, &ctx->clip, art, sky_info, cam, pc,
                       own != NULL ? &own->smooth : sky_filter ? &smooth : NULL, own != NULL ? &own->horz : &art->horizon, ring_horz,
                       box[0], box[2], 0, ring && sky_ring.enabled, own != NULL || (art->has_horizon && !ring), bands_threads() != 0 };
    if (stage.flat) stage.line = horizon_line(view, WORLD_SHIFT, pc->yaw, cam->matrix);
    static SkyStretch stretch;   /* port (sky_fill): the art reaching the view's top, built once a frame for all the bands */
    /* port (night): the flat colour over the art would be the day's navy beside the night's sky; at night the art is carried up to the top of the view as the
     * sky_fill switch does (every column takes its top rows' colour), whether the switch is on or not */
    const int night_fill = drive_light_night();
    if (stage.flat && (port_sky_fill() || night_fill))
        stage.horz = sky_stretch(&stretch, stage.horz, horizon->scale, stage.line, ctx->clip.y0,
                                 night_fill && port_night_stretch_clean() ? SKY_FILL_COLUMN_QUIET : port_sky_fill() ? port_sky_fill_mode() : SKY_FILL_COLUMN,
                                 night_fill && port_night_stretch_clean() && own != NULL ? own->bitmap.palette : art->palette);   /* the sky's texels index its own palette */   /* port (night_stretch_clean): no star carried up as a stripe */
    static SkyStretch below;   /* port (sky_fill_below): and downward, to the view's bottom, in 16:9 and 21:9 */
    if (stage.flat && port_sky_fill_below()) stage.horz = sky_stretch_below(&below, stage.horz, horizon->scale, stage.line, ctx->clip.y1);
    bands_rows(box[1] < 0 ? 0 : box[1], box[3] < view_screen_h ? box[3] : view_screen_h, sky_stage_band, &stage);
}

/* port: the track pass, by which way the camera looks (follow_off_road): the exe's pass ahead, or follow_behind's; returns whether the exe's pass was the one drawn */
static int draw_track(const Tri *tri, const PortedCamera *pc, TrackCamera *cam, DrawContext *ctx)
{
    int ahead = 1;   /* the exe's pass draws the track */
    if (shadow_race != NULL && shadow_race->port_follow_camera) {   /* port: the camera may look along the track either way */
        follow_arm_window(&render, follow_arm_segments());
        ahead = follow_off_road(pc) <= FOLLOW_EXE_BELOW;
        if (!ahead) {
            const FollowArm arm = { &render, cam, ctx, tri_is_circuit(tri), anim_tick, view_mirrored(), 0 };
            arm_drawing = 1;
            follow_behind_draw(&arm, follow_arm_segments(), render.draw_segments - 2);
            arm_drawing = 0;
        }
    }
    view_lamps_frame_start();
    street_halo_frame_begin();
    if (ahead) track_render_draw(&render, cam, ctx, view_mirrored(), anim_tick);
    return ahead;
}

/* port (render bands P2, P3): the frame's recorder */
static struct RasterRecorder recorder;

/* The storm's thunder and flash, and (port, weather) the sky toward its colour before the track covers it. */
static void cars_weather_sky(const WeatherTables *wt, uint8_t *pixels, const int32_t box[4], View *view, const TrackCamera *cam)
{
    storm_thunder_update();
    rain_roll_update();
    snow_roll_update();
    drive_light_set_flash(wt != NULL ? storm_flash_level() : 0);   /* port (WEATHER_STORM): the palette goes bright with the flash, the sky white below */
    if (wt != NULL) {   /* port (weather): the sky toward its colour, before the track covers it */
        const WeatherSky sky = weather_sky(pixels, view_screen_w, box[0] < 0 ? 0 : box[0], box[2] > view_screen_w ? view_screen_w : box[2], wt,
                                           view, cam->matrix, storm_flash_level());
        bands_rows(box[1] < 0 ? 0 : box[1], box[3] < view_screen_h ? box[3] : view_screen_h, weather_sky_band, (void *)&sky);
    }
}

/* The frame's track streaming and the modern road's style (weather, fog or none). */
static void cars_stream_style(const Tri *tri, const TrackArt *art, const PortedCamera *pc, const WeatherTables *wt)
{
    stream(tri, art, pc->node);
    if (!modern) modern_road_set_style(NULL);
    else if (wt != NULL) weather_style(art, wt, weather, snow_level(), port_weather_fog(), &look, look_filter, rain_open);
    else {
        modern_look_set_sample(&look, port_fog_sample(), port_fog_lift());
        modern_road_set_style(modern_look_style(&look, art, look_filter, look_fog, render.draw_segments));
    }
}

/* The headlights' pools, the track's rows transformed for the camera, and the models' view of it. */
static void cars_transform(TrackCamera *cam, DrawContext *ctx, View *view)
{
    headlights_update();   /* port (psx_headlights): the pools of the cars with their lights on, before the rows are built */
    if (port_headlight_relight() || (psx_light != NULL && psx_light->pool != NULL && psx_light->pool->cone)) track_render_relight(&render);   /* port (headlight_relight): the road's light with the pools of this frame */
    track_render_transform(&render, cam, ctx);
    memcpy(last_near_dir, ctx->near_dir, sizeof last_near_dir);
    last_near = ctx->near;
    model_view = (ModelView){ view, cam->position, cam->matrix,
                              shadow_race != NULL && model_draw_half_side(shadow_race->track, shadow_race->track_variant), 1 };
}

/* The cars of the frame, and the object sink their shapes are drawn through. */
static void cars_take_list(const TrackArt *art, const PortedCar *list, int count, QuadTarget *quad, View *view)
{
    car_count = count < 0 ? 0 : count > PORTED_VIEW_CARS ? PORTED_VIEW_CARS : count;
    for (int i = 0; i < car_count; i++) {
        cars[i].pose = list[i].pose;
        cars[i].car = list[i].car;
        cars[i].node = list[i].node;
    }
    static uint8_t shade[256];
    if (shadow_remap == NULL) object_sink_shade(art->palette, shade);
    object_pixels = (ObjectSink){ quad, art->palette, shadow_remap != NULL ? shadow_remap : shade, modern_view(view) };
}

/* The frame's recording by bands (with threads), the lamps' and the light's tables, and the rain's spray. */
static void cars_frame_begin(const TrackArt *art, const WeatherTables *wt, uint8_t *pixels)
{
    if (bands_threads() != 0) {
        raster_record_set_runner(bands_rows);
        raster_record_begin(&recorder, pixels);
    }
    if (psx_light == NULL && view_lamps_flares() && view_lamps_count() > 0) view_lamps_textures(art);
    if (psx_light != NULL) light_tables_prepare(art);
    reflect_frame(art);
    lit_level = 0;
    memset(lit_corner, 255, sizeof lit_corner);
    view_spray_frame(art, wt, weather_wet(weather));
}

void ported_view_render_cars(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *pc,
                             const PortedCar *list, int count, uint8_t *pixels)
{
    static View view;
    static HorizonTables horizon;
    const PortedViewport *vp = &view_viewport;
    view_init(&view, vp->left, vp->top, vp->width, vp->height, vp->fov, vp->fov_y != 0 ? vp->fov_y : vp->fov);
    /* port: the exe's rows scale grows with the width (0043a028); past 640 it keeps the 640's zoom */
    const int32_t rs = vp->render_scale > 0 ? vp->render_scale : 1;   /* port (render scale): the zoom of the unscaled width */
    horizon_tables(&horizon, vp->width, vp->fov, screen_horizon_scale(vp->scale, vp->width / rs));
    RasterTarget target = { pixels, view_screen_w, view_screen_h, 0, 0 };
    /* the box (00449f50: the layout's left, top, +0x20, +0x24); left + x, top + y only place the projection */
    const int32_t box[4] = { vp->sky_left, vp->sky_top, vp->sky_left + vp->sky_width, vp->sky_top + vp->sky_height };
    QuadTarget quad = { pixels, view_screen_w, box[0], box[1], box[2], box[3] };
    DrawContext ctx = { &view, { box[0], box[1], box[2], box[3] }, WORLD_SHIFT,
                        { 0, 0, 0x1999 },
                        { &quad, draw_uv, draw_full, draw_full_alt, draw_model, draw_cars, draw_skids,
                          modern ? draw_camera : NULL, draw_camera }, 0,
                        vp->near_scale };
    track_port_wide_board = wide_board_on();
    track_port_close_boxes = port_close_boxes(shadow_race != NULL && (shadow_race->port_reverse != 0 || shadow_race->port_follow_camera != 0));
    TrackCamera cam = { { pc->position[0], pc->position[1], pc->position[2] }, { 0 }, pc->node };
    track_camera_matrix(pc->pitch, pc->yaw, pc->roll, view_mirrored(), vp->scale, cam.matrix);  /* the layout's +4 (00449670) */
    /* 0044cc74 clears nothing (00473680 picks the surface): the horizon's
     * fills cover the view; the view (only: the rest of the surface holds
     * 004218d0's tiles and frame) cleared here for the frames without one. */
    rain_open = weather != WEATHER_CLEAR && shadow_race != NULL ? rain_open_at(shadow_race->chase.node) : 16;   /* port (weather_zones): once a frame, before anything of the weather */
    draw_sky(art, info, pc, pixels, &quad, &target, &view, &horizon, &ctx, &cam, box);
    const WeatherTables *wt = weather != WEATHER_CLEAR ? weather_tables(art) : NULL;
    cars_weather_sky(wt, pixels, box, &view, &cam);
    cars_stream_style(tri, art, pc, wt);
    cars_transform(&cam, &ctx, &view);
    cars_take_list(art, list, count, &quad, &view);
    skid_node = pc->node;
    /* port (render bands P2, P3): with threads the track's draws (road, scenery, cars, shadows, skid marks, smoke) are
     * recorded by the rasterizers, which find the recorder in the target, and replayed by bands at the end, in order */
    cars_frame_begin(art, wt, pixels);
    const int ahead = draw_track(tri, pc, &cam, &ctx);
    view_spray_frame_end();
    track_light_hook = NULL;
    lit_active = 0;
    if (shadow_race != NULL && shadow_race->port_follow_camera) draw_followed_car(pc, ahead);
    if (raster_active == &recorder) raster_record_end(&recorder);
    view_lamps_draw(&quad, art->palette);
    street_halo_draw(&quad, art->palette, drive_light_night());
    rain_over_view(pixels, box, &view, rs, wt);
}

/* port (weather): the frame's weather as weather_hooks.h asks for it; the thunder, the voices of the followed car and the lightning are there */
static WeatherFrame weather_frame(void) { return (WeatherFrame){ weather, snow_level(), rain_open, shadow_race, anim_tick }; }

static void storm_thunder_update(void) { const WeatherFrame f = weather_frame(); weather_hooks_thunder(&f); }
static void rain_roll_update(void) { const WeatherFrame f = weather_frame(); weather_hooks_rain_roll(&f); }
static void snow_roll_update(void) { const WeatherFrame f = weather_frame(); weather_hooks_snow_roll(&f); }
static int storm_flash_level(void) { const WeatherFrame f = weather_frame(); return weather_hooks_flash_level(&f); }

int ported_view_wipers_active(void) { return weather_hooks_wiper_blades() > 0 && weather_wet(weather); }

uint8_t ported_view_light_at(uint32_t node, int point, uint8_t *tunnel)
{
    *tunnel = (uint8_t)psx_light_tunnel(psx_light, node);
    return psx_light_at(psx_light, node, point);
}

/* port (the PS1 light in the rear mirror's follow pass): the same sinks the main pass draws with, and the hook set for the pass, from the tables
 * the last render made. Without a light (or before the first render) begin gives 0 and the mirror draws as it did. */
int ported_view_mirror_light_begin(void)
{
    if (psx_light == NULL || !tables_made) return 0;
    track_light_hook = light_hook;
    lit_level = 0;
    memset(lit_corner, 255, sizeof lit_corner);
    return 1;
}

void ported_view_mirror_light_end(void)
{
    track_light_hook = NULL;
    lit_active = 0;
    track_light_tunnel = 0;
}

void ported_view_mirror_sinks(void (**uv)(void *, const FshEntry *, const int32_t[8], const int32_t[8]),
                              void (**full)(void *, const FshEntry *, const int32_t[8]),
                              void (**full_alt)(void *, const FshEntry *, const int32_t[8]),
                              void (**camera)(void *, const View *, const FshEntry *, const int32_t[8], const int32_t *const[4]))
{
    *uv = draw_uv, *full = draw_full, *full_alt = draw_full_alt, *camera = draw_camera;
}

void ported_view_render(const Tri *tri, const TrackArt *art, const TrackInfo *info, uint32_t node, int32_t yaw,
                        uint8_t *pixels)
{
    TriNode n, cn;
    if (tri_node(tri, node, &n) != TRI_OK) return;
    const PortedCamera cam = { { n.position.x, n.position.y + EYE_HEIGHT, n.position.z }, n.orientation * 0x400 + yaw,
                               0, (int32_t)node, 0 };
    const int32_t parked = (int32_t)psx_ring_node((int32_t)node + car_ahead, tri->ring_nodes);   /* a modulo: the rings are not powers of two */
    if (cars[0].ok && tri_node(tri, (uint32_t)parked, &cn) == TRI_OK) {
        const CarPose pose = { { cn.position.x, cn.position.y, cn.position.z }, 0, 0, cn.orientation * 0x400, NULL, 1 };
        ported_view_render_from(tri, art, info, &cam, &pose, parked, pixels);
    } else {
        ported_view_render_from(tri, art, info, &cam, NULL, -1, pixels);
    }
}
