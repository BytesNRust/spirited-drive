/* The port's settings: the keys, the parse and format of port.cfg and the monitor's format (port_settings.h; split from port_settings.c by responsibility). */
#include "game/sys/port_settings.h"
#include "game/platform/platform.h"
#include "game/race/session_rules.h"
#include "game/race/weather.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_log.h"
#include "game/sys/port_profile.h"
#include "game/viewer/screen_layout.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game/sys/port_settings_private.h"

static const char *const FORMAT_NAMES[] = { "4:3", "16:9", "21:9" };
static const char *const LIGHT_TIME_NAMES[] = { "day", "dawn", "dusk", "night" };   /* by LIGHT_TIME_* */

/* the LIGHT_TIME_* a name stands for ("dawn" or "a", "day" or "b", "dusk" or "c", "night", any case), or -1 */
int port_settings_light_time_named(const char *name)
{
    char lower[8];
    size_t i = 0;
    for (; name[i] != '\0' && i < sizeof lower - 1; i++) lower[i] = (char)(name[i] >= 'A' && name[i] <= 'Z' ? name[i] + 32 : name[i]);
    lower[i] = '\0';
    if (name[i] != '\0') return -1;
    if (strcmp(lower, "day") == 0 || strcmp(lower, "b") == 0) return LIGHT_TIME_DAY;
    if (strcmp(lower, "dawn") == 0 || strcmp(lower, "a") == 0) return LIGHT_TIME_DAWN;
    if (strcmp(lower, "dusk") == 0 || strcmp(lower, "c") == 0) return LIGHT_TIME_DUSK;
    if (strcmp(lower, "night") == 0) return LIGHT_TIME_NIGHT;
    return -1;
}

PortSettings port_settings_defaults(void)
{
    /* the render threads are Auto by default: as many as the machine's logical cores, at most eight. The picture is the
     * same with any number of threads, and the wall is 1.7-3.5x quicker with them. */
    const PortSettings d = { .format = SCREEN_4_3, .refresh_hz = 60, .f3_ultra = 1, .render_scale = 1, .light_time = LIGHT_TIME_DAY,
                             .weather = WEATHER_CLEAR, .render_bands = -1, .camera_kind = -1 };
    return d;
}

static bool parse_int(const char *value, long *out)
{
    char *end;
    *out = strtol(value, &end, 10);
    return end != value && *end == '\0';
}

/* port: the keys that are a whole number in a range, by name and where the field is; a value that is not a number, or is out of the range, leaves the field as it was.
 * (A plain list, so a new key is one line, and test_port_settings walks it: each one is written by port_settings_format and read back.) */
static const PortSettingsKey INT_KEYS[] = {
    { "fullscreen", offsetof(PortSettings, fullscreen), 0, 1 },
    { "fov_adjust", offsetof(PortSettings, fov_adjust), FOV_MIN, FOV_MAX },
    { "view_ultra", offsetof(PortSettings, view_ultra), 0, 1 },
    { "f3_ultra", offsetof(PortSettings, f3_ultra), 0, 1 },
    { "render_scale", offsetof(PortSettings, render_scale), 1, 3 },
    { "laps", offsetof(PortSettings, laps), 0, 99 },
    { "perspective", offsetof(PortSettings, perspective), 0, 1 },
    { "texture_filter", offsetof(PortSettings, texture_filter), 0, 1 },
    { "fog", offsetof(PortSettings, fog), 0, 1 },
    { "light_effects", offsetof(PortSettings, light_effects), 0, 1 },
    { "headlight_beams", offsetof(PortSettings, headlight_beams), 0, 1 },
    { "car_soft_shadow", offsetof(PortSettings, car_soft_shadow), 0, 1 },
    { "free_records", offsetof(PortSettings, free_records), 0, 1 },
    { "fog_sample", offsetof(PortSettings, fog_sample), 0, 1 },
    { "lamp_flares", offsetof(PortSettings, lamp_flares), 0, 1 },
    { "sky_fill_below", offsetof(PortSettings, sky_fill_below), 0, 1 },
    { "radar_all_views", offsetof(PortSettings, radar_all_views), 0, 1 },
    { "tach_dim", offsetof(PortSettings, tach_dim), 0, 1 },
    { "gauge_wheel", offsetof(PortSettings, gauge_wheel), 0, 1 },
    { "police_glow", offsetof(PortSettings, police_glow), 0, 1 },
    { "rally_gravel_shoulder", offsetof(PortSettings, rally_gravel_shoulder), 0, 1 },
    { "chase_height", offsetof(PortSettings, chase_height), 0, 1 },
    { "brake_over_gas", offsetof(PortSettings, brake_over_gas), 0, 1 },
    { "headlight_cone", offsetof(PortSettings, headlight_cone), 0, 1 },
    { "close_houses", offsetof(PortSettings, close_houses), 0, 1 },
    { "reverse_rows", offsetof(PortSettings, reverse_rows), 0, 1 },
    { "rain_lamps", offsetof(PortSettings, rain_lamps), 0, 1 },
    { "gauge_exterior", offsetof(PortSettings, gauge_exterior), 0, 1 },
    { "rally_dry_lamps", offsetof(PortSettings, rally_dry_lamps), 0, 1 },
    { "traffic_halo", offsetof(PortSettings, traffic_halo), 0, 1 },
    { "police_halo", offsetof(PortSettings, police_halo), 0, 3 },
    { "rain_v2", offsetof(PortSettings, rain_v2), 0, 1 },
    { "rain_tyre_roll", offsetof(PortSettings, rain_tyre_roll), 0, 1 },
    { "snow_weather", offsetof(PortSettings, snow_weather), 0, 1 },
    { "analog_pad", offsetof(PortSettings, analog_pad), 0, 1 },
    { "sign_glow", offsetof(PortSettings, sign_glow), 0, 1 },
    { "night_tone", offsetof(PortSettings, night_tone), 0, 2 },
    { "city_lift", offsetof(PortSettings, city_lift), 0, 3 },
    { "vegas_lift", offsetof(PortSettings, vegas_lift), 0, 2 },
    { "rain_fx", offsetof(PortSettings, rain_fx), 0, 1 },
    { "rain_grip", offsetof(PortSettings, rain_grip), 0, 1 },
    { "finish_gate_light", offsetof(PortSettings, finish_gate_light), 0, 1 },
    { "gauge_light", offsetof(PortSettings, gauge_light), 0, 5 },
    { "replay_no_ffb", offsetof(PortSettings, replay_no_ffb), 0, 1 },
    { "last_camera", offsetof(PortSettings, last_camera), 0, 1 },
    { "camera_kind", offsetof(PortSettings, camera_kind), -1, 5 },
    { "sky_fill", offsetof(PortSettings, sky_fill), 0, 1 },
    { "car_colours", offsetof(PortSettings, car_colours), 0, 1 },
    { "reflectors", offsetof(PortSettings, reflectors), 0, 1 },
    { "handbrake_lamps", offsetof(PortSettings, handbrake_lamps), 0, 1 },
    { "rally_spray", offsetof(PortSettings, rally_spray), 0, 1 },
    { "wipers", offsetof(PortSettings, wipers), 0, 1 },
    { "weather_fog", offsetof(PortSettings, weather_fog), 0, 2 },
    { "spray_v4", offsetof(PortSettings, spray_v4), 0, 1 },
    { "car_night_lift", offsetof(PortSettings, car_night_lift), 0, 2 },
    { "menu_pad_cancel", offsetof(PortSettings, menu_pad_cancel), 0, 1 },
    { "tunnel_lamps", offsetof(PortSettings, tunnel_lamps), 0, 2 },
    { "wrong_way", offsetof(PortSettings, wrong_way), 0, 1 },
    { "light_edit", offsetof(PortSettings, light_edit), 0, 1 },
    { "chase_steady", offsetof(PortSettings, chase_steady), 0, 1 },
    { "snow_fx", offsetof(PortSettings, snow_fx), 0, 1 },
    { "lamp_halos", offsetof(PortSettings, lamp_halos), 0, 1 },
};

const PortSettingsKey *port_settings_int_keys(int *count)
{
    *count = (int)(sizeof INT_KEYS / sizeof INT_KEYS[0]);
    return INT_KEYS;
}

/* the keys that are not a plain range: a name, a set of values, a text, or a table of their own */
static void apply_named(PortSettings *s, const char *key, const char *value)
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
    } else if (strcmp(key, "weather") == 0) {
        const int w = weather_named(value);
        if (w >= 0) s->weather = w;
    } else if (strcmp(key, "light_time") == 0) {
        const int t = port_settings_light_time_named(value);
        if (t >= 0) s->light_time = t;
    } else if (strcmp(key, "psx_dir") == 0) {
        snprintf(s->psx_dir, sizeof s->psx_dir, "%s", value);
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

static void apply(PortSettings *s, const char *key, const char *value)
{
    key = port_profile_key_alias(key);   /* the keys an older file has (psx_light, psx_time ...) */
    for (size_t i = 0; i < sizeof INT_KEYS / sizeof INT_KEYS[0]; i++) {
        if (strcmp(key, INT_KEYS[i].key) != 0) continue;
        long n;
        if (parse_int(value, &n) && n >= INT_KEYS[i].min && n <= INT_KEYS[i].max) {
            int *const field = (int *)(void *)((char *)s + INT_KEYS[i].offset);
            *field = (int)n;
        }
        return;
    }
    apply_named(s, key, value);
}

void port_settings_take(void *to, const char *key, const char *value) { apply(to, key, value); }


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
                           "perspective=%d\ntexture_filter=%d\nfog=%d\nf3_ultra=%d\nrender_scale=%d\nlaps=%d\npsx_dir=%s\nlight_effects=%d\ncar_soft_shadow=%d\nlight_time=%s\nweather=%s\n"
                           "render_bands=%s\nbody_shine=%d\nfree_records=%d\ncar_colours=%d\nfog_sample=%d\nsky_fill=%d\nsky_fill_below=%d\nradar_all_views=%d\ntach_dim=%d\ngauge_wheel=%d\npolice_glow=%d\nreplay_no_ffb=%d\nlast_camera=%d\ncamera_kind=%d\nheadlight_beams=%d\nlamp_flares=%d\nrally_gravel_shoulder=%d\nclose_houses=%d\nbrake_over_gas=%d\nheadlight_cone=%d\nrain_fx=%d\nrain_grip=%d\ngauge_light=%d\nfinish_gate_light=%d\nnight_tone=%d\nrain_lamps=%d\ngauge_exterior=%d\nsign_glow=%d\nreverse_rows=%d\ncity_lift=%d\nvegas_lift=%d\nchase_height=%d\nrally_dry_lamps=%d\ntraffic_halo=%d\npolice_halo=%d\nanalog_pad=%d\nrain_v2=%d\nrain_tyre_roll=%d\nsnow_weather=%d\nreflectors=%d\nhandbrake_lamps=%d\nrally_spray=%d\nsnow_fx=%d\nlamp_halos=%d\nwipers=%d\nweather_fog=%d\nspray_v4=%d\ncar_night_lift=%d\nmenu_pad_cancel=%d\ntunnel_lamps=%d\nwrong_way=%d\nlight_edit=%d\nchase_steady=%d\nprofile=%s\n",
                           FORMAT_NAMES[s->format], s->refresh_hz, s->scale_integer ? "integer" : "fill",
                           s->fullscreen, s->fov_adjust, s->view_ultra, s->perspective, s->texture_filter, s->fog, s->f3_ultra,
                           s->render_scale, s->laps, s->psx_dir, s->light_effects, s->car_soft_shadow, LIGHT_TIME_NAMES[s->light_time], weather_name(s->weather), bands, s->body_shine,
                           s->free_records, s->car_colours, s->fog_sample, s->sky_fill, s->sky_fill_below, s->radar_all_views, s->tach_dim, s->gauge_wheel, s->police_glow, s->replay_no_ffb, s->last_camera, s->camera_kind, s->headlight_beams, s->lamp_flares, s->rally_gravel_shoulder, s->close_houses, s->brake_over_gas, s->headlight_cone, s->rain_fx, s->rain_grip, s->gauge_light, s->finish_gate_light, s->night_tone, s->rain_lamps, s->gauge_exterior, s->sign_glow, s->reverse_rows, s->city_lift, s->vegas_lift, s->chase_height, s->rally_dry_lamps, s->traffic_halo, s->police_halo, s->analog_pad, s->rain_v2, s->rain_tyre_roll, s->snow_weather, s->reflectors, s->handbrake_lamps, s->rally_spray, s->snow_fx, s->lamp_halos, s->wipers, s->weather_fog, s->spray_v4, s->car_night_lift, s->menu_pad_cancel, s->tunnel_lamps, s->wrong_way, s->light_edit, s->chase_steady, port_profile_key((Profile)s->profile));
    return n > 0 && (size_t)n < size ? (size_t)n : 0;
}

static bool (*display_probe)(int *width, int *height);

void port_settings_set_display_probe(bool (*probe)(int *width, int *height)) { display_probe = probe; }

ScreenFormat port_monitor_format(ScreenFormat fallback)
{
    int w = 0, h = 0;
    return display_probe != NULL && display_probe(&w, &h) && w > 0 && h > 0 ? port_profile_monitor_format(w, h) : fallback;
}
