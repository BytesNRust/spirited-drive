#include "viewer/ported_view.h"
#include "game/sys/fixed_math.h"
#include "game/sys/bands.h"
#include "game/gfx/raster_record.h"

#include "game/gfx/quad.h"
#include "game/race/horizon.h"
#include "game/race/sky_ring.h"
#include "game/race/sky_stretch.h"
#include "game/race/track_render.h"
#include "game/gfx/sprite_scaled.h"
#include "game/race/view.h"
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
#include "game/viewer/follow_arm.h"
#include "game/viewer/follow_behind.h"
#include "viewer/horizon_smooth.h"
#include "viewer/modern_look.h"
#include "viewer/modern_road.h"
#include "viewer/object_sink.h"
#include "viewer/body_shine.h"
#include "game/sys/port_settings.h"
#include "viewer/soft_shadow.h"
#include "viewer/raster.h"
#include "viewer/sky_smooth.h"
#include "viewer/psx_sky_draw.h"
#include "viewer/drive_light.h"
#include "viewer/weather_draw.h"
#include "viewer/drive_light.h"
#include "game/race/weather.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    WIDTH = 640,
    HEIGHT = 480,
    FOV = 96,              /* chase views (004b78a8, view 1) */
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

/* port (the tunnel lamps, psx_lamps.h): the lamp textures of the frame, by their bitmaps, and the darkening tables that let their fitting's
 * colours through; remade with `lit_tables` */
enum { LAMPS_MAX = 4, LAMP_VARIANTS = 8 };
static PsxLamp lamp_spec[LAMPS_MAX];
static int lamp_count;
static const FshEntry *lamp_bitmap[LAMPS_MAX][LAMP_VARIANTS];
static uint8_t lamp_tables[LAMPS_MAX][MODERN_FOG_LEVELS][256];
/* the tunnel's dim road and walls on the PS1 are olive / amber, never the texture's own grey: green and blue lost on every darkened level
 * (fitted to the clip's road, (38,34,16) for a grey texture) */
static const float TUNNEL_CAST[3] = { 1.0f, 0.8f, 0.15f }, NO_CAST[3] = { 1, 1, 1 };

void ported_view_set_psx_lamps(const PsxLamp *lamps, int count)
{
    lamp_count = lamps != NULL && count > 0 ? (count < LAMPS_MAX ? count : LAMPS_MAX) : 0;
    for (int i = 0; i < lamp_count; i++) lamp_spec[i] = lamps[i];
}

/* the lamp `texture` is, or -1 */
static int lamp_of(const FshEntry *texture)
{
    for (int i = 0; i < lamp_count; i++)
        for (int v = 0; v < LAMP_VARIANTS; v++)
            if (lamp_bitmap[i][v] == texture) return i;
    return -1;
}

static const uint8_t (*lit_tables_for(const FshEntry *texture))[256]
{
    const int lamp = lamp_of(texture);
    return lamp >= 0 ? lamp_tables[lamp] : track_light_tunnel ? tunnel_tables : lit_tables;
}

static const uint8_t *lit_table_for(const FshEntry *texture)
{
    return lit_active && (lit_level > 0 || lit_tinted) ? lit_tables_for(texture)[lit_level] : NULL;
}

static const uint8_t *lit_table(void) { return lit_active && (lit_level > 0 || lit_tinted) ? (track_light_tunnel ? tunnel_tables : lit_tables)[lit_level] : NULL; }

/* 004f9e94 = 004254e8 -> 00425210: 00486950 / 00487180 */
static void draw_uv(void *user, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8])
{
    const uint8_t *lit = lit_table_for(texture);
    if (lit != NULL) quad_draw_mode(user, texture, uv, screen, transparent(texture) ? MAP_REMAP_TRANSPARENT : MAP_REMAP, lit);
    else quad_draw(user, texture, uv, screen, transparent(texture));
}

static int modern;  /* port: EXTRAS' Modern 3D (ported_view_set_modern) */

void ported_view_set_modern(int on) { modern = on != 0; }

/* port (weather, re/notes/weather-design.md): WEATHER_CLEAR runs none of it. The tables are made again when the palette or the
 * weather changes. */
static int weather;
typedef struct {
    uint8_t palette[768];
    int weather, made;
    int sample, lift;    /* port (fog_sample): the colour's source the fog was made from */
    uint8_t fog[3];      /* the fog's colour */
    uint8_t sky[256];    /* the sky's palette moved toward its colour */
    uint8_t levels[MODERN_FOG_LEVELS][256];   /* all its levels: the fog's sky gets the fog's whole at the horizon */
    int sky_low, sky_high;
    float tint[3];       /* the time of day's tint the fog colour was made for */
    uint8_t rain[256];   /* a streak's lightening */
} WeatherTables;

void ported_view_set_weather(int w) { weather = w > WEATHER_CLEAR && w < WEATHER_COUNT ? w : WEATHER_CLEAR; }

static const WeatherTables *weather_tables(const TrackArt *art)
{
    static WeatherTables wt;
    float tint[3];
    drive_light_tint(tint);
    const int sample = port_fog_sample(), lift = port_fog_lift();
    if (wt.made && wt.weather == weather && wt.sample == sample && wt.lift == lift && memcmp(wt.tint, tint, sizeof tint) == 0 &&
        memcmp(wt.palette, art->palette, sizeof wt.palette) == 0)
        return &wt;
    static uint8_t scratch[MODERN_FOG_LEVELS][256];
    uint8_t base[3], sky[3], rain[3];
    if (sample) modern_look_fog_colour_sampled(art, lift, base);
    else modern_look_fog_colour(art, base);
    weather_fog_colour(weather, base, wt.fog);
    weather_fog_for_tint(wt.fog, tint, wt.fog);   /* the time of day's tint reaches the fog at half strength */
    weather_sky_colour(weather, wt.fog, sky);
    modern_fog_tables(art->palette, sky, scratch);
    wt.sky_low = weather == WEATHER_RAIN ? WEATHER_RAIN_SKY_LEVEL : WEATHER_SKY_LEVEL;
    wt.sky_high = weather == WEATHER_RAIN ? wt.sky_low : MODERN_FOG_LEVELS - 1;
    memcpy(wt.sky, scratch[wt.sky_low], sizeof wt.sky);
    memcpy(wt.levels, scratch, sizeof wt.levels);
    weather_rain_colour(rain);
    modern_fog_tables(art->palette, rain, scratch);
    memcpy(wt.rain, scratch[WEATHER_STREAK_LEVEL], sizeof wt.rain);
    memcpy(wt.palette, art->palette, sizeof wt.palette);
    memcpy(wt.tint, tint, sizeof tint);
    wt.weather = weather, wt.sample = sample, wt.lift = lift, wt.made = 1;
    return &wt;
}

typedef struct {
    uint8_t *pixels;
    int32_t pitch, left, right;   /* the columns clipped to the buffer */
    const WeatherTables *wt;
    int32_t cx, cy;               /* the view's centre and focal length, and the world's up in its space (unit) */
    float focal, up[3];
} WeatherSky;

/* the weather's sky for the view `view` at the camera matrix `matrix` (the world's up taken to the camera's space) */
static WeatherSky weather_sky(uint8_t *pixels, int32_t pitch, int32_t left, int32_t right, const WeatherTables *wt,
                              const View *view, const int32_t matrix[9])
{
    const int32_t up_world[3] = { 0, 0x10000, 0 };
    int32_t c[3];
    mat3_apply(up_world, matrix, c);
    const float len = sqrtf((float)c[0] * (float)c[0] + (float)c[1] * (float)c[1] + (float)c[2] * (float)c[2]);
    const float k = len > 0 ? 1.0f / len : 0.0f;
    return (WeatherSky){ pixels, pitch, left, right, wt, view->centre_x, view->centre_y, (float)view->focal_x,
                         { (float)c[0] * k, (float)c[1] * k, (float)c[2] * k } };
}

static void weather_sky_band(void *user, int top, int bottom)
{
    const WeatherSky *s = user;
    if (s->wt->sky_high > s->wt->sky_low)
        weather_sky_fade(s->pixels, s->pitch, s->left, s->right, top, bottom, s->wt->levels, s->wt->sky_low, s->wt->sky_high,
                         s->cx, s->cy, s->focal, s->up);
    else weather_sky_rows(s->pixels, s->pitch, s->left, s->right, top, bottom, s->wt->sky);
}
int ported_view_modern(void) { return modern; }

static int look_filter, look_fog;  /* port: EXTRAS' Texture Filter and Fog (ported_view_set_look) */
static ModernLook look;
void ported_view_set_dark_fog(int dark) { modern_look_set_dark(&look, dark); }

static void weather_style(const TrackArt *art, const WeatherTables *wt)
{
    const int fog = weather == WEATHER_FOG;
    ModernStyle style = *modern_look_fixed_fog(&look, art, look_filter, wt->fog, fog ? WEATHER_FOG_START : WEATHER_RAIN_FOG_START,
                                               fog ? WEATHER_FOG_END : WEATHER_RAIN_FOG_END);
    if (fog && weather_fog_curve())   /* the exponential variant (weather.h) */
        style.fog_exp = WEATHER_FOG_EXP_SCALE, style.fog_cap = WEATHER_FOG_EXP_CAP_PERCENT / 100.0f;
    modern_road_set_style(&style);
}

/* the rear-view mirror draws before the front view of the frame: its road gets the weather's style (not last frame's);
 * 0 (nothing set) in WEATHER_CLEAR or without Modern 3D */
int ported_view_mirror_weather_on(void) { return weather != WEATHER_CLEAR && modern; }

int ported_view_mirror_weather(const TrackArt *art)
{
    if (weather == WEATHER_CLEAR || !modern) return 0;
    weather_style(art, weather_tables(art));
    return 1;
}

/* the mirror's own sky, drawn by now: toward the weather's colour, meeting the fog at its horizon (its camera `matrix`) */
void ported_view_mirror_sky(const TrackArt *art, const QuadTarget *target, const View *view, const int32_t matrix[9])
{
    if (!ported_view_mirror_weather(art)) return;
    const WeatherTables *wt = weather_tables(art);
    const WeatherSky sky = weather_sky(target->pixels, target->pitch, target->clip_left, target->clip_right, wt, view, matrix);
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

/* port: the view the 3D objects' pixels clip and project by when Modern 3D is on, else NULL (the exe's mappers) */
static const View *modern_view(const View *view) { return modern ? view : NULL; }

/* port: the near road clipped, unsnapped and perspective-correct (Modern 3D) */
static void draw_camera(void *user, const View *view, const FshEntry *texture, const int32_t uv[8],
                        const int32_t *const cam[4])
{
    if (lit_active && (lit_tinted || lit_corner[0] < 255 || lit_corner[1] < 255 || lit_corner[2] < 255 || lit_corner[3] < 255)) {
        /* the intensities at the polygon's corners (texture (0,0), (w,0), (w,h), (0,h)) across the cell by its uv, a
         * darkness a pixel takes between two of the tables */
        float dark[4];
        const float w = (float)(int16_t)texture->width * 65536.0f, h = (float)(int16_t)texture->height * 65536.0f;
        for (int i = 0; i < 4; i++) {
            const float a = fminf(fmaxf(uv[2 * i] / w, 0), 1), b = fminf(fmaxf(uv[2 * i + 1] / h, 0), 1);
            const float top = lit_corner[0] + (lit_corner[1] - lit_corner[0]) * a;
            const float bottom = lit_corner[3] + (lit_corner[2] - lit_corner[3]) * a;
            dark[i] = (255.0f - (top + (bottom - top) * b)) * (MODERN_FOG_LEVELS - 1) / 255.0f;
        }
        modern_road_cell_lit(user, view, texture, uv, cam, transparent(texture), lit_tables_for(texture), dark);
    } else modern_road_cell(user, view, texture, uv, cam, transparent(texture));
}

/* 004f9e9c = 00425590 -> 004251d0: the whole texture as two triangles */
static void draw_full(void *user, const FshEntry *texture, const int32_t screen[8])
{
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

/* 004f9e90 = 00425558 -> 00425250: the whole texture as one quad */
static void draw_full_alt(void *user, const FshEntry *texture, const int32_t screen[8])
{
    const uint8_t *lit = lit_table_for(texture);
    if (lit != NULL) {
        const int32_t right = ((int32_t)(int16_t)texture->width << 16) - 1, bottom = ((int32_t)(int16_t)texture->height << 16) - 1;
        const int32_t uv[8] = { 0, 0, right, 0, right, bottom, 0, bottom };
        quad_draw_mode(user, texture, uv, screen, transparent(texture) ? MAP_REMAP_TRANSPARENT : MAP_REMAP, lit);
    } else quad_draw_full(user, texture, screen, transparent(texture));
}

/* port (the PS1 look's light): the track's light, or NULL for none (the frames are then the exe's, byte for byte) */
static const PsxLight *psx_light;

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

static RaceState *shadow_race;

void ported_view_set_psx_synth(int on) { synth_on = on; }

/* port (the PS1 light on the cars and the dash): the exe's light of the road's centre at the car's own node (psx_lgs_car_ratio; the exe gives every car the
 * viewer's, the port its own node's), the one factor of the cars and the dash; 0 and `out` untouched when there is no light script. A context's colour is
 * its own tunnel's tint, and a RAMP context's odd / even nodes are the lamps' alternation. */
static int ratio_at(int32_t node, float out[3])
{
    if (synth_on && psx_light != NULL) {   /* port: a light made from the road (psx_light_synth.h): no script, the tunnel's own factor */
        out[0] = out[1] = out[2] = psx_light_synth_car(psx_light, psx_ring_node(node, psx_light->nodes));
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
    return car != NULL && ratio_at(car->node, out);
}

/* the table for a car at `node`, NULL when its ambient is the world's */
static const uint8_t *car_light(int32_t node, const uint8_t palette[768])
{
    if (!(ambient_on || synth_on) || psx_light == NULL || palette == NULL) return NULL;
    static struct { int key[3]; uint8_t table[256]; int used; } cache[AMBIENT_TABLES];
    static uint8_t made_for[768];
    static int next, valid;
    float ratio[3];
    ratio_at(node, ratio);
    int key[3], neutral = 1;
    for (int k = 0; k < 3; k++) {
        key[k] = (int)(ratio[k] * AMBIENT_STEP + 0.5f);
        neutral = neutral && key[k] == AMBIENT_STEP;
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
    psx_lit_table(palette, ratio, cache[slot].table);
    return cache[slot].table;
}

/* Scenery models through the ported 3D engine (00458c04), one object per
 * model as the race keeps them. */
static const ViewModel *models;
static Object3D objects[PORTED_VIEW_MODELS];
static int object_ok[PORTED_VIEW_MODELS];
static ModelView model_view;
static ObjectSink object_pixels;
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

void ported_view_set_models(const ViewModel *list, const int *ok, int mirrored)
{
    if (list == models && mirrored == models_mirrored) return;
    for (int i = 0; i < PORTED_VIEW_MODELS; i++) {
        if (object_ok[i]) object3d_free(&objects[i]);
        object_ok[i] = list != NULL && ok[i] && object3d_create(&objects[i], &list[i].mesh);
        if (object_ok[i] && list[i].has_textures) object3d_bind(&objects[i], &list[i].textures);
        if (object_ok[i] && mirrored) object3d_mirror(&objects[i]);   /* 004576f0 on 004fbad8 */
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

static RaceState *shadow_race;
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

void ported_view_frame_rows(uint8_t behind[MIRROR_CAR_ROWS])
{
    if (shadow_race == NULL) return;
    const SkidRing ring = skid_ring();
    skid_rows(&skid_marks, &ring, shadow_race->chase.node);
    car_rows_ahead(shadow_race->cars, shadow_race->car_count, shadow_race->segments, shadow_race->chase.node, ahead_rows,
                   behind);
}

/* port: the mirror's rows again for the camera's node of this frame, the cars not moved on (mirror_car_rows_at) */
void ported_view_mirror_rows_again(uint8_t behind[MIRROR_CAR_ROWS])
{
    if (shadow_race == NULL) return;
    mirror_car_rows_at(shadow_race->cars, shadow_race->car_count, shadow_race->segments, shadow_race->chase.node, behind);
}

static const Car *followed_car(void)
{
    return shadow_race->cars[race_slot(shadow_race->player)];
}

/* 00513e88 / 00514298, *004c795c: NULL until the race has its smoke. */
static const PuffArt *puff_art;
static const PuffBlend *puff_blend;
static int32_t (*puff_rand)(void *ctx, int32_t n);
static void *puff_rand_ctx;
static CrashTrail *puff_trail;   /* 005146ac */

void ported_view_set_puffs(const PuffArt *art, const PuffBlend *blend, int32_t (*rand_below)(void *ctx, int32_t n),
                           void *ctx, CrashTrail *trail)
{
    puff_art = art, puff_blend = blend, puff_rand = rand_below, puff_rand_ctx = ctx, puff_trail = trail;
}

static PuffView puff_view(void)
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

/* 00458988 */
static void smoke_draw(void *ctx, int in_car)
{
    const TyreSmokeOps ops = { puff_rand_ctx, smoke_view_angle, puff_rand, smoke_puffs };
    tyre_smoke(&ops, (const Car *)ctx, in_car, shadow_race->camera_roll_add);
}

/* 00490570 with *004c55e0 ('smok') */
static void trail_puff(void *ctx, int32_t scale, int32_t x, int32_t y)
{
    (void)ctx;
    sprite_scaled_blend_at(object_pixels.target, puff_blend->rows, puff_art->smok, scale, x, y);
}


static int32_t last_near_dir[3];
static int32_t last_near;

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

/* 0045a724 */
static void row_prepare(void *ctx, Car *car, int pass)
{
    (void)ctx;
    const CarShadowOps ops = { NULL, shadow_contact, shadow_followed, NULL, 0 };
    int32_t screen[8];
    if (soft_shadow_on && shadow_remap != NULL && object_pixels.palette != NULL) {   /* port: the quad's corners, the shade not drawn (remap NULL) */
        const CarShadowOps cut = { NULL, shadow_contact, shadow_followed, follow_arm_clip_quad, 1 };   /* and the quad cut at the near plane, not lost */
        if (car_prepare(&cut, car, &model_view, pass, car == followed_car(), object_pixels.target, NULL, screen))
            soft_shadow_draw(object_pixels.target, shadow_remap, soft_half(), soft_shadow_level(psx_light_node(psx_light, (uint32_t)car->node)), screen);
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

/* 00458d4c(car, 0051c774, +0x391) */
static void render_body(void *ctx, Car *car, const int32_t matrix[9], const int32_t position[3])
{
    BodyCtx *b = ctx;
    const RaceState *race = shadow_race;
    const CarBodyOps ops = { b, body_eye, body_matrix, body_transform, body_project, body_sort, body_draw, body_sort_list,
                             body_draw_list, body_wheels, body_steer, body_smoke, body_palette };
    const CarLightInputs in = { { race->stop_notice[0], race->stop_notice[1] },
                                light_slots != NULL ? light_slots : NO_SLOT_COLOURS, race->finish_cars, race->tick,
                                &flash_tick, &flash };
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
    const CrashTrailView v = { mv->view, mv->camera_position, mv->camera_matrix, WORLD_SHIFT, NULL, trail_puff };
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
            object_sink_set_light(car_light(car->node, object_pixels.palette));
            body_shine_begin(object_pixels.palette, model_view.camera_matrix);
            render_slot(&cars[i], car, pass, &model_view, &sink, object_pixels.target);
            body_shine_end();
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
    object_sink_set_light(car_light(car->node, palette));
    body_shine_begin(palette, mv->camera_matrix);
    render_slot(&cars[slot], car, mirror, mv, &sink, target);
    body_shine_end();
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
        cars_on_row(&ops, &in, node);
        return;
    }
    const Object3DSink sink = object_sink(&object_pixels);
    for (int i = 0; i < car_count; i++)
        if (cars[i].ok && cars[i].node == node) {
            object_sink_set_light(car_light(node, object_pixels.palette));
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
    raster_fill(t->raster, x, y, w, h, colour);
}

/* 00484389 */
static void sky_polygon(void *user, uint8_t colour, const int32_t points[8])
{
    const SkyTargets *t = user;
    if (t->banded) flat_polygon_rows(t->full, colour, 4, points, t->top, t->bottom);
    else flat_polygon(t->quad, colour, 4, points);
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
static PortedViewport viewport = { 0, 0, WIDTH, HEIGHT, FOV, 0x10000, 0, 0, WIDTH, HEIGHT, 0, 0, 1 };

/* port: the frame the 3D view draws into, 640 x 480 but on a wide screen (ported_view_set_screen) */
static int32_t screen_w = WIDTH, screen_h = HEIGHT;

void ported_view_set_screen(int width, int height)
{
    screen_w = width, screen_h = height;
}

/* 00512040..: the road direction the last frame's transform left (the mirror draws with it). */
static int32_t last_near_dir[3] = { 0, 0, 0x1999 };
static int32_t last_near = 62259 >> WORLD_SHIFT;  /* *(00511fe8) +4 as the last frame's transform left it */

void ported_view_near_dir(int32_t out[3])
{
    memcpy(out, last_near_dir, sizeof last_near_dir);
}

void ported_view_set_viewport(const PortedViewport *v)
{
    viewport = *v;
}

static SkyRing sky_ring;   /* built once a frame by the caller, read by every band */

static void draw_sky_ring(const SkyTargets *targets, const View *view, const FshEntry *horz, const TrackInfo *info,
                          const TrackCamera *cam, int32_t yaw, int32_t roll)
{
    const SkyRing *ring = &sky_ring;
    const int32_t rs = viewport.render_scale > 0 ? viewport.render_scale : 1;
    SkyFrame frame = { view, cam->matrix, { 0 }, yaw, roll, view_mirrored(), SKY_WORLD_SHIFT, 0, { 0 },
                       { viewport.scale, viewport.width, viewport.sky_left, viewport.sky_top, viewport.sky_width,
                         viewport.sky_height }, screen_w / rs, screen_w, 0, sky_off, 1, rs };   /* port: wide fields, render scale */
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
        const int32_t x0 = st->box_left < 0 ? 0 : st->box_left, x1 = st->box_right > screen_w ? screen_w : st->box_right;
        if (x1 > x0) memset(st->pixels + (size_t)y * screen_w + x0, 0, (size_t)(x1 - x0));
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
        const HorizonTarget ht = { st->pixels, screen_w };
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

void ported_view_render_from(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *pc,
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
    track_render_build(&render, tri, art, camera_node);
    if (shadow_race != NULL) {
        render.build.knocked = shadow_race->scenery_state;  /* again, for a first build that bound the track */
        shadow_race->scenery_stream_node = render.build.first_node;
    }
}

void ported_view_forget_track(void)
{
    render.tri = NULL;
    ported_view_set_models(NULL, NULL, 0);  /* the next track's models sit at the same address, bound to these textures */
    horizon_smooth_cache_reset();           /* port: the sky bands kept for this track's bitmap */
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

void ported_view_render_cars(const Tri *tri, const TrackArt *art, const TrackInfo *info, const PortedCamera *pc,
                             const PortedCar *list, int count, uint8_t *pixels)
{
    static View view;
    static HorizonTables horizon;
    const PortedViewport *vp = &viewport;
    view_init(&view, vp->left, vp->top, vp->width, vp->height, vp->fov, vp->fov_y != 0 ? vp->fov_y : vp->fov);
    /* port: the exe's rows scale grows with the width (0043a028); past 640 it keeps the 640's zoom */
    const int32_t rs = vp->render_scale > 0 ? vp->render_scale : 1;   /* port (render scale): the zoom of the unscaled width */
    horizon_tables(&horizon, vp->width, vp->fov, screen_horizon_scale(vp->scale, vp->width / rs));
    RasterTarget target = { pixels, screen_w, screen_h, 0, 0 };
    /* the box (00449f50: the layout's left, top, +0x20, +0x24); left + x, top + y only place the projection */
    const int32_t box[4] = { vp->sky_left, vp->sky_top, vp->sky_left + vp->sky_width, vp->sky_top + vp->sky_height };
    QuadTarget quad = { pixels, screen_w, box[0], box[1], box[2], box[3] };
    DrawContext ctx = { &view, { box[0], box[1], box[2], box[3] }, WORLD_SHIFT,
                        { 0, 0, 0x1999 },
                        { &quad, draw_uv, draw_full, draw_full_alt, draw_model, draw_cars, draw_skids,
                          modern ? draw_camera : NULL, draw_camera }, 0,
                        vp->near_scale };
    TrackCamera cam = { { pc->position[0], pc->position[1], pc->position[2] }, { 0 }, pc->node };
    track_camera_matrix(pc->pitch, pc->yaw, pc->roll, view_mirrored(), vp->scale, cam.matrix);  /* the layout's +4 (00449670) */
    /* 0044cc74 clears nothing (00473680 picks the surface): the horizon's
     * fills cover the view; the view (only: the rest of the surface holds
     * 004218d0's tiles and frame) cleared here for the frames without one. */
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
    SkyStage stage = { pixels, &quad, &target, &view, &horizon, &ctx.clip, art, sky_info, &cam, pc,
                       own != NULL ? &own->smooth : sky_filter ? &smooth : NULL, own != NULL ? &own->horz : &art->horizon, ring_horz,
                       box[0], box[2], 0, ring && sky_ring.enabled, own != NULL || (art->has_horizon && !ring), bands_threads() != 0 };
    if (stage.flat) stage.line = horizon_line(&view, WORLD_SHIFT, pc->yaw, cam.matrix);
    static SkyStretch stretch;   /* port (sky_fill): the art reaching the view's top, built once a frame for all the bands */
    if (stage.flat && port_sky_fill()) stage.horz = sky_stretch(&stretch, stage.horz, horizon.scale, stage.line, ctx.clip.y0, port_sky_fill_mode(), art->palette);
    bands_rows(box[1] < 0 ? 0 : box[1], box[3] < screen_h ? box[3] : screen_h, sky_stage_band, &stage);
    const WeatherTables *wt = weather != WEATHER_CLEAR ? weather_tables(art) : NULL;
    if (wt != NULL) {   /* port (weather): the sky toward its colour, before the track covers it */
        const WeatherSky sky = weather_sky(pixels, screen_w, box[0] < 0 ? 0 : box[0], box[2] > screen_w ? screen_w : box[2], wt,
                                           &view, cam.matrix);
        bands_rows(box[1] < 0 ? 0 : box[1], box[3] < screen_h ? box[3] : screen_h, weather_sky_band, (void *)&sky);
    }
    stream(tri, art, pc->node);
    if (!modern) modern_road_set_style(NULL);
    else if (wt != NULL) weather_style(art, wt);
    else {
        modern_look_set_sample(&look, port_fog_sample(), port_fog_lift());
        modern_road_set_style(modern_look_style(&look, art, look_filter, look_fog, render.draw_segments));
    }
    track_render_transform(&render, &cam, &ctx);
    memcpy(last_near_dir, ctx.near_dir, sizeof last_near_dir);
    last_near = ctx.near;
    model_view = (ModelView){ &view, cam.position, cam.matrix,
                              shadow_race != NULL && model_draw_half_side(shadow_race->track, shadow_race->track_variant), 1 };
    car_count = count < 0 ? 0 : count > PORTED_VIEW_CARS ? PORTED_VIEW_CARS : count;
    for (int i = 0; i < car_count; i++) {
        cars[i].pose = list[i].pose;
        cars[i].car = list[i].car;
        cars[i].node = list[i].node;
    }
    static uint8_t shade[256];
    if (shadow_remap == NULL) object_sink_shade(art->palette, shade);
    object_pixels = (ObjectSink){ &quad, art->palette, shadow_remap != NULL ? shadow_remap : shade, modern_view(&view) };
    skid_node = pc->node;
    /* port (render bands P2, P3): with threads the track's draws (road, scenery, cars, shadows, skid marks, smoke) are
     * recorded by the rasterizers, which find the recorder in the target, and replayed by bands at the end, in order */
    static struct RasterRecorder recorder;
    if (bands_threads() != 0) {
        raster_record_set_runner(bands_rows);
        raster_record_begin(&recorder, pixels);
    }
    if (psx_light != NULL) {
        /* the tables again only when the palette or the tint moved (a fade changes the palette) */
        static uint8_t made_for[768];
        static float made_tint[3];
        static int made, made_cast;
        const int cast = lamp_count > 0;   /* a tunnel track: its darkened levels take the olive cast */
        if (!made || made_cast != cast || memcmp(made_for, art->palette, sizeof made_for) != 0 || memcmp(made_tint, lit_tint, sizeof made_tint) != 0) {
            modern_light_tables(art->palette, lit_tint, lit_tables);
            modern_light_tables_cast(art->palette, lit_tint, cast ? TUNNEL_CAST : NO_CAST, tunnel_tables);
            memcpy(made_for, art->palette, sizeof made_for), memcpy(made_tint, lit_tint, sizeof made_tint), made = 1, made_cast = cast;
        }
        for (int i = 0; i < lamp_count; i++)   /* the lamp textures of this track, in each of their variants */
            for (int v = 0; v < LAMP_VARIANTS; v++) {
                const TrackTexture *t = lamp_spec[i].slot >= 0 && lamp_spec[i].slot < TRACK_TEXTURE_SLOTS && v < TRACK_TEXTURE_VARIANTS
                                            ? &art->slots[lamp_spec[i].slot][v] : NULL;
                lamp_bitmap[i][v] = t != NULL && t->present ? &t->bitmap : NULL;
            }
        for (int i = 0; i < lamp_count; i++) {   /* the lamps' tables from the world's and the colours each shows (cheap: a texture and 16 tables) */
            const FshEntry *t = lamp_bitmap[i][0];
            uint8_t exempt[256];
            psx_lamp_exempt(&lamp_spec[i], t != NULL ? t->payload : NULL, t != NULL ? t->width : 0, t != NULL ? t->height : 0, exempt);
            psx_lamp_tables(exempt, (const uint8_t (*)[256])tunnel_tables, MODERN_FOG_LEVELS, lamp_tables[i]);   /* lamps hang in tunnels: their dim parts are cast too */
        }
        track_light_hook = light_hook;
        tables_made = 1;
    }
    lit_level = 0;
    memset(lit_corner, 255, sizeof lit_corner);
    int ahead = 1;   /* the exe's pass draws the track */
    if (shadow_race != NULL && shadow_race->port_follow_camera) {   /* port: the camera may look along the track either way */
        follow_arm_window(&render, FOLLOW_ARM_SEGMENTS);
        ahead = follow_off_road(pc) <= FOLLOW_EXE_BELOW;
        if (!ahead) {
            const FollowArm arm = { &render, &cam, &ctx, tri_is_circuit(tri), anim_tick, view_mirrored(), 0 };
            follow_behind_draw(&arm, FOLLOW_ARM_SEGMENTS, render.draw_segments - 2);
        }
    }
    if (ahead) track_render_draw(&render, &cam, &ctx, view_mirrored(), anim_tick);
    track_light_hook = NULL;
    lit_active = 0;
    if (shadow_race != NULL && shadow_race->port_follow_camera) draw_followed_car(pc, ahead);
    if (raster_active == &recorder) raster_record_end(&recorder);
    if (wt != NULL && weather == WEATHER_RAIN)   /* port (weather): the streaks over the whole view, under the 2D layer */
        weather_rain_draw(pixels, screen_w, box, rs, (uint32_t)anim_tick, wt->rain);
}

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
