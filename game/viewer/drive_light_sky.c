/* The baked light's files and the sky of the race's time (drive_light.c's split): the files' path, the tint and the sky read from the CD. */
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

/* the files' path: <psx_dir>/Z<TRACK><letter>.<ext> */
void file_path(char *out, size_t size, const char *psx_dir, const char *track, char letter, const char *ext)
{
    char upper[LIGHT_TRACK];
    size_t i = 0;
    for (; track[i] != '\0' && i < LIGHT_TRACK - 1; i++) upper[i] = (char)toupper((unsigned char)track[i]);
    upper[i] = '\0';
    snprintf(out, size, "%s/Z%s%c.%s", psx_dir, upper, letter, ext);
}

/* the LGS's ambient colour as a tint, neutral when the file is missing or does not read */
/* the most a night's ambient may carry (the mean of its channels): CY1's and TR1's nights, the darkest of the CD's (.45, .42) lowered to .35: TR2's valley still read as day at .45 */
const float NIGHT_TINT_CEILING = 0.35f;

void load_tint(const char *psx_dir, const char *track, char letter, int night, float ceiling)
{
    tint[0] = tint[1] = tint[2] = 1.0f;
    ambient_ok = 0;
    dash_ratio[0] = dash_ratio[1] = dash_ratio[2] = 1.0f;
    char path[LIGHT_PATH];
    file_path(path, sizeof path, psx_dir, track, letter, "LGS");
    size_t size = 0;
    uint8_t *file = platform_read_file(path, &size);
    if (file == NULL) {
        port_log("light_effects: no %s, the ambient stays neutral\n", path);
        return;
    }
    if (psx_lgs_parse(file, size, &ambient)) {
        if (night) light_tint_scale(&ambient, light.pool != NULL ? NIGHT_PALETTE : light_tint_night, 0);   /* the open road's: a tunnel's own lights (the contexts) stay lit, pools in the dark; with the pools the nodes carry the night, the ambient stays the dusk's */
        ambient_ok = 1, light_tint_script_tint(&ambient, tint);
        if (night) {   /* port: every night goes dark, the the console version's own brightest (TR2's orange dusk) too; the hue is kept */
            const float k = light_tint_night_ceiling_scale(tint, ceiling);
            if (k < 1.0f) {
                const float mul[3] = { k, k, k };
                light_tint_scale(&ambient, mul, 0);   /* a tunnel's own lights stay lit */
                light_tint_script_tint(&ambient, tint);
                port_log("light_effects: the night's tint brought under the ceiling (x%.2f)\n", k);
            }
        }
    }
    else port_log("light_effects: %s does not read, the ambient stays neutral\n", path);
    platform_free_file(file);
}

enum { NIGHT_STARS_SEED = 0x57a5 };

/* the night's darkness of the sky: light_tint_night, kept the same under the pools, whose palette is the dusk's (the sky is matched to it) */
void sky_scale_night(void)
{
    float mul[3];
    for (int k = 0; k < 3; k++) mul[k] = headlights_on ? light_tint_night[k] * light_tint_night[k] / NIGHT_PALETTE[k] : light_tint_night[k];
    sky_art_draw_scale(&sky_draw, mul);
    if (port_night_sky_hue()) sky_art_draw_rehue(&sky_draw, NIGHT_SKY_RAMP);   /* port (night_sky_hue): not the dusk art's orange and olive */
    if (headlights_on) {   /* port (headlight_beams): the night's stars in the dark of the sky */
        const int stars = sky_art_draw_stars(&sky_draw, NIGHT_STARS_SEED);
        port_log("psx_sky: stars %d, band rows 0..%d of horizon row %d (bitmap %dx%d), %d columns of land reach the band\n", stars, sky_draw.star_rows,
                 sky_draw.bitmap.horizon_row, sky_draw.bitmap.width, sky_draw.bitmap.height, sky_draw.star_land_columns);
    }
}

/* dawn and dusk: the the console version's own sky (Z<TRACK><A|C>.INF and .PSH) for the race; the day, a missing or unreadable file, or only the INF
 * leave the track's own sky (tinted with the world) */
void load_sky(const char *psx_dir, const char *track, char letter)
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
    if (sky_art_draw_make(&sky_draw, &sky, &inf)) {
        if (night_sky) sky_scale_night();
        ported_view_set_sky_art(&sky_draw);
        port_log("psx_sky: %dx%d panorama, gradient %d,%d,%d to %d,%d,%d\n", sky.width, sky.height, inf.top[0], inf.top[1], inf.top[2],
                 inf.bottom[0], inf.bottom[1], inf.bottom[2]);
    }
    psx_sky_free(&sky);
}
