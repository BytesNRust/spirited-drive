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
#include "game/race/street_lamp.h"
#include "game/race/window_lights.h"
#include "game/race/psx_light_make.h"
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
static HeadlightPool headlight_pool;   /* port (psx_headlights): the pools of light the cars lift the night's dark nodes by (ported_view keeps its nodes) */
static int headlights_on;               /* the race is at night with the pools: the ambient is the pooled night's, the nodes' light carries the dark */
/* with the pools the palette is the dusk's with the night's hue (psx_night_light over HEADLIGHT_FLOOR, 1 at most per channel) and the nodes carry the rest of the
 * dark (headlight_pool.h): open road and country go to the night's light, a tunnel and a car's pool keep the dusk's; the sky keeps psx_night_light */
static const float NIGHT_PALETTE[3] = { 0.80f, 0.96f, 1.0f };
static StreetLamp street_lamps[STREET_LAMPS_MAX];   /* port (psx_headlights): the street lamps flare at night (street_lamp.h) */
static int street_lamp_count;
static int street_lamp_on;
static int night_look;   /* the night's look beyond the sky: the dash darkens (the night, and Lost Vegas always) */
static const WindowBoard *window_boards;   /* port (psx_headlights): the night's lit windows (window_lights.h), one pool entry for all the boards */
static int window_board_count;
static const WindowDark *dark_boards;      /* the same, found by darkness (facades off the grid) */
static int dark_board_count;
enum { WINDOW_SEED = 0x71d0, WINDOW_ENTRIES = 1 };
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
/* the most a night's ambient may carry (the mean of its channels): CY1's and TR1's nights, the darkest of the CD's (.45, .42) */
static const float NIGHT_TINT_CEILING = 0.45f;

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
        if (night) psx_lgs_scale(&ambient, light.pool != NULL ? NIGHT_PALETTE : psx_night_light, 0);   /* the open road's: a tunnel's own lights (the contexts) stay lit, pools in the dark; with the pools the nodes carry the night, the ambient stays the dusk's */
        ambient_ok = 1, psx_lgs_tint(&ambient, tint);
        if (night) {   /* port: every night goes dark, the PS1's own brightest (TR2's orange dusk) too; the hue is kept */
            const float k = psx_night_ceiling_scale(tint, NIGHT_TINT_CEILING);
            if (k < 1.0f) {
                const float mul[3] = { k, k, k };
                psx_lgs_scale(&ambient, mul, 0);   /* a tunnel's own lights stay lit */
                psx_lgs_tint(&ambient, tint);
                port_log("psx_light: the night's tint brought under the ceiling (x%.2f)\n", k);
            }
        }
    }
    else port_log("psx_light: %s does not read, the ambient stays neutral\n", path);
    platform_free_file(file);
}

enum { NIGHT_STARS_SEED = 0x57a5 };

/* the night's darkness of the sky: psx_night_light, kept the same under the pools, whose palette is the dusk's (the sky is matched to it) */
static void sky_scale_night(void)
{
    float mul[3];
    for (int k = 0; k < 3; k++) mul[k] = headlights_on ? psx_night_light[k] * psx_night_light[k] / NIGHT_PALETTE[k] : psx_night_light[k];
    psx_sky_draw_scale(&sky_draw, mul);
    if (headlights_on) psx_sky_draw_stars(&sky_draw, NIGHT_STARS_SEED);   /* port (psx_headlights): the night's stars in the dark of the sky */
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
        if (night_sky) sky_scale_night();
        ported_view_set_psx_sky(&sky_draw);
        port_log("psx_sky: %dx%d panorama, gradient %d,%d,%d to %d,%d,%d\n", sky.width, sky.height, inf.top[0], inf.top[1], inf.top[2],
                 inf.bottom[0], inf.bottom[1], inf.bottom[2]);
    }
    psx_sky_free(&sky);
}

static int time_tinted;
static char light_letter = 'B';   /* the time's CD letter, for the rules file */
static int made_wanted;   /* port (own light): no CD light for this track, so one is made from its terrain (psx_light_make.h) in drive_light_load_synth */
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
    for (int k = 0; k < 3; k++) tint[k] = f[k] * (time != PSX_TIME_NIGHT ? 1.0f : headlights_on && (synth_wanted || made_wanted) ? NIGHT_PALETTE[k] : psx_night_light[k]);   /* a made light carries the night in its nodes with the pools */
    port_log("psx_light: no light of this track, the flat ambient of the time: tint %.2f %.2f %.2f\n", tint[0], tint[1], tint[2]);
}

int drive_light_time_tinted(void) { return time_tinted || split; }

void drive_light_load(int enabled, int time, const char *psx_dir, const char *track, uint32_t ring_nodes)
{
    time_tinted = enabled && time != PSX_TIME_DAY;
    night_sky = time == PSX_TIME_NIGHT;
    night_look = night_sky || (enabled && is_lost_vegas(track));   /* Lost Vegas is always night: its dash and effects are the night's, its sky already is */
    headlights_on = 0;
    street_lamp_on = 0;
    window_boards = NULL, window_board_count = 0, dark_boards = NULL, dark_board_count = 0;
    night_lamp_spec = NULL, night_lamp_count = 0, lamp_pool_count = 0;
    ported_view_set_police_lights(enabled);   /* the light effects: the police car's lights flash (police_lights.h) */
    alt_sky_wanted = 0;
    split = 0;
    ported_view_set_flares(port_psx_flares());
    ported_view_set_soft_shadow(port_psx_shadow());   /* its own flag: no light or PS1 files needed */
    ported_view_set_psx_lamps(NULL, 0);
    ported_view_set_street_lamps(NULL, 0, NULL);
    street_lamp_count = 0;
    ported_view_set_light(NULL, NULL);
    ported_view_set_psx_ambient(NULL, 0, NULL);
    ported_view_set_psx_synth(0);
    synth_wanted = 0;
    made_wanted = 0;
    ported_view_set_psx_sky(NULL);
    ported_view_set_psx_sky_info(NULL);
    ported_view_set_dark_fog(0);
    psx_sky_draw_free(&sky_draw);
    psx_light_free(&light);
    synth_on = 0;
    tint[0] = tint[1] = tint[2] = 1.0f;
    if (!enabled) return;
    const int have_dir = psx_dir != NULL && psx_dir[0] != '\0';   /* port: with no PS1 disc the light's own effects (pools, lamps, windows, the made light, fallback tints) still run */
    headlights_on = (time == PSX_TIME_NIGHT || is_lost_vegas(track)) && port_psx_headlights();
    street_lamp_count = headlights_on ? street_lamps_for(track, street_lamps, STREET_LAMPS_MAX) : 0;
    street_lamp_on = street_lamp_count > 0;
    window_board_count = headlights_on ? window_boards_for(track, &window_boards) : 0;
    dark_board_count = headlights_on ? window_darks_for(track, &dark_boards) : 0;
    synth_wanted = psx_light_synth_track(track);
    synth_time = time == PSX_TIME_DAWN ? PSX_SYNTH_DAWN : time == PSX_TIME_DUSK ? PSX_SYNTH_DUSK : time == PSX_TIME_NIGHT ? PSX_SYNTH_NIGHT : PSX_SYNTH_DAY;   /* the night: the dusk's doors, a tunnel not darker than the road */
    if (is_lost_vegas(track) || time == PSX_TIME_NIGHT) ported_view_set_dark_fog(1);   /* the night's fog is dark as Lost Vegas' */
    const char letter = LETTERS[time >= PSX_TIME_DAY && time <= PSX_TIME_NIGHT ? time : PSX_TIME_DAY];
    light_letter = time == PSX_TIME_NIGHT ? 'N' : letter;   /* the night has its own rules (the CD's C is the dusk) */
    if (time != PSX_TIME_DAY && have_dir) load_sky(psx_dir, track, letter);
    alt_sky_wanted = time != PSX_TIME_DAY && !sky_draw.ready;
    char path[LIGHT_PATH] = "";
    size_t size = 0;
    uint8_t *file = NULL;
    if (have_dir) {
        file_path(path, sizeof path, psx_dir, track, letter, "LGT");
        file = platform_read_file(path, &size);
    }
    if (file == NULL) {
        if (have_dir) port_log("psx_light: no %s\n", path);
        else port_log("psx_light: no PS1 directory, the port's own effects only\n");
        made_wanted = 1;
        if (time != PSX_TIME_DAY) use_fallback_tint(time);
        const PsxLamp *own_lamps = NULL;
        const int own_count = psx_lamps_for(track, &own_lamps);
        ported_view_set_psx_lamps(own_lamps, own_count);   /* the lamps' glow (psx_flares) needs them known even with no light */
        if (time == PSX_TIME_NIGHT || is_lost_vegas(track)) night_lamp_spec = own_lamps, night_lamp_count = own_count;   /* the lamps' own colours are kept lit in the night's palette */
        return;
    }
    PsxLight parsed = { 0 };
    const int ok = psx_light_parse(file, size, ring_nodes, &parsed);
    platform_free_file(file);
    if (!ok) {
        port_log("psx_light: %s does not fit the track (%u nodes)\n", path, ring_nodes);
        made_wanted = 1;
        if (time != PSX_TIME_DAY) use_fallback_tint(time);
        return;
    }
    light = parsed;
    light.pool = headlights_on ? &headlight_pool : NULL;
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
    if (time == PSX_TIME_NIGHT || is_lost_vegas(track)) night_lamp_spec = lamps, night_lamp_count = lamp_count;
    ported_view_set_light(&light, NULL);   /* the tint goes to the palette (drive_light_palette), not through the polygons' tables */
    port_log("psx_light: %s, %u nodes, tint %.2f %.2f %.2f\n", path, light.nodes, tint[0], tint[1], tint[2]);
}

void drive_light_set_reversed(int reversed)
{
    if (synth_on) return;   /* made from the track as it runs: already in its direction */
    psx_light_set_reversed(&light, reversed);
}

/* port (own light): the terrain's light of the time (psx_light_make.h), with a tunnel's rows laid over it where the road has one (the synth's rows are 255 in the open, so the lower of
 * the two is the right one everywhere) */
enum { MAX_LAMP_NODES = 1000, MAX_RULES = 512 };

/* the nodes of the track's street lamps, every board of street_lamps_for (CY1's posts, TR3's lanterns and bulbs, TR7's truss, TR4's barrels, ...), where the made night's pools go */
static size_t street_lamp_pool_nodes(const Tri *tri, uint32_t *out)
{
    if (!street_lamp_on || tri->descriptor_count == 0 || tri->descriptor_count > 64 || tri->scenery_count > MAX_LAMP_NODES) return 0;
    TriDescriptor descriptors[64];
    static TriScenery scenery[MAX_LAMP_NODES];
    for (uint32_t i = 0; i < tri->descriptor_count; i++)
        if (tri_descriptor(tri, i, &descriptors[i]) != TRI_OK) return 0;
    for (uint32_t i = 0; i < tri->scenery_count; i++)
        if (tri_scenery(tri, i, &scenery[i]) != TRI_OK) return 0;
    size_t n = 0;
    for (int i = 0; i < street_lamp_count && n < MAX_LAMP_NODES; i++)
        n += street_lamp_nodes(&street_lamps[i], descriptors, tri->descriptor_count, scenery, tri->scenery_count, out + n, MAX_LAMP_NODES - n);
    return n;
}

/* the track's own rules over the made light: the player's folder's psx_light_<TRACK><A|B|C|N>.txt (N the night), a rule a line (psx_light_make.h); no file, no rules */
static void apply_rules_file(const char *track, PsxLight *made)
{
    char name[64];
    if (track == NULL) return;
    snprintf(name, sizeof name, "psx_light_%s%c.txt", track, light_letter);
    if (!platform_user_file_exists(name)) return;
    size_t size = 0;
    uint8_t *text = platform_read_user_file(name, &size);
    if (text == NULL) return;
    static PsxLightRule rules[MAX_RULES];
    const int n = psx_light_rules_parse((const char *)text, size, rules, MAX_RULES);
    platform_free_file(text);
    psx_light_apply_rules(made->data, made->nodes, rules, n);
    port_log("psx_light: %d rules from %s\n", n, name);
}

static int make_light(const Tri *tri, const char *track, PsxLight *out)
{
    PsxLight made = { 0 };
    static uint32_t lamp_nodes[MAX_LAMP_NODES];
    const size_t lamps = synth_time == PSX_SYNTH_NIGHT ? street_lamp_pool_nodes(tri, lamp_nodes) : 0;
    if (!psx_light_make_lit(tri, synth_time, lamp_nodes, (int)lamps, &made)) return 0;
    if (lamps > 0) port_log("psx_light: %zu street lamp pools\n", lamps);
    PsxLight tunnels = { 0 };
    if (psx_light_synth(tri, synth_time, &tunnels) && tunnels.nodes == made.nodes) {
        for (size_t i = 0; i < (size_t)made.nodes * PSX_LIGHT_POINTS; i++)
            if (tunnels.data[i] < made.data[i]) made.data[i] = tunnels.data[i];
    }
    psx_light_free(&tunnels);
    apply_rules_file(track, &made);
    *out = made;
    return 1;
}

void drive_light_load_synth(const Tri *tri, const char *track)
{
    const int tunnel_track = synth_wanted && psx_light_synth_track(track);
    if ((!tunnel_track && !made_wanted) || light.data != NULL || tri == NULL) return;
    PsxLight made = { 0 };
    if (!make_light(tri, track, &made)) return;
    light = made;
    light.pool = headlights_on ? &headlight_pool : NULL;   /* port (psx_headlights): the made light's open nodes go to the night's, a tunnel keeps the dusk's, the pools lift */
    synth_on = 1;
    ported_view_set_light(&light, NULL);
    ported_view_set_psx_synth(tunnel_track);   /* the tunnel tracks' car and dash light follows their tunnels */
    port_log("psx_light: %s has no light on the CD, made from its terrain%s (%u nodes)\n", track, tunnel_track ? " and the road's tunnels" : "", light.nodes);
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
        if (night_sky) sky_scale_night();
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

/* the pool's last entries are the extras': the street lamp's three, then the windows' one */
static int street_entries(void) { return street_lamp_on ? STREET_LAMP_ENTRIES : 0; }
static int window_entries(void) { return window_board_count > 0 || dark_board_count > 0 ? WINDOW_ENTRIES : 0; }

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
        const StreetLampPaint p = street_lamp_paint(l, (uint8_t *)(uintptr_t)t->bitmap.payload, t->bitmap.width, t->bitmap.height, entry);
        port_log("street_lamps: board %d: %zu lens texels and %zu halo texels in entries %d %d %d\n", l->scenery, p.lens, p.halo, entry[0], entry[1], entry[2]);
    }
}

/* the lit windows' colour (a warm yellow, dimmer than the lamps' lens), the pool's very last entry, painted into each window board */
static void windows_night(TrackArt *art)
{
    static const uint8_t RGB[3] = { 236, 190, 96 };
    if ((window_board_count <= 0 && dark_board_count <= 0) || lamp_pool_count < window_entries() || art == NULL) return;
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

int drive_light_night(void) { return night_look; }

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

/* the dash's art: half the world's tint (the whole at night), as the dawn / dusk base, times the car's light ratio */
void drive_light_palette_half(const uint8_t palette[768], uint8_t out[768])
{
    psx_dash_palette(palette, tint, 1, night_look ? 1 : 2, out);   /* port: the dash is dark with the night; the dials (the HUD's class) are not touched */
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
