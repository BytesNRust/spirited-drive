#include "viewer/drive_light.h"
#include "viewer/drive_light_private.h"
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


static const char LETTERS[] = { 'B', 'A', 'C', 'C' };   /* by LIGHT_TIME_*: day, dawn, dusk, night (the dusk's files, darker) */

TrackNodeLight light;
float tint[3] = { 1, 1, 1 };
PsxLgs ambient;
int ambient_ok;   /* ambient holds the LGS read */
static uint32_t ambient_nodes;
float dash_ratio[3] = { 1, 1, 1 };   /* the dash's ambient over the world's tint, from the car's light (drive_light_dash_ratio) */
int split;   /* the race has the light and its script: the dash has its own ambient, so the 3D and the 2D go through the split present */
SkyArtDraw sky_draw;
int lamp_pool_count;
const TunnelLampTexture *night_lamp_spec;   /* the tunnel lamps of the track when the race is at night (drive_light_night_lamps), else none */
int night_lamp_count;
static HeadlightPool headlight_pool;   /* port (headlight_beams): the pools of light the cars lift the night's dark nodes by (ported_view keeps its nodes) */
int headlights_on;               /* the race is at night with the pools: the ambient is the pooled night's, the nodes' light carries the dark */
/* with the pools the palette is the dusk's with the night's hue (light_tint_night over HEADLIGHT_FLOOR, 1 at most per channel) and the nodes carry the rest of the
 * dark (headlight_pool.h): open road and country go to the night's light, a tunnel and a car's pool keep the dusk's; the sky keeps light_tint_night */
const float NIGHT_PALETTE[3] = { 0.80f, 0.96f, 1.0f };
/* port (night_sky_hue): the hue of the made night's sky, blue-black: the sky's colours keep their luminance and take these channels' proportions */
const float NIGHT_SKY_RAMP[3] = { 0.62f, 0.80f, 1.0f };
StreetLamp street_lamps[STREET_LAMPS_MAX];   /* port (headlight_beams): the street lamps flare at night (street_lamp.h) */
int street_lamp_count;
int street_lamp_on;
int street_lamp_by_day;   /* the lamps are lit in the daylight (the day's paint: no outer rim) */
int dusk_look;   /* port (gauge_exterior): the dusk: the exterior lights are on */
int night_look;   /* the night's look beyond the sky: the dash darkens (the night, and Lost Vegas always) */
const WindowBoard *window_boards;   /* port (headlight_beams): the night's lit windows (window_lights.h), one pool entry for all the boards */
int window_board_count;
const WindowDark *dark_boards;      /* the same, found by darkness (facades off the grid) */
int dark_board_count;
const WindowSign *sign_boards;     /* port (sign_glow): the City's lit matrix sign (window_lights.h), painted in the windows' entry */
int sign_board_count;
int night_sky;   /* the race is at night: a sky of the dusk is darkened by the night's light before it is matched to the shown palette */


static int time_tinted;
char light_letter = 'B';   /* the time's CD letter, for the rules file */
static int made_wanted;   /* port (own light): no CD light for this track, so one is made from its terrain (terrain_light.h) in drive_light_load_synth */
int synth_wanted, synth_on, synth_time;   /* a track the CD has no light for, with the light on; the light is one made from the road (drive_light_load_synth) */
int alt_sky_wanted;   /* the look is on at dawn or dusk and the CD holds no sky of its own for the track (drive_light_load_alt_sky) */

/* Lost Vegas (tr4) under the light effects: its light 10% darker than the CD's script gives, and its fog always dark */
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
static void city_fields(TrackNodeLight *l, const char *track)
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


/* port (night tone, dev env only: TNFS_NIGHT_TONE): the night's flat tint with no baked light. 0 the fallback above; 1 a tone a track: the CD's night tint of each track before its
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
        const float scale = light_tint_night_ceiling_scale(out, ceiling);
        for (int k = 0; k < 3; k++) out[k] *= scale * NIGHT_TONE[i].gain * (is_lost_vegas(track) ? LOST_VEGAS_DARKEN : 1.0f);   /* the gain over the ceiling: under it the ceiling would take it back */
        return 1;
    }
    return 0;
}

static void use_fallback_tint(int time, const char *track)
{
    const float *f = FALLBACK_TINT[time == LIGHT_TIME_DUSK || time == LIGHT_TIME_NIGHT];
    for (int k = 0; k < 3; k++) tint[k] = f[k] * (time != LIGHT_TIME_NIGHT ? 1.0f : headlights_on && (synth_wanted || made_wanted) ? NIGHT_PALETTE[k] : light_tint_night[k]);   /* a made light carries the night in its nodes with the pools */
    float tone[3];
    if (time == LIGHT_TIME_NIGHT && night_tone_tint(track, tone)) {
        for (int k = 0; k < 3; k++) tint[k] = tone[k];
        port_log("light_effects: night tone %d: tint %.2f %.2f %.2f\n", night_tone_mode(), tint[0], tint[1], tint[2]);
        return;   /* final: under its ceiling already */
    }
    if (time == LIGHT_TIME_NIGHT && city_level > 0) {   /* port (city_lift): the City's ceiling over the made night's tint too, as over the CD's; level 0 stays as it was */
        const float k = light_tint_night_ceiling_scale(tint, CITY_CEILING[city_level]);
        for (int c = 0; c < 3; c++) tint[c] *= k;
    }
    port_log("light_effects: no light of this track, the flat ambient of the time: tint %.2f %.2f %.2f\n", tint[0], tint[1], tint[2]);
}

int drive_light_time_tinted(void) { return time_tinted || split; }

/* port: every state drive_light_load sets, back to nothing for a race with no light (or before the light's own) */
static void reset_for_load(int enabled, int time, const char *track)
{
    time_tinted = enabled && time != LIGHT_TIME_DAY;
    night_sky = time == LIGHT_TIME_NIGHT;
    dusk_look = enabled && time == LIGHT_TIME_DUSK;
    night_look = night_sky || (enabled && is_lost_vegas(track));   /* Lost Vegas is always night: its dash and effects are the night's, its sky already is */
    headlights_on = 0;
    city_night3 = 0, city_level = 0;
    street_lamp_on = 0;
    window_boards = NULL, window_board_count = 0, dark_boards = NULL, dark_board_count = 0, sign_boards = NULL, sign_board_count = 0;
    night_lamp_spec = NULL, night_lamp_count = 0, lamp_pool_count = 0;
    ported_view_set_police_lights(enabled);   /* the light effects: the police car's lights flash (police_lights.h) */
    alt_sky_wanted = 0;
    split = 0;
    ported_view_set_flares(port_lamp_flares());
    ported_view_set_soft_shadow(port_car_soft_shadow());   /* its own flag: no light or PS1 files needed */
    ported_view_set_tunnel_lamps(NULL, 0);
    ported_view_set_street_lamps(NULL, 0, NULL);
    street_lamp_count = 0;
    ported_view_set_light(NULL, NULL);
    ported_view_set_light_ambient(NULL, 0, NULL);
    ported_view_set_light_synth(0);
    synth_wanted = 0;
    made_wanted = 0;
    ported_view_set_sky_art(NULL);
    ported_view_set_sky_art_info(NULL);
    ported_view_set_dark_fog(0);
    sky_art_draw_free(&sky_draw);
    track_node_light_free(&light);
    synth_on = 0;
    tint[0] = tint[1] = tint[2] = 1.0f;
}

/* port: the race has no CD light file (or no PS1 folder): the light is made from the terrain, with the fallback tint of the time */
static void load_without_cd_light(int time, const char *track, int have_dir, const char *path)
{
    if (have_dir) port_log("light_effects: no %s\n", path);
    else port_log("light_effects: no PS1 directory, the port's own effects only\n");
    made_wanted = 1;   /* port (own light): no CD light, the light is made from the terrain */
    if (time != LIGHT_TIME_DAY) use_fallback_tint(time, track);
    if (time == LIGHT_TIME_NIGHT && is_lost_vegas(track) && port_vegas_car_night() && night_tone_mode() == 0) {   /* port (vegas_car_night): the car was lit as by day against Lost Vegas' dark; the tint of the CD's own night (ZTR4C's, under the ceiling) */
        static const float VEGAS_NIGHT[3] = { 0.27f, 0.33f, 0.34f };
        const float lift_k = VEGAS_CEILING[port_vegas_lift()] / VEGAS_CEILING[0];   /* port (vegas_lift): the CD's tint rides its ceiling (the CD's own is above every level's); level 0 is as it was */
        for (int c = 0; c < 3; c++) tint[c] = VEGAS_NIGHT[c] * lift_k;
        port_log("light_effects: Lost Vegas' night tint %.2f %.2f %.2f\n", tint[0], tint[1], tint[2]);
    }
    const TunnelLampTexture *own_lamps = NULL;
    const int own_count = track_lamps_for(track, &own_lamps);
    ported_view_set_tunnel_lamps(own_lamps, own_count);   /* the lamps' glow (lamp_flares) needs them known even with no light */
    if (time == LIGHT_TIME_NIGHT || is_lost_vegas(track)) night_lamp_spec = own_lamps, night_lamp_count = own_count;   /* the lamps' own colours are kept lit in the night's palette */
}

/* The light effects of the track at this time of day: the headlights, the city's lift, the street lamps, the boards, the synth's time and the dark fog. */
static void light_effects_set(int time, const char *track)
{
    headlights_on = (time == LIGHT_TIME_NIGHT || is_lost_vegas(track)) && port_headlight_beams();
    city_night3 = headlights_on && is_city(track) && port_city_lift() >= 3;
    city_level = headlights_on && time == LIGHT_TIME_NIGHT && is_city(track) ? port_city_lift() : 0;
    street_lamp_count = headlights_on ? street_lamps_for(track, street_lamps, STREET_LAMPS_MAX)
                                      : port_headlight_beams() ? street_lamps_for_day(track, street_lamps, STREET_LAMPS_MAX) : 0;   /* port: the Light Effects' lamps lit by day too (street_lamp.h) */
    street_lamp_by_day = !headlights_on;
    street_lamp_on = street_lamp_count > 0;
    window_board_count = headlights_on ? window_boards_for(track, &window_boards) : 0;
    dark_board_count = headlights_on ? window_darks_for(track, &dark_boards) : 0;
    sign_board_count = headlights_on && port_sign_glow() ? window_signs_for(track, &sign_boards) : 0;   /* port (sign_glow) */
    synth_wanted = light_synth_track(track);
    synth_time = time == LIGHT_TIME_DAWN ? LIGHT_SYNTH_DAWN : time == LIGHT_TIME_DUSK ? LIGHT_SYNTH_DUSK : time == LIGHT_TIME_NIGHT ? LIGHT_SYNTH_NIGHT : LIGHT_SYNTH_DAY;   /* the night: the dusk's doors, a tunnel not darker than the road */
    if (is_lost_vegas(track) || time == LIGHT_TIME_NIGHT) ported_view_set_dark_fog(1);   /* the night's fog is dark as Lost Vegas' */
}

/* The parsed CD light taken as the race's: the port's flags on it, its tint, and the ambient script. */
static void light_adopt(const TrackNodeLight *parsed, const char *track, const char *psx_dir, char letter, int time, uint32_t ring_nodes)
{
    light = *parsed;
    light.gate_board = headlights_on && port_finish_gate_light() ? finish_gate_board(track) + 1 : 0;   /* port (finish_gate_light) */
    light.forest_road = port_forest_neutral() && terrain_light_forest_road_range(track, &light.forest_from, &light.forest_to);   /* port (forest_neutral): the forest's shade is not drawn as a tunnel */
    light.beam_tunnel = headlights_on ? TRACK_NODE_LIGHT_BEAM_TUNNEL : 0;
    light.object_floor = is_lost_vegas(track) && port_vegas_objects() ? VEGAS_OBJECT_LIGHT : 0;   /* port (vegas_objects) */
    light.pool = headlights_on ? &headlight_pool : NULL;
    city_fields(&light, track);   /* port (city_lift) */
    load_tint(psx_dir, track, letter, time == LIGHT_TIME_NIGHT, is_city(track) ? CITY_CEILING[port_city_lift()] : is_lost_vegas(track) ? VEGAS_CEILING[port_vegas_lift()] : NIGHT_TINT_CEILING);   /* port (city_lift, vegas_lift) */
    if (is_lost_vegas(track))
        for (int k = 0; k < 3; k++) tint[k] *= LOST_VEGAS_DARKEN;
    ambient_nodes = ring_nodes;
    if (ambient_ok) {
        ported_view_set_light_ambient(&ambient, ring_nodes, tint);
        track_node_light_set_script(&light, &ambient);   /* the exe's light of the track's points */
        split = 1;
    }
}

void drive_light_load(int enabled, int time, const char *psx_dir, const char *track, uint32_t ring_nodes)
{
    reset_for_load(enabled, time, track);
    if (!enabled) return;
    const int have_dir = psx_dir != NULL && psx_dir[0] != '\0';   /* port: with no PS1 disc the light's own effects (pools, lamps, windows, the made light, fallback tints) still run */
    light_effects_set(time, track);
    const char letter = LETTERS[time >= LIGHT_TIME_DAY && time <= LIGHT_TIME_NIGHT ? time : LIGHT_TIME_DAY];
    light_letter = time == LIGHT_TIME_NIGHT ? 'N' : letter;   /* the night has its own rules (the CD's C is the dusk) */
    if (time != LIGHT_TIME_DAY && have_dir) load_sky(psx_dir, track, letter);
    alt_sky_wanted = time != LIGHT_TIME_DAY && !sky_draw.ready;
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
    TrackNodeLight parsed = { 0 };
    const int ok = psx_lgt_parse(file, size, ring_nodes, &parsed);
    platform_free_file(file);
    if (!ok) {
        port_log("light_effects: %s does not fit the track (%u nodes)\n", path, ring_nodes);
        made_wanted = 1;
        if (time != LIGHT_TIME_DAY) use_fallback_tint(time, track);
        return;
    }
    if (port_forest_dark()) terrain_light_forest_apply(&parsed, track);   /* port (forest_dark): the CD's light too, in the file's node order */
    light_adopt(&parsed, track, psx_dir, letter, time, ring_nodes);
    const TunnelLampTexture *lamps = NULL;
    const int lamp_count = track_lamps_for(track, &lamps);   /* the tunnel's lamps stay bright under the light */
    ported_view_set_tunnel_lamps(lamps, lamp_count);
    if (time == LIGHT_TIME_NIGHT || is_lost_vegas(track)) night_lamp_spec = lamps, night_lamp_count = lamp_count;
    ported_view_set_light(&light, NULL);   /* the tint goes to the palette (drive_light_palette), not through the polygons' tables */
    port_log("light_effects: %s, %u nodes, tint %.2f %.2f %.2f\n", path, light.nodes, tint[0], tint[1], tint[2]);
}

static void lay_cd_pools(void);

void drive_light_set_reversed(int reversed)
{
    if (synth_on) return;   /* made from the track as it runs: already in its direction */
    track_node_light_set_reversed(&light, reversed);
    lay_cd_pools();
}

/* port (own light): the terrain's light of the time (terrain_light.h), with a tunnel's rows laid over it where the road has one (the synth's rows are 255 in the open, so the lower of
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

/* the track's own rules over the made light: the player's folder's track_node_light_<TRACK><A|B|C|N>.txt (N the night), a rule a line (terrain_light.h); no file, no rules */
static void apply_rules_file(const char *track, TrackNodeLight *made)
{
    char name[64];
    if (track == NULL) return;
    snprintf(name, sizeof name, "track_node_light_%s%c.txt", track, light_letter);
    if (!platform_user_file_exists(name)) return;
    size_t size = 0;
    uint8_t *text = platform_read_user_file(name, &size);
    if (text == NULL) return;
    static TerrainLightRule rules[MAX_RULES];
    const int n = terrain_light_rules_parse((const char *)text, size, rules, MAX_RULES);
    platform_free_file(text);
    terrain_light_apply_rules(made->data, made->nodes, rules, n);
    port_log("light_effects: %d rules from %s\n", n, name);
}

/* port (lamp_pool_tint, lamp_car_shade, lamp_pool_bright): which nodes a street lamp's pool lies on, for the road's warm tint and for the car's light */
static void lamp_pool_classes(const Tri *tri, TrackNodeLight *made, const uint32_t *lamp_nodes, size_t lamps)
{
    if (lamps > 0 && port_lamp_pool_tint()) {   /* port (lamp_pool_tint): which nodes a pool lies on, for the warm tint of the road there */
        made->pool_class = malloc(made->nodes);
        if (made->pool_class != NULL) terrain_light_lamp_classes(made->pool_class, made->nodes, tri_is_circuit(tri), lamp_nodes, (int)lamps);
    }
    if (lamps > 0 && (port_lamp_car_shade() || port_lamp_pool_bright() || city_night3)) {   /* port (lamp_car_shade, lamp_pool_bright): and the same classes for the car's light, and for the pool's own light */
        made->car_pool = malloc(made->nodes);
        if (made->car_pool != NULL) terrain_light_lamp_classes(made->car_pool, made->nodes, tri_is_circuit(tri), lamp_nodes, (int)lamps);
    }
    made->lamp_node = lamps > 0 ? malloc(lamps * sizeof *made->lamp_node) : NULL;   /* port (track light file): the lamps' nodes, which the export writes as `lamp` lines in place of the pools' runs */
    if (made->lamp_node != NULL) memcpy(made->lamp_node, lamp_nodes, lamps * sizeof *made->lamp_node), made->lamp_node_count = (int)lamps, made->lamp_ring = tri_is_circuit(tri);
}

/* port (trans_shadows): the parking's deck roofs nodes 465..470 too: the interior shade of its marked run, the hall's points beyond the foot kept as the hall keeps them */
static void trans_closed_run(TrackNodeLight *made, TrackNodeLight *tunnels, const char *track, int trans)
{
    uint32_t closed_from = 1, closed_to = 0, closed_row = 0;
    if (trans && light_synth_trans_closed(track, &closed_from, &closed_to, &closed_row) && tunnels->data != NULL && tunnels->nodes == made->nodes && closed_to < made->nodes) {
        uint32_t hall_from = 1, hall_to = 0;   /* port (trans_shadows): the parking's deck roofs nodes 465..470 too: the interior shade of its marked run, the hall's points beyond the foot kept as the hall keeps them */
        const int hall = port_hall_light() && light_synth_hall(track, &hall_from, &hall_to);
        for (uint32_t n = closed_from; n <= closed_to; n++)
            for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) {
                const int beyond = p == 3 || p == 4 || p == 5 || p >= 8;
                const uint8_t row = tunnels->data[(size_t)closed_row * TRACK_NODE_LIGHT_POINTS + (size_t)p];
                uint8_t *at = &made->data[(size_t)n * TRACK_NODE_LIGHT_POINTS + (size_t)p];
                if (!(hall && beyond) && row < *at) *at = row;
            }
        if (tunnels->tunnel != NULL)
            for (uint32_t n = closed_from; n <= closed_to; n++) tunnels->tunnel[n] = LIGHT_SYNTH_FLAG_TUNNEL;
    }
}

/* port (parking_dark, cavern_dark): the car keeps one dim tone through the parking's decks and TR6's cavern */
static void car_dim_runs(TrackNodeLight *made, const char *track)
{
    uint32_t park_from = 1, park_to = 0;
    if (port_parking_dark() && light_synth_parking(track, &park_from, &park_to) && park_to < made->nodes) {   /* port (parking_dark): the car keeps the hall's dark tone from the deck's first node to the exit tunnel */
        made->car_dim = calloc(made->nodes, 1);
        if (made->car_dim != NULL) memset(made->car_dim + park_from, 1, park_to - park_from + 1);
    }
    uint32_t cave_from = 1, cave_to = 0;
    if (port_cavern_dark() && light_synth_cavern(track, &cave_from, &cave_to) && cave_to < made->nodes) {   /* port (cavern_dark): the car keeps one dim tone through TR6's cavern */
        if (made->car_dim == NULL) made->car_dim = calloc(made->nodes, 1);
        if (made->car_dim != NULL) memset(made->car_dim + cave_from, 1, cave_to - cave_from + 1);
    }
}

/* port (trans_shadows): the airport's approach is open road: the scenery beside it keeps the open light, not the walls' shade */
static void open_approach(TrackNodeLight *made, int trans, uint32_t open_from, uint32_t open_to)
{
    if (trans && open_to < made->nodes) {   /* port (trans_shadows): the airport's approach is open road: the scenery beside it keeps the open light, not the walls' shade */
        uint8_t open_level = 0;
        for (size_t i = 0; i < (size_t)made->nodes * TRACK_NODE_LIGHT_POINTS; i++) open_level = made->data[i] > open_level ? made->data[i] : open_level;
        for (uint32_t n = open_from; n <= open_to; n++)
            for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++)
                if (p != 0 && p != 1 && p != 6) made->data[(size_t)n * TRACK_NODE_LIGHT_POINTS + (size_t)p] = open_level;
    }
}

/* port: the hall nodes' flag and the canyon's nodes of the made light, from the synth's tunnel rows. */
static void made_hall_canyon(TrackNodeLight *made, TrackNodeLight *tunnels, const char *track)
{
    if (tunnels->data != NULL && tunnels->nodes == made->nodes) {   /* port: the hall nodes' flag (the mark in the synth's row is lost to the terrain's lower light above) */
        made->hall = calloc(made->nodes, 1);
        if (made->hall != NULL)
            for (uint32_t n = 0; n < made->nodes; n++) made->hall[n] = tunnels->data[(size_t)n * TRACK_NODE_LIGHT_POINTS + LIGHT_SYNTH_HALL_POINT] == LIGHT_SYNTH_HALL_MARK;
    }
    if (port_car_tunnel_light() && !light_synth_hall(track, NULL, NULL)) {   /* port (car_tunnel_light): only TR7 has a hall; TR6's property-16 stretch after the mine is open road in the sun, not an orange hall */
        if (port_canyon_shade() && tunnels->tunnel != NULL && tunnels->nodes == made->nodes) {   /* port (canyon_shade): remember the canyon's nodes before the flag is cleared */
            made->canyon = calloc(made->nodes, 1);
            if (made->canyon != NULL)
                for (uint32_t n = 0; n < made->nodes; n++) made->canyon[n] = tunnels->tunnel[n] == LIGHT_SYNTH_FLAG_HALL;
        }
        if (made->hall != NULL) memset(made->hall, 0, made->nodes);
        if (tunnels->tunnel != NULL && tunnels->nodes == made->nodes)
            for (uint32_t n = 0; n < made->nodes; n++)
                if (tunnels->tunnel[n] == LIGHT_SYNTH_FLAG_HALL) tunnels->tunnel[n] = 0;
    }
}

static int make_light_code(const Tri *tri, const char *track, TrackNodeLight *out)
{
    TrackNodeLight made = { 0 };
    static uint32_t lamp_nodes[MAX_LAMP_NODES];
    const size_t lamps = synth_time == LIGHT_SYNTH_NIGHT ? street_lamp_pool_nodes(tri, lamp_nodes) : 0;
    if (!terrain_light_make_lit(tri, synth_time, lamp_nodes, (int)lamps, &made)) return 0;
    if (lamps > 0) port_log("light_effects: %zu street lamp pools\n", lamps);
    lamp_pool_classes(tri, &made, lamp_nodes, lamps);
    TrackNodeLight tunnels = { 0 };
    uint32_t open_from = 1, open_to = 0;
    const int trans = port_trans_shadows() && light_synth_trans(track, &open_from, &open_to);
    const int gallery = port_made_tunnels() && port_car_tunnel_light() && light_synth_gallery_track(track);   /* port (made_tunnels, car_tunnel_light): CL2's galleries (property 7) are tunnels of its light, as the CD's is */
    if (light_synth_ex(tri, synth_time, gallery, &tunnels) && tunnels.nodes == made.nodes) {
        uint32_t hall_from = 1, hall_to = 0;
        const int hall = port_hall_light() && light_synth_hall(track, &hall_from, &hall_to);
        for (size_t i = 0; i < (size_t)made.nodes * TRACK_NODE_LIGHT_POINTS; i++) {
            const uint32_t node = (uint32_t)(i / TRACK_NODE_LIGHT_POINTS), point = (uint32_t)(i % TRACK_NODE_LIGHT_POINTS);
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
        const int hall_range = port_hall_light() && light_synth_hall(track, &hall_from, &hall_to);
        light_synth_made_tunnels(&made, &tunnels, hall_range ? hall_from : 1, hall_range ? hall_to : 0);
        made.neutral_cast = light_synth_mine(track) || gallery;   /* the mine keeps its blue, the warm cast stays on TR7's tunnels */
        made.city_tunnel = light_synth_city_track(track);   /* 1 CY1, 2 CY2/CY3, 0 else */
    }
    track_node_light_free(&tunnels);
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

char track_light_note[160] = "";   /* a small line for the editor when the file was made with other settings than now (a record: the file is used all the same) */
char track_light_places[200] = "";   /* and the track's places file, when there is one */
char track_light_status[200] = "";   /* what the track light file did for this race, for the light editor's screen (drive_light_edit_lines) */

/* port (made_tunnels): a made light's warm tunnels take the look of their kind (light_synth_warm_look), which a track light file's retouches then change */
static void warm_looks(TrackNodeLight *made)
{
    for (int dark = 0; dark < 2 && made->city_tunnel != 0; dark++)
        (void)light_synth_warm_look(made->city_tunnel, dark, made->warm_cast[dark], &made->warm_percent[dark]);
}

static void bake_lamps(const Tri *tri, TrackNodeLight *made);

/* port (`lamp`): where each of the file's lamps stands in the world, in metres. A lamp line that matches a street lamp post (a scenery of the lamp boards at its node) is where the post is; else it is at
 * the node's centre. The reach, which the file gives in nodes, becomes metres by the road's own spacing there. */
static void place_lamps(const Tri *tri, TrackNodeLight *made)
{
    if (made->lamps == NULL || made->lamp_n <= 0 || made->lamp_placed || tri == NULL || tri->node_count == 0) return;
    uint8_t *post = calloc((size_t)made->lamp_n, 1);
    if (post == NULL) return;
    for (int i = 0; i < made->lamp_n; i++) {
        TrackNodeLightLamp *const l = &made->lamps[i];
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
static void bake_lamps(const Tri *tri, TrackNodeLight *made)
{
    const char *env = getenv("TNFS_LAMP_BAKE");
    if (made->data == NULL || (env != NULL && env[0] == '0') || made->nodes != tri->node_count) {
        port_log("lamps: not baked (data %d, env %d, nodes %u of %u)\n", made->data != NULL, env != NULL, made->nodes, tri->node_count);
        return;
    }
    unsigned lifted_cells = 0;
    free(made->lamp_lift);
    made->lamp_lift = calloc((size_t)made->nodes * TRACK_NODE_LIGHT_POINTS, 1);
    if (made->lamp_lift == NULL) return;
    for (int i = 0; i < made->lamp_n; i++) {
        const TrackNodeLightLamp *const l = &made->lamps[i];
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
                const float lift = LAMP_BAKE * strength * track_node_light_lamp_falloff((float)sqrt(dx * dx + dz * dz), l->reach);
                const size_t cell = (size_t)node * TRACK_NODE_LIGHT_POINTS + (size_t)p;
                const int weight = (int)(255.0f * (lift > 1.0f ? 1.0f : lift) + 0.5f), sum = made->lamp_lift[cell] + weight;
                if (weight == 0) continue;
                made->lamp_lift[cell] = (uint8_t)(sum > 255 ? 255 : sum);
                made->data[cell] = (uint8_t)(made->data[cell] + (TRACK_NODE_LIGHT_FULL - made->data[cell]) * weight / 255);
                lifted_cells++;
            }
        }
    }
    port_log("lamps: baked into %u row points\n", lifted_cells);
}

/* port (tunnel zone looks): a light made in code with tunnels gets the look its file has (the export writes it: the tunnels' light as a percent of full light at this time, and their colour, worked out from the
 * made light's own look, track_node_light_tunnel_look), so the code's light and its file are one light for the world and for the car */
static void made_look_set(TrackNodeLight *made)
{
    if (made->tunnel == NULL || made->look_set) return;
    float shown[3];
    drive_light_tint(shown);
    TrackNodeTunnelLook look;
    track_node_light_tunnel_look(made, shown, shown, 0, &look);
    if (!look.cast_on) return;
    const float mean = (shown[0] + shown[1] + shown[2]) / 3.0f;
    const double exact = (double)look.tint[0] * (double)(mean > 0.05f ? mean : 1.0f) * 100.0;
    made->look_set = 1;
    made->look_light = exact;
    for (int digits = 1; digits <= 12; digits++) {   /* the shortest number a person would write that gives the very same gain at run time (track_node_light_tunnel_look) */
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

static int tunnel_lamp_by_node(const void *a, const void *b)
{
    const TrackNodeLightLamp *x = a, *y = b;
    return x->node < y->node ? -1 : x->node > y->node;
}

/* port (tunnel_lamps): the lamps of the track's tunnels and caverns (the lamp textures of tunnel_lamp_textures.c) joined to the light's lamp list, at night: each lights a pool under it over the tunnel's own light
 * (view_tables.c's lamp_polygon_tunnel). Their reach is 1.8 nodes (the search window, TUNNEL_LAMP_REACH_NODES, is wider); nothing is baked into the rows, so a dark tunnel_tone is not scaled by them. */
enum { TUNNEL_LAMPS_MAX = 1024, TUNNEL_LAMP_REACH_NODES = 3 };
#define TUNNEL_LAMP_STRENGTH 3.0f   /* 300 percent: at 5.0 the pool saturated the tunnel tables by half its reach and the lamps (every 2 nodes, both sides) joined into one flat band, no pools (chase view, tunnel_tone 50) */
#define TUNNEL_LAMP_DAY_STRENGTH 1.5f   /* by day (tunnel_lamps 2: dawn, day, dusk) the same pools, at half: the day's tunnel is lit more than the night's, and at 3.0 the road turned strongly amber, brighter than at night (the day captures of 2026-11-11) */
#define TUNNEL_LAMP_REACH_ALONG 1.3f   /* along the road a pool reaches 1.3 nodes (8 m): the lamps hang every 2 nodes (12 m), so half way to the next a pool has 5 % of its light and the tunnel alternates light and shadow (at 1.8 round the pools overlapped into one even band; at 1.0 the pools were dim slivers); across it keeps TUNNEL_LAMP_REACH, the walls */
#define TUNNEL_LAMP_REACH 1.8f   /* a pool reaches 1.8 nodes (11 m at 6 m a node): CY1's lamp pair hangs 6 m apart in the middle of a 19 m tunnel (strips 4 and 9 are lanes, not walls), so at 1.2 the left wall was out of reach (7 m from its lamp) and only the right one (5.8 m) caught a sliver; at 1.8 both walls and the ceiling are lit and the road still has its gaps between lamps */

static void add_tunnel_lamps(const Tri *tri, const char *track, TrackNodeLight *made)
{
    static TunnelLamp found[TUNNEL_LAMPS_MAX];
    if (!port_tunnel_lamps() || (light_letter != 'N' && port_tunnel_lamps() < 2) || tri == NULL || made->nodes == 0) return;
    const size_t n = tunnel_lamps_find(tri, track, found, TUNNEL_LAMPS_MAX);
    if (n == 0) return;
    TrackNodeLightLamp *const list = calloc((size_t)made->lamp_n + n, sizeof *list);
    if (list == NULL) return;
    if (made->lamps != NULL && made->lamp_n > 0) memcpy(list, made->lamps, (size_t)made->lamp_n * sizeof *list);
    const float strength = light_letter == 'N' ? TUNNEL_LAMP_STRENGTH : TUNNEL_LAMP_DAY_STRENGTH;
    for (size_t i = 0; i < n; i++) {
        TrackNodeLightLamp *const l = &list[(size_t)made->lamp_n + i];
        l->node = found[i].node, l->x = found[i].x, l->z = found[i].z;
        l->reach = TUNNEL_LAMP_REACH * found[i].spacing;
        l->reach_along = TUNNEL_LAMP_REACH_ALONG * found[i].spacing;
        l->along[0] = found[i].along[0], l->along[1] = found[i].along[1];
        l->rgb[0] = strength, l->rgb[1] = 0.82f * strength, l->rgb[2] = 0.5f * strength;   /* the amber of the street lamps' warm light, a little paler, 300 percent */
        l->tunnel = 1;
    }
    const int total = made->lamp_n + (int)n;
    qsort(list, (size_t)total, sizeof *list, tunnel_lamp_by_node);
    free(made->lamps);
    made->lamps = list, made->lamp_n = total;
    made->tunnel_lamp_n += (int)n;
    made->lamp_placed = 1;
    made->lamp_ring = tri_is_circuit(tri);
    if (made->lamp_reach_nodes < TUNNEL_LAMP_REACH_NODES) made->lamp_reach_nodes = TUNNEL_LAMP_REACH_NODES;
    port_log("tunnel_lamps: %zu lamps placed (%d lamps in all, %.1f m between nodes)\n", n, total, (double)found[0].spacing);
}

/* port (track_light_file): the light data/track_light/<TRACK><letter>.txt holds for this track, time and flags, else the one made from the terrain (make_light_code); the file's retouches
 * (brightness, tunnel look) and the track's own rules go over either. TNFS_EXPORT_TRACK_LIGHT=<dir> writes the made light as such a file (tools/export_track_light.sh);
 * TNFS_TRACK_LIGHT_FILE=0 makes it in code as before, ignoring the files. */
static int make_light(const Tri *tri, const char *track, TrackNodeLight *out)
{
    const char *off = getenv("TNFS_TRACK_LIGHT_FILE");
    const uint32_t flags = track_light_flags();
    TrackNodeLight made = { 0 };
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
    add_tunnel_lamps(tri, track, &made);
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
    if (tri == NULL || light.data == NULL || light.pooled || synth_on || made_wanted || !headlights_on || synth_time != LIGHT_SYNTH_NIGHT) return;
    if (!port_lamp_pools() && !port_lamp_car_shade() && !port_lamp_pool_bright()) return;
    cd_lamp_count = street_lamp_pool_nodes(tri, cd_lamp_nodes);
    cd_lamp_ring = tri_is_circuit(tri);
}

static void lay_cd_pools(void)
{
    if (cd_lamp_count == 0 || light.data == NULL || light.pooled) return;
    if (terrain_light_add_pools(&light, cd_lamp_ring, cd_lamp_nodes, (int)cd_lamp_count, port_lamp_pools(), port_lamp_pools() && port_lamp_pool_tint(), port_lamp_car_shade() || port_lamp_pool_bright() || city_night3))
        port_log("light_effects: %zu street lamp pools over the CD's light\n", cd_lamp_count);
    cd_lamp_count = 0;
}

/* the made light of `track` installed as the race's light (the first load's, or a reload's: the one it replaces is freed): the forest's and the City's changes over it, the fields the made light
 * has as the CD's has, the pools; `made` is taken */
static void install_made(TrackNodeLight *made, const char *track, int tunnel_track)
{
    if (port_forest_dark()) terrain_light_forest_apply(made, track);
    if (city_night3 && made->data != NULL) {   /* port (city_lift 3): the open rows darkened to the CD's; a tunnel's own light stays */
        const int share = city_made_scale(track);
        if (share < 256)
            for (uint32_t n = 0; n < made->nodes; n++)
                if (made->tunnel == NULL || made->tunnel[n] == 0)
                    for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) made->data[(size_t)n * TRACK_NODE_LIGHT_POINTS + (size_t)p] = (uint8_t)(made->data[(size_t)n * TRACK_NODE_LIGHT_POINTS + (size_t)p] * share / 256);
    }
    track_node_light_free(&light);
    light = *made;
    light.gate_board = headlights_on && port_finish_gate_light() ? finish_gate_board(track) + 1 : 0;   /* port (finish_gate_light) */
    light.beam_tunnel = headlights_on ? TRACK_NODE_LIGHT_BEAM_TUNNEL : 0;
    city_fields(&light, track);   /* port (city_lift): the made light has the City's fields as the CD's has (lamp_pool_bright included) */
    light.object_floor = is_lost_vegas(track) && port_vegas_objects() ? VEGAS_OBJECT_LIGHT : 0;   /* port (vegas_objects): the made light's buildings and objects keep their light as the CD's do */
    light.pool = headlights_on ? &headlight_pool : NULL;   /* port (headlight_beams): the made light's open nodes go to the night's, a tunnel keeps the dusk's, the pools lift */
    synth_on = 1;
    split = 1;   /* the dash takes the lamps' light as a script's does: its own ambient, the 3D and the 2D told apart */
    ported_view_set_light(&light, NULL);
    ported_view_set_light_synth(tunnel_track || (light.tunnel != NULL && port_car_tunnel_light()));   /* the tunnel tracks' car and dash light follows their tunnels; so does any made light's that has tunnels (car_tunnel_light), as the CD's rows do */
}

const Tri *made_tri;       /* the race's track, kept for the light editor's reload (drive_light_reload) */
char made_track[16];
static int made_tunnel_track;

void drive_light_load_synth(const Tri *tri, const char *track)
{
    pool_cd_light(tri);
    const int tunnel_track = synth_wanted && light_synth_track(track);
    if ((!tunnel_track && !made_wanted) || light.data != NULL || tri == NULL) return;
    TrackNodeLight made = { 0 };
    if (!make_light(tri, track, &made)) return;
    made_tri = tri, made_tunnel_track = tunnel_track;
    snprintf(made_track, sizeof made_track, "%s", track);
    install_made(&made, track, tunnel_track);
    port_log("light_effects: %s has no light on the CD, made from its terrain%s (%u nodes)\n", track, tunnel_track ? " and the road's tunnels" : "", light.nodes);
}

int drive_light_reload(void)
{
    if (made_tri == NULL || !synth_on) return 0;
    TrackNodeLight made = { 0 };
    if (!make_light(made_tri, made_track, &made)) return 0;
    install_made(&made, made_track, made_tunnel_track);
    ported_view_light_changed();   /* the segments built hold the old rows: the main pass builds them again (the follow pass does each frame) */
    port_log("light_effects: %s's light made again\n", made_track);
    return 1;
}
