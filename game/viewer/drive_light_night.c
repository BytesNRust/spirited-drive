/* The light editor's reload and lines, and the night's look drawn (drive_light.c's split): the lamps, windows and palettes of the frame. */
#include "viewer/drive_light.h"
#include "viewer/class_overlay.h"
#include "viewer/drive_palette.h"
#include "viewer/ported_view.h"
#include "viewer/sky_art_draw.h"
#include "game/race/psx_sky.h"
#include "game/platform/platform.h"
#include "game/race/psx_lgs.h"
#include "game/race/light_tint.h"
#include "viewer/track_load.h"
#include "game/race/track_art.h"
#include "game/race/tunnel_lamp_textures.h"
#include "game/race/track_node_light.h"
#include "game/race/psx_lgt.h"
#include "game/race/street_lamp.h"
#include "game/race/track_mask.h"
#include "game/race/tunnel_lamps.h"
#include "game/race/finish_gate_light.h"
#include "game/race/window_lights.h"
#include "game/race/terrain_light.h"
#include "game/race/light_synth.h"
#include "game/race/track_light_file.h"
#include "game/race/track_geometry.h"
#include "game/race/track_objects.h"
#include "game/sys/reverse_track.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include "viewer/drive_light_private.h"

/* the FNV-1a of a file folded into `h` (a missing file adds nothing) */
static uint32_t hash_file(const char *path, uint32_t h)
{
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    for (size_t i = 0; file != NULL && i < size; i++) h = (h ^ file[i]) * 16777619u;
    if (file != NULL) platform_free_file(file);
    return h;
}

/* the editor's watch: the checksum of the light file and the places file together (either saved changes it); `path` the light file's */
int drive_light_file_state(char *path, size_t path_size, uint32_t *hash)
{
    if (made_tri == NULL || !synth_on) return 0;
    char found[1024], places[1024];
    const int have_light = track_light_find(made_track, light_letter, found, sizeof found);
    const int have_places = track_light_place_find(made_track, places, sizeof places);
    if (hash != NULL) *hash = have_light || have_places ? hash_file(have_places ? places : "", hash_file(have_light ? found : "", 2166136261u)) : 0;
    if (path != NULL && path_size > 0) snprintf(path, path_size, "%s", have_light ? found : "");
    return 1;
}

/* the light editor's lines for `node` (light_edit.h): the track and time, what the file did, and the values the made light has there */
int drive_light_edit_lines(uint32_t node, char lines[][96], int max)
{
    if (made_tri == NULL || !synth_on || light.data == NULL || node >= light.nodes || max < 6) return 0;
    int n = 0;
    snprintf(lines[n++], 96, "LIGHT EDIT  %s%c  node %u of %u", made_track, light_letter, node, light.nodes);
    snprintf(lines[n++], 96, "%s", track_light_status);
    if (track_light_places[0] != '\0' && n < max) snprintf(lines[n++], 96, "places: %s", track_light_places);
    if (track_light_note[0] != '\0' && n < max) snprintf(lines[n++], 96, "%s", track_light_note);
    const uint8_t *row = &light.data[(size_t)node * TRACK_NODE_LIGHT_POINTS];
    snprintf(lines[n++], 96, "row %u %u %u %u %u %u %u %u %u %u %u", row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7], row[8], row[9], row[10]);
    snprintf(lines[n++], 96, "tunnel %d  hall %d  dim car %d  canyon %d  pool %d  car pool %d", light.tunnel != NULL && light.tunnel[node], light.hall != NULL && light.hall[node],
             light.car_dim != NULL && light.car_dim[node], light.canyon != NULL && light.canyon[node], light.pool_class != NULL ? light.pool_class[node] : 0,
             light.car_pool != NULL ? light.car_pool[node] : 0);
    if (n < max) {   /* port (weather_zones): what the rain, fog and lightning make of this node */
        const int open = ported_view_weather_open(node);
        snprintf(lines[n++], 96, "weather: %s %d/16", open >= 16 ? "open" : "closed", open);
    }
    if (n < max) {   /* port (`lamp`): the street lamp of the stretch (the nearest within its reach) */
        const TrackNodeLightLamp *near = NULL;
        for (int i = 0; i < light.lamp_n; i++) {
            const int d = (int)light.lamps[i].node - (int)node;
            if ((d < 0 ? -d : d) <= light.lamp_reach_nodes && (near == NULL || (d < 0 ? -d : d) < abs((int)near->node - (int)node))) near = &light.lamps[i];
        }
        if (light_letter != 'N') snprintf(lines[n++], 96, "lamp: off (dawn/day/dusk)");
        else if (near == NULL) snprintf(lines[n++], 96, "street lamp: none");
        else snprintf(lines[n++], 96, "street lamp: node %u, light %.0f%% (colour %.2f %.2f %.2f), reach %.0f m", near->node, 100.0f * fmaxf(near->rgb[0], fmaxf(near->rgb[1], near->rgb[2])), near->rgb[0], near->rgb[1], near->rgb[2], near->reach);
    }
    const int zone_slot = light.tone_id != NULL && light.tone_id[node] > 0 && light.tone_id[node] <= light.tone_count && light.tone[light.tone_id[node] - 1].look;   /* a tunnel zone's own look shares the tone slots */
    if (light.tone_id != NULL && light.tone_id[node] > 0 && light.tone_id[node] <= light.tone_count && !zone_slot && n < max) {   /* the scene tone of the stretch (`tone`) */
        const TrackNodeLightTone *t = &light.tone[light.tone_id[node] - 1];
        char strip[64] = "";
        if (light.tone_mask[node] != (1u << TRACK_NODE_LIGHT_POINTS) - 1u)
            for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++)
                if (light.tone_mask[node] >> p & 1u) snprintf(strip + strlen(strip), sizeof strip - strlen(strip), " p%d", p);
        snprintf(lines[n++], 96, "scene tone %d of %d: %02x%02x%02x at %d%%%s", light.tone_id[node], light.tone_count, t->rgb[0], t->rgb[1], t->rgb[2], t->percent, strip);
    } else if (n < max) {
        snprintf(lines[n++], 96, "scene tone: none");
    }
    double look_light = 0.0;
    float look_cast[3];
    const int zone_look = track_node_light_zone_look_at(&light, node, &look_light, look_cast);
    if (zone_look && n < max) {   /* the tunnel's look here: its zone's own (a `tunnel a b light colour` line) or the time's `tunnel_tone`: its light as a percent of full light, and its colour */
        snprintf(lines[n++], 96, "tunnel look: light %.4g%%  colour %02x%02x%02x%s%s", look_light, (unsigned)(look_cast[0] * 255.0f + 0.5f), (unsigned)(look_cast[1] * 255.0f + 0.5f),
                 (unsigned)(look_cast[2] * 255.0f + 0.5f), zone_slot ? "  (this zone's own)" : "", light.hall != NULL && light.hall[node] ? "  (a hall: lit by its lamps)" : "");
    }
    else if (!zone_look && n < max)
        snprintf(lines[n++], 96, "tunnel look: the made tunnel's own");
    return n;
}

/* the PC's alternative sky (NTRACKFM/<TRACK>_T01.FAM, the art the original's morning / evening race ran on) for a dawn or dusk the CD has no console sky for:
 * only its 'horz' picture, palette and INFO (the ring), drawn through the console version sky's path (the nearest of the world as it is shown), so the time's tint is
 * not put on a sky that is already of its time; the rest of the art stays the midday's. Nothing when the CD holds no such FAM (Lost Vegas). */
static TrackInfo alt_info;

void drive_light_load_alt_sky(const char *data_root, const char *track)
{
    if (!alt_sky_wanted || data_root == NULL || track == NULL) return;
    alt_sky_wanted = 0;
    FshEntry horizon;
    uint8_t palette[768];
    uint8_t *file = NULL;
    if (track_viewer_pc_sky(data_root, track, &alt_info, &horizon, palette, &file) && sky_art_draw_make_indexed(&sky_draw, &horizon, palette)) {
        if (night_sky) sky_scale_night();
        ported_view_set_sky_art(&sky_draw);
        ported_view_set_sky_art_info(&alt_info);
        port_log("psx_sky: the PC's %s sky, %dx%d, ring %d\n", track, horizon.width, horizon.height, alt_info.ring);
    } else {
        port_log("psx_sky: no PC sky for %s, the track's sky stays\n", track);
    }
    platform_free_file(file);
}

int drive_light_night_lamps(const TunnelLampTexture **out)
{
    if (out != NULL) *out = night_lamp_spec;
    return night_lamp_count;
}

static uint8_t lamp_pool[16];

void drive_light_lamp_pool(const uint8_t entry[], int n)
{
    lamp_pool_count = n < (int)sizeof lamp_pool ? n : (int)sizeof lamp_pool;
    memcpy(lamp_pool, entry, (size_t)lamp_pool_count);
}

static int brighter(const uint8_t palette[768], int a, int b)
{
    return palette[3 * a] + palette[3 * a + 1] + palette[3 * a + 2] > palette[3 * b] + palette[3 * b + 1] + palette[3 * b + 2];
}

/* the pool's last entries are the extras': the street lamp's three, then the windows' one */
static int street_entries(void) { return street_lamp_on ? STREET_LAMP_ENTRIES : 0; }
static int window_entries(void) { return window_board_count > 0 || dark_board_count > 0 || sign_board_count > 0 ? WINDOW_ENTRIES : 0; }

int drive_light_street_lamp_entries(void)
{
    return street_entries() + window_entries();
}

/* the street lamp's colours (the lens bright, the halo's core and rim warmer and dimmer), in the last pool entries after the tunnels' colours */
static void street_lamps_night(TrackArt *art)
{
    static const uint8_t RGB[STREET_LAMP_ENTRIES][3] = { { 255, 236, 170 }, { 214, 160, 70 }, { 130, 84, 34 } };
    if (!street_lamp_on || lamp_pool_count < street_entries() + window_entries() || art == NULL) return;
    uint8_t entry[STREET_LAMP_ENTRIES];
    for (int k = 0; k < STREET_LAMP_ENTRIES; k++) {
        entry[k] = lamp_pool[lamp_pool_count - window_entries() - STREET_LAMP_ENTRIES + k];
        memcpy(art->palette + 3 * entry[k], RGB[k], 3);
        drive_palette_write(entry[k], RGB[k]);
    }
    int scenery[STREET_LAMPS_MAX];
    for (int i = 0; i < street_lamp_count; i++) scenery[i] = street_lamps[i].scenery;
    ported_view_set_street_lamps(scenery, street_lamp_count, entry);
    for (int i = 0; i < street_lamp_count; i++) {
        const StreetLamp *const l = &street_lamps[i];
        TrackTexture *t = l->scenery >= 0 && l->scenery < TRACK_SCENERY_TEXTURES ? &art->scenery[l->scenery] : NULL;
        if (t == NULL || !t->present) continue;
        uint8_t *const pixels = (uint8_t *)(uintptr_t)t->bitmap.payload;
        const StreetLampPaint p = street_lamp_by_day ? street_lamp_paint_day(l, pixels, t->bitmap.width, t->bitmap.height, entry)
                                                     : street_lamp_paint(l, pixels, t->bitmap.width, t->bitmap.height, entry);
        port_log("street_lamps: board %d: %zu lens texels and %zu halo texels in entries %d %d %d\n", l->scenery, p.lens, p.halo, entry[0], entry[1], entry[2]);
    }
}

/* the lit windows' colour (a warm yellow, dimmer than the lamps' lens), the pool's very last entry, painted into each window board */
static void windows_night(TrackArt *art)
{
    static const uint8_t RGB[3] = { 236, 190, 96 };
    if ((window_board_count <= 0 && dark_board_count <= 0 && sign_board_count <= 0) || lamp_pool_count < window_entries() || art == NULL) return;
    const uint8_t entry = lamp_pool[lamp_pool_count - 1];
    memcpy(art->palette + 3 * entry, RGB, 3);
    drive_palette_write(entry, RGB);
    int scenery[WINDOW_BOARDS_MAX];
    int n = 0;
    for (int i = 0; i < window_board_count && n < WINDOW_BOARDS_MAX; i++) {
        const WindowBoard *b = &window_boards[i];
        TrackTexture *t = b->scenery >= 0 && b->scenery < TRACK_SCENERY_TEXTURES ? &art->scenery[b->scenery] : NULL;
        if (t == NULL || !t->present) continue;
        const size_t lit = window_board_paint(b, (uint8_t *)(uintptr_t)t->bitmap.payload, t->bitmap.width, t->bitmap.height, entry, WINDOW_SEED);
        port_log("windows: board %d: %zu of %d windows lit in entry %d\n", b->scenery, lit, b->ncol * b->nrow, entry);
        scenery[n++] = b->scenery;
    }
    for (int i = 0; i < dark_board_count && n < WINDOW_BOARDS_MAX; i++) {
        const WindowDark *b = &dark_boards[i];
        TrackTexture *t = b->scenery >= 0 && b->scenery < TRACK_SCENERY_TEXTURES ? &art->scenery[b->scenery] : NULL;
        if (t == NULL || !t->present) continue;
        const size_t lit = window_dark_paint(b, (uint8_t *)(uintptr_t)t->bitmap.payload, t->bitmap.width, t->bitmap.height, art->palette, entry, WINDOW_SEED);
        port_log("windows: board %d (by darkness): %zu patches lit in entry %d\n", b->scenery, lit, entry);
        scenery[n++] = b->scenery;
    }
    for (int i = 0; i < sign_board_count && n < WINDOW_BOARDS_MAX; i++) {   /* port (sign_glow): the matrix sign's bulbs */
        const WindowSign *b = &sign_boards[i];
        TrackTexture *t = b->scenery >= 0 && b->scenery < TRACK_SCENERY_TEXTURES ? &art->scenery[b->scenery] : NULL;
        if (t == NULL || !t->present) continue;
        const size_t lit = window_sign_paint(b, (uint8_t *)(uintptr_t)t->bitmap.payload, t->bitmap.width, t->bitmap.height, art->palette, entry);
        port_log("sign_glow: board %d: %zu bulb texels lit in entry %d\n", b->scenery, lit, entry);
        scenery[n++] = b->scenery;
    }
    ported_view_set_window_boards(scenery, n, entry);
}

void drive_light_lamps_night(TrackArt *art)
{
    const int street = street_entries() + window_entries();   /* the pool's last entries are the extras' */
    street_lamps_night(art);
    windows_night(art);
    if (night_lamp_count <= 0 || lamp_pool_count - street <= 0 || art == NULL) return;
    uint8_t wanted[256] = { 0 }, colour[256], remap[256];
    int n = 0;
    for (int i = 0; i < night_lamp_count; i++) {
        const int slot = night_lamp_spec[i].slot;
        const TrackTexture *t = slot >= 0 && slot < TRACK_TEXTURE_SLOTS ? &art->slots[slot][0] : NULL;
        uint8_t exempt[256];
        tunnel_lamp_texture_exempt(&night_lamp_spec[i], t != NULL && t->present ? t->bitmap.payload : NULL, t != NULL ? t->bitmap.width : 0,
                        t != NULL ? t->bitmap.height : 0, exempt);
        for (int c = 0; c < 256; c++)
            if (exempt[c] && !wanted[c]) wanted[c] = 1, colour[n++] = (uint8_t)c;
    }
    for (int a = 1; a < n; a++) {   /* brightest first: with fewer entries than colours it is the glare that stays lit */
        const uint8_t c = colour[a];
        int b = a - 1;
        while (b >= 0 && brighter(art->palette, c, colour[b])) colour[b + 1] = colour[b], b--;
        colour[b + 1] = c;
    }
    for (int c = 0; c < 256; c++) remap[c] = (uint8_t)c;
    const int used = n < lamp_pool_count - street ? n : lamp_pool_count - street;
    for (int k = 0; k < used; k++) {
        remap[colour[k]] = lamp_pool[k];
        memcpy(art->palette + 3 * lamp_pool[k], art->palette + 3 * colour[k], 3);
        drive_palette_write(lamp_pool[k], art->palette + 3 * colour[k]);
    }
    size_t texels = 0;   /* the lamp textures (every variant) draw the entries of their own: raw or through the tables, the tint leaves them alone */
    for (int i = 0; i < night_lamp_count; i++) {
        const int slot = night_lamp_spec[i].slot;
        for (int v = 0; slot >= 0 && slot < TRACK_TEXTURE_SLOTS && v < TRACK_TEXTURE_VARIANTS; v++) {
            const TrackTexture *t = &art->slots[slot][v];
            if (t->present)
                texels += tunnel_lamp_texture_recolour_rect(&night_lamp_spec[i], (uint8_t *)(uintptr_t)t->bitmap.payload, t->bitmap.width, t->bitmap.height, remap);
        }
    }
    port_log("night_lamps: %d of %d lamp colours in entries of their own (brightest first), %zu texels\n", used, n, texels);
}

static int storm_flash;   /* port (WEATHER_STORM): 0 none .. 15 */

void drive_light_set_flash(int level) { storm_flash = level < 0 ? 0 : level > 15 ? 15 : level; }

void drive_light_palette(const uint8_t palette[768], uint8_t out[768])
{
    light_tint_palette(palette, tint, out);
    if (storm_flash > 0)
        for (int i = 0; i < 768; i++) out[i] = (uint8_t)((out[i] * (15 - storm_flash) + palette[i] * storm_flash + 7) / 15);
}

/* the 3D as it was when it was done, and the mask of a shown frame against it */
static uint8_t *world_frame, *world_mask;
static size_t world_size;

void drive_light_world_done(const uint8_t *frame, size_t count)
{
    if (!drive_light_active()) return;
    if (world_size != count) {
        uint8_t *f = realloc(world_frame, count), *m = realloc(world_mask, count);
        if (f != NULL) world_frame = f;
        if (m != NULL) world_mask = m;
        if (f == NULL || m == NULL) {
            world_size = 0;
            return;
        }
        world_size = count;
    }
    memcpy(world_frame, frame, count);
}

static int rect[4], rect_set, rect_width;

/* the 2D layer's classes (drive_light.h) per layer pixel, and the output's size against it */
static uint8_t *class_layer;
static size_t class_size;
static uint8_t *class_marks;   /* class_overlay_rows: a row of the layer that has a mark */
static int class_width, class_height, class_scale = 1;

void drive_light_tint(float out[3]) { memcpy(out, tint, sizeof tint); }

int drive_light_night(void) { return night_look; }

/* port (gauge_exterior): the gauges are lit when the exterior lights are: the night, and with gauge_exterior the dusk and the rain with its headlights on (rain_lamps) */
int drive_light_gauges_lit(void)
{
    return night_look || (port_gauge_exterior() && (dusk_look || (port_rain_lamps() && ported_view_rain())));
}

int drive_light_active(void) { return split || tint[0] != 1.0f || tint[1] != 1.0f || tint[2] != 1.0f; }

void drive_light_world_class(const uint8_t *layer, int width, int height, int scale)
{
    const size_t count = (size_t)width * (size_t)height;
    if (!drive_light_active() || layer == NULL) {
        class_width = 0;
        return;
    }
    if (class_size != count || class_height != height) {
        uint8_t *p = realloc(class_layer, count), *m = realloc(class_marks, (size_t)height);
        if (m != NULL) class_marks = m;
        if (p == NULL || m == NULL) {
            if (p != NULL) class_layer = p;
            class_width = 0;
            class_size = 0;
            return;
        }
        class_layer = p, class_size = count;
    }
    memcpy(class_layer, layer, count);
    class_overlay_rows(class_layer, width, height, class_marks);
    class_width = width, class_height = height, class_scale = scale < 1 ? 1 : scale;
}

void drive_light_dash_ratio(const float ratio[3])
{
    for (int k = 0; k < 3; k++) dash_ratio[k] = ratio != NULL ? ratio[k] : 1.0f;
}

/* the dash's art: half the world's tint (the whole at night), as the dawn / dusk base, times the car's light ratio */
void drive_light_palette_half(const uint8_t palette[768], uint8_t out[768])
{
    light_tint_dash_palette(palette, tint, 1, night_look ? 1 : 2, out);   /* port: the dash is dark with the night; the dials (the HUD's class) are not touched */
    if (dash_ratio[0] == 1.0f && dash_ratio[1] == 1.0f && dash_ratio[2] == 1.0f) return;
    uint8_t base[768];
    memcpy(base, out, sizeof base);
    light_tint_palette(base, dash_ratio, out);
}

int drive_light_palette_lit(const uint8_t palette[768], uint8_t out[768])
{
    /* the key's colours, per channel (the strongest 1): the mock's soft green, red, amber and light blue */
    static const float COLOUR[5][3] = { { 1, 1, 1 }, { 0.55f, 1.0f, 0.6f }, { 1.0f, 0.45f, 0.4f }, { 1.0f, 0.7f, 0.25f }, { 0.5f, 0.75f, 1.0f } };
    int key = port_gauge_light();
    if (key == 5) key = 3;   /* the car's colours: orange where the dash paints none */
    if (key <= 0 || key > 4 || !drive_light_gauges_lit()) return 0;
    for (int i = 0; i < 256; i++) {
        const float lum = (palette[3 * i] + palette[3 * i + 1] + palette[3 * i + 2]) / 3.0f;
        const float lit = lum * 1.6f + 20.0f > 255.0f ? 255.0f : lum * 1.6f + 20.0f;   /* a lit dial is brighter than the unlit art */
        for (int k = 0; k < 3; k++) out[3 * i + k] = (uint8_t)(lit * COLOUR[key][k] + 0.5f);
    }
    return 1;
}

int drive_light_palette_lit_colour(const uint8_t palette[768], const uint8_t rgb[3], uint8_t out[768])
{
    if (port_gauge_light() != 5 || !drive_light_gauges_lit()) return 0;
    const int top = rgb[0] > rgb[1] ? (rgb[0] > rgb[2] ? rgb[0] : rgb[2]) : (rgb[1] > rgb[2] ? rgb[1] : rgb[2]);
    if (top <= 0) return 0;
    for (int i = 0; i < 256; i++) {
        const float lum = (palette[3 * i] + palette[3 * i + 1] + palette[3 * i + 2]) / 3.0f;
        const float lit = lum * 1.6f + 20.0f > 255.0f ? 255.0f : lum * 1.6f + 20.0f;
        for (int k = 0; k < 3; k++) out[3 * i + k] = (uint8_t)(lit * (float)rgb[k] / (float)top + 0.5f);
    }
    return 1;
}

void drive_light_world_rect(int width, int x, int y, int w, int h)
{
    rect[0] = x, rect[1] = y, rect[2] = w, rect[3] = h, rect_width = width, rect_set = w > 0 && h > 0;
}

/* port: the split present tells three palettes apart; with the tint neutral, the dash's ratio neutral, no storm flash and no lit gauges they are all the
 * frame's palette and the mask picks between equal colours. Decided per frame, so a tunnel by day (the dash's ratio) still splits. */
int drive_light_needs_split(void)
{
    static int forced = -1;   /* TNFS_FORCE_SPLIT=1 (tests/day_plain_present.cmake): always the split present, the picture the plain one must equal */
    if (forced < 0) forced = getenv("TNFS_FORCE_SPLIT") != NULL;
    if (forced) return 1;
    const int neutral = tint[0] == 1.0f && tint[1] == 1.0f && tint[2] == 1.0f && dash_ratio[0] == 1.0f && dash_ratio[1] == 1.0f && dash_ratio[2] == 1.0f;
    return !neutral || storm_flash > 0 || (port_gauge_light() > 0 && drive_light_gauges_lit());
}

const uint8_t *drive_light_world_mask(const uint8_t *shown, size_t count)
{
    if (!drive_light_active() || !drive_light_needs_split() || world_size != count || world_frame == NULL) return NULL;
    light_tint_world_mask(shown, world_frame, count, world_mask);
    if (class_width > 0 && rect_width == class_width * class_scale && count % (size_t)rect_width == 0) {
        /* the 2D layer's own marks over the diff's: the dash's art half, the HUD and the gauges not at all (class_overlay.h: only the rows that have a mark) */
        class_overlay_apply(world_mask, rect_width, count / (size_t)rect_width, class_layer, class_width, class_height, class_scale, class_marks);
    }
    if (rect_set && rect_width > 0 && count % (size_t)rect_width == 0)
        light_tint_mask_rect(world_mask, rect_width, (int)(count / (size_t)rect_width), rect[0], rect[1], rect[2], rect[3]);
    return world_mask;
}
