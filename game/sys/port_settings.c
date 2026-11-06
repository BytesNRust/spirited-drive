/* port: the port's own settings file (port.cfg). */
#include "game/sys/port_settings.h"
#include "game/platform/platform.h"
#include "game/race/weather.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/viewer/screen_layout.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT_CFG_NAME "port.cfg"
#define FOV_MIN (-10)
#define FOV_MAX 20

static const char *const FORMAT_NAMES[] = { "4:3", "16:9", "21:9" };
static const char *const PSX_TIME_NAMES[] = { "day", "dawn", "dusk", "night" };   /* by PSX_TIME_* */

/* the PSX_TIME_* a name stands for ("dawn" or "a", "day" or "b", "dusk" or "c", "night", any case), or -1 */
static int psx_time_named(const char *name)
{
    char lower[8];
    size_t i = 0;
    for (; name[i] != '\0' && i < sizeof lower - 1; i++) lower[i] = (char)(name[i] >= 'A' && name[i] <= 'Z' ? name[i] + 32 : name[i]);
    lower[i] = '\0';
    if (name[i] != '\0') return -1;
    if (strcmp(lower, "day") == 0 || strcmp(lower, "b") == 0) return PSX_TIME_DAY;
    if (strcmp(lower, "dawn") == 0 || strcmp(lower, "a") == 0) return PSX_TIME_DAWN;
    if (strcmp(lower, "dusk") == 0 || strcmp(lower, "c") == 0) return PSX_TIME_DUSK;
    if (strcmp(lower, "night") == 0) return PSX_TIME_NIGHT;
    return -1;
}

PortSettings port_settings_defaults(void)
{
    /* the render threads are Auto by default: as many as the machine's logical cores, at most eight. The picture is the
     * same with any number of threads, and the wall is 1.7-3.5x quicker with them (re/notes/render-timing.md). */
    const PortSettings d = { .format = SCREEN_4_3, .refresh_hz = 60, .f3_ultra = 1, .render_scale = 1, .psx_time = PSX_TIME_DAY,
                             .weather = WEATHER_CLEAR, .render_bands = -1, .camera_kind = -1 };
    return d;
}

static bool parse_int(const char *value, long *out)
{
    char *end;
    *out = strtol(value, &end, 10);
    return end != value && *end == '\0';
}

static void apply(PortSettings *s, const char *key, const char *value)
{
    long n;
    if (strcmp(key, "format") == 0) {
        for (int i = 0; i < 3; i++)
            if (strcmp(value, FORMAT_NAMES[i]) == 0)
                s->format = (ScreenFormat)i;
    } else if (strcmp(key, "refresh_hz") == 0) {
        if (parse_int(value, &n) && (n == 30 || n == 60 || n == 120 || n == 144))
            s->refresh_hz = (int)n;
    } else if (strcmp(key, "scale") == 0) {
        if (strcmp(value, "fill") == 0)
            s->scale_integer = 0;
        else if (strcmp(value, "integer") == 0)
            s->scale_integer = 1;
    } else if (strcmp(key, "fullscreen") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->fullscreen = (int)n;
    } else if (strcmp(key, "fov_adjust") == 0) {
        if (parse_int(value, &n) && n >= FOV_MIN && n <= FOV_MAX)
            s->fov_adjust = (int)n;
    } else if (strcmp(key, "view_ultra") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->view_ultra = (int)n;
    } else if (strcmp(key, "f3_ultra") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->f3_ultra = (int)n;
    } else if (strcmp(key, "render_scale") == 0) {
        if (parse_int(value, &n) && n >= 1 && n <= 3)
            s->render_scale = (int)n;
    } else if (strcmp(key, "laps") == 0) {
        if (parse_int(value, &n) && n >= 0 && n <= 99)
            s->laps = (int)n;
    } else if (strcmp(key, "weather") == 0) {
        const int w = weather_named(value);
        if (w >= 0) s->weather = w;
    } else if (strcmp(key, "perspective") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->perspective = (int)n;
    } else if (strcmp(key, "texture_filter") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->texture_filter = (int)n;
    } else if (strcmp(key, "fog") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->fog = (int)n;
    } else if (strcmp(key, "psx_light") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->psx_light = (int)n;
    } else if (strcmp(key, "psx_headlights") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->psx_headlights = (int)n;
    } else if (strcmp(key, "psx_shadow") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->psx_shadow = (int)n;
    } else if (strcmp(key, "psx_time") == 0) {
        const int t = psx_time_named(value);
        if (t >= 0) s->psx_time = t;
    } else if (strcmp(key, "psx_dir") == 0) {
        snprintf(s->psx_dir, sizeof s->psx_dir, "%s", value);
    } else if (strcmp(key, "free_records") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->free_records = (int)n;
    } else if (strcmp(key, "fog_sample") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->fog_sample = (int)n;
    } else if (strcmp(key, "made_tunnels") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->made_tunnels = (int)n;
    } else if (strcmp(key, "psx_flares") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->psx_flares = (int)n;
    } else if (strcmp(key, "sky_fill_below") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->sky_fill_below = (int)n;
    } else if (strcmp(key, "radar_all_views") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->radar_all_views = (int)n;
    } else if (strcmp(key, "tach_dim") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->tach_dim = (int)n;
    } else if (strcmp(key, "gauge_wheel") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->gauge_wheel = (int)n;
    } else if (strcmp(key, "police_glow") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->police_glow = (int)n;
    } else if (strcmp(key, "car_tunnel_light") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->car_tunnel_light = (int)n;
    } else if (strcmp(key, "vegas_car_night") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->vegas_car_night = (int)n;
    } else if (strcmp(key, "rally_gravel_shoulder") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->rally_gravel_shoulder = (int)n;
    } else if (strcmp(key, "chase_height") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->chase_height = (int)n;
    } else if (strcmp(key, "lamp_pool_tint") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->lamp_pool_tint = (int)n;
    } else if (strcmp(key, "brake_over_gas") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->brake_over_gas = (int)n;
    } else if (strcmp(key, "lamp_pools") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->lamp_pools = (int)n;
    } else if (strcmp(key, "lamp_pool_bright") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->lamp_pool_bright = (int)n;
    } else if (strcmp(key, "headlight_cone") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->headlight_cone = (int)n;
    } else if (strcmp(key, "lamp_car_shade") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->lamp_car_shade = (int)n;
    } else if (strcmp(key, "tunnel_headlight") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->tunnel_headlight = (int)n;
    } else if (strcmp(key, "parking_dark") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->parking_dark = (int)n;
    } else if (strcmp(key, "forest_neutral") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->forest_neutral = (int)n;
    } else if (strcmp(key, "cavern_dark") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->cavern_dark = (int)n;
    } else if (strcmp(key, "vegas_objects") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->vegas_objects = (int)n;
    } else if (strcmp(key, "close_houses") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->close_houses = (int)n;
    } else if (strcmp(key, "reverse_rows") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->reverse_rows = (int)n;
    } else if (strcmp(key, "rain_lamps") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->rain_lamps = (int)n;
    } else if (strcmp(key, "gauge_exterior") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->gauge_exterior = (int)n;
    } else if (strcmp(key, "traffic_spray") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->traffic_spray = (int)n;
    } else if (strcmp(key, "rally_dry_lamps") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->rally_dry_lamps = (int)n;
    } else if (strcmp(key, "traffic_halo") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->traffic_halo = (int)n;
    } else if (strcmp(key, "police_halo") == 0) {
        if (parse_int(value, &n) && n >= 0 && n <= 3)
            s->police_halo = (int)n;
    } else if (strcmp(key, "rain_v2") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->rain_v2 = (int)n;
    } else if (strcmp(key, "shoulder_light") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->shoulder_light = (int)n;
    } else if (strcmp(key, "analog_pad") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->analog_pad = (int)n;
    } else if (strcmp(key, "sign_glow") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->sign_glow = (int)n;
    } else if (strcmp(key, "night_tone") == 0) {
        if (parse_int(value, &n) && n >= 0 && n <= 2)
            s->night_tone = (int)n;
    } else if (strcmp(key, "city_lift") == 0) {
        if (parse_int(value, &n) && n >= 0 && n <= 3)
            s->city_lift = (int)n;
    } else if (strcmp(key, "vegas_lift") == 0) {
        if (parse_int(value, &n) && n >= 0 && n <= 2)
            s->vegas_lift = (int)n;
    } else if (strcmp(key, "rain_fx") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->rain_fx = (int)n;
    } else if (strcmp(key, "rain_grip") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->rain_grip = (int)n;
    } else if (strcmp(key, "finish_gate_light") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->finish_gate_light = (int)n;
    } else if (strcmp(key, "gauge_light") == 0) {
        if (parse_int(value, &n) && n >= 0 && n <= 5)
            s->gauge_light = (int)n;
    } else if (strcmp(key, "canyon_shade") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->canyon_shade = (int)n;
    } else if (strcmp(key, "trans_shadows") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->trans_shadows = (int)n;
    } else if (strcmp(key, "headlight_relight") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->headlight_relight = (int)n;
    } else if (strcmp(key, "hall_light") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->hall_light = (int)n;
    } else if (strcmp(key, "replay_no_ffb") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->replay_no_ffb = (int)n;
    } else if (strcmp(key, "last_camera") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->last_camera = (int)n;
    } else if (strcmp(key, "camera_kind") == 0) {
        if (parse_int(value, &n) && n >= -1 && n <= 5)
            s->camera_kind = (int)n;
    } else if (strcmp(key, "sky_fill") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->sky_fill = (int)n;
    } else if (strcmp(key, "night_sky_hue") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->night_sky_hue = (int)n;
    } else if (strcmp(key, "forest_dark") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->forest_dark = (int)n;
    } else if (strcmp(key, "night_stretch_clean") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->night_stretch_clean = (int)n;
    } else if (strcmp(key, "car_colours") == 0) {
        if (parse_int(value, &n) && (n == 0 || n == 1))
            s->car_colours = (int)n;
    } else if (strcmp(key, "render_bands") == 0) {
        /* the render threads: "auto" for as many as the logical cores, or a count. Anything else keeps the default
         * (auto), so a damaged file cannot turn the threads off by accident. */
        if (strcmp(value, "auto") == 0)
            s->render_bands = -1;
        else if (parse_int(value, &n) && n >= 0 && n <= 8)
            s->render_bands = (int)n;
    } else if (strcmp(key, "profile") == 0) {
        const int p = port_profile_named(value);
        if (p >= 0) s->profile = p;
    } else if (strcmp(key, "body_shine") == 0) {
        if (parse_int(value, &n) && n >= 0 && n <= 4)
            s->body_shine = n == 0 ? 0 : 4;   /* off or the envmap: the strengths 1..3 of an older file are the envmap now */
    }
}

PortSettings port_settings_parse(const char *text, size_t size)
{
    PortSettings s = port_settings_defaults();
    size_t pos = 0;
    while (pos < size) {
        char line[PSX_DIR_MAX + 32];   /* port: wide enough for a psx_dir line, the other keys are short */
        size_t len = 0;
        while (pos < size && text[pos] != '\n') {
            if (len < sizeof line - 1)
                line[len++] = text[pos];
            pos++;
        }
        pos++;
        if (len > 0 && line[len - 1] == '\r')
            len--;
        line[len] = '\0';
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            apply(&s, line, eq + 1);
        }
    }
    return s;
}

size_t port_settings_format(const PortSettings *s, char *out, size_t size)
{
    /* the render threads: "auto" for the sentinel, the count for a number */
    char bands_text[8];
    const char *const bands = s->render_bands < 0 ? "auto" : bands_text;
    if (s->render_bands >= 0) snprintf(bands_text, sizeof bands_text, "%d", s->render_bands);
    const int n = snprintf(out, size,
                           "format=%s\nrefresh_hz=%d\nscale=%s\nfullscreen=%d\nfov_adjust=%d\nview_ultra=%d\n"
                           "perspective=%d\ntexture_filter=%d\nfog=%d\nf3_ultra=%d\nrender_scale=%d\nlaps=%d\npsx_dir=%s\npsx_light=%d\npsx_shadow=%d\npsx_time=%s\nweather=%s\n"
                           "render_bands=%s\nbody_shine=%d\nfree_records=%d\ncar_colours=%d\nfog_sample=%d\nsky_fill=%d\nsky_fill_below=%d\nradar_all_views=%d\ntach_dim=%d\ngauge_wheel=%d\npolice_glow=%d\nreplay_no_ffb=%d\nlast_camera=%d\ncamera_kind=%d\nnight_sky_hue=%d\nnight_stretch_clean=%d\npsx_headlights=%d\npsx_flares=%d\nmade_tunnels=%d\nheadlight_relight=%d\ncar_tunnel_light=%d\nvegas_car_night=%d\nrally_gravel_shoulder=%d\nlamp_pool_tint=%d\ntrans_shadows=%d\ncanyon_shade=%d\ntunnel_headlight=%d\nparking_dark=%d\nforest_neutral=%d\ncavern_dark=%d\nvegas_objects=%d\nclose_houses=%d\nbrake_over_gas=%d\nlamp_pools=%d\nlamp_car_shade=%d\nheadlight_cone=%d\nrain_fx=%d\nrain_grip=%d\ngauge_light=%d\nfinish_gate_light=%d\nlamp_pool_bright=%d\nnight_tone=%d\nrain_lamps=%d\ngauge_exterior=%d\nsign_glow=%d\nreverse_rows=%d\ncity_lift=%d\nvegas_lift=%d\nchase_height=%d\nhall_light=%d\nforest_dark=%d\ntraffic_spray=%d\nrally_dry_lamps=%d\ntraffic_halo=%d\npolice_halo=%d\nanalog_pad=%d\nrain_v2=%d\nshoulder_light=%d\nprofile=%s\n",
                           FORMAT_NAMES[s->format], s->refresh_hz, s->scale_integer ? "integer" : "fill",
                           s->fullscreen, s->fov_adjust, s->view_ultra, s->perspective, s->texture_filter, s->fog, s->f3_ultra,
                           s->render_scale, s->laps, s->psx_dir, s->psx_light, s->psx_shadow, PSX_TIME_NAMES[s->psx_time], weather_name(s->weather), bands, s->body_shine,
                           s->free_records, s->car_colours, s->fog_sample, s->sky_fill, s->sky_fill_below, s->radar_all_views, s->tach_dim, s->gauge_wheel, s->police_glow, s->replay_no_ffb, s->last_camera, s->camera_kind, s->night_sky_hue, s->night_stretch_clean, s->psx_headlights, s->psx_flares, s->made_tunnels, s->headlight_relight, s->car_tunnel_light, s->vegas_car_night, s->rally_gravel_shoulder, s->lamp_pool_tint, s->trans_shadows, s->canyon_shade, s->tunnel_headlight, s->parking_dark, s->forest_neutral, s->cavern_dark, s->vegas_objects, s->close_houses, s->brake_over_gas, s->lamp_pools, s->lamp_car_shade, s->headlight_cone, s->rain_fx, s->rain_grip, s->gauge_light, s->finish_gate_light, s->lamp_pool_bright, s->night_tone, s->rain_lamps, s->gauge_exterior, s->sign_glow, s->reverse_rows, s->city_lift, s->vegas_lift, s->chase_height, s->hall_light, s->forest_dark, s->traffic_spray, s->rally_dry_lamps, s->traffic_halo, s->police_halo, s->analog_pad, s->rain_v2, s->shoulder_light, port_profile_key((Profile)s->profile));
    return n > 0 && (size_t)n < size ? (size_t)n : 0;
}

/* whether `text` has a line starting with `key=` */
static bool names(const char *text, size_t size, const char *key)
{
    const size_t len = strlen(key);
    for (size_t pos = 0; pos < size; pos++) {
        const size_t start = pos;
        while (pos < size && text[pos] != '\n')
            pos++;
        const size_t line = pos - start;
        if (line > len && strncmp(text + start, key, len) == 0 && text[start + len] == '=')
            return true;
    }
    return false;
}

static bool (*display_probe)(int *width, int *height);

void port_settings_set_display_probe(bool (*probe)(int *width, int *height)) { display_probe = probe; }

ScreenFormat port_monitor_format(ScreenFormat fallback)
{
    int w = 0, h = 0;
    return display_probe != NULL && display_probe(&w, &h) && w > 0 && h > 0 ? port_profile_monitor_format(w, h) : fallback;
}

/* port.cfg: the mode's name and the machine's folder */
static PortSettings machine_load(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(PORT_CFG_NAME, &size);
    if (data == NULL) return port_settings_defaults();
    const PortSettings s = port_settings_parse((const char *)data, size);
    platform_free_file(data);
    return s;
}

bool port_machine_save(int profile, const char *psx_dir)
{
    char text[PSX_DIR_MAX + 64];
    if (strpbrk(psx_dir, "\r\n") != NULL) {   /* a line end would write another key */
        port_log("settings: the PS1 folder has a line end, not saved\n");
        return false;
    }
    const int n = snprintf(text, sizeof text, "profile=%s\npsx_dir=%s\n", port_profile_key((Profile)profile), psx_dir);
    return n > 0 && (size_t)n < sizeof text && platform_write_user_file(PORT_CFG_NAME, (const uint8_t *)text, (size_t)n);
}

/* the values of mode `p`'s table, with the machine's own */
PortSettings port_mode_values(int p, const char *psx_dir)
{
    PortSettings s = port_profile_settings((Profile)p, port_settings_defaults(), port_monitor_format(SCREEN_16_9));
    s.profile = p;
    snprintf(s.psx_dir, sizeof s.psx_dir, "%s", psx_dir);
    return s;
}

void port_settings_take(void *to, const char *key, const char *value) { apply(to, key, value); }

PortSettings port_settings_of_profile(int profile)
{
    const PortSettings machine = machine_load();
    PortSettings s = port_mode_values(profile, machine.psx_dir);
    size_t size = 0;
    uint8_t *data = platform_read_user_file(port_profile_file((Profile)profile), &size);
    if (data == NULL) return s;
    port_profile_read_free((Profile)profile, (const char *)data, size, port_settings_take, &s);
    platform_free_file(data);
    return s;
}

PortSettings port_settings_load(void) { return port_settings_of_profile(machine_load().profile); }

int port_settings_profile(void) { return machine_load().profile; }

bool port_settings_profile_chosen(void)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(PORT_CFG_NAME, &size);
    if (data == NULL) return false;
    const bool chosen = names((const char *)data, size, "profile");
    platform_free_file(data);
    return chosen;
}

bool port_mode_write(const PortSettings *s, const PortExtras *e, const char *keep, int *dropped)
{
    const Profile p = (Profile)s->profile;
    const PortSettings base = port_mode_values(s->profile, s->psx_dir);
    const PortExtras extras_base = port_profile_extras(p, port_extras_defaults());
    char now[PORT_CFG_TEXT_MAX], was[PORT_CFG_TEXT_MAX], extras_now[PORT_EXTRAS_TEXT_MAX], extras_was[PORT_EXTRAS_TEXT_MAX];
    char out[PORT_CFG_TEXT_MAX + PORT_EXTRAS_TEXT_MAX];
    if (port_settings_format(s, now, sizeof now) == 0 || port_settings_format(&base, was, sizeof was) == 0 ||
        port_extras_format(e, extras_now, sizeof extras_now) == 0 || port_extras_format(&extras_base, extras_was, sizeof extras_was) == 0) return false;
    const long settings = port_profile_differences(p, was, now, keep, out, sizeof out, dropped);
    if (settings < 0) return false;
    const long extras = port_profile_differences(p, extras_was, extras_now, keep, out + settings, sizeof out - (size_t)settings, dropped);
    if (extras < 0) return false;
    size_t n = (size_t)settings + (size_t)extras;
    if (n == 0) out[n++] = '\n';   /* nothing changed: an empty file, the mode's values */
    return platform_write_user_file(port_profile_file(p), (const uint8_t *)out, n);
}

bool port_mode_save(const PortSettings *s, const PortExtras *e)
{
    size_t size = 0;
    uint8_t *data = platform_read_user_file(port_profile_file((Profile)s->profile), &size);
    char *keep = data != NULL ? malloc(size + 1) : NULL;
    if (keep != NULL) {
        memcpy(keep, data, size);
        keep[size] = '\0';
    }
    if (data != NULL) platform_free_file(data);
    int dropped = 0;
    const bool written = port_mode_write(s, e, keep, &dropped);
    free(keep);
    return written;   /* what was not free in the mode is in the log (port_profile_differences) */
}

bool port_settings_save(const PortSettings *s)
{
    const PortSettings machine = machine_load();
    if (s->profile != machine.profile) {   /* a change of mode is port_profile_select's */
        port_log("settings: not saved: these are the settings of %s, the mode is %s\n", port_profile_key((Profile)s->profile), port_profile_key((Profile)machine.profile));
        return false;
    }
    if ((!port_settings_profile_chosen() || strcmp(s->psx_dir, machine.psx_dir) != 0) && !port_machine_save(s->profile, s->psx_dir)) return false;
    const PortExtras e = port_extras_load();
    return port_mode_save(s, &e);
}

bool port_profile_select(int p)
{
    if (p < 0 || p >= PROFILE_COUNT) return false;
    return port_machine_save(p, machine_load().psx_dir);
}

bool port_profile_reset(int p)
{
    if (p < 0 || p >= PROFILE_COUNT) return false;
    static const uint8_t EMPTY[] = "\n";
    return platform_write_user_file(port_profile_file((Profile)p), EMPTY, 1);
}

PlatformConfig port_platform_config(const char *title)
{
    const PortSettings s = port_settings_load();
    return (PlatformConfig){ screen_width(s.format), SCREEN_HEIGHT, title, s.scale_integer, s.fullscreen, s.refresh_hz };
}

int port_weather(void)
{
    const char *v = getenv("TNFS_WEATHER");
    const int w = v != NULL ? weather_named(v) : -1;
    return w >= 0 ? w : port_settings_load().weather;
}

int port_render_scale(void)
{
    const char *v = getenv("TNFS_RENDER_SCALE");
    if (v != NULL && v[0] >= '1' && v[0] <= '3' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().render_scale;
}

int port_psx_time(void)
{
    const char *v = getenv("TNFS_PSX_TIME");
    const int t = v != NULL ? psx_time_named(v) : -1;
    return t >= 0 ? t : port_settings_load().psx_time;
}

int port_fog_sample(void)
{
    const char *v = getenv("TNFS_FOG_SAMPLE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().fog_sample;
}

int port_fog_lift(void)
{
    const char *v = getenv("TNFS_FOG_LIFT");
    char *end;
    const long n = v != NULL ? strtol(v, &end, 10) : -1;
    return v != NULL && end != v && *end == '\0' && n >= 0 && n <= 100 ? (int)n : FOG_SAMPLE_LIFT_DEFAULT;
}

int port_two_sided_scenery(int automatic)
{
    const char *v = getenv("TNFS_TWO_SIDED");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return automatic != 0;
}

int port_close_boxes(int automatic)
{
    const char *v = getenv("TNFS_CLOSE_BOXES");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return automatic != 0;
}

int port_made_tunnels(void)
{
    if (!port_psx_light()) return 0;
    const char *v = getenv("TNFS_MADE_TUNNELS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().made_tunnels;
}

int port_psx_flares(void)
{
    if (!port_psx_light()) return 0;   /* the halo is the PS1 look's */
    const char *v = getenv("TNFS_PSX_FLARES");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().psx_flares;
}

int port_car_tunnel_light(void)
{
    if (!port_psx_light()) return 0;
    const char *v = getenv("TNFS_CAR_TUNNEL_LIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().car_tunnel_light;
}

int port_vegas_car_night(void)
{
    if (!port_psx_light()) return 0;
    const char *v = getenv("TNFS_VEGAS_CAR_NIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().vegas_car_night;
}

int port_rally_gravel_shoulder(void)
{
    const char *v = getenv("TNFS_RALLY_GRAVEL_SHOULDER");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rally_gravel_shoulder;
}

int port_chase_height(void)
{
    const char *v = getenv("TNFS_CHASE_HEIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().chase_height;
}

int port_lamp_pool_tint(void)
{
    if (!port_psx_headlights()) return 0;
    const char *v = getenv("TNFS_LAMP_POOL_TINT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().lamp_pool_tint;
}

int port_trans_shadows(void)
{
    if (!port_psx_light()) return 0;
    const char *v = getenv("TNFS_TRANS_SHADOWS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().trans_shadows;
}

int port_canyon_shade(void)
{
    if (!port_car_tunnel_light()) return 0;
    const char *v = getenv("TNFS_CANYON_SHADE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().canyon_shade;
}

int port_tunnel_headlight(void)
{
    if (!port_psx_headlights()) return 0;
    const char *v = getenv("TNFS_TUNNEL_HEADLIGHT");
    const int on = v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0' ? v[0] - '0' : port_settings_load().tunnel_headlight;
    return on ? 180 : 0;
}

int port_parking_dark(void)
{
    if (!port_car_tunnel_light()) return 0;
    const char *v = getenv("TNFS_PARKING_DARK");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().parking_dark;
}

int port_forest_neutral(void)
{
    if (!port_psx_light()) return 0;
    const char *v = getenv("TNFS_FOREST_NEUTRAL");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().forest_neutral;
}

int port_cavern_dark(void)
{
    if (!port_car_tunnel_light()) return 0;
    const char *v = getenv("TNFS_CAVERN_DARK");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().cavern_dark;
}

int port_vegas_objects(void)
{
    if (!port_psx_light()) return 0;
    const char *v = getenv("TNFS_VEGAS_OBJECTS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().vegas_objects;
}

int port_close_houses(void)
{
    const char *v = getenv("TNFS_CLOSE_HOUSES");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().close_houses;
}

int port_brake_over_gas(void)
{
    const char *v = getenv("TNFS_BRAKE_OVER_GAS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().brake_over_gas;
}

int port_lamp_pools(void)
{
    if (!port_psx_headlights()) return 0;
    const char *v = getenv("TNFS_LAMP_POOLS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().lamp_pools;
}

int port_lamp_car_shade(void)
{
    if (!port_psx_headlights()) return 0;
    const char *v = getenv("TNFS_LAMP_CAR_SHADE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().lamp_car_shade;
}

int port_rain_grip_active(void)
{
    return port_rain_grip() && weather_wet(port_weather());
}

int port_gauge_light(void)
{
    const char *v = getenv("TNFS_GAUGE_LIGHT");
    if (v != NULL && v[0] >= '0' && v[0] <= '5' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().gauge_light;
}

int port_finish_gate_light(void)
{
    const char *v = getenv("TNFS_FINISH_GATE_LIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().finish_gate_light;
}

int port_rain_grip(void)
{
    const char *v = getenv("TNFS_RAIN_GRIP");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rain_grip;
}

int port_rain_fx(void)
{
    const char *v = getenv("TNFS_RAIN_FX");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rain_fx;
}

/* Perspective Correction on: the cone is drawn on Modern 3D's road (TNFS_MODERN_3D overrides the setting) */
static int perspective_on(void)
{
    const char *v = getenv("TNFS_MODERN_3D");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().perspective;
}

int port_lamp_pool_bright(void)
{
    if (!port_psx_headlights()) return 0;
    const char *v = getenv("TNFS_LAMP_POOL_BRIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().lamp_pool_bright;
}

int port_headlight_cone(void)
{
    if (!port_psx_headlights() || !perspective_on()) return 0;   /* the cone needs perspective (Modern 3D): without it the node pool, as the exe's */
    const char *v = getenv("TNFS_HEADLIGHT_CONE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().headlight_cone;
}

int port_reverse_rows(void)
{
    const char *v = getenv("TNFS_REVERSE_ROWS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().reverse_rows;
}

int port_rain_lamps(void)
{
    const char *v = getenv("TNFS_RAIN_LAMPS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rain_lamps;
}

int port_traffic_spray(void)
{
    const char *v = getenv("TNFS_TRAFFIC_SPRAY");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().traffic_spray;
}

int port_rally_dry_lamps(void)
{
    const char *v = getenv("TNFS_RALLY_DRY_LAMPS");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rally_dry_lamps;
}

int port_traffic_halo(void)
{
    const char *v = getenv("TNFS_TRAFFIC_HALO");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().traffic_halo;
}

int port_rain_v2(void)
{
    const char *v = getenv("TNFS_RAIN_V2");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().rain_v2;
}

int port_shoulder_light(void)
{
    const char *v = getenv("TNFS_SHOULDER_LIGHT");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().shoulder_light;
}

int port_police_halo(void)
{
    const char *v = getenv("TNFS_POLICE_HALO");
    if (v != NULL && v[0] >= '0' && v[0] <= '3' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().police_halo;
}

int port_analog_pad(void)
{
    const char *v = getenv("TNFS_ANALOG_PAD");
    if (v != NULL && v[0] >= '0' && v[0] <= '1' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().analog_pad;
}

int port_sign_glow(void)
{
    if (!port_psx_headlights()) return 0;
    const char *v = getenv("TNFS_SIGN_GLOW");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().sign_glow;
}

int port_gauge_exterior(void)
{
    const char *v = getenv("TNFS_GAUGE_EXTERIOR");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().gauge_exterior;
}

int port_night_tone(void)
{
    const char *v = getenv("TNFS_NIGHT_TONE");
    if (v != NULL && v[0] >= '0' && v[0] <= '2' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().night_tone;
}

int port_city_lift(void)
{
    if (!port_psx_light()) return 0;
    const char *v = getenv("TNFS_CITY_LIFT");
    if (v != NULL && v[0] >= '0' && v[0] <= '3' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().city_lift;
}

int port_vegas_lift(void)
{
    if (!port_psx_light()) return 0;
    const char *v = getenv("TNFS_VEGAS_LIFT");
    if (v != NULL && v[0] >= '0' && v[0] <= '2' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().vegas_lift;
}

int port_headlight_relight(void)
{
    if (!port_psx_headlights()) return 0;
    const char *v = getenv("TNFS_HEADLIGHT_RELIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().headlight_relight;
}

int port_hall_light(void)
{
    if (!port_psx_light()) return 0;   /* the PS1 look's */
    const char *v = getenv("TNFS_HALL_LIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().hall_light;
}

int port_replay_no_ffb(void)
{
    const char *v = getenv("TNFS_REPLAY_NO_FFB");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().replay_no_ffb;
}

int port_last_camera(void)
{
    const char *v = getenv("TNFS_LAST_CAMERA");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().last_camera;
}

int sky_fill_below_applies(const PortSettings *s) { return s->sky_fill_below != 0 && s->format != SCREEN_4_3; }

int port_sky_fill_below(void)
{
    PortSettings s = port_settings_load();
    const char *v = getenv("TNFS_SKY_FILL_BELOW");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') s.sky_fill_below = v[0] - '0';
    return sky_fill_below_applies(&s);
}

int port_radar_all_views(void)
{
    const char *v = getenv("TNFS_RADAR_ALL_VIEWS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().radar_all_views;
}

int port_tach_dim(void)
{
    const char *v = getenv("TNFS_TACH_DIM");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().tach_dim;
}

int port_gauge_wheel(void)
{
    const char *v = getenv("TNFS_GAUGE_WHEEL");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().gauge_wheel;
}

int port_police_glow(void)
{
    const char *v = getenv("TNFS_POLICE_GLOW");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().police_glow;
}

int port_sky_fill(void)
{
    const char *v = getenv("TNFS_SKY_FILL");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().sky_fill;
}

int port_night_sky_hue(void)
{
    const char *v = getenv("TNFS_NIGHT_SKY_HUE");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().night_sky_hue;
}

int port_forest_dark(void)
{
    const char *v = getenv("TNFS_FOREST_DARK");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().forest_dark;
}

int port_night_stretch_clean(void)
{
    const char *v = getenv("TNFS_NIGHT_STRETCH_CLEAN");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().night_stretch_clean;
}

int port_sky_fill_mode(void)
{
    const char *v = getenv("TNFS_SKY_FILL_MODE");
    if (v != NULL && (strcmp(v, "a") == 0 || strcmp(v, "1") == 0)) return 1;
    if (v != NULL && (strcmp(v, "b") == 0 || strcmp(v, "2") == 0)) return 2;
    return SKY_FILL_MODE_DEFAULT;
}

int port_free_records(void)
{
    const char *v = getenv("TNFS_FREE_RECORDS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().free_records;
}

int port_car_colours(void)
{
    const char *v = getenv("TNFS_CAR_COLOURS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().car_colours;
}

int port_psx_headlights(void)
{
    const char *v = getenv("TNFS_PSX_HEADLIGHTS");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().psx_headlights;
}

int port_psx_shadow(void)
{
    const char *v = getenv("TNFS_PSX_SHADOW");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().psx_shadow;
}

int port_emissive_lamps(void)
{
    const char *v = getenv("TNFS_EMISSIVE_LAMPS");   /* a dev override; the game follows the Graphics Light Effects switch */
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_psx_light();
}

int port_body_shine(void)
{
    const char *v = getenv("TNFS_BODY_SHINE");
    if (v != NULL && v[0] >= '0' && v[0] <= '4' && v[1] == '\0') return v[0] - '0';
    return port_settings_load().body_shine;
}

int port_psx_light(void)
{
    const char *v = getenv("TNFS_PSX_LIGHT");
    if (v != NULL && (v[0] == '0' || v[0] == '1') && v[1] == '\0') return v[0] - '0';
    return port_settings_load().psx_light;
}
