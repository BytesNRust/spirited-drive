#include "viewer/drive_light.h"
#include "viewer/drive_palette.h"
#include "viewer/ported_view.h"
#include "viewer/psx_sky_draw.h"
#include "game/race/psx_sky.h"
#include "game/platform/platform.h"
#include "game/race/psx_lgs.h"
#include "viewer/track_load.h"
#include "game/race/track_art.h"
#include "game/race/psx_lamps.h"
#include "game/race/psx_light.h"
#include "game/race/psx_light_synth.h"
#include "game/sys/port_log.h"
#include "game/sys/port_settings.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

enum { LIGHT_PATH = 320, LIGHT_TRACK = 16 };

static const char LETTERS[] = { 'B', 'A', 'C', 'C' };   /* by PSX_TIME_*: day, dawn, dusk, night (the dusk's files, darker) */

static PsxLight light;
static float tint[3] = { 1, 1, 1 };
static PsxLgs ambient;
static int ambient_ok;   /* ambient holds the LGS read */
static uint32_t ambient_nodes;
static float dash_ratio[3] = { 1, 1, 1 };   /* the dash's ambient over the world's tint, from the car's light (drive_light_dash_ratio) */
static int split;   /* the race has the light and its script: the dash has its own ambient, so the 3D and the 2D go through the split present */
static PsxSkyDraw sky_draw;
static int lamp_pool_count;
static const PsxLamp *night_lamp_spec;   /* the tunnel lamps of the track when the race is at night (drive_light_night_lamps), else none */
static int night_lamp_count;
static int night_sky;   /* the race is at night: a sky of the dusk is darkened by the night's light before it is matched to the shown palette */

/* the files' path: <psx_dir>/Z<TRACK><letter>.<ext> */
static void file_path(char *out, size_t size, const char *psx_dir, const char *track, char letter, const char *ext)
{
    char upper[LIGHT_TRACK];
    size_t i = 0;
    for (; track[i] != '\0' && i < LIGHT_TRACK - 1; i++) upper[i] = (char)toupper((unsigned char)track[i]);
    upper[i] = '\0';
    snprintf(out, size, "%s/Z%s%c.%s", psx_dir, upper, letter, ext);
}

/* the LGS's ambient colour as a tint, neutral when the file is missing or does not read */
static void load_tint(const char *psx_dir, const char *track, char letter, int night)
{
    tint[0] = tint[1] = tint[2] = 1.0f;
    ambient_ok = 0;
    dash_ratio[0] = dash_ratio[1] = dash_ratio[2] = 1.0f;
    char path[LIGHT_PATH];
    file_path(path, sizeof path, psx_dir, track, letter, "LGS");
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file == NULL) {
        port_log("psx_light: no %s, the ambient stays neutral\n", path);
        return;
    }
    if (psx_lgs_parse(file, size, &ambient)) {
        if (night) psx_lgs_scale(&ambient, psx_night_light, 0);   /* the open road's: a tunnel's own lights (the contexts) stay lit, pools in the dark */
        ambient_ok = 1, psx_lgs_tint(&ambient, tint);
    }
    else port_log("psx_light: %s does not read, the ambient stays neutral\n", path);
    platform_free_file(file);
}

/* dawn and dusk: the PS1's own sky (Z<TRACK><A|C>.INF and .PSH) for the race; the day, a missing or unreadable file, or only the INF
 * leave the track's own sky (tinted with the world) */
static void load_sky(const char *psx_dir, const char *track, char letter)
{
    char path[LIGHT_PATH];
    file_path(path, sizeof path, psx_dir, track, letter, "PSH");
    size_t psh_size = 0;
    uint8_t *psh = platform_read_file(path, &psh_size);
    if (psh == NULL) {
        port_log("psx_sky: no %s, the track's sky stays\n", path);
        return;
    }
    PsxSky sky;
    const int panorama = psx_sky_panorama(psh, psh_size, &sky);
    platform_free_file(psh);
    if (!panorama) {
        port_log("psx_sky: %s has no horizon tiles hrz0..hrz7, the track's sky stays\n", path);
        return;
    }
    PsxSkyInf inf = { { 0 }, { 0 }, { 0 }, 0, 0, 0, 0, 0 };
    file_path(path, sizeof path, psx_dir, track, letter, "INF");
    size_t inf_size = 0;
    uint8_t *text = platform_read_file(path, &inf_size);
    const int gradient = text != NULL && psx_sky_inf_parse(text, inf_size, &inf);
    if (text != NULL) platform_free_file(text);
    if (!gradient) port_log("psx_sky: %s does not read, no gradient above the horizon\n", path);
    if (psx_sky_draw_make(&sky_draw, &sky, &inf)) {
        if (night_sky) psx_sky_draw_scale(&sky_draw, psx_night_light);
        ported_view_set_psx_sky(&sky_draw);
        port_log("psx_sky: %dx%d panorama, gradient %d,%d,%d to %d,%d,%d\n", sky.width, sky.height, inf.top[0], inf.top[1], inf.top[2],
                 inf.bottom[0], inf.bottom[1], inf.bottom[2]);
    }
    psx_sky_free(&sky);
}

static int time_tinted;
static int synth_wanted, synth_on, synth_time;   /* a track the CD has no light for, with the light on; the light is one made from the road (drive_light_load_synth) */
static int alt_sky_wanted;   /* the look is on at dawn or dusk and the CD holds no sky of its own for the track (drive_light_load_alt_sky) */

/* Lost Vegas (tr4) under the PS1 look: its light 10% darker than the CD's script gives, and its fog always dark */
static const float LOST_VEGAS_DARKEN = 0.9f;

static int is_lost_vegas(const char *track) { return track != NULL && strcasecmp(track, "TR4") == 0; }

/* A track the PS1 CD holds no light for (Burnt Sienna is tr6 and its ZTR6 has other nodes, Transtropolis is tr7 and has no Z file): a flat ambient
 * for the time, no light at the nodes. The average of the three TR tracks whose scripts give a time of day (ZTR1, ZTR2, ZTR3's LGS start colours, as
 * a part of 128), dawn then dusk; the one place to change for a pick per track. */
static const float FALLBACK_TINT[2][3] = { { 0.96f, 0.73f, 0.33f }, { 0.88f, 0.55f, 0.46f } };


static void use_fallback_tint(int time)
{
    const float *f = FALLBACK_TINT[time == PSX_TIME_DUSK || time == PSX_TIME_NIGHT];
    for (int k = 0; k < 3; k++) tint[k] = f[k] * (time == PSX_TIME_NIGHT ? psx_night_light[k] : 1.0f);
    port_log("psx_light: no light of this track, the flat ambient of the time: tint %.2f %.2f %.2f\n", tint[0], tint[1], tint[2]);
}

int drive_light_time_tinted(void) { return time_tinted || split; }

void drive_light_load(int enabled, int time, const char *psx_dir, const char *track, uint32_t ring_nodes)
{
    time_tinted = enabled && time != PSX_TIME_DAY;
    night_sky = time == PSX_TIME_NIGHT;
    night_lamp_spec = NULL, night_lamp_count = 0, lamp_pool_count = 0;
    alt_sky_wanted = 0;
    split = 0;
    ported_view_set_soft_shadow(port_psx_shadow());   /* its own flag: no light or PS1 files needed */
    ported_view_set_psx_lamps(NULL, 0);
    ported_view_set_light(NULL, NULL);
    ported_view_set_psx_ambient(NULL, 0, NULL);
    ported_view_set_psx_synth(0);
    synth_wanted = 0;
    ported_view_set_psx_sky(NULL);
    ported_view_set_psx_sky_info(NULL);
    ported_view_set_dark_fog(0);
    psx_sky_draw_free(&sky_draw);
    psx_light_free(&light);
    synth_on = 0;
    tint[0] = tint[1] = tint[2] = 1.0f;
    if (!enabled || psx_dir == NULL || psx_dir[0] == '\0') return;
    synth_wanted = psx_light_synth_track(track);
    synth_time = time == PSX_TIME_DAWN ? PSX_SYNTH_DAWN : time == PSX_TIME_DUSK ? PSX_SYNTH_DUSK : time == PSX_TIME_NIGHT ? PSX_SYNTH_NIGHT : PSX_SYNTH_DAY;   /* the night: the dusk's doors, a tunnel not darker than the road */
    if (is_lost_vegas(track) || time == PSX_TIME_NIGHT) ported_view_set_dark_fog(1);   /* the night's fog is dark as Lost Vegas' */
    const char letter = LETTERS[time >= PSX_TIME_DAY && time <= PSX_TIME_NIGHT ? time : PSX_TIME_DAY];
    if (time != PSX_TIME_DAY) load_sky(psx_dir, track, letter);
    alt_sky_wanted = time != PSX_TIME_DAY && !sky_draw.ready;
    char path[LIGHT_PATH];
    file_path(path, sizeof path, psx_dir, track, letter, "LGT");
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file == NULL) {
        port_log("psx_light: no %s\n", path);
        if (time != PSX_TIME_DAY) use_fallback_tint(time);
        return;
    }
    PsxLight parsed = { 0 };
    const int ok = psx_light_parse(file, size, ring_nodes, &parsed);
    platform_free_file(file);
    if (!ok) {
        port_log("psx_light: %s does not fit the track (%u nodes)\n", path, ring_nodes);
        if (time != PSX_TIME_DAY) use_fallback_tint(time);
        return;
    }
    light = parsed;
    load_tint(psx_dir, track, letter, time == PSX_TIME_NIGHT);
    if (is_lost_vegas(track))
        for (int k = 0; k < 3; k++) tint[k] *= LOST_VEGAS_DARKEN;
    ambient_nodes = ring_nodes;
    if (ambient_ok) {
        ported_view_set_psx_ambient(&ambient, ring_nodes, tint);
        psx_light_set_script(&light, &ambient);   /* the exe's light of the track's points */
        split = 1;
    }
    const PsxLamp *lamps = NULL;
    const int lamp_count = psx_lamps_for(track, &lamps);   /* the tunnel's lamps stay bright under the light */
    ported_view_set_psx_lamps(lamps, lamp_count);
    if (time == PSX_TIME_NIGHT) night_lamp_spec = lamps, night_lamp_count = lamp_count;
    ported_view_set_light(&light, NULL);   /* the tint goes to the palette (drive_light_palette), not through the polygons' tables */
    port_log("psx_light: %s, %u nodes, tint %.2f %.2f %.2f\n", path, light.nodes, tint[0], tint[1], tint[2]);
}

void drive_light_set_reversed(int reversed)
{
    if (synth_on) return;   /* made from the track as it runs: already in its direction */
    psx_light_set_reversed(&light, reversed);
}

void drive_light_load_synth(const Tri *tri, const char *track)
{
    if (!synth_wanted || light.data != NULL || tri == NULL || !psx_light_synth_track(track)) return;
    PsxLight made = { 0 };
    if (!psx_light_synth(tri, synth_time, &made)) return;
    light = made;
    synth_on = 1;
    ported_view_set_light(&light, NULL);
    ported_view_set_psx_synth(1);
    port_log("psx_light: %s has no light on the CD, made from the road's tunnels (%u nodes)\n", track, light.nodes);
}

/* the PC's alternative sky (NTRACKFM/<TRACK>_T01.FAM, the art the original's morning / evening race ran on) for a dawn or dusk the CD has no PS1 sky for:
 * only its 'horz' picture, palette and INFO (the ring), drawn through the PS1 sky's path (the nearest of the world as it is shown), so the time's tint is
 * not put on a sky that is already of its time; the rest of the art stays the midday's. Nothing when the CD holds no such FAM (Lost Vegas). */
static TrackInfo alt_info;

void drive_light_load_alt_sky(const char *data_root, const char *track)
{
    if (!alt_sky_wanted || data_root == NULL || track == NULL) return;
    alt_sky_wanted = 0;
    FshEntry horizon;
    uint8_t palette[768];
    uint8_t *file = NULL;
    if (track_viewer_pc_sky(data_root, track, &alt_info, &horizon, palette, &file) && psx_sky_draw_make_indexed(&sky_draw, &horizon, palette)) {
        if (night_sky) psx_sky_draw_scale(&sky_draw, psx_night_light);
        ported_view_set_psx_sky(&sky_draw);
        ported_view_set_psx_sky_info(&alt_info);
        port_log("psx_sky: the PC's %s sky, %dx%d, ring %d\n", track, horizon.width, horizon.height, alt_info.ring);
    } else {
        port_log("psx_sky: no PC sky for %s, the track's sky stays\n", track);
    }
    platform_free_file(file);
}

int drive_light_night_lamps(const PsxLamp **out)
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

void drive_light_lamps_night(TrackArt *art)
{
    if (night_lamp_count <= 0 || lamp_pool_count <= 0 || art == NULL) return;
    uint8_t wanted[256] = { 0 }, colour[256], remap[256];
    int n = 0;
    for (int i = 0; i < night_lamp_count; i++) {
        const int slot = night_lamp_spec[i].slot;
        const TrackTexture *t = slot >= 0 && slot < TRACK_TEXTURE_SLOTS ? &art->slots[slot][0] : NULL;
        uint8_t exempt[256];
        psx_lamp_exempt(&night_lamp_spec[i], t != NULL && t->present ? t->bitmap.payload : NULL, t != NULL ? t->bitmap.width : 0,
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
    const int used = n < lamp_pool_count ? n : lamp_pool_count;
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
                texels += psx_lamp_recolour_rect(&night_lamp_spec[i], (uint8_t *)(uintptr_t)t->bitmap.payload, t->bitmap.width, t->bitmap.height, remap);
        }
    }
    port_log("night_lamps: %d of %d lamp colours in entries of their own (brightest first), %zu texels\n", used, n, texels);
}

void drive_light_palette(const uint8_t palette[768], uint8_t out[768])
{
    psx_tint_palette(palette, tint, out);
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
static int class_width, class_scale = 1;

void drive_light_tint(float out[3]) { memcpy(out, tint, sizeof tint); }

int drive_light_active(void) { return split || tint[0] != 1.0f || tint[1] != 1.0f || tint[2] != 1.0f; }

void drive_light_world_class(const uint8_t *layer, int width, int height, int scale)
{
    const size_t count = (size_t)width * (size_t)height;
    if (!drive_light_active() || layer == NULL) {
        class_width = 0;
        return;
    }
    if (class_size != count) {
        uint8_t *p = realloc(class_layer, count);
        if (p == NULL) {
            class_width = 0;
            return;
        }
        class_layer = p, class_size = count;
    }
    memcpy(class_layer, layer, count);
    class_width = width, class_scale = scale < 1 ? 1 : scale;
}

void drive_light_dash_ratio(const float ratio[3])
{
    for (int k = 0; k < 3; k++) dash_ratio[k] = ratio != NULL ? ratio[k] : 1.0f;
}

/* the dash's art: half the world's tint, as the dawn / dusk base, times the car's light ratio */
void drive_light_palette_half(const uint8_t palette[768], uint8_t out[768])
{
    uint8_t full[768];
    psx_tint_palette(palette, tint, full);
    for (int i = 0; i < 768; i++) out[i] = (uint8_t)((palette[i] + full[i] + 1) / 2);
    if (dash_ratio[0] == 1.0f && dash_ratio[1] == 1.0f && dash_ratio[2] == 1.0f) return;
    uint8_t base[768];
    memcpy(base, out, sizeof base);
    psx_tint_palette(base, dash_ratio, out);
}

void drive_light_world_rect(int width, int x, int y, int w, int h)
{
    rect[0] = x, rect[1] = y, rect[2] = w, rect[3] = h, rect_width = width, rect_set = w > 0 && h > 0;
}

const uint8_t *drive_light_world_mask(const uint8_t *shown, size_t count)
{
    if (!drive_light_active() || world_size != count || world_frame == NULL) return NULL;
    psx_world_mask(shown, world_frame, count, world_mask);
    if (class_width > 0 && rect_width == class_width * class_scale && count % (size_t)rect_width == 0) {
        /* the 2D layer's own marks over the diff's: the dash's art half, the HUD and the gauges not at all */
        const size_t rows = count / (size_t)rect_width;
        for (size_t y = 0; y < rows; y++) {
            const uint8_t *row = class_layer + (y / (size_t)class_scale) * (size_t)class_width;
            for (size_t x = 0; x < (size_t)rect_width; x++) {
                const uint8_t c = row[x / (size_t)class_scale];
                if (c == PSX_CLASS_DASH) world_mask[y * (size_t)rect_width + x] = 2;
                else if (c == PSX_CLASS_HUD) world_mask[y * (size_t)rect_width + x] = 0;
            }
        }
    }
    if (rect_set && rect_width > 0 && count % (size_t)rect_width == 0)
        psx_mask_rect(world_mask, rect_width, (int)(count / (size_t)rect_width), rect[0], rect[1], rect[2], rect[3]);
    return world_mask;
}
