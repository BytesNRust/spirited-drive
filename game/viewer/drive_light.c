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
#include "game/race/finish_gate_light.h"
#include "game/race/window_lights.h"
#include "game/race/psx_light_make.h"
#include "game/race/psx_light_synth.h"
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
/* port (night_sky_hue): the hue of the made night's sky, blue-black: the sky's colours keep their luminance and take these channels' proportions */
static const float NIGHT_SKY_RAMP[3] = { 0.62f, 0.80f, 1.0f };
static StreetLamp street_lamps[STREET_LAMPS_MAX];   /* port (psx_headlights): the street lamps flare at night (street_lamp.h) */
static int street_lamp_count;
static int street_lamp_on;
static int street_lamp_by_day;   /* the lamps are lit in the daylight (the day's paint: no outer rim) */
static int dusk_look;   /* port (gauge_exterior): the dusk: the exterior lights are on */
static int night_look;   /* the night's look beyond the sky: the dash darkens (the night, and Lost Vegas always) */
static const WindowBoard *window_boards;   /* port (psx_headlights): the night's lit windows (window_lights.h), one pool entry for all the boards */
static int window_board_count;
static const WindowDark *dark_boards;      /* the same, found by darkness (facades off the grid) */
static int dark_board_count;
static const WindowSign *sign_boards;     /* port (sign_glow): the City's lit matrix sign (window_lights.h), painted in the windows' entry */
static int sign_board_count;
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
/* the most a night's ambient may carry (the mean of its channels): CY1's and TR1's nights, the darkest of the CD's (.45, .42) lowered to .35: TR2's valley still read as day at .45 */
static const float NIGHT_TINT_CEILING = 0.35f;

static void load_tint(const char *psx_dir, const char *track, char letter, int night, float ceiling)
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
            const float k = psx_night_ceiling_scale(tint, ceiling);
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
    if (port_night_sky_hue()) psx_sky_draw_rehue(&sky_draw, NIGHT_SKY_RAMP);   /* port (night_sky_hue): not the dusk art's orange and olive */
    if (headlights_on) {   /* port (psx_headlights): the night's stars in the dark of the sky */
        const int stars = psx_sky_draw_stars(&sky_draw, NIGHT_STARS_SEED);
        port_log("psx_sky: stars %d, band rows 0..%d of horizon row %d (bitmap %dx%d), %d columns of land reach the band\n", stars, sky_draw.star_rows,
                 sky_draw.bitmap.horizon_row, sky_draw.bitmap.width, sky_draw.bitmap.height, sky_draw.star_land_columns);
    }
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
static const int VEGAS_OBJECT_LIGHT = 245;   /* port (vegas_objects): the light of 255 its buildings and objects keep, so the windows and the neon stand out */

/* port (city_lift, vegas_lift): level 0, 1, 2. The ceiling the City and Lost Vegas nights' tint is brought under (NIGHT_TINT_CEILING at 0) and how far the open night's dark floor is lifted (of 256) */
static const float CITY_CEILING[4] = { 0.35f, 0.50f, 0.70f, 0.42f };   /* level 3: darker than 1, with the beams, the tunnels and the lamps standing out of it */
static const float VEGAS_CEILING[3] = { 0.35f, 0.55f, 0.80f };
static const int CITY_FLOOR_LIFT[4] = { 0, 120, 190, 30 }, CITY_TUNNEL_FLOOR = 215, CITY_BEAM_OPEN[3] = { 190, 230, 230 }, VEGAS_FLOOR_LIFT[3] = { 0, 90, 170 };   /* the open night's dark floor lifted (of 256) */

static int city_night3;   /* port (city_lift 3): this track is a City night with level 3 */
static int city_level;    /* port (city_lift): this track is a City night with the headlights on: its level (0 when it is not) */

static int is_city(const char *track) { return track != NULL && strncasecmp(track, "CY", 2) == 0; }
/* port (city_lift 3): the beam's open-ground light for CY1..CY3 (the narrow roads and walls of CY2 and CY3 leave the beam fainter than CY1's); TNFS_CITY_BEAM_OPEN=n (1..255) takes it for the captures */
static int city_beam_open(const char *track)
{
    const char *env = getenv("TNFS_CITY_BEAM_OPEN");
    if (env != NULL && atoi(env) > 0) return atoi(env) > 255 ? 255 : atoi(env);
    const int n = track != NULL && track[2] >= '1' && track[2] <= '3' ? track[2] - '1' : 0;
    return CITY_BEAM_OPEN[n];
}
static int is_lost_vegas(const char *track);

/* port (city_lift 3): the open ground's light of the City's made night, a share (of 256) of the terrain's, a track a share: the CD's own rows of CY2 and CY3 (narrow roads between walls) are darker than
 * CY1's, the made rows (the terrain's sky share) are not as dark; fitted to the CD's frames (mean road luminance). TNFS_CITY_MADE_SCALE=n (1..256) takes it for the fit */
static const int CITY_MADE_SCALE[3] = { 196, 96, 86 };
static int city_made_scale(const char *track)
{
    const char *env = getenv("TNFS_CITY_MADE_SCALE");
    if (env != NULL && atoi(env) > 0) return atoi(env) > 256 ? 256 : atoi(env);
    return track != NULL && track[2] >= '1' && track[2] <= '3' ? CITY_MADE_SCALE[track[2] - '1'] : 256;
}

/* port (city_lift): the light's City fields, the same for the CD's light and for the one made from the terrain (a player with no PS1 disc) */
static void city_fields(PsxLight *l, const char *track)
{
    l->shoulder_road = headlights_on ? port_shoulder_light() : 0;   /* shoulder_light */
    l->lamp_bright = headlights_on ? (port_lamp_pool_bright() || city_night3) : 0;   /* lamp_pool_bright; city_lift 3 has it in the City: the lamps' pools stand out of the darker base */
    l->beam_on_top = city_night3;   /* the beam adds over the raised floor */
    l->lamp_local = city_night3;   /* local lamp pools */
    l->beam_open = city_night3 ? city_beam_open(track) : 0;   /* a visible patch of the beams on the dark City road */
    l->tunnel_floor = city_night3 ? CITY_TUNNEL_FLOOR : 0;   /* brighter tunnels */
    l->night_lift = !headlights_on ? 0 : is_city(track) ? CITY_FLOOR_LIFT[port_city_lift()] : is_lost_vegas(track) ? VEGAS_FLOOR_LIFT[port_vegas_lift()] : 0;   /* city_lift, vegas_lift */
}
static int is_lost_vegas(const char *track) { return track != NULL && strcasecmp(track, "TR4") == 0; }

/* A track the PS1 CD holds no light for (Burnt Sienna is tr6 and its ZTR6 has other nodes, Transtropolis is tr7 and has no Z file): a flat ambient
 * for the time, no light at the nodes. The average of the three TR tracks whose scripts give a time of day (ZTR1, ZTR2, ZTR3's LGS start colours, as
 * a part of 128), dawn then dusk; the one place to change for a pick per track. */
static const float FALLBACK_TINT[2][3] = { { 0.96f, 0.73f, 0.33f }, { 0.88f, 0.55f, 0.46f } };


/* port (night tone, dev env only: TNFS_NIGHT_TONE): the night's flat tint with no PS1 light. 0 the fallback above; 1 a tone a track: the CD's night tint of each track before its
 * ceiling (the disc's logged tint over the ceiling's scale, so city_lift and vegas_lift bring it under their ceilings as on the CD path); 2 one neutral cool blue-grey for all (final, no
 * ceiling). `gain` (of the tint under the ceiling) is the track's brightness match to the CD's frame (mean road luminance of the road area of the frame). */
static const struct { const char *track; float tint[3], gain; } NIGHT_TONE[] = {
    { "CY1", { 0.59f, 0.34f, 0.43f }, 1.1f }, { "CY2", { 0.44f, 0.63f, 0.70f }, 1.0f }, { "CY3", { 0.40f, 0.42f, 0.68f }, 1.18f },
    { "CL1", { 0.80f, 0.70f, 0.60f }, 0.92f }, { "CL2", { 0.71f, 0.41f, 0.43f }, 1.0f }, { "CL3", { 0.55f, 0.37f, 0.40f }, 0.98f },
    { "AL1", { 0.81f, 0.61f, 0.54f }, 0.89f }, { "AL2", { 0.75f, 0.42f, 0.36f }, 0.91f }, { "AL3", { 0.66f, 0.37f, 0.29f }, 0.85f },
    { "TR1", { 0.55f, 0.43f, 0.30f }, 0.86f }, { "TR2", { 0.82f, 0.63f, 0.40f }, 0.95f }, { "TR3", { 0.74f, 0.57f, 0.68f }, 1.21f },
    { "TR4", { 0.80f, 0.96f, 1.00f }, 1.15f },
};
static const float NIGHT_TONE_COOL[3] = { 0.40f, 0.44f, 0.52f };

static int night_tone_mode(void) { return port_night_tone(); }

/* port (night tone): the tint the mode gives this track's night, under the ceiling the CD path would bring it to (the City's, Lost Vegas' with its darkening, or the night's);
 * 0 when the mode has none (mode 0, or a track the table lacks) */
static int night_tone_tint(const char *track, float out[3])
{
    const int mode = night_tone_mode();
    if (mode == 2) {   /* the cool tone as the owner saw it (city_lift 3, vegas_lift 2), the City's and Lost Vegas' following their level's ceiling from there; the other tracks as seen */
        const float scale = is_city(track) ? CITY_CEILING[port_city_lift()] / CITY_CEILING[3] : is_lost_vegas(track) ? VEGAS_CEILING[port_vegas_lift()] / VEGAS_CEILING[2] : 1.0f;
        for (int k = 0; k < 3; k++) out[k] = NIGHT_TONE_COOL[k] * scale;
        return 1;
    }
    if (mode != 1 || track == NULL) return 0;
    for (size_t i = 0; i < sizeof NIGHT_TONE / sizeof NIGHT_TONE[0]; i++) {
        if (strcasecmp(track, NIGHT_TONE[i].track) != 0) continue;
        for (int k = 0; k < 3; k++) out[k] = NIGHT_TONE[i].tint[k];
        const float ceiling = is_city(track) ? CITY_CEILING[port_city_lift()] : is_lost_vegas(track) ? VEGAS_CEILING[port_vegas_lift()] : NIGHT_TINT_CEILING;
        const float scale = psx_night_ceiling_scale(out, ceiling);
        for (int k = 0; k < 3; k++) out[k] *= scale * NIGHT_TONE[i].gain * (is_lost_vegas(track) ? LOST_VEGAS_DARKEN : 1.0f);   /* the gain over the ceiling: under it the ceiling would take it back */
        return 1;
    }
    return 0;
}

static void use_fallback_tint(int time, const char *track)
{
    const float *f = FALLBACK_TINT[time == PSX_TIME_DUSK || time == PSX_TIME_NIGHT];
    for (int k = 0; k < 3; k++) tint[k] = f[k] * (time != PSX_TIME_NIGHT ? 1.0f : headlights_on && (synth_wanted || made_wanted) ? NIGHT_PALETTE[k] : psx_night_light[k]);   /* a made light carries the night in its nodes with the pools */
    float tone[3];
    if (time == PSX_TIME_NIGHT && night_tone_tint(track, tone)) {
        for (int k = 0; k < 3; k++) tint[k] = tone[k];
        port_log("psx_light: night tone %d: tint %.2f %.2f %.2f\n", night_tone_mode(), tint[0], tint[1], tint[2]);
        return;   /* final: under its ceiling already */
    }
    if (time == PSX_TIME_NIGHT && city_level > 0) {   /* port (city_lift): the City's ceiling over the made night's tint too, as over the CD's; level 0 stays as it was */
        const float k = psx_night_ceiling_scale(tint, CITY_CEILING[city_level]);
        for (int c = 0; c < 3; c++) tint[c] *= k;
    }
    port_log("psx_light: no light of this track, the flat ambient of the time: tint %.2f %.2f %.2f\n", tint[0], tint[1], tint[2]);
}

int drive_light_time_tinted(void) { return time_tinted || split; }

/* port: every state drive_light_load sets, back to nothing for a race with no light (or before the light's own) */
static void reset_for_load(int enabled, int time, const char *track)
{
    time_tinted = enabled && time != PSX_TIME_DAY;
    night_sky = time == PSX_TIME_NIGHT;
    dusk_look = enabled && time == PSX_TIME_DUSK;
    night_look = night_sky || (enabled && is_lost_vegas(track));   /* Lost Vegas is always night: its dash and effects are the night's, its sky already is */
    headlights_on = 0;
    city_night3 = 0, city_level = 0;
    street_lamp_on = 0;
    window_boards = NULL, window_board_count = 0, dark_boards = NULL, dark_board_count = 0, sign_boards = NULL, sign_board_count = 0;
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
}

/* port: the race has no CD light file (or no PS1 folder): the light is made from the terrain, with the fallback tint of the time */
static void load_without_cd_light(int time, const char *track, int have_dir, const char *path)
{
    if (have_dir) port_log("psx_light: no %s\n", path);
    else port_log("psx_light: no PS1 directory, the port's own effects only\n");
    made_wanted = 1;   /* port (own light): no CD light, the light is made from the terrain */
    if (time != PSX_TIME_DAY) use_fallback_tint(time, track);
    if (time == PSX_TIME_NIGHT && is_lost_vegas(track) && port_vegas_car_night() && night_tone_mode() == 0) {   /* port (vegas_car_night): the car was lit as by day against Lost Vegas' dark; the tint of the CD's own night (ZTR4C's, under the ceiling) */
        static const float VEGAS_NIGHT[3] = { 0.27f, 0.33f, 0.34f };
        const float lift_k = VEGAS_CEILING[port_vegas_lift()] / VEGAS_CEILING[0];   /* port (vegas_lift): the CD's tint rides its ceiling (the CD's own is above every level's); level 0 is as it was */
        for (int c = 0; c < 3; c++) tint[c] = VEGAS_NIGHT[c] * lift_k;
        port_log("psx_light: Lost Vegas' night tint %.2f %.2f %.2f\n", tint[0], tint[1], tint[2]);
    }
    const PsxLamp *own_lamps = NULL;
    const int own_count = psx_lamps_for(track, &own_lamps);
    ported_view_set_psx_lamps(own_lamps, own_count);   /* the lamps' glow (psx_flares) needs them known even with no light */
    if (time == PSX_TIME_NIGHT || is_lost_vegas(track)) night_lamp_spec = own_lamps, night_lamp_count = own_count;   /* the lamps' own colours are kept lit in the night's palette */
}

/* The light effects of the track at this time of day: the headlights, the city's lift, the street lamps, the boards, the synth's time and the dark fog. */
static void light_effects_set(int time, const char *track)
{
    headlights_on = (time == PSX_TIME_NIGHT || is_lost_vegas(track)) && port_psx_headlights();
    city_night3 = headlights_on && is_city(track) && port_city_lift() >= 3;
    city_level = headlights_on && time == PSX_TIME_NIGHT && is_city(track) ? port_city_lift() : 0;
    street_lamp_count = headlights_on ? street_lamps_for(track, street_lamps, STREET_LAMPS_MAX)
                                      : port_psx_headlights() ? street_lamps_for_day(track, street_lamps, STREET_LAMPS_MAX) : 0;   /* port: the Light Effects' lamps lit by day too (street_lamp.h) */
    street_lamp_by_day = !headlights_on;
    street_lamp_on = street_lamp_count > 0;
    window_board_count = headlights_on ? window_boards_for(track, &window_boards) : 0;
    dark_board_count = headlights_on ? window_darks_for(track, &dark_boards) : 0;
    sign_board_count = headlights_on && port_sign_glow() ? window_signs_for(track, &sign_boards) : 0;   /* port (sign_glow) */
    synth_wanted = psx_light_synth_track(track);
    synth_time = time == PSX_TIME_DAWN ? PSX_SYNTH_DAWN : time == PSX_TIME_DUSK ? PSX_SYNTH_DUSK : time == PSX_TIME_NIGHT ? PSX_SYNTH_NIGHT : PSX_SYNTH_DAY;   /* the night: the dusk's doors, a tunnel not darker than the road */
    if (is_lost_vegas(track) || time == PSX_TIME_NIGHT) ported_view_set_dark_fog(1);   /* the night's fog is dark as Lost Vegas' */
}

/* The parsed CD light taken as the race's: the port's flags on it, its tint, and the ambient script. */
static void light_adopt(const PsxLight *parsed, const char *track, const char *psx_dir, char letter, int time, uint32_t ring_nodes)
{
    light = *parsed;
    light.gate_board = headlights_on && port_finish_gate_light() ? finish_gate_board(track) + 1 : 0;   /* port (finish_gate_light) */
    light.forest_road = port_forest_neutral() && psx_light_forest_road_range(track, &light.forest_from, &light.forest_to);   /* port (forest_neutral): the forest's shade is not drawn as a tunnel */
    light.beam_tunnel = headlights_on ? port_tunnel_headlight() : 0;
    light.object_floor = is_lost_vegas(track) && port_vegas_objects() ? VEGAS_OBJECT_LIGHT : 0;   /* port (vegas_objects) */
    light.pool = headlights_on ? &headlight_pool : NULL;
    city_fields(&light, track);   /* port (city_lift) */
    load_tint(psx_dir, track, letter, time == PSX_TIME_NIGHT, is_city(track) ? CITY_CEILING[port_city_lift()] : is_lost_vegas(track) ? VEGAS_CEILING[port_vegas_lift()] : NIGHT_TINT_CEILING);   /* port (city_lift, vegas_lift) */
    if (is_lost_vegas(track))
        for (int k = 0; k < 3; k++) tint[k] *= LOST_VEGAS_DARKEN;
    ambient_nodes = ring_nodes;
    if (ambient_ok) {
        ported_view_set_psx_ambient(&ambient, ring_nodes, tint);
        psx_light_set_script(&light, &ambient);   /* the exe's light of the track's points */
        split = 1;
    }
}

void drive_light_load(int enabled, int time, const char *psx_dir, const char *track, uint32_t ring_nodes)
{
    reset_for_load(enabled, time, track);
    if (!enabled) return;
    const int have_dir = psx_dir != NULL && psx_dir[0] != '\0';   /* port: with no PS1 disc the light's own effects (pools, lamps, windows, the made light, fallback tints) still run */
    light_effects_set(time, track);
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
        load_without_cd_light(time, track, have_dir, path);
        return;
    }
    PsxLight parsed = { 0 };
    const int ok = psx_light_parse(file, size, ring_nodes, &parsed);
    platform_free_file(file);
    if (!ok) {
        port_log("psx_light: %s does not fit the track (%u nodes)\n", path, ring_nodes);
        made_wanted = 1;
        if (time != PSX_TIME_DAY) use_fallback_tint(time, track);
        return;
    }
    if (port_forest_dark()) psx_light_forest_apply(&parsed, track);   /* port (forest_dark): the CD's light too, in the file's node order */
    light_adopt(&parsed, track, psx_dir, letter, time, ring_nodes);
    const PsxLamp *lamps = NULL;
    const int lamp_count = psx_lamps_for(track, &lamps);   /* the tunnel's lamps stay bright under the light */
    ported_view_set_psx_lamps(lamps, lamp_count);
    if (time == PSX_TIME_NIGHT || is_lost_vegas(track)) night_lamp_spec = lamps, night_lamp_count = lamp_count;
    ported_view_set_light(&light, NULL);   /* the tint goes to the palette (drive_light_palette), not through the polygons' tables */
    port_log("psx_light: %s, %u nodes, tint %.2f %.2f %.2f\n", path, light.nodes, tint[0], tint[1], tint[2]);
}

static void lay_cd_pools(void);

void drive_light_set_reversed(int reversed)
{
    if (synth_on) return;   /* made from the track as it runs: already in its direction */
    psx_light_set_reversed(&light, reversed);
    lay_cd_pools();
}

/* port (own light): the terrain's light of the time (psx_light_make.h), with a tunnel's rows laid over it where the road has one (the synth's rows are 255 in the open, so the lower of
 * the two is the right one everywhere) */
enum { MAX_LAMP_NODES = 1000, MAX_RULES = 512 };

/* the nodes of the track's street lamps, every board of street_lamps_for (CY1's posts, TR3's lanterns and bulbs, TR7's truss, TR4's barrels, ...), where the made night's pools go */
static size_t street_lamp_pool_nodes(const Tri *tri, uint32_t *out)
{
    if (!street_lamp_on || tri->descriptor_count == 0 || tri->descriptor_count > 64 || tri->scenery_count == 0) return 0;
    TriDescriptor descriptors[64];
    TriScenery *const scenery = malloc((size_t)tri->scenery_count * sizeof *scenery);   /* sized to the track: MAX_LAMP_NODES is the room for the nodes alone */
    if (scenery == NULL) return 0;
    size_t n = 0;
    int ok = 1;
    for (uint32_t i = 0; ok && i < tri->descriptor_count; i++) ok = tri_descriptor(tri, i, &descriptors[i]) == TRI_OK;
    for (uint32_t i = 0; ok && i < tri->scenery_count; i++) ok = tri_scenery(tri, i, &scenery[i]) == TRI_OK;
    if (ok) n = street_lamps_nodes(street_lamps, street_lamp_count, descriptors, tri->descriptor_count, scenery, tri->scenery_count, out, MAX_LAMP_NODES);
    free(scenery);
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

/* port (lamp_pool_tint, lamp_car_shade, lamp_pool_bright): which nodes a street lamp's pool lies on, for the road's warm tint and for the car's light */
static void lamp_pool_classes(const Tri *tri, PsxLight *made, const uint32_t *lamp_nodes, size_t lamps)
{
    if (lamps > 0 && port_lamp_pool_tint()) {   /* port (lamp_pool_tint): which nodes a pool lies on, for the warm tint of the road there */
        made->pool_class = malloc(made->nodes);
        if (made->pool_class != NULL) psx_light_lamp_classes(made->pool_class, made->nodes, tri_is_circuit(tri), lamp_nodes, (int)lamps);
    }
    if (lamps > 0 && (port_lamp_car_shade() || port_lamp_pool_bright() || city_night3)) {   /* port (lamp_car_shade, lamp_pool_bright): and the same classes for the car's light, and for the pool's own light */
        made->car_pool = malloc(made->nodes);
        if (made->car_pool != NULL) psx_light_lamp_classes(made->car_pool, made->nodes, tri_is_circuit(tri), lamp_nodes, (int)lamps);
    }
    made->lamp_node = lamps > 0 ? malloc(lamps * sizeof *made->lamp_node) : NULL;   /* port (track light file): the lamps' nodes, which the export writes as `lamp` lines in place of the pools' runs */
    if (made->lamp_node != NULL) memcpy(made->lamp_node, lamp_nodes, lamps * sizeof *made->lamp_node), made->lamp_node_count = (int)lamps, made->lamp_ring = tri_is_circuit(tri);
}

/* port (trans_shadows): the parking's deck roofs nodes 465..470 too: the interior shade of its marked run, the hall's points beyond the foot kept as the hall keeps them */
static void trans_closed_run(PsxLight *made, PsxLight *tunnels, const char *track, int trans)
{
    uint32_t closed_from = 1, closed_to = 0, closed_row = 0;
    if (trans && psx_light_synth_trans_closed(track, &closed_from, &closed_to, &closed_row) && tunnels->data != NULL && tunnels->nodes == made->nodes && closed_to < made->nodes) {
        uint32_t hall_from = 1, hall_to = 0;   /* port (trans_shadows): the parking's deck roofs nodes 465..470 too: the interior shade of its marked run, the hall's points beyond the foot kept as the hall keeps them */
        const int hall = port_hall_light() && psx_light_synth_hall(track, &hall_from, &hall_to);
        for (uint32_t n = closed_from; n <= closed_to; n++)
            for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
                const int beyond = p == 3 || p == 4 || p == 5 || p >= 8;
                const uint8_t row = tunnels->data[(size_t)closed_row * PSX_LIGHT_POINTS + (size_t)p];
                uint8_t *at = &made->data[(size_t)n * PSX_LIGHT_POINTS + (size_t)p];
                if (!(hall && beyond) && row < *at) *at = row;
            }
        if (tunnels->tunnel != NULL)
            for (uint32_t n = closed_from; n <= closed_to; n++) tunnels->tunnel[n] = PSX_SYNTH_FLAG_TUNNEL;
    }
}

/* port (parking_dark, cavern_dark): the car keeps one dim tone through the parking's decks and TR6's cavern */
static void car_dim_runs(PsxLight *made, const char *track)
{
    uint32_t park_from = 1, park_to = 0;
    if (port_parking_dark() && psx_light_synth_parking(track, &park_from, &park_to) && park_to < made->nodes) {   /* port (parking_dark): the car keeps the hall's dark tone from the deck's first node to the exit tunnel */
        made->car_dim = calloc(made->nodes, 1);
        if (made->car_dim != NULL) memset(made->car_dim + park_from, 1, park_to - park_from + 1);
    }
    uint32_t cave_from = 1, cave_to = 0;
    if (port_cavern_dark() && psx_light_synth_cavern(track, &cave_from, &cave_to) && cave_to < made->nodes) {   /* port (cavern_dark): the car keeps one dim tone through TR6's cavern */
        if (made->car_dim == NULL) made->car_dim = calloc(made->nodes, 1);
        if (made->car_dim != NULL) memset(made->car_dim + cave_from, 1, cave_to - cave_from + 1);
    }
}

/* port (trans_shadows): the airport's approach is open road: the scenery beside it keeps the open light, not the walls' shade */
static void open_approach(PsxLight *made, int trans, uint32_t open_from, uint32_t open_to)
{
    if (trans && open_to < made->nodes) {   /* port (trans_shadows): the airport's approach is open road: the scenery beside it keeps the open light, not the walls' shade */
        uint8_t open_level = 0;
        for (size_t i = 0; i < (size_t)made->nodes * PSX_LIGHT_POINTS; i++) open_level = made->data[i] > open_level ? made->data[i] : open_level;
        for (uint32_t n = open_from; n <= open_to; n++)
            for (int p = 0; p < PSX_LIGHT_POINTS; p++)
                if (p != 0 && p != 1 && p != 6) made->data[(size_t)n * PSX_LIGHT_POINTS + (size_t)p] = open_level;
    }
}

/* port: the hall nodes' flag and the canyon's nodes of the made light, from the synth's tunnel rows. */
static void made_hall_canyon(PsxLight *made, PsxLight *tunnels, const char *track)
{
    if (tunnels->data != NULL && tunnels->nodes == made->nodes) {   /* port: the hall nodes' flag (the mark in the synth's row is lost to the terrain's lower light above) */
        made->hall = calloc(made->nodes, 1);
        if (made->hall != NULL)
            for (uint32_t n = 0; n < made->nodes; n++) made->hall[n] = tunnels->data[(size_t)n * PSX_LIGHT_POINTS + PSX_SYNTH_HALL_POINT] == PSX_SYNTH_HALL_MARK;
    }
    if (port_car_tunnel_light() && !psx_light_synth_hall(track, NULL, NULL)) {   /* port (car_tunnel_light): only TR7 has a hall; TR6's property-16 stretch after the mine is open road in the sun, not an orange hall */
        if (port_canyon_shade() && tunnels->tunnel != NULL && tunnels->nodes == made->nodes) {   /* port (canyon_shade): remember the canyon's nodes before the flag is cleared */
            made->canyon = calloc(made->nodes, 1);
            if (made->canyon != NULL)
                for (uint32_t n = 0; n < made->nodes; n++) made->canyon[n] = tunnels->tunnel[n] == PSX_SYNTH_FLAG_HALL;
        }
        if (made->hall != NULL) memset(made->hall, 0, made->nodes);
        if (tunnels->tunnel != NULL && tunnels->nodes == made->nodes)
            for (uint32_t n = 0; n < made->nodes; n++)
                if (tunnels->tunnel[n] == PSX_SYNTH_FLAG_HALL) tunnels->tunnel[n] = 0;
    }
}

static int make_light_code(const Tri *tri, const char *track, PsxLight *out)
{
    PsxLight made = { 0 };
    static uint32_t lamp_nodes[MAX_LAMP_NODES];
    const size_t lamps = synth_time == PSX_SYNTH_NIGHT ? street_lamp_pool_nodes(tri, lamp_nodes) : 0;
    if (!psx_light_make_lit(tri, synth_time, lamp_nodes, (int)lamps, &made)) return 0;
    if (lamps > 0) port_log("psx_light: %zu street lamp pools\n", lamps);
    lamp_pool_classes(tri, &made, lamp_nodes, lamps);
    PsxLight tunnels = { 0 };
    uint32_t open_from = 1, open_to = 0;
    const int trans = port_trans_shadows() && psx_light_synth_trans(track, &open_from, &open_to);
    const int gallery = port_made_tunnels() && port_car_tunnel_light() && psx_light_synth_gallery_track(track);   /* port (made_tunnels, car_tunnel_light): CL2's galleries (property 7) are tunnels of its light, as the CD's is */
    if (psx_light_synth_ex(tri, synth_time, gallery, &tunnels) && tunnels.nodes == made.nodes) {
        uint32_t hall_from = 1, hall_to = 0;
        const int hall = port_hall_light() && psx_light_synth_hall(track, &hall_from, &hall_to);
        for (size_t i = 0; i < (size_t)made.nodes * PSX_LIGHT_POINTS; i++) {
            const uint32_t node = (uint32_t)(i / PSX_LIGHT_POINTS), point = (uint32_t)(i % PSX_LIGHT_POINTS);
            const int beyond = point == 3 || point == 4 || point == 5 || point >= 8;   /* the points beyond the foot: the scenery's side */
            if (hall && node >= hall_from && node <= hall_to && beyond) continue;   /* port (hall_light): the hall keeps the terrain's light there */
            if (tunnels.data[i] < made.data[i]) made.data[i] = tunnels.data[i];
        }
    }
    trans_closed_run(&made, &tunnels, track, trans);
    car_dim_runs(&made, track);
    open_approach(&made, trans, open_from, open_to);
    made_hall_canyon(&made, &tunnels, track);
    if (tunnels.tunnel != NULL && tunnels.nodes == made.nodes && port_made_tunnels()) {   /* port (made_tunnels): the road's own tunnel nodes, as the PS1 files' rows mark theirs */
        uint32_t hall_from = 1, hall_to = 0;
        const int hall_range = port_hall_light() && psx_light_synth_hall(track, &hall_from, &hall_to);
        psx_light_synth_made_tunnels(&made, &tunnels, hall_range ? hall_from : 1, hall_range ? hall_to : 0);
        made.neutral_cast = psx_light_synth_mine(track) || gallery;   /* i074: the mine keeps its blue, the warm cast stays on TR7's tunnels */
        made.city_tunnel = psx_light_synth_city_track(track);   /* 1 CY1, 2 CY2/CY3, 0 else */
    }
    psx_light_free(&tunnels);
    *out = made;
    return 1;
}

/* the port's flags that make_light_code reads (and the direction the track runs), as the bits a track light file is made for: a file is read only where they are the same */
static uint32_t track_light_flags(void)
{
    const int bits[] = { port_trans_shadows(), port_made_tunnels(), port_car_tunnel_light(), port_hall_light(), port_parking_dark(), port_cavern_dark(), port_canyon_shade(),
                         port_lamp_pool_tint(), port_lamp_car_shade(), port_lamp_pool_bright(), city_night3, street_lamp_on, reverse_track_wanted() };
    uint32_t flags = 0;
    for (size_t i = 0; i < sizeof bits / sizeof *bits; i++) flags |= (bits[i] != 0 ? 1u : 0u) << i;
    return flags;
}

static char track_light_note[160] = "";   /* a small line for the editor when the file was made with other settings than now (a record: the file is used all the same) */
static char track_light_places[200] = "";   /* and the track's places file, when there is one */
static char track_light_status[200] = "";   /* what the track light file did for this race, for the light editor's screen (drive_light_edit_lines) */

/* port (made_tunnels): a made light's warm tunnels take the look of their kind (psx_light_synth_warm_look), which a track light file's retouches then change */
static void warm_looks(PsxLight *made)
{
    for (int dark = 0; dark < 2 && made->city_tunnel != 0; dark++)
        (void)psx_light_synth_warm_look(made->city_tunnel, dark, made->warm_cast[dark], &made->warm_percent[dark]);
}

static void bake_lamps(const Tri *tri, PsxLight *made);

/* port (`lamp`): where each of the file's lamps stands in the world, in metres. A lamp line that matches a street lamp post (a scenery of the lamp boards at its node) is where the post is; else it is at
 * the node's centre. The reach, which the file gives in nodes, becomes metres by the road's own spacing there. */
static void place_lamps(const Tri *tri, PsxLight *made)
{
    if (made->lamps == NULL || made->lamp_n <= 0 || made->lamp_placed || tri == NULL || tri->node_count == 0) return;
    uint8_t *post = calloc((size_t)made->lamp_n, 1);
    if (post == NULL) return;
    for (int i = 0; i < made->lamp_n; i++) {
        PsxLightLamp *const l = &made->lamps[i];
        TriNode here, next;
        if (tri_node(tri, l->node % tri->node_count, &here) != TRI_OK || tri_node(tri, (l->node + 1) % tri->node_count, &next) != TRI_OK) continue;
        const double dx = (next.position.x - here.position.x) / 65536.0, dz = (next.position.z - here.position.z) / 65536.0;
        l->x = (float)(here.position.x / 65536.0), l->z = (float)(here.position.z / 65536.0);
        l->reach *= (float)sqrt(dx * dx + dz * dz);
    }
    for (uint32_t s = 0; street_lamp_on && s < tri->scenery_count; s++) {
        TrackObject o;
        if (track_object(tri, s, &o) != TRI_OK || (o.descriptor.type != TRACK_OBJECT_BILLBOARD && o.descriptor.type != TRACK_OBJECT_TWO_SIDED)) continue;
        int board = 0;
        for (int j = 0; j < street_lamp_count && !board; j++) board = o.descriptor.resource_id / 4 == street_lamps[j].scenery;
        for (int i = 0; board && i < made->lamp_n; i++)
            if (!post[i] && made->lamps[i].node == o.node) post[i] = 1, made->lamps[i].x = (float)(o.origin.x / 65536.0), made->lamps[i].z = (float)(o.origin.z / 65536.0);
    }
    free(post);
    made->lamp_placed = 1;
    bake_lamps(tri, made);
    port_log("lamps: %d placed (first node %u at %.1f, %.1f, reach %.1f m)\n", made->lamp_n, made->lamps[0].node, made->lamps[0].x, made->lamps[0].z, made->lamps[0].reach);
}

/* port (`lamp`): the lamps' light baked into the light rows at load: every row point within a lamp's reach takes LAMP_BAKE x the lamp's strength x its falloff at the point's place in the world more
 * light (to full), so the light the polygons interpolate between their corners is the pool, at no cost a frame. TNFS_LAMP_BAKE=0 leaves the rows alone (the lamps' colour and the car's light stay). */
static void bake_lamps(const Tri *tri, PsxLight *made)
{
    const char *env = getenv("TNFS_LAMP_BAKE");
    if (made->data == NULL || (env != NULL && env[0] == '0') || made->nodes != tri->node_count) {
        port_log("lamps: not baked (data %d, env %d, nodes %u of %u)\n", made->data != NULL, env != NULL, made->nodes, tri->node_count);
        return;
    }
    unsigned lifted_cells = 0;
    free(made->lamp_lift);
    made->lamp_lift = calloc((size_t)made->nodes * PSX_LIGHT_POINTS, 1);
    if (made->lamp_lift == NULL) return;
    for (int i = 0; i < made->lamp_n; i++) {
        const PsxLightLamp *const l = &made->lamps[i];
        const float strength = fmaxf(l->rgb[0], fmaxf(l->rgb[1], l->rgb[2]));
        const int span = made->lamp_reach_nodes + 1;
        for (int d = -span; d <= span; d++) {
            int64_t node = (int64_t)l->node + d;
            if (made->lamp_ring) node = ((node % made->nodes) + made->nodes) % made->nodes;
            else if (node < 0 || node >= made->nodes) continue;
            TrackSegment segment;
            if (track_decode_segment(tri, (uint32_t)node / TRI_NODES_PER_SEGMENT, &segment) != TRI_OK) continue;
            for (int p = 0; p < TRACK_ROW_POINTS; p++) {
                const TrackPoint at = segment.rows[node % TRI_NODES_PER_SEGMENT][p];
                const double dx = at.x / 65536.0 - l->x, dz = at.z / 65536.0 - l->z;
                const float lift = LAMP_BAKE * strength * psx_light_lamp_falloff((float)sqrt(dx * dx + dz * dz), l->reach);
                const size_t cell = (size_t)node * PSX_LIGHT_POINTS + (size_t)p;
                const int weight = (int)(255.0f * (lift > 1.0f ? 1.0f : lift) + 0.5f), sum = made->lamp_lift[cell] + weight;
                if (weight == 0) continue;
                made->lamp_lift[cell] = (uint8_t)(sum > 255 ? 255 : sum);
                made->data[cell] = (uint8_t)(made->data[cell] + (PSX_LIGHT_FULL - made->data[cell]) * weight / 255);
                lifted_cells++;
            }
        }
    }
    port_log("lamps: baked into %u row points\n", lifted_cells);
}

/* port (tunnel zone looks): a light made in code with tunnels gets the look its file has (the export writes it: the tunnels' light as a percent of full light at this time, and their colour, worked out from the
 * made light's own look, psx_light_tunnel_look), so the code's light and its file are one light for the world and for the car */
static void made_look_set(PsxLight *made)
{
    if (made->tunnel == NULL || made->look_set) return;
    float shown[3];
    drive_light_tint(shown);
    PsxTunnelLook look;
    psx_light_tunnel_look(made, shown, shown, 0, &look);
    if (!look.cast_on) return;
    const float mean = (shown[0] + shown[1] + shown[2]) / 3.0f;
    const double exact = (double)look.tint[0] * (double)(mean > 0.05f ? mean : 1.0f) * 100.0;
    made->look_set = 1;
    made->look_light = exact;
    for (int digits = 1; digits <= 12; digits++) {   /* the shortest number a person would write that gives the very same gain at run time (psx_light_tunnel_look) */
        char text[32];
        snprintf(text, sizeof text, "%.*g", digits, exact);
        const double shorter = strtod(text, NULL);
        if ((float)(mean > 0.05f ? shorter / 100.0 / (double)mean : shorter / 100.0) == look.tint[0]) {
            made->look_light = shorter;
            break;
        }
    }
    memcpy(made->look_cast, look.cast, sizeof made->look_cast);
}

/* port (track_light_file): the light data/track_light/<TRACK><letter>.txt holds for this track, time and flags, else the one made from the terrain (make_light_code); the file's retouches
 * (brightness, tunnel look) and the track's own rules go over either. TNFS_EXPORT_TRACK_LIGHT=<dir> writes the made light as such a file (tools/export_track_light.sh);
 * TNFS_TRACK_LIGHT_FILE=0 makes it in code as before, ignoring the files. */
static int make_light(const Tri *tri, const char *track, PsxLight *out)
{
    const char *off = getenv("TNFS_TRACK_LIGHT_FILE");
    const uint32_t flags = track_light_flags();
    PsxLight made = { 0 };
    TrackLightFile file = { 0 };
    char path[1024] = "", why[160] = "";
    const uint32_t nodes = tri != NULL ? tri->node_count : 0;
    track_light_note[0] = '\0';
    track_light_places[0] = '\0';
    const int have = port_settings_profile() == PROFILE_RESTOMOD && (off == NULL || off[0] != '0') && track_light_load(track, light_letter, nodes, flags, &file, path, sizeof path, why, sizeof why);   /* the file only in Restomod '26: Stock and Restored make the light in code */
    if (have) {
        made = file.light;
        memset(&file.light, 0, sizeof file.light);
        made.from_file = 1;
        snprintf(track_light_status, sizeof track_light_status, "track light: file %s", path);
        snprintf(track_light_places, sizeof track_light_places, "%s", file.place_path[0] != '\0' ? file.place_path : "no places file");
        snprintf(track_light_note, sizeof track_light_note, "%s", why);
    } else {
        if (!make_light_code(tri, track, &made)) {
            track_light_file_free(&file);
            return 0;
        }
        warm_looks(&made);
        made_look_set(&made);
        snprintf(track_light_status, sizeof track_light_status, "track light: made in code (no file for %s%c)", track, light_letter);
        const char *dir = getenv("TNFS_EXPORT_TRACK_LIGHT");
        if (dir != NULL && dir[0] != '\0') {
            char *text = NULL, file_path[1024];
            size_t size = 0;
            snprintf(file_path, sizeof file_path, "%s/%s%c.txt", dir, track, light_letter);
            if (track_light_format(&made, track, light_letter, flags, &text, &size)) {
                FILE *f = fopen(file_path, "wb");
                if (f != NULL) {
                    fwrite(text, 1, size, f);
                    fclose(f);
                    port_log("track light: wrote %s\n", file_path);
                }
                free(text);
            }
            snprintf(file_path, sizeof file_path, "%s/%s.places.txt", dir, track);   /* the places are the same at every time of day: the export script keeps one */
            if (track_light_place_format(&made, track, &text, &size)) {
                FILE *f = fopen(file_path, "wb");
                if (f != NULL) {
                    fwrite(text, 1, size, f);
                    fclose(f);
                }
                free(text);
            }
        }
    }
    if (have) {
        track_light_apply(&file.tweaks, &made, port_track_light_lamp() && light_letter == 'N', tri_is_circuit(tri));   /* a file's own retouches (the code's light has none); its `lamp` lines with track_light_lamp, at night only: the lamps do nothing in the dawn, day and dusk files */
        place_lamps(tri, &made);
    }
    track_light_file_free(&file);
    apply_rules_file(track, &made);
    *out = made;
    return 1;
}

/* port (lamp_pools): the CD's own light (a night's LGT) has no street lamps in it; the made pools go over it, where the lamps stand. The lamps are kept here and laid when the track's direction is
 * known (drive_light_set_reversed): the CD's rows are of the file's own direction, the lamps' nodes of the track as it runs */
static uint32_t cd_lamp_nodes[MAX_LAMP_NODES];
static size_t cd_lamp_count;
static int cd_lamp_ring;

static void pool_cd_light(const Tri *tri)
{
    cd_lamp_count = 0;
    if (tri == NULL || light.data == NULL || light.pooled || synth_on || made_wanted || !headlights_on || synth_time != PSX_SYNTH_NIGHT) return;
    if (!port_lamp_pools() && !port_lamp_car_shade() && !port_lamp_pool_bright()) return;
    cd_lamp_count = street_lamp_pool_nodes(tri, cd_lamp_nodes);
    cd_lamp_ring = tri_is_circuit(tri);
}

static void lay_cd_pools(void)
{
    if (cd_lamp_count == 0 || light.data == NULL || light.pooled) return;
    if (psx_light_add_pools(&light, cd_lamp_ring, cd_lamp_nodes, (int)cd_lamp_count, port_lamp_pools(), port_lamp_pools() && port_lamp_pool_tint(), port_lamp_car_shade() || port_lamp_pool_bright() || city_night3))
        port_log("psx_light: %zu street lamp pools over the CD's light\n", cd_lamp_count);
    cd_lamp_count = 0;
}

/* the made light of `track` installed as the race's light (the first load's, or a reload's: the one it replaces is freed): the forest's and the City's changes over it, the fields the made light
 * has as the CD's has, the pools; `made` is taken */
static void install_made(PsxLight *made, const char *track, int tunnel_track)
{
    if (port_forest_dark()) psx_light_forest_apply(made, track);
    if (city_night3 && made->data != NULL) {   /* port (city_lift 3): the open rows darkened to the CD's; a tunnel's own light stays */
        const int share = city_made_scale(track);
        if (share < 256)
            for (uint32_t n = 0; n < made->nodes; n++)
                if (made->tunnel == NULL || made->tunnel[n] == 0)
                    for (int p = 0; p < PSX_LIGHT_POINTS; p++) made->data[(size_t)n * PSX_LIGHT_POINTS + (size_t)p] = (uint8_t)(made->data[(size_t)n * PSX_LIGHT_POINTS + (size_t)p] * share / 256);
    }
    psx_light_free(&light);
    light = *made;
    light.gate_board = headlights_on && port_finish_gate_light() ? finish_gate_board(track) + 1 : 0;   /* port (finish_gate_light) */
    light.beam_tunnel = headlights_on ? port_tunnel_headlight() : 0;
    city_fields(&light, track);   /* port (city_lift): the made light has the City's fields as the CD's has (lamp_pool_bright included) */
    light.object_floor = is_lost_vegas(track) && port_vegas_objects() ? VEGAS_OBJECT_LIGHT : 0;   /* port (vegas_objects): the made light's buildings and objects keep their light as the CD's do */
    light.pool = headlights_on ? &headlight_pool : NULL;   /* port (psx_headlights): the made light's open nodes go to the night's, a tunnel keeps the dusk's, the pools lift */
    synth_on = 1;
    split = 1;   /* the dash takes the lamps' light as a script's does: its own ambient, the 3D and the 2D told apart */
    ported_view_set_light(&light, NULL);
    ported_view_set_psx_synth(tunnel_track || (light.tunnel != NULL && port_car_tunnel_light()));   /* the tunnel tracks' car and dash light follows their tunnels; so does any made light's that has tunnels (car_tunnel_light), as the CD's rows do */
}

static const Tri *made_tri;       /* the race's track, kept for the light editor's reload (drive_light_reload) */
static char made_track[16];
static int made_tunnel_track;

void drive_light_load_synth(const Tri *tri, const char *track)
{
    pool_cd_light(tri);
    const int tunnel_track = synth_wanted && psx_light_synth_track(track);
    if ((!tunnel_track && !made_wanted) || light.data != NULL || tri == NULL) return;
    PsxLight made = { 0 };
    if (!make_light(tri, track, &made)) return;
    made_tri = tri, made_tunnel_track = tunnel_track;
    snprintf(made_track, sizeof made_track, "%s", track);
    install_made(&made, track, tunnel_track);
    port_log("psx_light: %s has no light on the CD, made from its terrain%s (%u nodes)\n", track, tunnel_track ? " and the road's tunnels" : "", light.nodes);
}

int drive_light_reload(void)
{
    if (made_tri == NULL || !synth_on) return 0;
    PsxLight made = { 0 };
    if (!make_light(made_tri, made_track, &made)) return 0;
    install_made(&made, made_track, made_tunnel_track);
    port_log("psx_light: %s's light made again\n", made_track);
    return 1;
}

int drive_light_file_state(char *path, size_t path_size, uint32_t *hash)
{
    if (made_tri == NULL || !synth_on) return 0;
    char found[1024];
    if (!track_light_find(made_track, light_letter, found, sizeof found)) {
        if (hash != NULL) *hash = 0;
        if (path != NULL && path_size > 0) path[0] = '\0';
        return 1;
    }
    size_t size = 0;
    uint8_t *file = platform_read_file(found, &size);
    uint32_t h = 2166136261u;
    for (size_t i = 0; file != NULL && i < size; i++) h = (h ^ file[i]) * 16777619u;
    if (file != NULL) platform_free_file(file);
    if (hash != NULL) *hash = h;
    if (path != NULL && path_size > 0) snprintf(path, path_size, "%s", found);
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
    const uint8_t *row = &light.data[(size_t)node * PSX_LIGHT_POINTS];
    snprintf(lines[n++], 96, "row %u %u %u %u %u %u %u %u %u %u %u", row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7], row[8], row[9], row[10]);
    snprintf(lines[n++], 96, "tunnel %d  hall %d  dim car %d  canyon %d  pool %d  car pool %d", light.tunnel != NULL && light.tunnel[node], light.hall != NULL && light.hall[node],
             light.car_dim != NULL && light.car_dim[node], light.canyon != NULL && light.canyon[node], light.pool_class != NULL ? light.pool_class[node] : 0,
             light.car_pool != NULL ? light.car_pool[node] : 0);
    if (n < max) {   /* port (weather_zones): what the rain, fog and lightning make of this node */
        const int open = ported_view_weather_open(node);
        snprintf(lines[n++], 96, "weather: %s %d/16", open >= 16 ? "open" : "closed", open);
    }
    if (n < max) {   /* port (`lamp`): the street lamp of the stretch (the nearest within its reach) */
        const PsxLightLamp *near = NULL;
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
        const PsxLightTone *t = &light.tone[light.tone_id[node] - 1];
        char strip[64] = "";
        if (light.tone_mask[node] != (1u << PSX_LIGHT_POINTS) - 1u)
            for (int p = 0; p < PSX_LIGHT_POINTS; p++)
                if (light.tone_mask[node] >> p & 1u) snprintf(strip + strlen(strip), sizeof strip - strlen(strip), " p%d", p);
        snprintf(lines[n++], 96, "scene tone %d of %d: %02x%02x%02x at %d%%%s", light.tone_id[node], light.tone_count, t->rgb[0], t->rgb[1], t->rgb[2], t->percent, strip);
    } else if (n < max) {
        snprintf(lines[n++], 96, "scene tone: none");
    }
    double look_light = 0.0;
    float look_cast[3];
    const int zone_look = psx_light_zone_look_at(&light, node, &look_light, look_cast);
    if (zone_look && n < max) {   /* the tunnel's look here: its zone's own (a `tunnel a b light colour` line) or the time's `tunnel_tone`: its light as a percent of full light, and its colour */
        snprintf(lines[n++], 96, "tunnel look: light %.4g%%  colour %02x%02x%02x%s%s", look_light, (unsigned)(look_cast[0] * 255.0f + 0.5f), (unsigned)(look_cast[1] * 255.0f + 0.5f),
                 (unsigned)(look_cast[2] * 255.0f + 0.5f), zone_slot ? "  (this zone's own)" : "", light.hall != NULL && light.hall[node] ? "  (a hall: lit by its lamps)" : "");
    }
    else if (!zone_look && n < max)
        snprintf(lines[n++], 96, "tunnel look: the made tunnel's own");
    return n;
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

static int storm_flash;   /* port (WEATHER_STORM): 0 none .. 15 */

void drive_light_set_flash(int level) { storm_flash = level < 0 ? 0 : level > 15 ? 15 : level; }

void drive_light_palette(const uint8_t palette[768], uint8_t out[768])
{
    psx_tint_palette(palette, tint, out);
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
static int class_width, class_scale = 1;

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
                else if (c >= PSX_CLASS_LIT && c < PSX_CLASS_LIT_COLOUR + PSX_CLASS_LIT_COLOURS) world_mask[y * (size_t)rect_width + x] = c;   /* LIT, and LIT_COLOUR + slot: the platform's mask 3, 4 + slot */
            }
        }
    }
    if (rect_set && rect_width > 0 && count % (size_t)rect_width == 0)
        psx_mask_rect(world_mask, rect_width, (int)(count / (size_t)rect_width), rect[0], rect[1], rect[2], rect[3]);
    return world_mask;
}
